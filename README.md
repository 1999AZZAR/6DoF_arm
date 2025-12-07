# 6DOF Robot Arm Serial Control System

A complete Arduino-based 6DOF robot arm control system with Python Qt6 GUI interface for easy control and monitoring.

**[Try the Interactive Demo](https://wokwi.com/projects/449667336179085313)** - Experience it online first!

## Table of Contents

- [Interactive Demo](#interactive-demo)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Project Structure](#project-structure)
- [Usage](#usage)
  - [Arduino Code](#arduino-code)
  - [Python GUI](#python-gui)
- [Serial Protocol](#serial-protocol)
  - [Commands (Python → Arduino)](#commands-python--arduino)
  - [Responses (Arduino → Python)](#responses-arduino--python)
- [Safety Features](#safety-features)
- [Planning Sets](#planning-sets)
- [Customization](#customization)
  - [Joint Limits](#joint-limits)
  - [Movement Speed](#movement-speed)
  - [Preset Positions](#preset-positions)
- [Troubleshooting](#troubleshooting)
  - [Connection Issues](#connection-issues)
  - [Movement Issues](#movement-issues)
  - [GUI Issues](#gui-issues)
- [Development](#development)
  - [Adding New Features](#adding-new-features)
  - [Code Structure](#code-structure)
- [License](#license)
- [Contributing](#contributing)
- [Support](#support)

## Features

- **Serial Communication**: Simple text-based protocol for reliable communication
- **Real-time Control**: Live joint position control with sliders
- **Safety Features**: Joint limits validation, emergency stop, movement validation
- **Preset Positions**: Pre-programmed positions for common tasks (Home, Pick, Place, Wave)
- **Planning Sets**: Record, save, load, and playback movement sequences
- **Visual Feedback**: Real-time position display and status monitoring
- **Modern GUI**: Clean Qt6 interface with dark theme
- **Optimized Codebase**: Focused on Arduino Uno and Mega compatibility

## Hardware Requirements

- **Arduino Uno** or **Arduino Mega** (recommended)
- 6DOF Robot Arm with servo motors
- USB cable for serial communication
- Servo motors connected to pins 4, 5, 6, 7, 8, 9

## Software Requirements

- Arduino IDE (for uploading code to Arduino)
- Python 3.8+
- PyQt6
- pyserial

## Installation

1. **Arduino Setup**:

   ```bash
   # For Arduino Uno:
   arduino-cli compile --fqbn arduino:avr:uno --build-path ../build_uno .
   arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno .

   # For Arduino Mega:
   arduino-cli compile --fqbn arduino:avr:mega --build-path ../build_mega .
   arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega .
   ```
2. **Python Dependencies**:

   ```bash
   pip install -r requirements.txt
   ```

## Interactive Demo

🚀 **Try it online first!** - No hardware required

Experience the 6DOF Robot Arm in your browser:

- [**Wokwi Online Simulator**](https://wokwi.com/projects/449667336179085313)
- Test all commands in real-time
- See servo movements visually
- Perfect for learning and testing

## Project Structure

```
6DoF_arm/
├── .git/                    # Git repository
├── .gitignore              # Git ignore rules
├── code/                    # Arduino project folder (Uno/Mega compatible)
│   ├── code.ino            # Main Arduino sketch
│   └── config.h            # Configuration header file
├── arm_control_gui.py      # Python Qt6 GUI application
├── requirements.txt        # Python dependencies
├── LICENSE                 # MIT License file
└── README.md              # This documentation
```

## Usage

### Arduino Code

The Arduino code (`code/code.ino` and `code/config.h`) provides:

- Serial communication at 115200 baud
- Joint control with safety limits
- Emergency stop functionality
- Real-time position feedback

**Joint Pin Mapping**:

- Joint 1 (Base): Pin 9
- Joint 2 (Shoulder): Pin 8
- Joint 3 (Elbow): Pin 7
- Joint 4 (Wrist Rotation): Pin 6
- Joint 5 (Wrist Bend): Pin 5
- Joint 6 (Gripper): Pin 4

### Python GUI

Run the GUI application:

```bash
python arm_control_gui.py
```

**GUI Features**:

1. **Connection Panel**:

   - Select serial port from dropdown
   - Connect/Disconnect button
2. **Joint Control**:

   - 6 sliders for individual joint control (J1-J6)
   - Real-time position display
   - Joint limits enforced automatically
3. **Preset Positions**:

   - **Home**: Default safe position
   - **Pick Ready**: Position for picking objects
   - **Pick**: Actual picking position (gripper closes)
   - **Place Ready**: Position for placing objects
   - **Place**: Actual placing position (gripper opens)
   - **Wave**: Fun demonstration sequence
4. **Safety Controls**:

   - **Emergency Stop**: Immediately halts all movement
   - **Get Status**: Requests current position from Arduino
5. **Status Display**:

   - Real-time communication log
   - Error messages and feedback

## Serial Protocol

### Commands (Python → Arduino)

```
J1:90      - Set joint 1 to 90 degrees
J2:45      - Set joint 2 to 45 degrees
HOME       - Move to home position
STOP       - Emergency stop
STATUS     - Request current positions
PICK_SEQUENCE    - Automated pick operation
PLACE_SEQUENCE   - Automated place operation
WAVE_SEQUENCE    - Greeting wave motion
INSPECT_SEQUENCE - Pan left-right inspection
DRAW_CIRCLE      - Draw circular pattern
RESET_SEQUENCE   - Careful return to home
FOLD             - Move to minimum positions (assembly/storage)
```

### Responses (Arduino → Python)

```
OK:J1:90,J2:45,J3:0,J4:108,J5:80,J6:152    - Current positions
ERROR:Joint limit exceeded                       - Error message
ERROR:Invalid command                           - Command error
SEQUENCE:0:MySequence,1:PickAndPlace            - Available sequences

## Prebuilt Command Testing

### Basic Movement Commands

**Individual Joint Control:**
```bash
J1:90      # Move base to 90°
J2:45      # Move shoulder to 45°
J3:60      # Move elbow to 60°
J4:120     # Move wrist rotation to 120°
J5:80      # Move wrist bend to 80°
J6:150     # Move gripper to 150°
STATUS     # Check current positions
```

**Position Commands:**

```bash
HOME       # Move to home position (92,85,45,108,80,152)
STATUS     # Verify home position
STOP       # Emergency stop (if needed)
```

### Joint Limits Testing

**Valid Ranges:**

```bash
J1:0       # Base minimum
J1:180     # Base maximum
J2:30      # Shoulder minimum (mechanical limit)
J2:150     # Shoulder maximum (mechanical limit)
J3:0       # Elbow minimum
J3:180     # Elbow maximum
```

**Error Testing:**

```bash
J1:200     # Should give error (over 180°)
J2:10      # Should give error (under 30°)
J2:160     # Should give error (over 150°)
J7:90      # Should give error (invalid joint)
```

### Sequence Commands

**Recording Sequences:**

```bash
RECORD_START:0:PickSequence   # Start recording sequence 0
J1:45                         # Record base movement
J2:90                         # Record shoulder movement
J3:135                        # Record elbow movement
J4:60                         # Record wrist rotation
RECORD_STOP                   # Stop recording
LIST_SEQUENCES               # Verify sequence saved
```

**Playing Sequences:**

```bash
PLAY_SEQUENCE:0              # Play recorded sequence
STATUS                       # Check final position
```

**Sequence Management:**

```bash
LIST_SEQUENCES               # Show all sequences
DELETE_SEQUENCE:0            # Delete sequence 0
LIST_SEQUENCES               # Verify deletion
```

### Arduino Serial Monitor Setup

1. **Upload code** to Arduino Uno/Mega
2. **Open Serial Monitor** (Tools → Serial Monitor)
3. **Set baud rate** to `115200`
4. **Line ending** to `Newline` or `Both NL & CR`
5. **Send commands** one by one and observe responses

### Complete Test Sequence

```bash
# Initial status
STATUS

# Test individual joints
J1:90
STATUS
J2:45
STATUS
J3:60
STATUS

# Test home command
HOME
STATUS

# Test emergency stop
J1:180
STOP
STATUS

# Test sequence recording
RECORD_START:0:TestMove
J1:30
J2:80
J3:120
RECORD_STOP

# Test sequence playback
LIST_SEQUENCES
PLAY_SEQUENCE:0
STATUS

# Clean up
DELETE_SEQUENCE:0
LIST_SEQUENCES

### Prebuilt Sequence Testing

**Pick and Place Operations:**
```bash
PICK_SEQUENCE    # Automated pick operation
STATUS          # Check final position
PLACE_SEQUENCE   # Automated place operation
STATUS          # Check final position
```

**Demonstration Sequences:**
```bash
WAVE_SEQUENCE    # Greeting wave motion
STATUS          # Check final position
INSPECT_SEQUENCE # Pan inspection motion
STATUS          # Check final position
DRAW_CIRCLE     # Circular drawing pattern
STATUS          # Check final position
```

**Safety Sequences:**
```bash
RESET_SEQUENCE   # Careful return to home
STATUS          # Verify home position
FOLD             # Move to minimum positions (assembly/storage)
STATUS          # Verify folded position
STOP            # Emergency stop (if needed)
STATUS          # Check stopped position
```

### Movement Conflict Testing

**Error Testing:**
```bash
PICK_SEQUENCE
WAVE_SEQUENCE    # Should give error (movement in progress)
STOP            # Emergency stop to clear
STATUS          # Check position
```

### Complete Prebuilt Test Sequence

```bash
# Initial setup
STATUS
HOME

# Test all prebuilt sequences
PICK_SEQUENCE
STATUS
PLACE_SEQUENCE
STATUS
WAVE_SEQUENCE
STATUS
INSPECT_SEQUENCE
STATUS
DRAW_CIRCLE
STATUS
RESET_SEQUENCE
STATUS
FOLD
STATUS

# Test error conditions
PICK_SEQUENCE
WAVE_SEQUENCE    # Should error
STOP            # Clear movement
STATUS          # Final check
```

# Clean up
DELETE_SEQUENCE:0
LIST_SEQUENCES
```

## Benchmark Results

### Arduino Uno (Final Optimized)

- **Flash Usage**: 36% (11784/32256 bytes)
- **RAM Usage**: 97% (1998/2048 bytes, 50 bytes free)
- **Status**: ✅ Excellent performance, reliable system
- **Movement Speed**: 25ms intervals (fast and smooth)
- **Sequences**: 2 user sequences × 15 waypoints + 7 prebuilt sequences

### Arduino Mega (Final Optimized)

- **Flash Usage**: 5% (12944/253952 bytes)
- **RAM Usage**: 25% (2109/8192 bytes, 6083 bytes free)
- **Status**: ✅ Perfect performance, highly recommended
- **Movement Speed**: 25ms intervals (optimal smoothness)
- **Sequences**: 2 user sequences × 15 waypoints + 7 prebuilt sequences

## Safety Features

- **Joint Limits**: Each joint has defined minimum and maximum angles
- **Emergency Stop**: Immediate halt of all movements
- **Validation**: Commands are validated before execution
- **Movement Interruption**: Emergency stop can interrupt ongoing movements

## Planning Sets

The system supports creating and managing movement sequences (Planning Sets) for automated operations:

### Recording Sequences

1. Enter a sequence name in the text field
2. Click "Start Recording" (button turns red)
3. Move joints using sliders - each movement is recorded as a waypoint
4. Click "Stop Recording" when finished

### Managing Sequences

- **Play Selected**: Execute the selected sequence from the list
- **Delete Selected**: Remove the selected sequence
- **Refresh**: Update the sequence list from Arduino
- **Save to File**: Export sequences to JSON file for backup
- **Load from File**: Import sequences from JSON file

### Sequence Protocol

```
RECORD_START:sequence_index:name    - Start recording sequence
RECORD_STOP                        - Stop recording
PLAY_SEQUENCE:sequence_index       - Play recorded sequence
LIST_SEQUENCES                     - Get list of sequences
DELETE_SEQUENCE:sequence_index     - Delete sequence
```

### Arduino Storage

- **Capacity**: Up to 2 sequences in Arduino memory
- **Length**: Each sequence can have up to 50 waypoints
- **Persistence**: Sequences are stored in RAM (lost on power cycle)

## Prebuilt Movement Sequences

The system includes several pre-programmed movement sequences for common robotic operations:

### Pick Sequence
- **Command**: `PICK_SEQUENCE`
- **Operation**: Approaches object, closes gripper, lifts up
- **Use Case**: Automated picking operations

### Place Sequence
- **Command**: `PLACE_SEQUENCE`
- **Operation**: Moves to position, lowers down, opens gripper, lifts up
- **Use Case**: Automated placing operations

### Wave Sequence
- **Command**: `WAVE_SEQUENCE`
- **Operation**: Raises arm and waves left-right 3 times
- **Use Case**: Greeting or demonstration motions

### Inspect Sequence
- **Command**: `INSPECT_SEQUENCE`
- **Operation**: Pans base from left to right and back
- **Use Case**: Area scanning or inspection

### Draw Circle
- **Command**: `DRAW_CIRCLE`
- **Operation**: Moves joints in circular pattern
- **Use Case**: Drawing or tracing circular paths

### Reset Sequence
- **Command**: `RESET_SEQUENCE`
- **Operation**: Slow, careful return to home position
- **Use Case**: Safe reset after operations

### Fold Sequence
- **Command**: `FOLD`
- **Operation**: Move all joints to their minimum positions
- **Use Case**: Assembly preparation or robot storage/turn-down
- **Joint Positions**: All joints move to JOINT_MIN values

### Sequence Characteristics

- **Blocking Operations**: Each sequence runs to completion
- **Safety Integration**: All sequences respect emergency stop
- **Movement Prevention**: Cannot start if another movement is active
- **Status Updates**: Send position updates after completion

## Customization

### Joint Limits

Modify the `JOINT_MIN` and `JOINT_MAX` arrays in the Arduino code to adjust joint ranges:

```cpp
const int JOINT_MIN[6] = {0, 30, 0, 0, 0, 90};
const int JOINT_MAX[6] = {180, 150, 180, 180, 180, 180};
```

### Movement Speed

Adjust `MOVE_SPEED` in the Arduino code to change movement speed:

```cpp
const int MOVE_SPEED = 50;  // Delay in ms between movement steps
```

### Preset Positions

Modify the `send_preset_command` method in the Python code to customize preset positions.

## Troubleshooting

### Connection Issues

1. Check that Arduino Uno/Mega is connected (`arduino-cli board list`)
2. Verify correct port selection (typically `/dev/ttyACM0` or `/dev/ttyUSB0`)
3. Ensure Arduino code is uploaded and running
4. Check serial baud rate (115200)

### Movement Issues

1. Verify servo connections: pins 4,5,6,7,8,9 on both Uno and Mega
2. Check servo power supply (adequate current for 6 servos)
3. Confirm joint limits are appropriate for your arm
4. Test individual joints with manual commands

### GUI Issues

1. Ensure PyQt6 and pyserial are installed
2. Check Python version (3.8+ required)
3. Run GUI with proper display (not headless)

## Development

### Adding New Features

- **New Commands**: Add to `processCommand()` in Arduino code
- **New GUI Elements**: Extend the Qt6 interface in Python
- **Additional Safety**: Enhance validation in Arduino code

### Code Structure

- **Arduino**: Modular functions optimized for Uno/Mega
- **Python**: Object-oriented Qt6 application with threading
- **Communication**: Simple text protocol for reliability
- **Compatibility**: Tested and optimized for Arduino Uno and Mega

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## Support

For issues and questions:

1. Check the troubleshooting section
2. Review the serial protocol documentation
3. Open an issue on GitHub

---

**Note**: Always test safety features before operating with real hardware. Ensure proper power supplies and mechanical constraints to prevent damage to equipment or injury.
