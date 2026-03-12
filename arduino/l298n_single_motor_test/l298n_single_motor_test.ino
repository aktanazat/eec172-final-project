const int DRIVE_ENA = 3;
const int DRIVE_IN1 = A0;
const int DRIVE_IN2 = A1;

const int STEER_ENB = 12;
const int STEER_IN3 = A3;
const int STEER_IN4 = A4;

int g_drivePwm = 255;
char g_line[64];
int g_lineIdx = 0;

static void setRearRaw(int pwm, int in1, int in2) {
  digitalWrite(DRIVE_IN1, in1);
  digitalWrite(DRIVE_IN2, in2);
  analogWrite(DRIVE_ENA, pwm);
}

static void setRear(int signedPwm) {
  int pwm = signedPwm;
  if (pwm > 255) pwm = 255;
  if (pwm < -255) pwm = -255;

  if (pwm > 0) {
    setRearRaw(pwm, HIGH, LOW);
    Serial.print("OK REAR FWD PWM=");
    Serial.println(pwm);
  } else if (pwm < 0) {
    setRearRaw(-pwm, LOW, HIGH);
    Serial.print("OK REAR REV PWM=");
    Serial.println(-pwm);
  } else {
    setRearRaw(0, LOW, LOW);
    Serial.println("OK REAR STOP");
  }
}

static void setSteer(int signedCmd) {
  if (signedCmd > 0) {
    digitalWrite(STEER_IN3, HIGH);
    digitalWrite(STEER_IN4, LOW);
    digitalWrite(STEER_ENB, HIGH);
    Serial.println("OK STEER RIGHT");
  } else if (signedCmd < 0) {
    digitalWrite(STEER_IN3, LOW);
    digitalWrite(STEER_IN4, HIGH);
    digitalWrite(STEER_ENB, HIGH);
    Serial.println("OK STEER LEFT");
  } else {
    digitalWrite(STEER_IN3, LOW);
    digitalWrite(STEER_IN4, LOW);
    digitalWrite(STEER_ENB, LOW);
    Serial.println("OK STEER STOP");
  }
}

static void handleCommand(const char *line) {
  int speed = 0;
  int steer = 0;
  int value = 0;

  if (strcmp(line, "F") == 0 || strcmp(line, "f") == 0) {
    setRear(g_drivePwm);
    return;
  }

  if (strcmp(line, "R") == 0 || strcmp(line, "r") == 0) {
    setRear(-g_drivePwm);
    return;
  }

  if (strcmp(line, "S") == 0 || strcmp(line, "s") == 0) {
    setRear(0);
    return;
  }

  if (strcmp(line, "A") == 0 || strcmp(line, "a") == 0) {
    setSteer(-1);
    return;
  }

  if (strcmp(line, "D") == 0 || strcmp(line, "d") == 0) {
    setSteer(1);
    return;
  }

  if (strcmp(line, "C") == 0 || strcmp(line, "c") == 0) {
    setSteer(0);
    return;
  }

  if (sscanf(line, "PWM,%d", &value) == 1) {
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    g_drivePwm = value;
    Serial.print("OK PWM=");
    Serial.println(g_drivePwm);
    return;
  }

  if (sscanf(line, "DRIVE,%d", &value) == 1) {
    setRear(value);
    return;
  }

  if (sscanf(line, "STEER,%d", &value) == 1) {
    if (value > 0) setSteer(1);
    else if (value < 0) setSteer(-1);
    else setSteer(0);
    return;
  }

  if (sscanf(line, "$M,%d,%d", &speed, &steer) == 2) {
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    if (steer > 100) steer = 100;
    if (steer < -100) steer = -100;

    setRear((speed * 255) / 100);
    if (steer > 0) setSteer(1);
    else if (steer < 0) setSteer(-1);
    else setSteer(0);
    return;
  }

  Serial.print("ERR unknown cmd: ");
  Serial.println(line);
}

void setup() {
  Serial.begin(115200);

  pinMode(DRIVE_ENA, OUTPUT);
  pinMode(DRIVE_IN1, OUTPUT);
  pinMode(DRIVE_IN2, OUTPUT);
  pinMode(STEER_ENB, OUTPUT);
  pinMode(STEER_IN3, OUTPUT);
  pinMode(STEER_IN4, OUTPUT);

  setRear(0);
  setSteer(0);

  delay(300);
  Serial.println("BOOT");
  Serial.println("Dual motor bench mode ready");
  Serial.println("Rear: F R S PWM,<0..255> DRIVE,<-255..255>");
  Serial.println("Steer: A(left) D(right) C(stop) STEER,<-255..255>");
  Serial.println("Combo: $M,<-100..100>,<-100..100>");
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      g_line[g_lineIdx] = '\0';
      if (g_lineIdx > 0) {
        handleCommand(g_line);
      }
      g_lineIdx = 0;
      continue;
    }

    if (g_lineIdx < (int)sizeof(g_line) - 1) {
      g_line[g_lineIdx++] = c;
    }
  }
}
