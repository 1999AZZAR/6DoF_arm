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

// Joint command prefix
#define CMD_JOINT_PREFIX "J"

// Response prefixes
#define RESP_OK "OK:"
#define RESP_ERROR "ERROR:"

// Joint names for display/debugging
extern const char* JOINT_NAMES[6];

#endif // CONFIG_H
