#include <Servo.h>

// ===== Pin Map =====
const int PIN_SERVO  = 9;   // SG90 signal
const int PIN_BUZZER = 10;  // optional buzzer

// HC-SR04 pins
const int TRIG_N = 2,  ECHO_N = 3;  // North/front
const int TRIG_E = 4,  ECHO_E = 5;  // East/right
const int TRIG_W = 6,  ECHO_W = 7;  // West/left
const int TRIG_S = A0, ECHO_S = A1; // South/rear (A0/A1 as digital)

Servo speedServo;

// ===== Config =====
const unsigned long SENSOR_PERIOD_MS = 500;   // 2 Hz total (human-readable)
const unsigned long PULSE_TIMEOUT_US = 20000; // ~3.4m max
const int MIN_CM = 2;
const int MAX_CM = 400;
const int JUMP_CM = 150; // reject sudden spikes
const int INVALID_CM = 999;
const int INTER_SENSOR_DELAY_MS = 5;

unsigned long lastSensorMs = 0;
bool streamEnabled = true;
int lastN = INVALID_CM;
int lastE = INVALID_CM;
int lastW = INVALID_CM;
int lastS = INVALID_CM;

void setup() {
  Serial.begin(4800);

  pinMode(TRIG_N, OUTPUT); pinMode(ECHO_N, INPUT);
  pinMode(TRIG_E, OUTPUT); pinMode(ECHO_E, INPUT);
  pinMode(TRIG_W, OUTPUT); pinMode(ECHO_W, INPUT);
  pinMode(TRIG_S, OUTPUT); pinMode(ECHO_S, INPUT);

  pinMode(PIN_BUZZER, OUTPUT);
  speedServo.attach(PIN_SERVO);

  digitalWrite(TRIG_N, LOW);
  digitalWrite(TRIG_E, LOW);
  digitalWrite(TRIG_W, LOW);
  digitalWrite(TRIG_S, LOW);

  Serial.println("Arduino ready");
}

void loop() {
  // Read incoming commands
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("$M,")) {
      handleMotorCmd(line);
    } else if (line.startsWith("STREAM,")) {
      handleStreamCmd(line);
    } else if (line.startsWith("BUZZER,")) {
      handleBuzzerCmd(line);
    }
  }

  // Periodic sensor frame
  unsigned long now = millis();
  if (streamEnabled && (now - lastSensorMs >= SENSOR_PERIOD_MS)) {
    lastSensorMs = now;

    int dN = readFilteredCm(TRIG_N, ECHO_N, lastN);
    delay(INTER_SENSOR_DELAY_MS);
    int dE = readFilteredCm(TRIG_E, ECHO_E, lastE);
    delay(INTER_SENSOR_DELAY_MS);
    int dW = readFilteredCm(TRIG_W, ECHO_W, lastW);
    delay(INTER_SENSOR_DELAY_MS);
    int dS = readFilteredCm(TRIG_S, ECHO_S, lastS);

    lastN = dN;
    lastE = dE;
    lastW = dW;
    lastS = dS;

    // Send $S,<front>,<right>,<left>,<rear>\n
    Serial.print("$S,");
    Serial.print(dN); Serial.print(",");
    Serial.print(dE); Serial.print(",");
    Serial.print(dW); Serial.print(",");
    Serial.print(dS);
    Serial.print("\n");
  }
}

// ===== Helpers =====
int readCm(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long dur = pulseIn(echoPin, HIGH, PULSE_TIMEOUT_US);
  if (dur == 0) return INVALID_CM; // timeout / no echo
  int cm = (int)(dur / 58);
  if (cm < MIN_CM || cm > MAX_CM) return INVALID_CM;
  return cm;
}

int readFilteredCm(int trigPin, int echoPin, int lastValue) {
  int cm = readCm(trigPin, echoPin);
  if (cm == INVALID_CM) return (lastValue == INVALID_CM) ? INVALID_CM : lastValue;
  if (lastValue != INVALID_CM && abs(cm - lastValue) > JUMP_CM) return lastValue;
  return cm;
}

void handleMotorCmd(const String &line) {
  // Format: $M,<speed_pct>,<steer_angle>
  int c1 = line.indexOf(',', 3);
  if (c1 < 0) return;

  int speedPct = line.substring(3, c1).toInt();   // 0..100
  int steerDeg = line.substring(c1 + 1).toInt();  // 0..180

  speedPct = constrain(speedPct, 0, 100);
  steerDeg = constrain(steerDeg, 0, 180);

  // SG90 shows speed
  int speedAngle = map(speedPct, 0, 100, 0, 180);
  speedServo.write(speedAngle);

  // Show steering angle on Serial Monitor
  Serial.print("MOTOR speed_pct=");
  Serial.print(speedPct);
  Serial.print(" (SG90->");
  Serial.print(speedAngle);
  Serial.print(" deg), steer=");
  Serial.println(steerDeg);
}

void handleStreamCmd(const String &line) {
  // Format: STREAM,1 or STREAM,0
  int comma = line.indexOf(',');
  if (comma < 0) return;
  int state = line.substring(comma + 1).toInt();
  streamEnabled = (state != 0);
  Serial.print("STREAM ");
  Serial.println(streamEnabled ? "ON" : "OFF");
}

void handleBuzzerCmd(const String &line) {
  // Format: BUZZER,1 or BUZZER,0
  int comma = line.indexOf(',');
  if (comma < 0) return;
  int state = line.substring(comma + 1).toInt();
  digitalWrite(PIN_BUZZER, (state != 0) ? HIGH : LOW);
}
