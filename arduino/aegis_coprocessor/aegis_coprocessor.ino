#include <SoftwareSerial.h>
#include <Servo.h>
#include <stdio.h>

static const int BUS_RX = 10;
static const int BUS_TX = 11;

static const int PIN_STEP_1 = 2;
static const int PIN_STEP_2 = 3;
static const int PIN_STEP_3 = 4;
static const int PIN_STEP_4 = 5;

static const int PIN_SERVO = 6;
static const int PIN_TRIG = 7;
static const int PIN_ECHO = 8;
static const int PIN_TILT = 9;

static const int PIN_BUZZ = 12;

static const int PIN_JOY_X = A0;
static const int PIN_JOY_Y = A1;
static const int PIN_LIGHT = A2;
static const int PIN_RGB_R = A3;
static const int PIN_RGB_G = A4;
static const int PIN_RGB_B = A5;

static const int RADAR_SWEEP_STEPS = 512;
static const int SECTOR_COUNT = 16;

SoftwareSerial bus(BUS_RX, BUS_TX);
Servo shieldServo;

char busLine[96];
int busLineIdx = 0;
char serialLine[96];
int serialLineIdx = 0;
unsigned long g_busRxBytes = 0;
unsigned long g_busRxLines = 0;
unsigned long g_ctrlFrames = 0;
unsigned long g_pingFrames = 0;

int g_servoDeg = 90;
int g_stepMode = 2;
int g_buzzMode = 0;
int g_rgbCode = 0;
int g_roundState = 0;

long g_radarPos = 0;
int g_radarDir = 1;
int g_sector = 0;
int g_stepIndex = 0;

int g_distCm = 400;
int g_lux = 0;
int g_tilt = 0;
int g_tempC10 = 250;
int g_hum10 = 500;
int g_aux = 0;
int g_joyX = 512;
int g_joyY = 512;

unsigned long g_lastSensorMs = 0;
unsigned long g_lastFrameMs = 0;
unsigned long g_lastStepMs = 0;
unsigned long g_lastLogMs = 0;

static const int STEP_SEQ[4][4] = {
  {HIGH, LOW,  HIGH, LOW},
  {LOW,  HIGH, HIGH, LOW},
  {LOW,  HIGH, LOW,  HIGH},
  {HIGH, LOW,  LOW,  HIGH}
};

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int readDistanceRawCm(void) {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long us = pulseIn(PIN_ECHO, HIGH, 25000UL);
  if (us == 0) return 400;

  int cm = (int)(us / 58UL);
  if (cm < 2) cm = 2;
  if (cm > 400) cm = 400;
  return cm;
}

static int median3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { int t = a; a = b; b = t; }
  return b;
}

static int readDistanceCm(void) {
  static int lastGood = 400;
  int a = readDistanceRawCm();
  int b = readDistanceRawCm();
  int c = readDistanceRawCm();
  int dist = median3(a, b, c);

  if (dist >= 400) return lastGood;
  lastGood = dist;
  return dist;
}

static int averagedAnalogRead(int pin) {
  int a = analogRead(pin);
  int b = analogRead(pin);
  int c = analogRead(pin);
  int d = analogRead(pin);
  return (a + b + c + d) / 4;
}

static int smoothAnalogRead(int pin, int current) {
  int sample = averagedAnalogRead(pin);
  return ((current * 7) + sample) / 8;
}

static void applyRgbCode(int code) {
  int r = LOW;
  int g = LOW;
  int b = LOW;

  switch (code) {
    case 1: g = HIGH; break;
    case 2: r = HIGH; g = HIGH; break;
    case 3: r = HIGH; break;
    case 4: g = HIGH; b = HIGH; break;
    case 5: r = HIGH; b = HIGH; break;
    case 6: r = HIGH; g = HIGH; b = HIGH; break;
    default: break;
  }

  digitalWrite(PIN_RGB_R, r);
  digitalWrite(PIN_RGB_G, g);
  digitalWrite(PIN_RGB_B, b);
}

static void applyOutputs(void) {
  shieldServo.write(clampInt(g_servoDeg, 0, 180));

  if (g_buzzMode == 0) {
    noTone(PIN_BUZZ);
  } else if (g_buzzMode == 1) {
    tone(PIN_BUZZ, 2400);
  } else {
    if (((millis() / 180UL) % 2UL) == 0UL) {
      tone(PIN_BUZZ, 1900);
    } else {
      noTone(PIN_BUZZ);
    }
  }

  applyRgbCode(g_rgbCode);
}

static void applyStepPhase(int idx) {
  digitalWrite(PIN_STEP_1, STEP_SEQ[idx][0]);
  digitalWrite(PIN_STEP_2, STEP_SEQ[idx][1]);
  digitalWrite(PIN_STEP_3, STEP_SEQ[idx][2]);
  digitalWrite(PIN_STEP_4, STEP_SEQ[idx][3]);
}

static void stepMotor(int delta) {
  if (delta > 0) {
    g_stepIndex = (g_stepIndex + 1) & 3;
  } else {
    g_stepIndex = (g_stepIndex + 3) & 3;
  }
  applyStepPhase(g_stepIndex);
}

static void stepRadar(void) {
  unsigned long now = millis();
  if (now - g_lastStepMs < 8) return;
  g_lastStepMs = now;

  int stepDelta = 0;
  if (g_stepMode == 1) {
    stepDelta = 1;
  } else if (g_stepMode == -1) {
    stepDelta = -1;
  } else if (g_stepMode == 2) {
    stepDelta = g_radarDir;
    if (g_radarPos >= RADAR_SWEEP_STEPS) g_radarDir = -1;
    if (g_radarPos <= -RADAR_SWEEP_STEPS) g_radarDir = 1;
  }

  if (stepDelta != 0) {
    stepMotor(stepDelta);
    g_radarPos += stepDelta;
  }

  long shifted = g_radarPos + RADAR_SWEEP_STEPS;
  long span = (2L * RADAR_SWEEP_STEPS) + 1L;
  if (shifted < 0) shifted = 0;
  if (shifted > span) shifted = span;
  g_sector = (int)((shifted * SECTOR_COUNT) / (span + 1L));
  if (g_sector < 0) g_sector = 0;
  if (g_sector >= SECTOR_COUNT) g_sector = SECTOR_COUNT - 1;
}

static void sampleSensors(void) {
  unsigned long now = millis();
  if (now - g_lastSensorMs < 120) return;
  g_lastSensorMs = now;

  g_distCm = readDistanceCm();
  g_lux = smoothAnalogRead(PIN_LIGHT, g_lux);
  g_tilt = 0;
  g_joyX = smoothAnalogRead(PIN_JOY_X, g_joyX);
  g_joyY = smoothAnalogRead(PIN_JOY_Y, g_joyY);
  g_tempC10 = 250;
  g_hum10 = 500;
  g_aux = g_joyY;
}

static void sendSensorFrame(void) {
  unsigned long now = millis();
  if (now - g_lastFrameMs < 180) return;
  g_lastFrameMs = now;

  char frame[128];
  snprintf(frame,
           sizeof(frame),
           "$S,%lu,%d,%d,%d,%d,%d,%d,%d,%d\n",
           now,
           g_sector,
           g_distCm,
           g_lux,
           g_tilt,
           g_tempC10,
           g_hum10,
           g_aux,
           g_joyX);

  bus.print(frame);
  bus.listen();
}

static void parseControl(const char *line) {
  int servo = 90;
  int stepMode = 0;
  int buzzMode = 0;
  int rgbCode = 0;
  int roundState = 0;

  if (sscanf(line, "$C,%d,%d,%d,%d,%d", &servo, &stepMode, &buzzMode, &rgbCode, &roundState) == 5) {
    g_servoDeg = clampInt(servo, 0, 180);
    g_stepMode = clampInt(stepMode, -1, 2);
    g_buzzMode = clampInt(buzzMode, 0, 2);
    g_rgbCode = clampInt(rgbCode, 0, 6);
    g_roundState = clampInt(roundState, 0, 9);
    g_ctrlFrames++;
    return;
  }

  if (strcmp(line, "PING") == 0) {
    g_pingFrames++;
    bus.println("$A,PONG");
    return;
  }
}

static void pollBus(void) {
  while (bus.available()) {
    char c = (char)bus.read();
    g_busRxBytes++;
    if (c == '$') {
      busLineIdx = 0;
    }
    if (c == '\r' || c == '\n') {
      busLine[busLineIdx] = '\0';
      if (busLineIdx > 0) {
        g_busRxLines++;
        parseControl(busLine);
      }
      busLineIdx = 0;
      continue;
    }

    if (busLineIdx < (int)sizeof(busLine) - 1) {
      busLine[busLineIdx++] = c;
    }
  }
}

static void pollSerial(void) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      serialLine[serialLineIdx] = '\0';
      if (serialLineIdx > 0) parseControl(serialLine);
      serialLineIdx = 0;
      continue;
    }

    if (serialLineIdx < (int)sizeof(serialLine) - 1) {
      serialLine[serialLineIdx++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  bus.begin(9600);
  bus.listen();

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_TILT, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_STEP_1, OUTPUT);
  pinMode(PIN_STEP_2, OUTPUT);
  pinMode(PIN_STEP_3, OUTPUT);
  pinMode(PIN_STEP_4, OUTPUT);

  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  shieldServo.attach(PIN_SERVO);
  applyStepPhase(0);

  applyOutputs();

  Serial.println("AEGIS coprocessor ready");
  Serial.println("Expecting: $C,<servo>,<step>,<buzz>,<rgb>,<state>");
}

void loop() {
  pollBus();
  pollSerial();

  stepRadar();
  sampleSensors();
  applyOutputs();
  sendSensorFrame();

  if (millis() - g_lastLogMs >= 1000) {
    g_lastLogMs = millis();
    Serial.print("DBG busB=");
    Serial.print(g_busRxBytes);
    Serial.print(" busL=");
    Serial.print(g_busRxLines);
    Serial.print(" ctrl=");
    Serial.print(g_ctrlFrames);
    Serial.print(" ping=");
    Serial.print(g_pingFrames);
    Serial.print(" joyX=");
    Serial.print(g_joyX);
    Serial.print(" joyY=");
    Serial.print(g_joyY);
    Serial.print(" rgb=");
    Serial.print(g_rgbCode);
    Serial.print(" state=");
    Serial.println(g_roundState);
  }
}
