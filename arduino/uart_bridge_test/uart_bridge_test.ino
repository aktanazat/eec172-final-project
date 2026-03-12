#include <SoftwareSerial.h>
#include <stdio.h>

SoftwareSerial cc(10, 11); // RX, TX

// HC-SR04 pins (update if your wiring differs)
const int TRIG_FRONT = 2;
const int ECHO_FRONT = A2;
const int TRIG_RIGHT = 4;
const int ECHO_RIGHT = 5;
const int TRIG_LEFT = 6;
const int ECHO_LEFT = 7;
const int TRIG_REAR = 8;
const int ECHO_REAR = 9;

const int DRIVE_ENA = 3;
const int DRIVE_IN1 = A0;
const int DRIVE_IN2 = A1;

const int STEER_ENB = 12;
const int STEER_IN3 = A3;
const int STEER_IN4 = A4;

int curSpeed = 0;
int curSteer = 0;
unsigned long lastMotorMs = 0;

char lineBuf[64];
int lineIdx = 0;
char serialBuf[64];
int serialIdx = 0;
unsigned long lastTxMs = 0;
unsigned long lastLogMs = 0;
unsigned long txFrames = 0;
unsigned long rxBytes = 0;
unsigned long rxLines = 0;
unsigned long rxCmd = 0;
unsigned long rxUnknown = 0;

static int readDistanceCm(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long us = pulseIn(echoPin, HIGH, 25000UL);
  if (us == 0) {
    return 400;
  }
  int cm = (int)(us / 58UL);
  if (cm < 2) cm = 2;
  if (cm > 400) cm = 400;
  return cm;
}

static int readDistanceIsolated(int trigPin, int echoPin) {
  int cm = readDistanceCm(trigPin, echoPin);
  delay(25);
  return cm;
}

static void printSensorReading(const char *name, int cm) {
  Serial.print(name);
  Serial.print("=");
  if (cm >= 400) {
    Serial.print("TIMEOUT");
  } else {
    Serial.print(cm);
    Serial.print("cm");
  }
}

static void applyMotor(int speed, int steer) {
  curSpeed = speed;
  curSteer = steer;
  lastMotorMs = millis();

  // rear drive motor
  if (speed > 0) {
    digitalWrite(DRIVE_IN1, HIGH);
    digitalWrite(DRIVE_IN2, LOW);
    analogWrite(DRIVE_ENA, 255);
  } else if (speed < 0) {
    digitalWrite(DRIVE_IN1, LOW);
    digitalWrite(DRIVE_IN2, HIGH);
    analogWrite(DRIVE_ENA, 255);
  } else {
    digitalWrite(DRIVE_IN1, LOW);
    digitalWrite(DRIVE_IN2, LOW);
    analogWrite(DRIVE_ENA, 0);
  }

  // front steering motor
  if (steer > 0) {
    digitalWrite(STEER_IN3, HIGH);
    digitalWrite(STEER_IN4, LOW);
    digitalWrite(STEER_ENB, HIGH);
  } else if (steer < 0) {
    digitalWrite(STEER_IN3, LOW);
    digitalWrite(STEER_IN4, HIGH);
    digitalWrite(STEER_ENB, HIGH);
  } else {
    digitalWrite(STEER_IN3, LOW);
    digitalWrite(STEER_IN4, LOW);
    digitalWrite(STEER_ENB, LOW);
  }
}

static void handleMotorLine(const char *s) {
  int speed = 0;
  int steer = 0;
  if (sscanf(s, "$M,%d,%d", &speed, &steer) == 2) {
    rxCmd++;
    applyMotor(speed, steer);
    Serial.print("RX motor cmd -> speed=");
    Serial.print(speed);
    Serial.print(" steer=");
    Serial.println(steer);
  } else {
    rxUnknown++;
    Serial.print("RX unknown: ");
    Serial.println(s);
  }
}

void setup() {
  Serial.begin(115200);
  cc.begin(9600);

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);
  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_REAR, OUTPUT);
  pinMode(ECHO_REAR, INPUT);

  pinMode(DRIVE_ENA, OUTPUT);
  pinMode(DRIVE_IN1, OUTPUT);
  pinMode(DRIVE_IN2, OUTPUT);
  analogWrite(DRIVE_ENA, 0);

  pinMode(STEER_ENB, OUTPUT);
  pinMode(STEER_IN3, OUTPUT);
  pinMode(STEER_IN4, OUTPUT);
  digitalWrite(STEER_ENB, LOW);

  Serial.println("Arduino UART bridge test started");
}

void loop() {
  int front = readDistanceIsolated(TRIG_FRONT, ECHO_FRONT);
  int right = readDistanceIsolated(TRIG_RIGHT, ECHO_RIGHT);
  int left = readDistanceIsolated(TRIG_LEFT, ECHO_LEFT);
  int rear = readDistanceIsolated(TRIG_REAR, ECHO_REAR);

  while (cc.available()) {
    char c = (char)cc.read();
    rxBytes++;
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      rxLines++;
      lineBuf[lineIdx] = '\0';
      if (lineIdx > 0) {
        handleMotorLine(lineBuf);
      }
      lineIdx = 0;
    } else if (lineIdx < (int)sizeof(lineBuf) - 1) {
      lineBuf[lineIdx++] = c;
    }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialBuf[serialIdx] = '\0';
      if (serialIdx > 0) handleMotorLine(serialBuf);
      serialIdx = 0;
    } else if (serialIdx < (int)sizeof(serialBuf) - 1) {
      serialBuf[serialIdx++] = c;
    }
  }

  if ((curSpeed != 0 || curSteer != 0) && millis() - lastMotorMs > 500) {
    applyMotor(0, 0);
    Serial.println("WATCHDOG: motor stopped (no cmd for 500ms)");
  }

  if (millis() - lastTxMs >= 100) {
    char frame[40];
    snprintf(frame, sizeof(frame), "$S,%d,%d,%d,%d\n", front, right, left, rear);
    cc.print(frame);
    txFrames++;
    lastTxMs = millis();
  }

  if (millis() - lastLogMs >= 1000) {
    lastLogMs = millis();
  }
}
