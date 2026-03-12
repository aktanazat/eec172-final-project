#include <SoftwareSerial.h>
#include <stdio.h>

static const int BUS_RX = 10;
static const int BUS_TX = 11;

static const int PIN_TILT = 9;
static const int PIN_JOY_X = A0;
static const int PIN_JOY_Y = A1;
static const int PIN_LIGHT = A2;
static const int PIN_RGB_R = A3;
static const int PIN_RGB_G = A4;
static const int PIN_RGB_B = A5;

SoftwareSerial bus(BUS_RX, BUS_TX);

char busLine[96];
int busLineIdx = 0;

int g_servoDeg = 90;
int g_rgbCode = 0;
int g_roundState = 0;

unsigned long g_busRxBytes = 0;
unsigned long g_busRxLines = 0;
unsigned long g_ctrlFrames = 0;
unsigned long g_badFrames = 0;
unsigned long g_txFrames = 0;
unsigned long g_lastTxMs = 0;
unsigned long g_lastLogMs = 0;

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
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

static void parseControl(const char *line) {
  int servo = 90;
  int stepMode = 0;
  int buzzMode = 0;
  int rgbCode = 0;
  int roundState = 0;

  if (sscanf(line, "$C,%d,%d,%d,%d,%d", &servo, &stepMode, &buzzMode, &rgbCode, &roundState) == 5) {
    g_servoDeg = clampInt(servo, 0, 180);
    g_rgbCode = clampInt(rgbCode, 0, 6);
    g_roundState = roundState;
    g_ctrlFrames++;

    applyRgbCode(g_rgbCode);

    Serial.print("CTRL servo=");
    Serial.print(g_servoDeg);
    Serial.print(" rgb=");
    Serial.print(g_rgbCode);
    Serial.print(" state=");
    Serial.println(g_roundState);
    return;
  }

  g_badFrames++;
  Serial.print("BAD ");
  Serial.println(line);
}

static void pollBus(void) {
  while (bus.available()) {
    char c = (char)bus.read();
    g_busRxBytes++;

    if (c == '\r' || c == '\n') {
      busLine[busLineIdx] = '\0';
      if (busLineIdx > 0) {
        g_busRxLines++;
        parseControl(busLine);
      }
      busLineIdx = 0;
      continue;
    }

    if (c == '$') {
      busLineIdx = 0;
    }

    if (busLineIdx < (int)sizeof(busLine) - 1) {
      busLine[busLineIdx++] = c;
    }
  }
}

static void sendSensorFrame(void) {
  unsigned long now = millis();
  int joyX;
  int joyY;
  int lux;
  int tilt;
  int sector;
  int distCm;
  char frame[128];

  if (now - g_lastTxMs < 250) return;
  g_lastTxMs = now;

  joyX = analogRead(PIN_JOY_X);
  joyY = analogRead(PIN_JOY_Y);
  lux = analogRead(PIN_LIGHT);
  tilt = (digitalRead(PIN_TILT) == LOW) ? 1 : 0;

  sector = (joyY * 15) / 1023;
  distCm = 80 + ((1023 - joyX) * 320) / 1023;

  snprintf(frame,
           sizeof(frame),
           "$S,%lu,%d,%d,%d,%d,%d,%d,%d,%d\n",
           now,
           sector,
           distCm,
           lux,
           tilt,
           250,
           500,
           joyY,
           joyX);

  bus.print(frame);
  bus.listen();
  g_txFrames++;
}

void setup() {
  Serial.begin(115200);
  bus.begin(9600);
  bus.listen();

  pinMode(PIN_TILT, INPUT_PULLUP);
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  applyRgbCode(g_rgbCode);

  Serial.println("AEGIS transport probe ready");
  Serial.println("TX: periodic $S frames");
  Serial.println("RX: $C,<servo>,<step>,<buzz>,<rgb>,<state>");
}

void loop() {
  int joyX;
  int joyY;
  int lux;
  int tilt;

  pollBus();
  sendSensorFrame();

  if (millis() - g_lastLogMs < 1000) return;
  g_lastLogMs = millis();

  joyX = analogRead(PIN_JOY_X);
  joyY = analogRead(PIN_JOY_Y);
  lux = analogRead(PIN_LIGHT);
  tilt = (digitalRead(PIN_TILT) == LOW) ? 1 : 0;

  Serial.print("DBG tx=");
  Serial.print(g_txFrames);
  Serial.print(" rxB=");
  Serial.print(g_busRxBytes);
  Serial.print(" rxL=");
  Serial.print(g_busRxLines);
  Serial.print(" ctrl=");
  Serial.print(g_ctrlFrames);
  Serial.print(" bad=");
  Serial.print(g_badFrames);
  Serial.print(" joyX=");
  Serial.print(joyX);
  Serial.print(" joyY=");
  Serial.print(joyY);
  Serial.print(" lux=");
  Serial.print(lux);
  Serial.print(" tilt=");
  Serial.print(tilt);
  Serial.print(" servo=");
  Serial.print(g_servoDeg);
  Serial.print(" rgb=");
  Serial.println(g_rgbCode);
}
