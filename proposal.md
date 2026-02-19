# ParkPilot: Autonomous Obstacle-Avoidance & Self-Parking RC Car with Cloud Telemetry

**EEC172 Final Project Proposal**
**Team Members:** Aktan Azat, Gezheng Kang
**Date:** February 2026

---

## 1. Description

ParkPilot is an RC car controlled by a CC3200 that avoids obstacles, parallel-parks itself, and sends telemetry to AWS. An IR remote selects between three modes: manual drive, autonomous obstacle avoidance, and self-parking. Four ultrasonic sensors provide proximity data rendered as a radar map on a 128x128 OLED. An Arduino co-processor handles sensor reads and motor actuation over UART.

AWS IoT is used for telemetry and parking guidance. The device shadow stores sensor distances, mode, and speed. An IoT Rule triggers a Lambda function that generates a parking guidance overlay stored in S3. A separate SNS rule emails collision alerts when the BMA222 accelerometer detects impact.

Existing hobby autonomous cars (Donkey Car, AWS DeepRacer) use cameras and ML on powerful compute. ParkPilot uses only ultrasonic ranging on a constrained MCU with classical control algorithms and cloud-assisted guidance.

---

## 2. Design

### 2.1 Functional Specification

**Modes:**

- **IDLE** (boot / STOP pressed): Car stationary. OLED shows WiFi status, battery, mode prompt. Telemetry idle.
- **MANUAL** (IR button 1): IR remote directional control. Numpad maps to direction vectors. Volume keys control speed. OLED renders live radar.
- **AUTO AVOID** (IR button 2): Car drives forward. If front sensor reads below threshold, compare left vs right, steer toward open side. If all blocked, reverse and re-scan.
- **SELF PARK** (IR button 3): Car drives along right-side wall, measures gaps with front and rear sensors. When gap is large enough, it requests cloud-generated guidance via Lambda/S3, then executes a parallel parking maneuver. If the cloud request times out, it falls back to a local plan.

**Parking sequence:** Pull forward past gap, cut wheel and reverse in, straighten and pull forward, fine-adjust with sensors.

**Collision detection:** Runs in parallel during all active modes. BMA222 polled continuously. Acceleration spike above threshold triggers emergency stop, red OLED flash, and SNS email via shadow update.

### 2.2 System Architecture

**CC3200 (main MCU):**
- Runs mode management, obstacle avoidance logic, parking state machine
- Renders OLED via SPI
- Decodes IR remote via GPIO interrupts and SysTick (same as Lab 3)
- Reads BMA222 via I2C for collision detection
- Communicates with AWS IoT via WiFi/HTTPS REST
- Sends drive commands and receives sensor data from Arduino via UART

**Arduino (co-processor):**
- Reads 4x HC-SR04 ultrasonics using pulseIn() at ~20 Hz
- Packages readings into UART frames for CC3200
- Receives motor/steering commands from CC3200
- Generates PWM for steering servo and L293D motor driver

**UART protocol:**

Arduino to CC3200 (sensor data, 20 Hz):
```
$S,<front_cm>,<rear_cm>,<left_cm>,<right_cm>\n
```

CC3200 to Arduino (motor commands):
```
$M,<speed_pct>,<steer_angle>\n
```

**Communication protocols:**

- SPI: CC3200 to OLED (display rendering)
- I2C: CC3200 to BMA222 (collision detection)
- UART: CC3200 to Arduino (sensor data and motor commands)
- GPIO interrupt: IR receiver to CC3200 (mode selection, manual input)
- HTTPS/REST: CC3200 to AWS (shadow telemetry, Lambda trigger, S3 fetch)

**Sensors:**

- HC-SR04 ultrasonic x4 (GPIO via Arduino): front, rear, left, right proximity
- TSOP381xx IR receiver x1 (GPIO interrupt on CC3200): remote control
- BMA222 accelerometer x1 (I2C on CC3200): collision detection

---

## 3. Implementation Goals

### Minimal Goals
- RC car chassis assembled with motors, battery, Arduino
- Arduino reads 4 ultrasonics and transmits over UART
- CC3200 receives sensor data and displays distances on OLED
- IR remote switches between manual and idle modes
- Manual drive via IR remote
- Autonomous obstacle avoidance runs on CC3200
- AWS shadow updates with sensor telemetry during demo
- System boots standalone from flash

### Target Goals
- OLED real-time top-down radar map with color-coded proximity
- BMA222 collision triggers SNS email alert via IoT Rule
- All 3 modes operational and switchable via IR
- Self-parking with AWS Lambda guidance
- 2-minute demo: manual, auto avoid, self-park, idle
- Clean wiring with zip ties

### Stretch Goals
- Lambda computes parking arc trajectory, stores guidance image in S3
- CC3200 fetches guidance image and overlays on OLED before parking
- Speed-proportional avoidance (slow near walls instead of binary stop)
- Buffered telemetry upload after WiFi reconnect

### Verification Metrics

- Obstacle course completion (auto avoid): 3+ consecutive clean runs
- Parking success rate: 8/10 in standard slot
- Collisions during scripted demo: 0
- Mode switch response: under 300 ms
- Sensor update freshness: under 100 ms

---

## 4. Bill of Materials

| Item | Qty | Source | Cost |
|------|-----|--------|------|
| CC3200 LaunchPad | 1 | Lab kit | $0 |
| Adafruit OLED 1.5" (SSD1351) | 1 | Lab kit | $0 |
| AT&T S10-S3 IR Remote | 1 | Lab kit | $0 |
| TSOP381xx IR Receiver | 1 | Lab kit | $0 |
| 100 Ohm Resistor | 1 | Lab kit | $0 |
| 100 uF Capacitor | 1 | Lab kit | $0 |
| Arduino Uno R3 | 1 | Arduino kit | $0 |
| HC-SR04 Ultrasonic Sensor | 1 | Arduino kit | $0 |
| HC-SR04 Ultrasonic Sensor | 3 | Amazon | ~$6 |
| L293D Motor Driver IC | 1 | Arduino kit | $0 |
| Servo Motor | 1 | Arduino kit | $0 |
| Active Buzzer | 1 | Arduino kit | $0 |
| 830-point Breadboard | 1 | Arduino kit | $0 |
| 9V Battery + Snap Connector | 1 | Arduino kit | $0 |
| RC Car Chassis + DC Motors | 1 | Amazon | ~$12 |
| 7.4V Li-Po Battery (drive power) | 1 | Amazon | ~$8 |
| Jumper wires (M-M, M-F) | ~30 | Arduino kit | $0 |
| Zip ties + double-sided tape | 1 pack | Hardware store | ~$3 |
| **Total** | | | **~$29** |

**From Arduino kit (available, no additional cost):** UNO R3, 1x HC-SR04, L293D, servo, active buzzer, 830-point breadboard, 9V battery with snap connector, jumper wires. The L293D replaces the previously planned L298N. The active buzzer provides audible collision alerts. The 9V battery powers the Arduino and logic during bench testing; the 7.4V LiPo powers the drive motors on the car.

---

## 5. Task Division

| Area | Lead |
|------|------|
| Arduino firmware (sensors, motors, UART) | Gezheng |
| CC3200 UART parsing + OLED radar | Aktan |
| IR remote + mode state machine | Aktan |
| AWS IoT (shadow, Lambda, S3, SNS) | Aktan |
| Parking algorithm | Both |
| Physical assembly + wiring | Both |

---

## 6. Timeline

| Week | Milestone |
|------|-----------|
| 8 | Proposal submitted. Car purchased. Arduino sensor test on bench. |
| 8-9 | Arduino firmware done. CC3200 UART parsing. IR modes. OLED radar prototype. |
| 9 | Integration: CC3200 driving car through Arduino. Avoidance tuned. AWS telemetry. |
| 9-10 | Parking algorithm. Lambda/S3 guidance. Collision + SNS. |
| 10 | System polished. Wiring cleaned. Demo rehearsed. |
