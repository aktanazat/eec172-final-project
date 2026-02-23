#include <Servo.h>

// ===== PIN DEFINITIONS =====
Servo myServo;
int servoPin = 9;
int motor_IN1 = 7, motor_IN2 = 8, motor_ENA = 6;
int buzzerPin = 10;

// ===== SETUP =====
void setup() {
  Serial.begin(9600); // Match CC3200 baud rate
  myServo.attach(servoPin);
  pinMode(motor_IN1, OUTPUT);
  pinMode(motor_IN2, OUTPUT);
  pinMode(motor_ENA, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  Serial.println("Arduino initialized");
}

// ===== MAIN LOOP =====
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); // Remove whitespace
    Serial.print(">>> Received: ");
    Serial.println(cmd);
    handleCommand(cmd);
  }
}

// ===== COMMAND DISPATCHER =====
void handleCommand(String cmd) {
  if (cmd.startsWith("SERVO,")) {
    handleServo(cmd);
  } else if (cmd.startsWith("MOTOR,")) {
    handleMotor(cmd);
  } else if (cmd.startsWith("BUZZER,")) {
    handleBuzzer(cmd);
  } else if (cmd.startsWith("FRAME")) {
    sendFakeFrame();
  } else {
    Serial.println("ERROR: Unknown command");
  }
}

// ===== SERVO CONTROL =====
void handleServo(String cmd) {
  int angle = cmd.substring(6).toInt();
  angle = constrain(angle, 0, 180);
  myServo.write(angle);
  Serial.print("Servo -> ");
  Serial.println(angle);
}

// ===== MOTOR CONTROL =====
void handleMotor(String cmd) {
  int comma1 = cmd.indexOf(',');
  int comma2 = cmd.indexOf(',', comma1 + 1);
  int dir = cmd.substring(comma1 + 1, comma2).toInt();
  int pwm = cmd.substring(comma2 + 1).toInt();
  
  pwm = constrain(pwm, 0, 255);
  
  digitalWrite(motor_IN1, (dir == 1) ? HIGH : LOW);
  digitalWrite(motor_IN2, (dir == 1) ? LOW : HIGH);
  analogWrite(motor_ENA, pwm);
  
  Serial.print("Motor -> dir:");
  Serial.print(dir);
  Serial.print(" pwm:");
  Serial.println(pwm);
}

// ===== BUZZER CONTROL =====
void handleBuzzer(String cmd) {
  int state = cmd.substring(7).toInt();
  digitalWrite(buzzerPin, (state == 1) ? HIGH : LOW);
  Serial.print("Buzzer -> ");
  Serial.println((state == 1) ? "ON" : "OFF");
}

// ===== FAKE FRAME SENDER =====
void sendFakeFrame() {
  Serial.println("$S,050,100,030,080");
}