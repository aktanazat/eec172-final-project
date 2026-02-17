# ParkPilot: Autonomous Obstacle-Avoidance & Self-Parking RC Car with Cloud Telemetry

**EEC172 Final Project Proposal**
**Team Members:** [Your Names]
**Date:** February 2026

---

## 1. Description (Product Overview)

ParkPilot is an autonomous RC car platform built around the CC3200 microcontroller that can navigate obstacles, parallel-park itself, and stream live telemetry to the cloud. The car operates in three user-selectable modes controlled via an IR remote: manual drive, autonomous obstacle avoidance, and self-parking. Four ultrasonic sensors provide 360-degree proximity awareness, rendered in real-time as a top-down radar map on a 128x128 OLED display. An Arduino co-processor handles time-critical sensor reads and motor actuation, communicating with the CC3200 over UART.

AWS IoT provides the cloud backbone. The device shadow stores live telemetry (sensor distances, mode, speed). An IoT Rule triggers a Lambda function that processes sensor snapshots into a parking guidance image, stored in S3 and fetched by the car for OLED display. A separate SNS rule sends email alerts when the on-board accelerometer detects a collision event. This creates a full edge-to-cloud-to-edge data loop representative of real-world autonomous vehicle architectures.

**Similar existing products:** Existing hobby autonomous cars (Donkey Car, AWS DeepRacer) rely on cameras and ML on powerful compute platforms. ParkPilot is different because it uses only ultrasonic ranging on a constrained embedded MCU, implements a classical control-algorithm approach to parking, and offloads path computation to a serverless cloud function -- demonstrating IoT principles rather than ML.

---

## 2. Design

### 2.1 Functional Specification

#### State Machine

```
                         [POWER ON / BOOT]
                               |
                        Flash boot, WiFi connect,
                        AWS shadow sync, init sensors
                               |
                               v
          +--------------> [IDLE] <-----------------+
          |                  |  |                    |
          |       IR: MODE1  |  |  IR: MODE2        |
          |                  v  v                    |
          |     [MANUAL DRIVE]  [AUTO AVOID]         |
          |        |                  |              |
          |   IR remote maps to      | Continuous   |
          |   drive commands.        | loop:        |
          |   OLED shows live        | read sensors,|
          |   radar. Telemetry       | decide turn, |
          |   posts to AWS.          | execute.     |
          |        |                  |              |
          |   IR: STOP          IR: STOP             |
          +--------+------------------+--------------+
          |
          |   IR: MODE3
          +-----------> [SCAN FOR GAP]
                              |
                         Drive along wall,
                         measure gap with
                         front + rear sensors
                              |
                        Gap large enough?
                       /              \
                     NO                YES
                     |                  |
                (keep scanning)   POST gap dims to AWS
                                       |
                                  Lambda computes
                                  parking arc image
                                       |
                                  GET image from S3,
                                  display on OLED
                                       |
                                       v
                                [PARK EXECUTE]
                                  |
                             Execute parking maneuver:
                             1. Pull forward past gap
                             2. Cut wheel, reverse in
                             3. Straighten, pull forward
                             4. Fine-adjust with sensors
                                  |
                                  v
                              [PARKED]
                                  |
                             IR: any key
                                  |
                                  v
                               [IDLE]
```

#### Collision Detection (Parallel State)

At all times during MANUAL, AUTO, and PARK modes, the BMA222 accelerometer is polled. If a sudden acceleration spike exceeds a threshold (indicating impact):
- OLED flashes a red warning
- Car enters emergency stop
- Shadow update triggers SNS email: "ParkPilot collision detected"

#### Mode Descriptions

| Mode | Trigger | Behavior |
|------|---------|----------|
| **IDLE** | Boot / STOP pressed | Car stationary. OLED shows status screen (WiFi status, battery, mode prompt). Telemetry idle. |
| **MANUAL** | IR button 1 | IR remote directional control. Numpad maps to direction vectors. Volume keys control speed. OLED renders live radar. |
| **AUTO AVOID** | IR button 2 | Car drives forward. Reactive obstacle avoidance: if front sensor < threshold, compare left vs right, steer toward open side. If all blocked, reverse and re-scan. |
| **SELF PARK** | IR button 3 | Car drives along right-side wall. Detects parking gap. Posts to cloud for path computation. Fetches guidance image. Executes parallel parking state machine. |

### 2.2 System Architecture

#### Block Diagram

```
+------------------------------------------------------------------+
|                        CLOUD (AWS)                                |
|                                                                   |
|  +----------+    IoT Rule     +--------+    Store    +----+       |
|  | IoT Core | -------------> | Lambda | ----------> | S3 |       |
|  | (Shadow) |                +--------+             +----+       |
|  +----------+    IoT Rule     +-------+                          |
|       |       -------------> |  SNS  | --> Email alert           |
|       |                       +-------+                          |
+-------|----------------------------------------------------------+
        | WiFi / HTTPS REST (GET, POST)
        |
+-------|----------------------------------------------------------+
|       v                  CC3200 (Main MCU)                       |
|  +---------+                                                     |
|  | WiFi +  |    SPI     +------+                                 |
|  | REST    |----------->| OLED | (128x128 SSD1351)               |
|  | Client  |            +------+                                 |
|  +---------+                                                     |
|                                                                   |
|  +----------------+   I2C   +----------+                         |
|  | Control Logic  |-------->| BMA222   | (accelerometer)         |
|  | (state machine,|         +----------+                         |
|  |  mode mgmt,    |                                              |
|  |  parking algo) |   GPIO interrupt                             |
|  |                |<--------+-----------+                        |
|  +----------------+         | IR Recv   | <--- AT&T Remote       |
|         |                   +-----------+                        |
|         | UART (TX/RX)                                           |
+---------|--------------------------------------------------------+
          |
+---------|--------------------------------------------------------+
|         v              ARDUINO (Co-Processor)                    |
|  +-------------+                                                 |
|  | UART Parse  |    Trigger/Echo GPIO                            |
|  | + Command   |<---->  HC-SR04 x4 (Front, Rear, Left, Right)   |
|  | Handler     |                                                 |
|  +-------------+                                                 |
|         |           PWM                                          |
|         +---------> Steering Servo                               |
|         |           PWM                                          |
|         +---------> L298N Motor Driver --> DC Motor              |
|                                                                   |
+------------------------------------------------------------------+
```

#### Component Roles

**CC3200 (Brain):**
- Runs all decision-making: mode management, obstacle avoidance logic, parking state machine
- Renders OLED graphics via SPI
- Decodes IR remote via GPIO interrupts + SysTick timing (same approach as Lab 3)
- Reads BMA222 accelerometer via I2C for collision detection
- Communicates with AWS IoT via WiFi/HTTPS REST
- Sends drive commands to Arduino via UART
- Receives sensor distance data from Arduino via UART

**Arduino (Sensor + Actuator Hub):**
- Reads 4x HC-SR04 ultrasonic sensors using pulseIn() timing
- Packages readings into UART frames sent to CC3200
- Receives motor/steering commands from CC3200 via UART
- Generates PWM for steering servo and motor driver
- Runs at a fixed loop rate (~20 Hz sensor sweep)

#### UART Protocol (CC3200 <-> Arduino)

Arduino to CC3200 (sensor data, 20 Hz):
```
$S,<front_cm>,<rear_cm>,<left_cm>,<right_cm>\n
Example: $S,045,120,030,088\n
```

CC3200 to Arduino (motor commands):
```
$M,<speed_pct>,<steer_angle>\n
Example: $M,+050,+015\n    (50% forward, 15 degrees right)
Example: $M,-030,000\n     (30% reverse, straight)
Example: $M,000,000\n      (stop)
```

#### Communication Protocol Summary

| Protocol | Connection | Purpose |
|----------|-----------|---------|
| **SPI** | CC3200 -> OLED | Display rendering (radar map, status, parking guidance) |
| **I2C** | CC3200 -> BMA222 | Collision/impact detection, tilt sensing |
| **UART** | CC3200 <-> Arduino | Sensor data ingest + motor command relay |
| **GPIO Interrupt** | IR Receiver -> CC3200 | Mode selection, manual drive input |
| **HTTPS/REST** | CC3200 <-> AWS | Shadow telemetry, S3 image fetch, Lambda trigger |

#### Sensing Devices

| Sensor | Qty | Interface | Purpose |
|--------|-----|-----------|---------|
| HC-SR04 Ultrasonic | 4 | GPIO (via Arduino) | 360-degree proximity detection |
| TSOP381xx IR Receiver | 1 | GPIO interrupt (CC3200) | Remote control decoding |
| BMA222 Accelerometer | 1 | I2C (on-board CC3200) | Collision detection, tilt |

---

## 3. Implementation Goals

### Minimal Goals (Must achieve)
- [ ] RC car chassis assembled with motors, battery, and Arduino mounted
- [ ] Arduino reads 4 ultrasonic sensors and transmits over UART
- [ ] CC3200 receives sensor data over UART and displays raw distances on OLED
- [ ] IR remote switches between at least 2 modes (manual + idle)
- [ ] Manual drive mode: IR remote controls car movement
- [ ] AWS IoT shadow updates with sensor telemetry via POST
- [ ] System boots standalone from flash

### Target Goals (Expected to achieve)
- [ ] Autonomous obstacle avoidance mode: car navigates without hitting walls
- [ ] OLED renders a real-time top-down radar map with color-coded proximity
- [ ] BMA222 collision detection triggers SNS email alert via AWS IoT Rule
- [ ] Self-parking: car detects a gap and executes a basic parallel park maneuver
- [ ] All 3 modes fully operational and switchable via IR remote
- [ ] Clean wiring with zip-tied harness along car chassis

### Stretch Goals (If time permits)
- [ ] Lambda computes parking arc trajectory, generates guidance image stored in S3
- [ ] CC3200 fetches guidance image from S3 and displays on OLED before parking
- [ ] Accumulated sensor data processed by Lambda into an environment heatmap
- [ ] Speed-proportional obstacle avoidance (slow down near walls instead of binary stop/go)
- [ ] OLED animation during parking sequence showing planned path vs actual path
- [ ] Second OLED on rear of car showing backup camera-style proximity bars

---

## 4. Bill of Materials

| Item | Qty | Source | Cost |
|------|-----|--------|------|
| CC3200 LaunchPad | 1 | Lab kit | $0.00 |
| Adafruit OLED 1.5" (SSD1351) | 1 | Lab kit | $0.00 |
| AT&T S10-S3 IR Remote | 1 | Lab kit | $0.00 |
| TSOP381xx IR Receiver | 1 | Lab kit | $0.00 |
| 100 Ohm Resistor | 1 | Lab kit | $0.00 |
| 100 uF Capacitor | 1 | Lab kit | $0.00 |
| Arduino Uno (from Arduino kit) | 1 | Own | $0.00 |
| HC-SR04 Ultrasonic Sensor | 4 | Arduino kit / Amazon | ~$8.00 |
| L298N Dual H-Bridge Motor Driver | 1 | Amazon | ~$5.00 |
| SG90 Micro Servo (steering) | 1 | Arduino kit | $0.00 |
| RC Car Chassis (used/cheap) | 1 | Thrift store / Amazon | ~$12.00 |
| 7.4V Li-Po Battery or 4xAA holder | 1 | Amazon | ~$8.00 |
| Breadboard (small, for car mount) | 1 | Arduino kit / lab | $0.00 |
| Jumper wires (M-M, M-F) | ~30 | Arduino kit / lab | $0.00 |
| Zip ties + double-sided tape | 1 pack | Hardware store | ~$3.00 |
| **Total** | | | **~$36.00** |

---

## Task Division (Suggested)

| Area | Lead | Details |
|------|------|---------|
| Arduino firmware (sensor reads, motor control, UART) | Partner A | Write Arduino sketch, calibrate ultrasonics, tune PWM |
| CC3200 UART parsing + OLED radar rendering | Partner B | Implement UART RX handler, write radar draw functions |
| IR remote decoding + mode state machine | Partner A | Reuse Lab 3 IR code, map buttons to modes/commands |
| AWS IoT integration (shadow, Lambda, S3, SNS) | Partner B | Reuse Lab 4 code, write Lambda function, set up S3 bucket |
| Parking algorithm state machine | Both | Design together, implement on CC3200 |
| Physical assembly + wiring | Both | Mount components, route wires, test fit |

---

## Timeline

| Week | Milestone |
|------|-----------|
| Week 8 (Proposal due) | Proposal submitted. RC car purchased. Arduino sensor test working on bench. |
| Week 8-9 | Arduino firmware complete (sensors + motors). CC3200 UART parsing working. IR modes functional. OLED radar rendering prototype. |
| Week 9 | Integration: CC3200 controlling car through Arduino. Obstacle avoidance tuned. AWS telemetry posting. |
| Week 9-10 | Parking algorithm implemented and tuned. Lambda/S3 pipeline working. Collision detection + SNS alert. |
| Week 10 (Demo) | Full system polished. Wiring cleaned up. Demo scenario rehearsed with cardboard obstacle course. |
