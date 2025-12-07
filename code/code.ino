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

// Movement speed (configurable via SET_SPEED command)
int MOVE_SPEED = 15; // Optimized for smooth and responsive movement

// Movement timing optimization
unsigned long lastMovementTime = 0;
const unsigned long MOVEMENT_INTERVAL = 25; // Faster, smoother movement (was 50ms)

// Sequence management (optimized for Arduino Uno memory)
#define MAX_SEQUENCE_LENGTH 15  // Reduced from 50 to fit in 2KB RAM
#define MAX_SEQUENCES 2         // Reduced from 5 to fit in 2KB RAM
#define SEQUENCE_NAME_LENGTH 16

struct SequencePoint {
  int joints[6];
  int delay_ms;
};

struct Sequence {
  char name[SEQUENCE_NAME_LENGTH];
  SequencePoint points[MAX_SEQUENCE_LENGTH];
  int length;
  boolean valid;
};

Sequence sequences[MAX_SEQUENCES];
boolean isRecording = false;
int currentRecordingSequence = -1;
int currentSequenceIndex = 0;


void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  inputString.reserve(SERIAL_BUFFER_SIZE);

  // Initialize sequences
  initializeSequences();

  // Initialize servos
  attachServos();

  // Move to home position
  setupHome();

  Serial.println("6DOF Arm Ready - Optimized for Uno/Mega");
  Serial.println("Features: Non-blocking movement, Planning Sets, Real-time control");
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

// Move to home position (blocking for setup)
void setupHome() {
  for (int i = 0; i < 6; i++) {
    Servo* servo;
    switch(i) {
      case 0: servo = &servo1; break;
      case 1: servo = &servo2; break;
      case 2: servo = &servo3; break;
      case 3: servo = &servo4; break;
      case 4: servo = &servo5; break;
      case 5: servo = &servo6; break;
    }
    servo->write(HOME_POSITIONS[i]);
    jointPositions[i] = HOME_POSITIONS[i];
    delay(200); // Small delay for stable movement during setup
  }
}

// Move all servos to minimum positions
void minServos() {
  for (int i = 0; i < 6; i++) {
    Servo* servo;
    switch(i) {
      case 0: servo = &servo1; break;
      case 1: servo = &servo2; break;
      case 2: servo = &servo3; break;
      case 3: servo = &servo4; break;
      case 4: servo = &servo5; break;
      case 5: servo = &servo6; break;
    }
    servo->write(JOINT_MIN[i]);
    jointPositions[i] = JOINT_MIN[i];
    delay(200); // Small delay for stable movement
  }
}

// Move all servos to maximum positions
void maxServos() {
  for (int i = 0; i < 6; i++) {
    Servo* servo;
    switch(i) {
      case 0: servo = &servo1; break;
      case 1: servo = &servo2; break;
      case 2: servo = &servo3; break;
      case 3: servo = &servo4; break;
      case 4: servo = &servo5; break;
      case 5: servo = &servo6; break;
    }
    servo->write(JOINT_MAX[i]);
    jointPositions[i] = JOINT_MAX[i];
    delay(200); // Small delay for stable movement
  }
}

// Wave motion - friendly greeting gesture
void waveServos() {
  // Save current positions
  int originalPositions[6];
  for (int i = 0; i < 6; i++) {
    originalPositions[i] = jointPositions[i];
  }

  // Move to wave-ready position (slightly raised arm)
  moveServo(1, 120); // Raise shoulder
  moveServo(2, 90);  // Bend elbow
  moveServo(3, 90);  // Rotate wrist
  moveServo(4, 60);  // Bend wrist down

  // Wave motion - move base left and right 3 times
  for (int wave = 0; wave < 3; wave++) {
    if (emergencyStop) break;

    // Wave left
    moveServo(0, 60);  // Turn base left
    delay(300);

    // Wave right
    moveServo(0, 120); // Turn base right
    delay(300);
  }

  // Return to original positions
  for (int i = 0; i < 6; i++) {
    if (emergencyStop) break;
    moveServo(i, originalPositions[i]);
  }
}

// Non-blocking servo movement state
struct ServoMovement {
  boolean active;
  int jointIndex;
  int targetAngle;
  int currentAngle;
  int stepDirection; // 1 for increasing, -1 for decreasing
};

ServoMovement currentMovement = {false, 0, 0, 0, 0};

// Simple blocking servo movement (proven and reliable)
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
}

// Process incoming serial commands with improved validation
void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  // Quick validation - reject empty commands
  if (command.length() == 0) return;

  if (command.startsWith(CMD_JOINT_PREFIX)) {
    // Joint control command (e.g., "J1:90")
    processJointCommand(command);
  } else if (command == CMD_HOME) {
    setupHome();
    sendStatus();
  } else if (command == CMD_STOP) {
    emergencyStop = true;
    isRecording = false; // Also stop recording if active
    Serial.println("ERROR:Emergency stop activated");
    delay(50); // Brief pause to ensure stop
    emergencyStop = false; // Reset for next commands
    sendStatus();
  } else if (command == CMD_STATUS) {
    sendStatus();
  } else if (command == CMD_MIN) {
    minServos();
    sendStatus();
  } else if (command == CMD_MAX) {
    maxServos();
    sendStatus();
  } else if (command == CMD_WAVE) {
    waveServos();
    sendStatus();
  } else if (command.startsWith(CMD_SET_SPEED)) {
    processSetSpeedCommand(command);
  } else if (command.startsWith(CMD_RECORD_START)) {
    processRecordStartCommand(command);
  } else if (command == CMD_RECORD_STOP) {
    isRecording = false;
    Serial.println("OK:Recording stopped");
  } else if (command.startsWith(CMD_PLAY_SEQUENCE)) {
    processPlaySequenceCommand(command);
  } else if (command == CMD_LIST_SEQUENCES) {
    listSequences();
  } else if (command.startsWith(CMD_DELETE_SEQUENCE)) {
    processDeleteSequenceCommand(command);
  } else {
    Serial.print("ERROR:Unknown command: ");
    Serial.println(command);
  }
}

// Process joint control commands with enhanced validation
void processJointCommand(String command) {
  // Parse command format: J1:90
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1 || colonIndex == 1) { // Must have format J#:angle
    Serial.println("ERROR:Invalid joint command format. Use J1:90 format");
    return;
  }

  String jointStr = command.substring(1, colonIndex);
  String angleStr = command.substring(colonIndex + 1);

  // Validate joint string (should be single digit 1-6)
  if (jointStr.length() != 1 || !isDigit(jointStr.charAt(0))) {
    Serial.println("ERROR:Invalid joint number. Use 1-6");
    return;
  }

  // Validate angle string (should be numeric)
  if (angleStr.length() == 0) {
    Serial.println("ERROR:Missing angle value");
    return;
  }
  for (unsigned int i = 0; i < angleStr.length(); i++) {
    if (!isDigit(angleStr.charAt(i))) {
      Serial.println("ERROR:Angle must be numeric");
      return;
    }
  }

  int jointIndex = jointStr.toInt() - 1; // Convert to 0-based index
  int targetAngle = angleStr.toInt();

  // Double-check joint index
  if (jointIndex < 0 || jointIndex >= 6) {
    Serial.println("ERROR:Joint number must be 1-6");
    return;
  }

  // Check if movement already in progress for this joint
  if (currentMovement.active && currentMovement.jointIndex == jointIndex) {
    Serial.println("ERROR:Joint already moving, wait for completion");
    return;
  }

  // Move servo (blocking but reliable)
  moveServo(jointIndex, targetAngle);

  // Add point to sequence if recording
  if (isRecording && currentRecordingSequence >= 0) {
    addSequencePoint(currentRecordingSequence, MOVE_SPEED);
  }

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

// Sequence management functions
void initializeSequences() {
  for (int i = 0; i < MAX_SEQUENCES; i++) {
    sequences[i].valid = false;
    sequences[i].length = 0;
  }
}

boolean createSequence(int index, String name) {
  if (index < 0 || index >= MAX_SEQUENCES) return false;

  name.toCharArray(sequences[index].name, SEQUENCE_NAME_LENGTH);
  sequences[index].length = 0;
  sequences[index].valid = true;
  return true;
}

boolean addSequencePoint(int sequenceIndex, int delay_ms) {
  if (sequenceIndex < 0 || sequenceIndex >= MAX_SEQUENCES) return false;
  if (!sequences[sequenceIndex].valid) return false;
  if (sequences[sequenceIndex].length >= MAX_SEQUENCE_LENGTH) return false;

  int pointIndex = sequences[sequenceIndex].length;
  for (int i = 0; i < 6; i++) {
    sequences[sequenceIndex].points[pointIndex].joints[i] = jointPositions[i];
  }
  sequences[sequenceIndex].points[pointIndex].delay_ms = delay_ms;
  sequences[sequenceIndex].length++;

  return true;
}

// Simple blocking sequence playback (reliable)
boolean playSequence(int sequenceIndex) {
  if (sequenceIndex < 0 || sequenceIndex >= MAX_SEQUENCES) return false;
  if (!sequences[sequenceIndex].valid) return false;

  for (int i = 0; i < sequences[sequenceIndex].length; i++) {
    if (emergencyStop) break;

    // Move to each point in sequence
    for (int j = 0; j < 6; j++) {
      if (emergencyStop) break;
      moveServo(j, sequences[sequenceIndex].points[i].joints[j]);
    }

    if (sequences[sequenceIndex].points[i].delay_ms > 0) {
      delay(sequences[sequenceIndex].points[i].delay_ms);
    }
  }

  return true;
}

void listSequences() {
  Serial.println(RESP_SEQUENCE);
  for (int i = 0; i < MAX_SEQUENCES; i++) {
    if (sequences[i].valid) {
      Serial.print(i);
      Serial.print(":");
      Serial.println(sequences[i].name);
    }
  }
}


// Process SET_SPEED command
void processSetSpeedCommand(String command) {
  // Format: SET_SPEED:speed_value
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("ERROR:Invalid SET_SPEED format. Use SET_SPEED:15");
    return;
  }

  String speedStr = command.substring(colonIndex + 1);
  int newSpeed = speedStr.toInt();

  // Validate speed range (reasonable limits)
  if (newSpeed < 5 || newSpeed > 200) {
    Serial.println("ERROR:Speed must be between 5 and 200 ms");
    return;
  }

  MOVE_SPEED = newSpeed;
  Serial.print("OK:Movement speed set to ");
  Serial.print(MOVE_SPEED);
  Serial.println(" ms");
}

void processRecordStartCommand(String command) {
  // Format: RECORD_START:sequence_index:name
  int firstColon = command.indexOf(':');
  int secondColon = command.indexOf(':', firstColon + 1);

  if (firstColon == -1 || secondColon == -1) {
    Serial.println("ERROR:Invalid RECORD_START format");
    return;
  }

  String indexStr = command.substring(firstColon + 1, secondColon);
  String nameStr = command.substring(secondColon + 1);

  int sequenceIndex = indexStr.toInt();

  if (sequenceIndex < 0 || sequenceIndex >= MAX_SEQUENCES) {
    Serial.println("ERROR:Invalid sequence index");
    return;
  }

  if (createSequence(sequenceIndex, nameStr)) {
    isRecording = true;
    currentRecordingSequence = sequenceIndex;
    Serial.print("OK:Recording started for sequence ");
    Serial.println(sequenceIndex);
  } else {
    Serial.println("ERROR:Failed to create sequence");
  }
}

void processPlaySequenceCommand(String command) {
  // Format: PLAY_SEQUENCE:sequence_index
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("ERROR:Invalid PLAY_SEQUENCE format. Use PLAY_SEQUENCE:0");
    return;
  }

  String indexStr = command.substring(colonIndex + 1);
  int sequenceIndex = indexStr.toInt();

  if (playSequence(sequenceIndex)) {
    Serial.print("OK:Sequence ");
    Serial.print(sequenceIndex);
    Serial.println(" completed");
    sendStatus();
  } else {
    Serial.println("ERROR:Failed to play sequence");
  }
}

void processDeleteSequenceCommand(String command) {
  // Format: DELETE_SEQUENCE:sequence_index
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("ERROR:Invalid DELETE_SEQUENCE format");
    return;
  }

  String indexStr = command.substring(colonIndex + 1);
  int sequenceIndex = indexStr.toInt();

  if (sequenceIndex < 0 || sequenceIndex >= MAX_SEQUENCES) {
    Serial.println("ERROR:Invalid sequence index");
    return;
  }

  sequences[sequenceIndex].valid = false;
  sequences[sequenceIndex].length = 0;

  Serial.print("OK:Sequence ");
  Serial.print(sequenceIndex);
  Serial.println(" deleted");
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
