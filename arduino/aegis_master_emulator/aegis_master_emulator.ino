#include <SoftwareSerial.h>
#include <stdio.h>

static const int BUS_RX = 10;
static const int BUS_TX = 11;

static const int PIN_JOY_X = A0;
static const int PIN_JOY_Y = A1;
static const int PIN_JOY_SW = 2;
static const int PIN_START_BTN = 4;
static const int PIN_STATUS_LED = 13;
static const int PIN_LCD_RS = 5;
static const int PIN_LCD_E = 6;
static const int PIN_LCD_D4 = 7;
static const int PIN_LCD_D5 = 8;
static const int PIN_LCD_D6 = 9;
static const int PIN_LCD_D7 = 12;

static const int SENSOR_TIMEOUT_MS = 800;
static const int CALIBRATION_MS = 4000;
static const int PREP_MS = 3000;
static const int ACTIVE_ROUND_MS = 45000;
static const int JUDGE_MS = 2200;

static const int SECTOR_COUNT = 16;

SoftwareSerial bus(BUS_RX, BUS_TX);

char busLine[128];
int busLineIdx = 0;
char serialLine[128];
int serialLineIdx = 0;

struct SensorFrame {
  unsigned long ms;
  int sector;
  int distCm;
  int lux;
  int tilt;
  int tempC10;
  int hum10;
  int irRaw;
  int joy;
};

SensorFrame g_sensor = {0, 0, 400, 0, 0, 250, 500, 0, 0};
unsigned long g_lastSensorRxMs = 0;

int g_servoDeg = 90;
int g_stepMode = 2;
int g_buzzMode = 0;
int g_rgbCode = 0;

int g_defenderScore = 0;
int g_attackerScore = 0;
int g_missionDifficulty = 2;
int g_shieldSector = 0;

int g_baseDist = 200;
int g_baseLux = 500;
int g_baseTemp10 = 250;
int g_baseHum10 = 500;

long g_calDistSum = 0;
long g_calLuxSum = 0;
long g_calTempSum = 0;
long g_calHumSum = 0;
int g_calCount = 0;

unsigned long g_stateStartMs = 0;
unsigned long g_lastControlTxMs = 0;
unsigned long g_lastScoreTickMs = 0;
unsigned long g_lastLogMs = 0;
unsigned long g_lastLcdMs = 0;
int g_waitingForSensor = 0;

int g_prevStartBtn = HIGH;
int g_prevJoyBtn = HIGH;

enum RoundState {
  RS_BOOT = 0,
  RS_CALIBRATE = 1,
  RS_PREP = 2,
  RS_ACTIVE = 3,
  RS_JUDGE = 4,
  RS_END = 5
};

RoundState g_state = RS_BOOT;

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static const char *stateName(RoundState state) {
  if (state == RS_BOOT) return "BOOT";
  if (state == RS_CALIBRATE) return "CAL ";
  if (state == RS_PREP) return "PREP";
  if (state == RS_ACTIVE) return "ACT ";
  if (state == RS_JUDGE) return "JDG ";
  return "END ";
}

static void lcdPulseEnable(void) {
  digitalWrite(PIN_LCD_E, LOW);
  delayMicroseconds(1);
  digitalWrite(PIN_LCD_E, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_LCD_E, LOW);
  delayMicroseconds(100);
}

static void lcdWriteNibble(unsigned char nibble) {
  digitalWrite(PIN_LCD_D4, (nibble & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D5, (nibble & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D6, (nibble & 0x04) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D7, (nibble & 0x08) ? HIGH : LOW);
  lcdPulseEnable();
}

static void lcdSend(unsigned char value, int isData) {
  digitalWrite(PIN_LCD_RS, isData ? HIGH : LOW);
  lcdWriteNibble((unsigned char)(value >> 4));
  lcdWriteNibble((unsigned char)(value & 0x0F));
}

static void lcdCommand(unsigned char value) {
  lcdSend(value, 0);
  if (value == 0x01 || value == 0x02) delayMicroseconds(2000);
}

static void lcdData(unsigned char value) {
  lcdSend(value, 1);
}

static void lcdSetCursor(int col, int row) {
  static const unsigned char rowOffsets[2] = {0x00, 0x40};
  lcdCommand((unsigned char)(0x80 | (rowOffsets[row] + col)));
}

static void lcdPrintText(const char *text) {
  while (*text) {
    lcdData((unsigned char)*text++);
  }
}

static void lcdInit(void) {
  pinMode(PIN_LCD_RS, OUTPUT);
  pinMode(PIN_LCD_E, OUTPUT);
  pinMode(PIN_LCD_D4, OUTPUT);
  pinMode(PIN_LCD_D5, OUTPUT);
  pinMode(PIN_LCD_D6, OUTPUT);
  pinMode(PIN_LCD_D7, OUTPUT);

  digitalWrite(PIN_LCD_RS, LOW);
  digitalWrite(PIN_LCD_E, LOW);
  delayMicroseconds(50000);

  lcdWriteNibble(0x03);
  delayMicroseconds(4500);
  lcdWriteNibble(0x03);
  delayMicroseconds(4500);
  lcdWriteNibble(0x03);
  delayMicroseconds(150);
  lcdWriteNibble(0x02);

  lcdCommand(0x28);
  lcdCommand(0x0C);
  lcdCommand(0x06);
  lcdCommand(0x01);
}

static void setState(RoundState next) {
  g_state = next;
  g_stateStartMs = millis();

  if (g_state == RS_BOOT) {
    g_servoDeg = 90;
    g_stepMode = 0;
    g_buzzMode = 0;
    g_rgbCode = 4;
  } else if (g_state == RS_CALIBRATE) {
    g_calDistSum = 0;
    g_calLuxSum = 0;
    g_calTempSum = 0;
    g_calHumSum = 0;
    g_calCount = 0;
    g_servoDeg = 90;
    g_stepMode = 2;
    g_buzzMode = 0;
    g_rgbCode = 4;
  } else if (g_state == RS_PREP) {
    g_servoDeg = 90;
    g_stepMode = 2;
    g_buzzMode = 0;
    g_rgbCode = 5;
  } else if (g_state == RS_ACTIVE) {
    g_defenderScore = 0;
    g_attackerScore = 0;
    g_lastScoreTickMs = millis();
    g_stepMode = 2;
  } else if (g_state == RS_JUDGE) {
    g_stepMode = 0;
    g_buzzMode = 0;
    g_rgbCode = (g_defenderScore >= g_attackerScore) ? 1 : 3;
  } else {
    g_stepMode = 0;
    g_buzzMode = 0;
    g_rgbCode = 6;
  }

  Serial.print("STATE -> ");
  Serial.println((int)g_state);
}

static void sendControlFrame(void) {
  unsigned long now = millis();
  if (g_waitingForSensor && (now - g_lastControlTxMs) < 160) return;
  if (!g_waitingForSensor && (now - g_lastControlTxMs) < 90) return;
  g_lastControlTxMs = now;

  char out[96];
  snprintf(out,
           sizeof(out),
           "$C,%d,%d,%d,%d,%d\n",
           g_servoDeg,
           g_stepMode,
           g_buzzMode,
           g_rgbCode,
           (int)g_state);
  bus.print(out);
  g_waitingForSensor = 1;
}

static void handleSensorLine(const char *line) {
  unsigned long ms = 0;
  int sector = 0;
  int distCm = 0;
  int lux = 0;
  int tilt = 0;
  int tempC10 = 0;
  int hum10 = 0;
  int irRaw = 0;
  int joy = 0;

  if (sscanf(line,
             "$S,%lu,%d,%d,%d,%d,%d,%d,%d,%d",
             &ms,
             &sector,
             &distCm,
             &lux,
             &tilt,
             &tempC10,
             &hum10,
             &irRaw,
             &joy) == 9) {
    g_sensor.ms = ms;
    g_sensor.sector = clampInt(sector, 0, SECTOR_COUNT - 1);
    g_sensor.distCm = clampInt(distCm, 2, 400);
    g_sensor.lux = clampInt(lux, 0, 1023);
    g_sensor.tilt = (tilt != 0) ? 1 : 0;
    g_sensor.tempC10 = clampInt(tempC10, -200, 800);
    g_sensor.hum10 = clampInt(hum10, 0, 1000);
    g_sensor.irRaw = (irRaw != 0) ? 1 : 0;
    g_sensor.joy = joy;
    g_lastSensorRxMs = millis();
    g_waitingForSensor = 0;
    return;
  }

  if (line[0] == '$' && line[1] == 'A') {
    g_waitingForSensor = 0;
    Serial.print("ACK: ");
    Serial.println(line);
    return;
  }

  Serial.print("Bad bus line: ");
  Serial.println(line);
}

static void pollBus(void) {
  while (bus.available()) {
    char c = (char)bus.read();
    if (c == '\r' || c == '\n') {
      busLine[busLineIdx] = '\0';
      if (busLineIdx > 0) handleSensorLine(busLine);
      busLineIdx = 0;
      continue;
    }

    if (busLineIdx < (int)sizeof(busLine) - 1) {
      busLine[busLineIdx++] = c;
    }
  }
}

static void startRound(void) {
  setState(RS_CALIBRATE);
}

static void handleSerialCommand(const char *line) {
  int v = 0;

  if (strcmp(line, "START") == 0 || strcmp(line, "start") == 0) {
    startRound();
    return;
  }

  if (strcmp(line, "RESET") == 0 || strcmp(line, "reset") == 0) {
    setState(RS_BOOT);
    return;
  }

  if (sscanf(line, "MISSION,%d", &v) == 1) {
    g_missionDifficulty = clampInt(v, 1, 5);
    Serial.print("MISSION level=");
    Serial.println(g_missionDifficulty);
    return;
  }

  if (strcmp(line, "STATUS") == 0 || strcmp(line, "status") == 0) {
    Serial.print("state=");
    Serial.print((int)g_state);
    Serial.print(" def=");
    Serial.print(g_defenderScore);
    Serial.print(" atk=");
    Serial.print(g_attackerScore);
    Serial.print(" difficulty=");
    Serial.println(g_missionDifficulty);
    return;
  }

  Serial.print("Unknown serial cmd: ");
  Serial.println(line);
}

static void pollSerial(void) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      serialLine[serialLineIdx] = '\0';
      if (serialLineIdx > 0) handleSerialCommand(serialLine);
      serialLineIdx = 0;
      continue;
    }

    if (serialLineIdx < (int)sizeof(serialLine) - 1) {
      serialLine[serialLineIdx++] = c;
    }
  }
}

static int computeThreatLevel(void) {
  int threat = 0;
  int dist = g_sensor.distCm;
  int luxDelta = g_sensor.lux - g_baseLux;
  if (luxDelta < 0) luxDelta = -luxDelta;

  if (dist < 70) threat += 1;
  if (dist < 45) threat += 2;
  if (dist < 25) threat += 3;

  if (luxDelta > 90) threat += 1;
  if (luxDelta > 180) threat += 2;

  if (g_sensor.tilt) threat += 3;

  int tempDelta = g_sensor.tempC10 - g_baseTemp10;
  if (tempDelta < 0) tempDelta = -tempDelta;
  if (tempDelta > 20) threat += 1;

  threat = threat * g_missionDifficulty;
  threat = clampInt(threat, 0, 15);
  return threat;
}

static void updateInputTriggers(void) {
  int startNow = digitalRead(PIN_START_BTN);
  int joyNow = digitalRead(PIN_JOY_SW);

  if (g_prevStartBtn == HIGH && startNow == LOW) {
    if (g_state == RS_BOOT || g_state == RS_END) {
      startRound();
    }
  }

  if (g_prevJoyBtn == HIGH && joyNow == LOW) {
    if (g_state == RS_END) {
      startRound();
    }
  }

  g_prevStartBtn = startNow;
  g_prevJoyBtn = joyNow;
}

static void updateStateMachine(void) {
  unsigned long now = millis();

  if ((now - g_lastSensorRxMs) > SENSOR_TIMEOUT_MS) {
    g_buzzMode = 2;
    g_rgbCode = 3;
  }

  if (g_state == RS_BOOT) {
    return;
  }

  if (g_state == RS_CALIBRATE) {
    g_calDistSum += g_sensor.distCm;
    g_calLuxSum += g_sensor.lux;
    g_calTempSum += g_sensor.tempC10;
    g_calHumSum += g_sensor.hum10;
    g_calCount++;

    if ((now - g_stateStartMs) >= CALIBRATION_MS) {
      if (g_calCount > 0) {
        g_baseDist = (int)(g_calDistSum / g_calCount);
        g_baseLux = (int)(g_calLuxSum / g_calCount);
        g_baseTemp10 = (int)(g_calTempSum / g_calCount);
        g_baseHum10 = (int)(g_calHumSum / g_calCount);
      }
      setState(RS_PREP);
    }
    return;
  }

  if (g_state == RS_PREP) {
    if ((now - g_stateStartMs) >= PREP_MS) {
      setState(RS_ACTIVE);
    }
    return;
  }

  if (g_state == RS_ACTIVE) {
    int joyX = analogRead(PIN_JOY_X);
    g_shieldSector = map(joyX, 0, 1023, 0, SECTOR_COUNT - 1);
    g_shieldSector = clampInt(g_shieldSector, 0, SECTOR_COUNT - 1);
    g_servoDeg = map(g_shieldSector, 0, SECTOR_COUNT - 1, 20, 160);

    int threatSector = g_sensor.sector;
    int threat = computeThreatLevel();
    int delta = g_shieldSector - threatSector;
    if (delta < 0) delta = -delta;
    int blocked = (delta <= 1);

    if ((now - g_lastScoreTickMs) >= 200) {
      g_lastScoreTickMs = now;

      if (threat >= 6) {
        if (blocked) g_defenderScore += (2 + threat);
        else g_attackerScore += (2 + threat);
      } else if (threat >= 3) {
        if (blocked) g_defenderScore += (1 + threat);
        else g_attackerScore += (1 + threat);
      } else {
        g_defenderScore += 1;
      }

      if (g_sensor.tilt) g_attackerScore += g_missionDifficulty;
    }

    if (threat >= 9) {
      g_buzzMode = 1;
      g_rgbCode = 3;
    } else if (threat >= 4) {
      g_buzzMode = 2;
      g_rgbCode = 2;
    } else {
      g_buzzMode = 0;
      g_rgbCode = 1;
    }

    if ((now - g_stateStartMs) >= ACTIVE_ROUND_MS) {
      setState(RS_JUDGE);
    }
    return;
  }

  if (g_state == RS_JUDGE) {
    if ((now - g_stateStartMs) >= JUDGE_MS) {
      setState(RS_END);
    }
    return;
  }
}

static void updateStatusLed(void) {
  if (g_state == RS_ACTIVE) {
    digitalWrite(PIN_STATUS_LED, ((millis() / 180) % 2) ? HIGH : LOW);
  } else if (g_state == RS_END) {
    digitalWrite(PIN_STATUS_LED, HIGH);
  } else {
    digitalWrite(PIN_STATUS_LED, LOW);
  }
}

static void updateLcd(void) {
  unsigned long now = millis();
  if (now - g_lastLcdMs < 140) return;
  g_lastLcdMs = now;

  char line0[17];
  char line1[17];
  int dispDef = g_defenderScore % 1000;
  int dispAtk = g_attackerScore % 1000;
  int dispDist = clampInt(g_sensor.distCm, 0, 999);

  snprintf(line0, sizeof(line0), "%-4s S%02d H%02d", stateName(g_state), g_sensor.sector, g_shieldSector);
  snprintf(line1, sizeof(line1), "D%03d A%03d %03d", dispDef, dispAtk, dispDist);

  lcdSetCursor(0, 0);
  lcdPrintText("                ");
  lcdSetCursor(0, 0);
  lcdPrintText(line0);

  lcdSetCursor(0, 1);
  lcdPrintText("                ");
  lcdSetCursor(0, 1);
  lcdPrintText(line1);
}

static void logStatus(void) {
  if (millis() - g_lastLogMs < 700) return;
  g_lastLogMs = millis();

  Serial.print("state=");
  Serial.print((int)g_state);
  Serial.print(" sec=");
  Serial.print(g_sensor.sector);
  Serial.print(" shield=");
  Serial.print(g_shieldSector);
  Serial.print(" dist=");
  Serial.print(g_sensor.distCm);
  Serial.print(" lux=");
  Serial.print(g_sensor.lux);
  Serial.print(" tilt=");
  Serial.print(g_sensor.tilt);
  Serial.print(" def=");
  Serial.print(g_defenderScore);
  Serial.print(" atk=");
  Serial.print(g_attackerScore);
  Serial.print(" diff=");
  Serial.println(g_missionDifficulty);
}

void setup() {
  Serial.begin(115200);
  bus.begin(9600);
  lcdInit();

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_START_BTN, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);

  setState(RS_BOOT);
  lcdSetCursor(0, 0);
  lcdPrintText("AEGIS-172");
  lcdSetCursor(0, 1);
  lcdPrintText("Booting...");

  Serial.println("AEGIS master emulator ready");
  Serial.println("Commands: START | RESET | MISSION,<1..5> | STATUS");
  Serial.println("UART TX: $C,<servo>,<step>,<buzz>,<rgb>,<state>");
  Serial.println("UART RX: $S,<ms>,<sector>,<dist>,<lux>,<tilt>,<temp10>,<hum10>,<ir>,<joy>");
}

void loop() {
  pollBus();
  pollSerial();
  updateInputTriggers();
  updateStateMachine();
  updateStatusLed();
  updateLcd();
  sendControlFrame();
  logStatus();
}
