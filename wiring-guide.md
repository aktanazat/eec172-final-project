# AEGIS-172 Wiring Guide

## Power

| Source | Connects To | Notes |
|--------|-------------|-------|
| 9V battery red (+) | L293D pin 8 (VCC2) directly | NO power supply module in between |
| 9V battery black (-) | GND rail | |
| Arduino 5V | L293D pin 16 (VCC1) | Logic power only |
| Arduino 5V | All HC-SR04 VCC pins | |
| CC3200 3.3V | IR receiver VCC | |
| All GNDs tied together | GND rail | Arduino + CC3200 + 9V battery |

---

## L293D (straddles breadboard center gap, notch/dot = pin 1 top-left)

```
            L293D
          +---U---+
     D3 > | 1  16 | < Arduino 5V
     A0 > | 2  15 | < A4
 REAR M+ <| 3  14 |> FRONT M+
    GND > | 4  13 | < GND
    GND > | 5  12 | < GND
 REAR M- <| 6  11 |> FRONT M-
     A1 > | 7  10 | < A3
9V BAT+ > | 8   9 | < D12
          +-------+
```

### Left Channel (rear drive motor)

| L293D Pin | Function | Connects To |
|-----------|----------|-------------|
| Pin 1 (ENA) | Drive speed PWM | Arduino D3 |
| Pin 2 (IN1) | Drive direction | Arduino A0 |
| Pin 3 (OUT1) | Drive output | Rear motor wire 1 |
| Pin 4 (GND) | Ground | GND rail |
| Pin 5 (GND) | Ground | GND rail |
| Pin 6 (OUT2) | Drive output | Rear motor wire 2 |
| Pin 7 (IN2) | Drive direction | Arduino A1 |

### Right Channel (front steering motor)

| L293D Pin | Function | Connects To |
|-----------|----------|-------------|
| Pin 9 (ENB) | Steer enable | Arduino D12 |
| Pin 10 (IN3) | Steer direction | Arduino A3 |
| Pin 11 (OUT3) | Steer output | Front motor wire 1 |
| Pin 12 (GND) | Ground | GND rail |
| Pin 13 (GND) | Ground | GND rail |
| Pin 14 (OUT4) | Steer output | Front motor wire 2 |
| Pin 15 (IN4) | Steer direction | Arduino A4 |

### Power Pins

| L293D Pin | Function | Connects To |
|-----------|----------|-------------|
| Pin 8 (VCC2) | Motor power | 9V battery red (+) directly |
| Pin 16 (VCC1) | Logic power | Arduino 5V |

---

## Voltage Divider (Arduino TX to CC3200 RX)

```
Arduino D11 ---[1k]---+---[2k]--- GND
                       |
                  CC3200 RX (PIN_59)
```

| From | Through | To |
|------|---------|-----|
| Arduino D11 (TX) | 1k ohm resistor | Junction point |
| Junction point | 2k ohm resistor | GND rail |
| Junction point | Wire | CC3200 PIN_59 (UART1 RX) |
| CC3200 PIN_58 (UART1 TX) | Wire (direct, no resistor) | Arduino D10 (RX) |

---

## IR Receiver (VS1838B or similar, dome facing you)

```
  _____
 |  O  |
 | | | |
  1 2 3
```

| IR Pin | Connects To |
|--------|-------------|
| Pin 1 (OUT) | CC3200 PIN_03 |
| Pin 2 (GND) | GND rail |
| Pin 3 (VCC) | CC3200 3.3V |

---

## HC-SR04 Ultrasonic Sensors (x4)

Each sensor has 4 pins:

| Sensor | VCC | GND | TRIG | ECHO |
|--------|-----|-----|------|------|
| Front | 5V | GND | Arduino D2 | Arduino A2 |
| Right | 5V | GND | Arduino D4 | Arduino D5 |
| Left | 5V | GND | Arduino D6 | Arduino D7 |
| Rear | 5V | GND | Arduino D8 | Arduino D9 |

---

## Arduino UNO Pin Summary

| Pin | Function |
|-----|----------|
| D2 | Front sensor TRIG |
| D3 | L293D ENA (rear drive PWM) |
| D4 | Right sensor TRIG |
| D5 | Right sensor ECHO |
| D6 | Left sensor TRIG |
| D7 | Left sensor ECHO |
| D8 | Rear sensor TRIG |
| D9 | Rear sensor ECHO |
| D10 | SoftwareSerial RX (from CC3200 TX) |
| D11 | SoftwareSerial TX (to CC3200 RX via voltage divider) |
| D12 | L293D ENB (front steer enable) |
| A0 | L293D IN1 (rear drive direction) |
| A1 | L293D IN2 (rear drive direction) |
| A2 | Front sensor ECHO |
| A3 | L293D IN3 (front steer direction) |
| A4 | L293D IN4 (front steer direction) |
| 5V | L293D pin 16, all HC-SR04 VCC |
| GND | GND rail |

---

## CC3200 Pin Summary

| CC3200 Pin | Function |
|------------|----------|
| PIN_01 | I2C SDA (BMA222) |
| PIN_02 | I2C SCL (BMA222) |
| PIN_03 | IR receiver input |
| PIN_05 | SPI CLK (OLED) |
| PIN_07 | SPI MOSI (OLED) |
| PIN_08 | SPI CS (OLED) |
| PIN_18 | OLED RST |
| PIN_53 | OLED DC |
| PIN_55 | UART0 TX (debug terminal) |
| PIN_57 | UART0 RX (debug terminal) |
| PIN_58 | UART1 TX (to Arduino D10) |
| PIN_59 | UART1 RX (from Arduino D11 via voltage divider) |
| PIN_64 | Status LED |
| 3.3V | IR receiver VCC |
| GND | GND rail |

---

## Motor Wiring Notes

- If a motor spins the wrong direction, swap its two wires on the L293D OUT pins
- Rear motor: OUT1 (pin 3) and OUT2 (pin 6)
- Front motor: OUT3 (pin 11) and OUT4 (pin 14)
- Motor wires must be soldered to the motor tabs

---

## Test Commands (Serial Monitor at 115200 baud)

| Command | Expected Result |
|---------|----------------|
| $M,100,0 | Rear motor full forward |
| $M,-50,0 | Rear motor half reverse |
| $M,0,30 | Front motor turns right |
| $M,0,-30 | Front motor turns left |
| $M,50,30 | Forward + turn right |
| $M,0,0 | Everything stops |
| (wait 500ms) | Watchdog auto-stops motors |
