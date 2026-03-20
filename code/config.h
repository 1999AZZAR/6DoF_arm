/*
 * 6DOF Robot Arm Configuration
 * Header file containing all configuration constants and pin definitions
 * Optimized for Arduino Uno and Mega compatibility
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

// Number of joints
#define NUM_JOINTS 6

// Joint limits (degrees) - adjusted for realistic robot arm constraints
const int JOINT_MIN[NUM_JOINTS] = {0, 30, 0, 0, 0, 90};
const int JOINT_MAX[NUM_JOINTS] = {180, 150, 180, 180, 180, 180};

// Default home position
const int HOME_POSITIONS[NUM_JOINTS] = {92, 85, 45, 108, 80, 152};

// Movement speed defaults
#define DEFAULT_MOVE_SPEED 15
#define MIN_MOVE_SPEED 5
#define MAX_MOVE_SPEED 200

// Serial communication settings
#define SERIAL_BAUD_RATE 115200
#define SERIAL_BUFFER_SIZE 100

// Sequence storage limits (tuned for 2KB RAM on Uno)
#define MAX_SEQUENCE_LENGTH 15
#define MAX_SEQUENCES 2
#define SEQUENCE_NAME_LENGTH 16

// Command definitions
#define CMD_HOME "HOME"
#define CMD_STOP "STOP"
#define CMD_STATUS "STATUS"
#define CMD_MIN "MIN"
#define CMD_MAX "MAX"
#define CMD_WAVE "WAVE"
#define CMD_SET_SPEED "SET_SPEED"
#define CMD_RECORD_START "RECORD_START"
#define CMD_RECORD_STOP "RECORD_STOP"
#define CMD_PLAY_SEQUENCE "PLAY_SEQUENCE"
#define CMD_LIST_SEQUENCES "LIST_SEQUENCES"
#define CMD_DELETE_SEQUENCE "DELETE_SEQUENCE"

// Joint command prefix
#define CMD_JOINT_PREFIX "J"

// Response prefixes
#define RESP_OK "OK:"
#define RESP_ERROR "ERROR:"
#define RESP_SEQUENCE "SEQUENCE:"

#endif // CONFIG_H
