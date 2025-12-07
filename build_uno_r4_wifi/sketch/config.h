#line 1 "/home/azzar/project/microcontrollers/Arduino/SKETCH/6DoF_arm/code/config.h"
/*
 * 6DOF Robot Arm Configuration
 * Header file containing all configuration constants and pin definitions
 */

#ifndef CONFIG_H
#define CONFIG_H

// Servo pin assignments
#define SERVO_PIN_1 9   // Base rotation
#define SERVO_PIN_2 8   // Shoulder
#define SERVO_PIN_3 7   // Elbow
#define SERVO_PIN_4 6   // Wrist rotation
#define SERVO_PIN_5 5   // Wrist bend
#define SERVO_PIN_6 4   // Gripper

// Servo pin array for easy access
const int SERVO_PINS[6] = {
    SERVO_PIN_1, SERVO_PIN_2, SERVO_PIN_3,
    SERVO_PIN_4, SERVO_PIN_5, SERVO_PIN_6
};

// Potentiometer pin assignments (for teaching mode)
// Note: Using only A0-A5 for compatibility with Uno R4
#define POT_PIN_1 A0   // Base rotation potentiometer
#define POT_PIN_2 A1   // Shoulder potentiometer
#define POT_PIN_3 A2   // Elbow potentiometer
#define POT_PIN_4 A3   // Wrist rotation potentiometer
#define POT_PIN_5 A4   // Wrist bend potentiometer
#define POT_PIN_6 A5   // Gripper potentiometer

// Potentiometer pin array for easy access
const int POT_PINS[6] = {
    POT_PIN_1, POT_PIN_2, POT_PIN_3,
    POT_PIN_4, POT_PIN_5, POT_PIN_6
};

// Joint limits (degrees) - adjusted for realistic robot arm constraints
const int JOINT_MIN[6] = {0, 30, 0, 0, 0, 90};
const int JOINT_MAX[6] = {180, 150, 180, 180, 180, 180};

// Default home position
const int HOME_POSITIONS[6] = {92, 85, 45, 108, 80, 152};

// Movement speed (delay in ms between steps)
#define MOVE_SPEED 50

// Serial communication settings
#define SERIAL_BAUD_RATE 115200
#define SERIAL_BUFFER_SIZE 200

// Command definitions
#define CMD_HOME "HOME"
#define CMD_STOP "STOP"
#define CMD_STATUS "STATUS"
#define CMD_RECORD_START "RECORD_START"
#define CMD_RECORD_STOP "RECORD_STOP"
#define CMD_PLAY_SEQUENCE "PLAY_SEQUENCE"
#define CMD_SAVE_SEQUENCE "SAVE_SEQUENCE"
#define CMD_LOAD_SEQUENCE "LOAD_SEQUENCE"
#define CMD_LIST_SEQUENCES "LIST_SEQUENCES"
#define CMD_DELETE_SEQUENCE "DELETE_SEQUENCE"
#define CMD_TEACH_START "TEACH_START"
#define CMD_TEACH_STOP "TEACH_STOP"
#define CMD_READ_POSITIONS "READ_POSITIONS"

// Joint command prefix
#define CMD_JOINT_PREFIX "J"

// Response prefixes
#define RESP_OK "OK:"
#define RESP_ERROR "ERROR:"
#define RESP_SEQUENCE "SEQUENCE:"

// Joint names for display/debugging
extern const char* JOINT_NAMES[6];

#endif // CONFIG_H
