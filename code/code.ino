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

// Sequence playback state for non-blocking playback
struct SequencePlayback {
  boolean active;
  int sequenceIndex;
  int currentStep;
  unsigned long lastStepTime;
};

SequencePlayback currentPlayback = {false, -1, 0, 0};

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

  // Update servo movements (non-blocking)
  updateServoMovement();

  // Update sequence playback (non-blocking)
  updateSequencePlayback();
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

// Non-blocking servo movement state
struct ServoMovement {
  boolean active;
  int jointIndex;
  int targetAngle;
  int currentAngle;
  int stepDirection; // 1 for increasing, -1 for decreasing
};

ServoMovement currentMovement = {false, 0, 0, 0, 0};

// Initialize servo movement (non-blocking)
void startServoMovement(int jointIndex, int targetAngle) {
  if (emergencyStop || jointIndex < 0 || jointIndex >= 6) return;

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

  currentMovement.active = true;
  currentMovement.jointIndex = jointIndex;
  currentMovement.targetAngle = targetAngle;
  currentMovement.currentAngle = jointPositions[jointIndex];
  currentMovement.stepDirection = (targetAngle > jointPositions[jointIndex]) ? 1 : -1;

  movementInProgress = true;
}

// Update servo movement (call in main loop - non-blocking)
void updateServoMovement() {
  if (!currentMovement.active || emergencyStop) {
    if (emergencyStop) {
      currentMovement.active = false;
      movementInProgress = false;
      Serial.println("ERROR:Emergency stop active");
    }
    return;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastMovementTime >= MOVEMENT_INTERVAL) {
    // Move one step
    Servo* servo;
    switch(currentMovement.jointIndex) {
      case 0: servo = &servo1; break;
      case 1: servo = &servo2; break;
      case 2: servo = &servo3; break;
      case 3: servo = &servo4; break;
      case 4: servo = &servo5; break;
      case 5: servo = &servo6; break;
      default: return;
    }

    servo->write(currentMovement.currentAngle);
    jointPositions[currentMovement.jointIndex] = currentMovement.currentAngle;

    // Check if we've reached the target
    if (currentMovement.currentAngle == currentMovement.targetAngle) {
      currentMovement.active = false;
      movementInProgress = false;
      lastMovementTime = currentTime;
      return;
    }

    // Move to next angle
    currentMovement.currentAngle += currentMovement.stepDirection;
    lastMovementTime = currentTime;
  }
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
    if (!movementInProgress) {
      setupHome();
      sendStatus();
    } else {
      Serial.println("ERROR:Movement in progress, cannot go home");
    }
  } else if (command == CMD_STOP) {
    emergencyStop = true;
    currentMovement.active = false; // Stop any ongoing movement
    movementInProgress = false;
    isRecording = false; // Also stop recording if active
    Serial.println("ERROR:Emergency stop activated");
    delay(50); // Brief pause to ensure stop
    emergencyStop = false; // Reset for next commands
    sendStatus();
  } else if (command == CMD_STATUS) {
    sendStatus();
  } else if (command.startsWith(CMD_RECORD_START)) {
    processRecordStartCommand(command);
  } else if (command == CMD_RECORD_STOP) {
    isRecording = false;
    Serial.println("OK:Recording stopped");
  } else if (command.startsWith(CMD_PLAY_SEQUENCE)) {
    if (!movementInProgress) {
      processPlaySequenceCommand(command);
    } else {
      Serial.println("ERROR:Movement in progress, cannot play sequence");
    }
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

  // Start servo movement (non-blocking)
  startServoMovement(jointIndex, targetAngle);

  // Add point to sequence if recording
  if (isRecording && currentRecordingSequence >= 0) {
    addSequencePoint(currentRecordingSequence, MOVEMENT_INTERVAL);
  }

  // Send status immediately (movement will continue in background)
  sendStatus();
}

// Send current status with system state indicators
void sendStatus() {
  Serial.print(RESP_OK);
  for (int i = 0; i < 6; i++) {
    Serial.print("J");
    Serial.print(i + 1);
    Serial.print(":");
    Serial.print(jointPositions[i]);
    if (i < 5) Serial.print(",");
  }

  // Add system state indicators for better monitoring
  Serial.print("|MOVING:");
  Serial.print(currentMovement.active ? "1" : "0");
  Serial.print("|RECORDING:");
  Serial.print(isRecording ? "1" : "0");
  Serial.print("|PLAYBACK:");
  Serial.print(currentPlayback.active ? "1" : "0");

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

// Start sequence playback (non-blocking)
boolean startSequencePlayback(int sequenceIndex) {
  if (sequenceIndex < 0 || sequenceIndex >= MAX_SEQUENCES) return false;
  if (!sequences[sequenceIndex].valid) return false;
  if (sequences[sequenceIndex].length == 0) return false;

  // Check if any movement is in progress
  if (currentMovement.active || currentPlayback.active) {
    return false; // Cannot start playback while moving
  }

  currentPlayback.active = true;
  currentPlayback.sequenceIndex = sequenceIndex;
  currentPlayback.currentStep = 0;
  currentPlayback.lastStepTime = millis();

  return true;
}

// Update sequence playback (call in main loop)
void updateSequencePlayback() {
  if (!currentPlayback.active || emergencyStop) {
    if (emergencyStop) {
      currentPlayback.active = false;
      Serial.println("ERROR:Sequence playback stopped by emergency");
    }
    return;
  }

  // Check if current movement is complete before proceeding to next step
  if (currentMovement.active) return;

  unsigned long currentTime = millis();
  int delayTime = sequences[currentPlayback.sequenceIndex].points[currentPlayback.currentStep].delay_ms;

  if (currentTime - currentPlayback.lastStepTime >= delayTime) {
    // Move to next step
    if (currentPlayback.currentStep >= sequences[currentPlayback.sequenceIndex].length) {
      // Sequence complete
      currentPlayback.active = false;
      Serial.print("OK:Sequence ");
      Serial.print(currentPlayback.sequenceIndex);
      Serial.println(" completed");
      sendStatus();
      return;
    }

    // Execute current step - move all joints simultaneously
    for (int j = 0; j < 6; j++) {
      int targetAngle = sequences[currentPlayback.sequenceIndex].points[currentPlayback.currentStep].joints[j];
      // Use direct servo write for simultaneous movement (bypass movement queue)
      Servo* servo;
      switch(j) {
        case 0: servo = &servo1; break;
        case 1: servo = &servo2; break;
        case 2: servo = &servo3; break;
        case 3: servo = &servo4; break;
        case 4: servo = &servo5; break;
        case 5: servo = &servo6; break;
      }
      servo->write(targetAngle);
      jointPositions[j] = targetAngle;
    }

    currentPlayback.lastStepTime = currentTime;
    currentPlayback.currentStep++;
  }
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

  // Validate numeric input
  if (indexStr.length() == 0) {
    Serial.println("ERROR:Missing sequence index");
    return;
  }
  for (unsigned int i = 0; i < indexStr.length(); i++) {
    if (!isDigit(indexStr.charAt(i))) {
      Serial.println("ERROR:Sequence index must be numeric");
      return;
    }
  }

  int sequenceIndex = indexStr.toInt();

  if (startSequencePlayback(sequenceIndex)) {
    Serial.print("OK:Started playing sequence ");
    Serial.println(sequenceIndex);
  } else {
    Serial.println("ERROR:Failed to start sequence playback (movement in progress or invalid sequence)");
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
