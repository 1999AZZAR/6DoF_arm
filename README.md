# 6DOF Robot Arm Serial Control System

A complete Arduino-based 6DOF robot arm control system with Python Qt6 GUI interface for easy control and monitoring.

## Table of Contents

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

## Hardware Requirements

- Arduino Nano (or compatible)
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
   # Copy the 'code' folder to your Arduino sketches directory
   # Open code/code.ino in Arduino IDE
   # Upload to your Arduino Nano
   ```

2. **Python Dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

## Project Structure

```
6DoF_arm/
├── .git/                    # Git repository
├── .gitignore              # Git ignore rules
├── code/                    # Arduino project folder
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
```

### Responses (Arduino → Python)

```
OK:J1:90,J2:45,J3:0,J4:108,J5:80,J6:152    - Current positions
ERROR:Joint limit exceeded                       - Error message
ERROR:Invalid command                           - Command error
```

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

### Teaching by Demonstration

For more intuitive programming, you can physically move the robot arm to "teach" it sequences:

#### Hardware Setup:
- Connect potentiometers to each joint: `A0, A1, A2, A3, A6, A7`
- Potentiometers should be mechanically linked to joint positions
- Calibration: Ensure pot range (0-1023) maps to joint range (min-max)

#### Teaching Process:
1. Enter sequence name
2. Click "Start Teaching" (button turns red)
3. **Physically move the robot arm by hand**
4. GUI shows live position feedback from potentiometers
5. Arduino automatically records positions as you move
6. Click "Stop Teaching" when finished

#### Teaching Commands:
```
TEACH_START:sequence_index:name    - Start teaching mode
TEACH_STOP                        - Stop teaching mode
READ_POSITIONS                    - Get current pot positions
```

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

- **Capacity**: Up to 5 sequences in Arduino memory
- **Length**: Each sequence can have up to 50 waypoints
- **Persistence**: Sequences are stored in RAM (lost on power cycle)

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

1. Check that Arduino is connected and visible in device manager
2. Verify correct COM port selection
3. Ensure Arduino code is uploaded and running
4. Check serial baud rate (115200)

### Movement Issues

1. Verify servo connections match pin definitions
2. Check servo power supply (adequate current)
3. Confirm joint limits are appropriate for your arm
4. Test individual joints with manual commands

### GUI Issues

1. Ensure all Python dependencies are installed
2. Check Python version (3.8+ required)
3. Run from command line to see error messages

## Development

### Adding New Features

- **New Commands**: Add to `processCommand()` in Arduino code
- **New GUI Elements**: Extend the Qt6 interface in Python
- **Additional Safety**: Enhance validation in Arduino code

### Code Structure

- **Arduino**: Modular functions for each operation
- **Python**: Object-oriented Qt6 application with threading
- **Communication**: Simple text protocol for reliability

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2025 Azzar Budiyanto**

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

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
