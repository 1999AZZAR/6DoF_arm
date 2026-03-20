/*
 * 6DOF Robot Arm Serial Control
 * Arduino-based 6DOF robot arm with serial communication
 * Compatible with Python Qt6 GUI interface
 *
 * Serial Protocol:
 * Commands:
 *   J1:90 .. J6:152  - Set joint angle (validated against limits)
 *   HOME              - Go to home position
 *   MIN / MAX         - Move all joints to min/max limits
 *   WAVE              - Perform wave gesture
 *   SET_SPEED:15      - Set movement delay per step (5-200 ms)
 *   STOP              - Emergency stop
 *   STATUS            - Request current positions
 *   RECORD_START:idx:name / RECORD_STOP
 *   PLAY_SEQUENCE:idx / LIST_SEQUENCES / DELETE_SEQUENCE:idx
 *
 * Responses:
 *   OK:J1:90,J2:45,J3:0,J4:108,J5:80,J6:152
 *   ERROR:<description>
 *   SEQUENCE:<index>:<name>
 */

#include <Servo.h>
#include "config.h"

// Servo array replaces individual servo variables
Servo servos[NUM_JOINTS];

// Current joint positions
int jointPositions[NUM_JOINTS];

// Serial input buffer (avoids String heap fragmentation)
char inputBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;

// Safety flags
boolean emergencyStop = false;

// Configurable movement speed (ms delay per degree step)
int moveSpeed = DEFAULT_MOVE_SPEED;

// Sequence management
struct SequencePoint {
  int joints[NUM_JOINTS];
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

// ---------- Setup & Loop ----------

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  initializeSequences();

  for (int i = 0; i < NUM_JOINTS; i++) {
    servos[i].attach(SERVO_PINS[i]);
  }

  moveToPositions(HOME_POSITIONS, 200);

  Serial.println(F("6DOF Arm Ready"));
  sendStatus();
}

void loop() {
  readSerial();
}

// ---------- Serial I/O ----------

void readSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        inputBuffer[bufferIndex] = '\0';
        processCommand(String(inputBuffer));
        bufferIndex = 0;
      }
    } else if (bufferIndex < SERIAL_BUFFER_SIZE - 1) {
      inputBuffer[bufferIndex++] = c;
    }
  }
}

void sendStatus() {
  Serial.print(RESP_OK);
  for (int i = 0; i < NUM_JOINTS; i++) {
    Serial.print('J');
    Serial.print(i + 1);
    Serial.print(':');
    Serial.print(jointPositions[i]);
    if (i < NUM_JOINTS - 1) Serial.print(',');
  }
  Serial.println();
}

// ---------- Movement ----------

int clampAngle(int jointIndex, int angle) {
  if (angle < JOINT_MIN[jointIndex]) return JOINT_MIN[jointIndex];
  if (angle > JOINT_MAX[jointIndex]) return JOINT_MAX[jointIndex];
  return angle;
}

void moveServo(int jointIndex, int targetAngle) {
  if (emergencyStop) return;
  if (jointIndex < 0 || jointIndex >= NUM_JOINTS) return;

  targetAngle = clampAngle(jointIndex, targetAngle);

  int current = jointPositions[jointIndex];
  int step = (current < targetAngle) ? 1 : -1;

  for (int a = current; a != targetAngle; a += step) {
    if (emergencyStop) break;
    servos[jointIndex].write(a);
    delay(moveSpeed);
  }
  servos[jointIndex].write(targetAngle);
  jointPositions[jointIndex] = targetAngle;
}

void moveToPositions(const int* positions, int stepDelay) {
  for (int i = 0; i < NUM_JOINTS; i++) {
    int target = clampAngle(i, positions[i]);
    servos[i].write(target);
    jointPositions[i] = target;
    delay(stepDelay);
  }
}

// ---------- Preset Movements ----------

void moveAllToMin() {
  moveToPositions(JOINT_MIN, 200);
}

void moveAllToMax() {
  moveToPositions(JOINT_MAX, 200);
}

void waveGesture() {
  int saved[NUM_JOINTS];
  for (int i = 0; i < NUM_JOINTS; i++) saved[i] = jointPositions[i];

  moveServo(1, 120);
  moveServo(2, 90);
  moveServo(3, 90);
  moveServo(4, 60);

  for (int w = 0; w < 3 && !emergencyStop; w++) {
    moveServo(0, 60);
    delay(300);
    moveServo(0, 120);
    delay(300);
  }

  for (int i = 0; i < NUM_JOINTS && !emergencyStop; i++) {
    moveServo(i, saved[i]);
  }
}

// ---------- Command Processing ----------

void processCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (command.length() == 0) return;

  if (command.startsWith(CMD_JOINT_PREFIX)) {
    processJointCommand(command);
  } else if (command == CMD_HOME) {
    moveToPositions(HOME_POSITIONS, 200);
    sendStatus();
  } else if (command == CMD_STOP) {
    emergencyStop = true;
    isRecording = false;
    Serial.println(F("ERROR:Emergency stop activated"));
    delay(50);
    emergencyStop = false;
    sendStatus();
  } else if (command == CMD_STATUS) {
    sendStatus();
  } else if (command == CMD_MIN) {
    moveAllToMin();
    sendStatus();
  } else if (command == CMD_MAX) {
    moveAllToMax();
    sendStatus();
  } else if (command == CMD_WAVE) {
    waveGesture();
    sendStatus();
  } else if (command.startsWith(CMD_SET_SPEED)) {
    processSetSpeedCommand(command);
  } else if (command.startsWith(CMD_RECORD_START)) {
    processRecordStartCommand(command);
  } else if (command == CMD_RECORD_STOP) {
    isRecording = false;
    Serial.println(F("OK:Recording stopped"));
  } else if (command.startsWith(CMD_PLAY_SEQUENCE)) {
    processPlaySequenceCommand(command);
  } else if (command == CMD_LIST_SEQUENCES) {
    listSequences();
  } else if (command.startsWith(CMD_DELETE_SEQUENCE)) {
    processDeleteSequenceCommand(command);
  } else {
    Serial.print(F("ERROR:Unknown command: "));
    Serial.println(command);
  }
}

void processJointCommand(String command) {
  int colonIndex = command.indexOf(':');
  if (colonIndex < 2) {
    Serial.println(F("ERROR:Invalid format. Use J1:90"));
    return;
  }

  int jointNum = command.substring(1, colonIndex).toInt();
  if (jointNum < 1 || jointNum > NUM_JOINTS) {
    Serial.println(F("ERROR:Joint must be 1-6"));
    return;
  }

  String angleStr = command.substring(colonIndex + 1);
  if (angleStr.length() == 0) {
    Serial.println(F("ERROR:Missing angle"));
    return;
  }
  for (unsigned int i = 0; i < angleStr.length(); i++) {
    if (!isDigit(angleStr.charAt(i))) {
      Serial.println(F("ERROR:Angle must be numeric"));
      return;
    }
  }

  int jointIndex = jointNum - 1;
  int targetAngle = angleStr.toInt();

  if (targetAngle < JOINT_MIN[jointIndex] || targetAngle > JOINT_MAX[jointIndex]) {
    Serial.print(F("ERROR:Joint "));
    Serial.print(jointNum);
    Serial.print(F(" range is "));
    Serial.print(JOINT_MIN[jointIndex]);
    Serial.print('-');
    Serial.println(JOINT_MAX[jointIndex]);
    return;
  }

  moveServo(jointIndex, targetAngle);

  if (isRecording && currentRecordingSequence >= 0) {
    addSequencePoint(currentRecordingSequence, moveSpeed);
  }

  sendStatus();
}

void processSetSpeedCommand(String command) {
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println(F("ERROR:Use SET_SPEED:15"));
    return;
  }

  int newSpeed = command.substring(colonIndex + 1).toInt();
  if (newSpeed < MIN_MOVE_SPEED || newSpeed > MAX_MOVE_SPEED) {
    Serial.print(F("ERROR:Speed must be "));
    Serial.print(MIN_MOVE_SPEED);
    Serial.print('-');
    Serial.print(MAX_MOVE_SPEED);
    Serial.println(F(" ms"));
    return;
  }

  moveSpeed = newSpeed;
  Serial.print(F("OK:Speed set to "));
  Serial.print(moveSpeed);
  Serial.println(F("ms"));
}

// ---------- Sequence Management ----------

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

boolean addSequencePoint(int seqIdx, int delay_ms) {
  if (seqIdx < 0 || seqIdx >= MAX_SEQUENCES) return false;
  if (!sequences[seqIdx].valid) return false;
  if (sequences[seqIdx].length >= MAX_SEQUENCE_LENGTH) return false;

  int pt = sequences[seqIdx].length;
  for (int i = 0; i < NUM_JOINTS; i++) {
    sequences[seqIdx].points[pt].joints[i] = jointPositions[i];
  }
  sequences[seqIdx].points[pt].delay_ms = delay_ms;
  sequences[seqIdx].length++;
  return true;
}

boolean playSequence(int seqIdx) {
  if (seqIdx < 0 || seqIdx >= MAX_SEQUENCES) return false;
  if (!sequences[seqIdx].valid) return false;

  for (int i = 0; i < sequences[seqIdx].length && !emergencyStop; i++) {
    for (int j = 0; j < NUM_JOINTS && !emergencyStop; j++) {
      moveServo(j, sequences[seqIdx].points[i].joints[j]);
    }
    if (sequences[seqIdx].points[i].delay_ms > 0) {
      delay(sequences[seqIdx].points[i].delay_ms);
    }
  }
  return true;
}

void listSequences() {
  Serial.println(RESP_SEQUENCE);
  for (int i = 0; i < MAX_SEQUENCES; i++) {
    if (sequences[i].valid) {
      Serial.print(i);
      Serial.print(':');
      Serial.println(sequences[i].name);
    }
  }
}

void processRecordStartCommand(String command) {
  int c1 = command.indexOf(':');
  int c2 = command.indexOf(':', c1 + 1);
  if (c1 == -1 || c2 == -1) {
    Serial.println(F("ERROR:Use RECORD_START:0:name"));
    return;
  }

  int seqIdx = command.substring(c1 + 1, c2).toInt();
  if (seqIdx < 0 || seqIdx >= MAX_SEQUENCES) {
    Serial.println(F("ERROR:Invalid sequence index"));
    return;
  }

  String name = command.substring(c2 + 1);
  if (createSequence(seqIdx, name)) {
    isRecording = true;
    currentRecordingSequence = seqIdx;
    Serial.print(F("OK:Recording sequence "));
    Serial.println(seqIdx);
  } else {
    Serial.println(F("ERROR:Failed to create sequence"));
  }
}

void processPlaySequenceCommand(String command) {
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println(F("ERROR:Use PLAY_SEQUENCE:0"));
    return;
  }

  int seqIdx = command.substring(colonIndex + 1).toInt();
  if (playSequence(seqIdx)) {
    Serial.print(F("OK:Sequence "));
    Serial.print(seqIdx);
    Serial.println(F(" done"));
    sendStatus();
  } else {
    Serial.println(F("ERROR:Failed to play sequence"));
  }
}

void processDeleteSequenceCommand(String command) {
  int colonIndex = command.indexOf(':');
  if (colonIndex == -1) {
    Serial.println(F("ERROR:Use DELETE_SEQUENCE:0"));
    return;
  }

  int seqIdx = command.substring(colonIndex + 1).toInt();
  if (seqIdx < 0 || seqIdx >= MAX_SEQUENCES) {
    Serial.println(F("ERROR:Invalid sequence index"));
    return;
  }

  sequences[seqIdx].valid = false;
  sequences[seqIdx].length = 0;
  Serial.print(F("OK:Sequence "));
  Serial.print(seqIdx);
  Serial.println(F(" deleted"));
}
