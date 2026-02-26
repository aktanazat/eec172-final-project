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

The system operates in four modes selected by IR remote, with collision detection running in parallel across all active modes:

![State Machine Diagram](website/diagrams/state-machine.svg)

- **IDLE** (boot / STOP pressed): Car stationary. OLED shows WiFi status, battery, mode prompt. Telemetry idle.
- **MANUAL** (IR button 1): IR remote directional control. Numpad maps to direction vectors. Volume keys control speed. OLED renders live radar.
- **AUTO AVOID** (IR button 2): Car drives forward. If front sensor reads below threshold, compare left vs right, steer toward open side. If all blocked, reverse and re-scan.
- **SELF PARK** (IR button 3): Car drives along right-side wall, measures gaps with front and rear sensors. When gap is large enough, it requests cloud-generated guidance via Lambda/S3, then executes a parallel parking maneuver in stages: pull forward past gap, cut wheel and reverse in, straighten and pull forward, fine-adjust with sensors. If the cloud request times out, it falls back to a local plan.
- **COLLISION STOP** (any active mode): BMA222 acceleration spike above threshold triggers emergency stop, red OLED flash, and SNS email alert through shadow update. Returns to IDLE after alert.

### 2.2 System Architecture

![Architecture Block Diagram](website/diagrams/architecture.svg)

**CC3200 (main MCU):** Runs mode management, obstacle avoidance logic, parking state machine, SPI OLED rendering, I2C BMA222 collision monitoring, WiFi/HTTPS telemetry to AWS IoT, and UART command/sensor exchange with Arduino.

**Arduino (co-processor):** Reads 4x HC-SR04 ultrasonics using pulseIn() at ~20 Hz, packages readings into UART sensor frames (`$S,<front>,<rear>,<left>,<right>\n`), receives motor commands (`$M,<speed_pct>,<steer_angle>\n`), and generates PWM for steering servo and L293D motor driver.

**Communication Protocols:** SPI (CC3200 to OLED), I2C (CC3200 to BMA222), UART (CC3200 to Arduino), GPIO interrupt (IR receiver to CC3200), HTTPS/REST (CC3200 to AWS).

**Sensors:** HC-SR04 ultrasonic x4 (front, rear, left, right proximity), TSOP381xx IR receiver x1 (remote control), BMA222 accelerometer x1 (collision detection).

---

## 3. Implementation Goals

**Minimal:** Car assembly with motors/battery/Arduino. Ultrasonic UART transmission. CC3200 UART parsing and OLED distance display. IR mode switching and manual drive. Autonomous obstacle avoidance. AWS shadow telemetry. Standalone boot from flash.

**Target:** Real-time top-down OLED radar map. BMA222 collision-triggered SNS alert. All three modes switchable by IR. Self-parking with Lambda guidance. Two-minute live demo sequence. Clean wiring.

**Stretch:** Lambda parking arc trajectory and S3 guidance image. OLED overlay of fetched guidance image. Speed-proportional avoidance. Buffered telemetry replay after reconnect.

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
