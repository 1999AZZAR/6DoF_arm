/*
 * 6DOF Robot Arm Serial Control
 * Arduino Nano controlled 6DOF robot arm with serial communication
 * Compatible with Python Qt6 GUI interface
 *
 * Serial Protocol:
 * Commands from Python:
 *   J1:90     - Set joint 1 to 90 degrees
 *   J2:45     - Set joint 2 to 45 degrees
 *   J3:0      - Set joint 3 to 0 degrees
 *   J4:108    - Set joint 4 to 108 degrees
 *   J5:80     - Set joint 5 to 80 degrees
 *   J6:152    - Set joint 6 to 152 degrees
 *   HOME      - Go to home position
 *   STOP      - Emergency stop
 *   STATUS    - Request current positions
 *
 * Responses to Python:
 *   OK:J1:90,J2:45,J3:0,J4:108,J5:80,J6:152
 *   ERROR:Joint limit exceeded
 *   ERROR:Invalid command
 */

#include <Servo.h>
#include "config.h"

// Joint names for display/debugging
const char* JOINT_NAMES[6] = {
    "Base", "Shoulder", "Elbow", "Wrist Rot", "Wrist Bend", "Gripper"
};

// Servo objects for 6DOF arm
Servo servo1; // Base rotation
Servo servo2; // Shoulder
Servo servo3; // Elbow
Servo servo4; // Wrist rotation
Servo servo5; // Wrist bend
Servo servo6; // Gripper

// Current joint positions
int jointPositions[6] = {92, 85, 45, 108, 80, 152};

// Serial communication
String inputString = "";
boolean stringComplete = false;

// Safety flags
boolean emergencyStop = false;
boolean movementInProgress = false;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  inputString.reserve(SERIAL_BUFFER_SIZE);

  // Initialize servos
  attachServos();

  // Move to home position
  setupHome();

  Serial.println("6DOF Arm Ready");
  Serial.println("OK:J1:92,J2:85,J3:45,J4:108,J5:80,J6:152");
}

void loop() {
  // Handle serial communication
  if (stringComplete) {
    processCommand(inputString);
    inputString = "";
    stringComplete = false;
  }
}

// Attach all servos
void attachServos() {
  servo1.attach(SERVO_PINS[0]);
  servo2.attach(SERVO_PINS[1]);
  servo3.attach(SERVO_PINS[2]);
  servo4.attach(SERVO_PINS[3]);
  servo5.attach(SERVO_PINS[4]);
  servo6.attach(SERVO_PINS[5]);
}

// Move to home position
void setupHome() {
  for (int i = 0; i < 6; i++) {
    moveServo(i, HOME_POSITIONS[i]);
    jointPositions[i] = HOME_POSITIONS[i];
  }
}

// Move single servo with speed control
void moveServo(int jointIndex, int targetAngle) {
  if (emergencyStop) {
    Serial.println("ERROR:Emergency stop active");
    return;
  }

  Servo* servo;
  switch(jointIndex) {
    case 0: servo = &servo1; break;
    case 1: servo = &servo2; break;
    case 2: servo = &servo3; break;
    case 3: servo = &servo4; break;
    case 4: servo = &servo5; break;
    case 5: servo = &servo6; break;
    default: return;
  }

  movementInProgress = true;
  int currentAngle = jointPositions[jointIndex];

  if (currentAngle < targetAngle) {
    for (int angle = currentAngle; angle <= targetAngle; angle++) {
      if (emergencyStop) break;
      servo->write(angle);
      delay(MOVE_SPEED);
    }
  } else if (currentAngle > targetAngle) {
    for (int angle = currentAngle; angle >= targetAngle; angle--) {
      if (emergencyStop) break;
      servo->write(angle);
      delay(MOVE_SPEED);
    }
  }

  jointPositions[jointIndex] = targetAngle;
  movementInProgress = false;
}

// Process incoming serial commands
void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.startsWith(CMD_JOINT_PREFIX)) {
    // Joint control command (e.g., "J1:90")
    processJointCommand(command);
  } else if (command == CMD_HOME) {
    setupHome();
    sendStatus();
  } else if (command == CMD_STOP) {
    emergencyStop = true;
    movementInProgress = false;
    Serial.println("ERROR:Emergency stop activated");
    delay(100); // Brief pause to ensure stop
    emergencyStop = false; // Reset for next commands
    sendStatus();
  } else if (command == CMD_STATUS) {
    sendStatus();
  } else {
    Serial.println("ERROR:Invalid command");
  }
}

// Process joint control commands
void processJointCommand(String command) {
  // Parse command format: J1:90
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("ERROR:Invalid joint command format");
    return;
  }

  String jointStr = command.substring(1, colonIndex);
  String angleStr = command.substring(colonIndex + 1);

  int jointIndex = jointStr.toInt() - 1; // Convert to 0-based index
  int targetAngle = angleStr.toInt();

  // Validate joint index
  if (jointIndex < 0 || jointIndex >= 6) {
    Serial.println("ERROR:Invalid joint number (1-6)");
    return;
  }

  // Validate angle limits
  if (targetAngle < JOINT_MIN[jointIndex] || targetAngle > JOINT_MAX[jointIndex]) {
    Serial.print("ERROR:Joint ");
    Serial.print(jointIndex + 1);
    Serial.print(" angle out of range (");
    Serial.print(JOINT_MIN[jointIndex]);
    Serial.print("-");
    Serial.print(JOINT_MAX[jointIndex]);
    Serial.println(")");
    return;
  }

  // Move servo
  moveServo(jointIndex, targetAngle);
  sendStatus();
}

// Send current status
void sendStatus() {
  Serial.print(RESP_OK);
  for (int i = 0; i < 6; i++) {
    Serial.print("J");
    Serial.print(i + 1);
    Serial.print(":");
    Serial.print(jointPositions[i]);
    if (i < 5) Serial.print(",");
  }
  Serial.println();
}

// Serial event handler
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
}
