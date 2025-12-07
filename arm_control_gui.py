#!/usr/bin/env python3
"""
6DOF Robot Arm Control GUI
Python Qt6 interface for controlling Arduino-based 6DOF robot arm

Features:
- Real-time joint position control with sliders
- Preset positions (Home, Pick, Place, etc.)
- Emergency stop functionality
- Serial communication with Arduino
- Live position feedback
"""

import sys
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QSlider, QPushButton, QGroupBox, QTextEdit, QComboBox,
    QMessageBox, QGridLayout, QFrame, QFileDialog, QListWidget, QLineEdit, QSpinBox
)
from PyQt6.QtCore import Qt, QTimer, QThread, pyqtSignal
from PyQt6.QtGui import QFont, QPalette, QColor
import time

class SerialWorker(QThread):
    """Worker thread for serial communication"""
    data_received = pyqtSignal(str)

    def __init__(self, port, baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        self.running = False

    def run(self):
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True

            while self.running:
                if self.serial_conn.in_waiting:
                    data = self.serial_conn.readline().decode('utf-8').strip()
                    if data:
                        self.data_received.emit(data)
                time.sleep(0.01)  # Small delay to prevent CPU hogging

        except Exception as e:
            print(f"Serial error: {e}")

    def stop(self):
        self.running = False
        if self.serial_conn:
            self.serial_conn.close()

    def send_command(self, command):
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.write(f"{command}\n".encode())
                self.serial_conn.flush()
            except Exception as e:
                print(f"Send error: {e}")

class ArmControlGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.serial_worker = None
        self.current_positions = [92, 85, 45, 108, 80, 152]  # Default home position
        self.joint_limits = [(0, 180), (30, 150), (0, 180), (0, 180), (0, 180), (90, 180)]

        self.init_ui()

    def init_ui(self):
        """Initialize the user interface"""
        self.setWindowTitle("6DOF Robot Arm Control")
        self.setGeometry(100, 100, 800, 600)

        # Create central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Main layout
        main_layout = QHBoxLayout(central_widget)

        # Left panel - Joint controls
        left_panel = self.create_joint_controls()
        main_layout.addWidget(left_panel)

        # Right panel - Presets and status
        right_panel = self.create_right_panel()
        main_layout.addWidget(right_panel)

        # Status bar
        self.statusBar().showMessage("Ready - Not connected")

        # Timer for status updates
        self.status_timer = QTimer()
        self.status_timer.timeout.connect(self.request_status)
        self.status_timer.start(1000)  # Update every second

        # Planning sets variables
        self.is_recording = False
        self.current_sequence_index = 0

    def create_joint_controls(self):
        """Create joint control sliders"""
        group = QGroupBox("Joint Control")
        layout = QVBoxLayout(group)

        self.joint_sliders = []
        self.joint_labels = []
        self.joint_value_labels = []

        joint_names = ["Base (J1)", "Shoulder (J2)", "Elbow (J3)",
                      "Wrist Rot (J4)", "Wrist Bend (J5)", "Gripper (J6)"]

        for i, name in enumerate(joint_names):
            # Joint label
            joint_layout = QVBoxLayout()

            label = QLabel(name)
            label.setFont(QFont("Arial", 10, QFont.Weight.Bold))
            joint_layout.addWidget(label)

            # Slider and value layout
            slider_layout = QHBoxLayout()

            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setRange(self.joint_limits[i][0], self.joint_limits[i][1])
            slider.setValue(self.current_positions[i])
            slider.setTickPosition(QSlider.TickPosition.TicksBelow)
            slider.setTickInterval(30)
            slider.valueChanged.connect(lambda value, idx=i: self.on_slider_changed(idx, value))

            value_label = QLabel(f"{self.current_positions[i]}°")
            value_label.setFixedWidth(40)
            value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

            slider_layout.addWidget(slider)
            slider_layout.addWidget(value_label)

            joint_layout.addLayout(slider_layout)

            # Add separator
            if i < 5:
                separator = QFrame()
                separator.setFrameShape(QFrame.Shape.HLine)
                separator.setFrameShadow(QFrame.Shadow.Sunken)
                joint_layout.addWidget(separator)

            layout.addLayout(joint_layout)

            self.joint_sliders.append(slider)
            self.joint_labels.append(label)
            self.joint_value_labels.append(value_label)

        return group

    def create_right_panel(self):
        """Create right panel with presets and status"""
        panel = QWidget()
        layout = QVBoxLayout(panel)

        # Connection controls
        conn_group = QGroupBox("Connection")
        conn_layout = QVBoxLayout(conn_group)

        port_layout = QHBoxLayout()
        port_layout.addWidget(QLabel("Serial Port:"))
        self.port_combo = QComboBox()
        self.update_ports()
        port_layout.addWidget(self.port_combo)

        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        port_layout.addWidget(self.connect_btn)

        conn_layout.addLayout(port_layout)
        layout.addWidget(conn_group)

        # Speed control
        speed_group = QGroupBox("Movement Speed")
        speed_layout = QHBoxLayout(speed_group)

        speed_layout.addWidget(QLabel("Speed (ms):"))

        self.speed_spinbox = QSpinBox()
        self.speed_spinbox.setRange(5, 200)
        self.speed_spinbox.setValue(15)
        self.speed_spinbox.setSuffix(" ms")
        self.speed_spinbox.valueChanged.connect(self.on_speed_changed)
        speed_layout.addWidget(self.speed_spinbox)

        self.set_speed_btn = QPushButton("Set Speed")
        self.set_speed_btn.clicked.connect(self.set_movement_speed)
        speed_layout.addWidget(self.set_speed_btn)

        layout.addWidget(speed_group)

        # Preset positions
        preset_group = QGroupBox("Preset Positions")
        preset_layout = QGridLayout(preset_group)

        presets = [
            ("Home", "HOME"),
            ("Fold", "FOLD"),
            ("Pick Ready", "PICK_READY"),
            ("Pick", "PICK"),
            ("Place Ready", "PLACE_READY"),
            ("Place", "PLACE"),
            ("Wave", "WAVE")
        ]

        row, col = 0, 0
        for name, cmd in presets:
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, c=cmd: self.send_preset_command(c))
            preset_layout.addWidget(btn, row, col)
            col += 1
            if col > 1:
                col = 0
                row += 1

        layout.addWidget(preset_group)

        # Planning Sets
        planning_group = QGroupBox("Planning Sets")
        planning_layout = QVBoxLayout(planning_group)

        # Recording controls
        record_layout = QHBoxLayout()
        self.sequence_name_edit = QLineEdit()
        self.sequence_name_edit.setPlaceholderText("Sequence name")
        record_layout.addWidget(self.sequence_name_edit)

        self.record_btn = QPushButton("Start Recording")
        self.record_btn.clicked.connect(self.toggle_recording)
        self.record_btn.setStyleSheet("QPushButton { background-color: orange; color: white; }")
        record_layout.addWidget(self.record_btn)

        planning_layout.addLayout(record_layout)

        # Sequence list and controls
        self.sequence_list = QListWidget()
        self.sequence_list.setMaximumHeight(100)
        planning_layout.addWidget(self.sequence_list)

        sequence_btn_layout = QHBoxLayout()
        self.refresh_sequences_btn = QPushButton("Refresh")
        self.refresh_sequences_btn.clicked.connect(self.refresh_sequence_list)
        sequence_btn_layout.addWidget(self.refresh_sequences_btn)

        self.play_sequence_btn = QPushButton("Play Selected")
        self.play_sequence_btn.clicked.connect(self.play_selected_sequence)
        sequence_btn_layout.addWidget(self.play_sequence_btn)

        self.delete_sequence_btn = QPushButton("Delete Selected")
        self.delete_sequence_btn.clicked.connect(self.delete_selected_sequence)
        sequence_btn_layout.addWidget(self.delete_sequence_btn)

        planning_layout.addLayout(sequence_btn_layout)

        # Save/Load controls
        file_btn_layout = QHBoxLayout()
        self.save_sequence_btn = QPushButton("Save to File")
        self.save_sequence_btn.clicked.connect(self.save_sequence_to_file)
        file_btn_layout.addWidget(self.save_sequence_btn)

        self.load_sequence_btn = QPushButton("Load from File")
        self.load_sequence_btn.clicked.connect(self.load_sequence_from_file)
        file_btn_layout.addWidget(self.load_sequence_btn)

        planning_layout.addLayout(file_btn_layout)

        layout.addWidget(planning_group)

        # Emergency controls
        emergency_group = QGroupBox("Safety")
        emergency_layout = QVBoxLayout(emergency_group)

        self.emergency_btn = QPushButton("EMERGENCY STOP")
        self.emergency_btn.setStyleSheet("QPushButton { background-color: red; color: white; font-weight: bold; }")
        self.emergency_btn.clicked.connect(self.emergency_stop)
        emergency_layout.addWidget(self.emergency_btn)

        self.status_btn = QPushButton("Get Status")
        self.status_btn.clicked.connect(self.request_status)
        emergency_layout.addWidget(self.status_btn)

        layout.addWidget(emergency_group)

        # Status display
        status_group = QGroupBox("Status")
        status_layout = QVBoxLayout(status_group)

        self.status_text = QTextEdit()
        self.status_text.setMaximumHeight(150)
        self.status_text.setReadOnly(True)
        status_layout.addWidget(self.status_text)

        layout.addWidget(status_group)

        return panel

    def update_ports(self):
        """Update available serial ports"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")

    def toggle_connection(self):
        """Connect or disconnect from serial port"""
        if self.serial_worker and self.serial_worker.isRunning():
            self.disconnect_serial()
        else:
            self.connect_serial()

    def connect_serial(self):
        """Connect to serial port"""
        try:
            port_info = self.port_combo.currentText()
            if not port_info:
                QMessageBox.warning(self, "Error", "No serial port selected")
                return

            port = port_info.split(" - ")[0]

            self.serial_worker = SerialWorker(port)
            self.serial_worker.data_received.connect(self.on_data_received)
            self.serial_worker.start()

            self.connect_btn.setText("Disconnect")
            self.statusBar().showMessage(f"Connected to {port}")
            self.add_status_message(f"Connected to {port}")

            # Refresh sequence list on connection
            self.refresh_sequence_list()

        except Exception as e:
            QMessageBox.critical(self, "Connection Error", f"Failed to connect: {str(e)}")

    def disconnect_serial(self):
        """Disconnect from serial port"""
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker.wait()
            self.serial_worker = None

        self.connect_btn.setText("Connect")
        self.statusBar().showMessage("Disconnected")
        self.add_status_message("Disconnected")

    def on_slider_changed(self, joint_index, value):
        """Handle slider value changes"""
        self.joint_value_labels[joint_index].setText(f"{value}°")
        self.send_joint_command(joint_index + 1, value)

    def on_speed_changed(self, value):
        """Handle speed spinbox changes"""
        # Could auto-set speed here, but we'll use the button for explicit setting
        pass

    def set_movement_speed(self):
        """Set the movement speed on Arduino"""
        if self.serial_worker:
            speed = self.speed_spinbox.value()
            command = f"SET_SPEED:{speed}"
            self.serial_worker.send_command(command)
            self.add_status_message(f"Set movement speed to {speed}ms")

    def send_joint_command(self, joint_num, angle):
        """Send joint position command"""
        if self.serial_worker:
            command = f"J{joint_num}:{angle}"
            self.serial_worker.send_command(command)
            self.add_status_message(f"Sent: {command}")

    def send_preset_command(self, preset):
        """Send preset position command"""
        if self.serial_worker:
            if preset == "PICK_READY":
                # Custom pick ready position
                commands = ["J1:90", "J2:100", "J3:80", "J4:90", "J5:90", "J6:152"]
                for cmd in commands:
                    self.serial_worker.send_command(cmd)
                    time.sleep(0.1)  # Small delay between commands
                self.add_status_message("Sent: Pick Ready sequence")
            elif preset == "PICK":
                commands = ["J1:90", "J2:120", "J3:100", "J4:90", "J5:90", "J6:120"]
                for cmd in commands:
                    self.serial_worker.send_command(cmd)
                    time.sleep(0.1)
                self.add_status_message("Sent: Pick sequence")
            elif preset == "PLACE_READY":
                commands = ["J1:45", "J2:80", "J3:60", "J4:135", "J5:90", "J6:152"]
                for cmd in commands:
                    self.serial_worker.send_command(cmd)
                    time.sleep(0.1)
                self.add_status_message("Sent: Place Ready sequence")
            elif preset == "PLACE":
                commands = ["J1:45", "J2:100", "J3:80", "J4:135", "J5:90", "J6:152"]
                for cmd in commands:
                    self.serial_worker.send_command(cmd)
                    time.sleep(0.1)
                self.add_status_message("Sent: Place sequence")
            else:
                # Use Arduino's built-in commands for HOME, FOLD, WAVE
                self.serial_worker.send_command(preset)
                self.add_status_message(f"Sent: {preset}")

    def emergency_stop(self):
        """Send emergency stop command"""
        if self.serial_worker:
            self.serial_worker.send_command("STOP")
            self.add_status_message("EMERGENCY STOP SENT!")

        # Visual feedback
        original_style = self.emergency_btn.styleSheet()
        self.emergency_btn.setStyleSheet("QPushButton { background-color: darkred; color: white; font-weight: bold; }")
        QTimer.singleShot(2000, lambda: self.emergency_btn.setStyleSheet(original_style))

    def request_status(self):
        """Request current status from Arduino"""
        if self.serial_worker:
            self.serial_worker.send_command("STATUS")

    def on_data_received(self, data):
        """Handle incoming serial data"""
        self.add_status_message(f"Received: {data}")

        if data.startswith("OK:"):
            # Parse position data: OK:J1:90,J2:45,J3:0,J4:108,J5:80,J6:152
            try:
                positions_part = data[3:]  # Remove "OK:"
                position_pairs = positions_part.split(",")

                for pair in position_pairs:
                    if ":" in pair:
                        joint_part, angle_part = pair.split(":")
                        joint_num = int(joint_part[1:]) - 1  # J1 -> 0
                        angle = int(angle_part)

                        if 0 <= joint_num < 6:
                            self.current_positions[joint_num] = angle
                            # Update slider without triggering valueChanged
                            self.joint_sliders[joint_num].blockSignals(True)
                            self.joint_sliders[joint_num].setValue(angle)
                            self.joint_sliders[joint_num].blockSignals(False)
                            self.joint_value_labels[joint_num].setText(f"{angle}°")

            except Exception as e:
                self.add_status_message(f"Parse error: {e}")

        elif data.startswith("SEQUENCE:"):
            # Handle sequence list response
            self.sequence_list.clear()
            lines = data.split('\n')
            for line in lines[1:]:  # Skip the SEQUENCE: header
                line = line.strip()
                if line and ':' in line:
                    self.sequence_list.addItem(line)

        elif data.startswith("ERROR:"):
            error_msg = data[6:]
            QMessageBox.warning(self, "Arduino Error", error_msg)
            self.add_status_message(f"ERROR: {error_msg}")

    def add_status_message(self, message):
        """Add message to status display"""
        current_time = time.strftime("%H:%M:%S")
        self.status_text.append(f"[{current_time}] {message}")

        # Limit the number of lines
        cursor = self.status_text.textCursor()
        cursor.movePosition(cursor.MoveOperation.Start)
        for _ in range(50):  # Keep last 50 lines
            cursor.movePosition(cursor.MoveOperation.Down)
            cursor.movePosition(cursor.MoveOperation.EndOfLine, cursor.MoveMode.KeepAnchor)
        cursor.removeSelectedText()

    def toggle_recording(self):
        """Start or stop recording a sequence"""
        if not self.serial_worker:
            QMessageBox.warning(self, "Error", "Not connected to Arduino")
            return

        sequence_name = self.sequence_name_edit.text().strip()
        if not sequence_name:
            QMessageBox.warning(self, "Error", "Please enter a sequence name")
            return

        if not self.is_recording:
            # Start recording
            command = f"RECORD_START:{self.current_sequence_index}:{sequence_name}"
            self.serial_worker.send_command(command)
            self.is_recording = True
            self.record_btn.setText("Stop Recording")
            self.record_btn.setStyleSheet("QPushButton { background-color: red; color: white; }")
            self.add_status_message(f"Started recording sequence: {sequence_name}")
        else:
            # Stop recording
            self.serial_worker.send_command("RECORD_STOP")
            self.is_recording = False
            self.record_btn.setText("Start Recording")
            self.record_btn.setStyleSheet("QPushButton { background-color: orange; color: white; }")
            self.add_status_message("Stopped recording sequence")
            self.refresh_sequence_list()

    def play_selected_sequence(self):
        """Update GUI with current potentiometer positions during teaching"""
        if self.serial_worker and self.is_teaching:
            self.serial_worker.send_command("READ_POSITIONS")

    def play_selected_sequence(self):
        """Play the selected sequence"""
        if not self.serial_worker:
            QMessageBox.warning(self, "Error", "Not connected to Arduino")
            return

        current_item = self.sequence_list.currentItem()
        if not current_item:
            QMessageBox.warning(self, "Error", "Please select a sequence to play")
            return

        # Extract sequence index from item text (format: "index: name")
        sequence_text = current_item.text()
        try:
            sequence_index = int(sequence_text.split(":")[0])
            command = f"PLAY_SEQUENCE:{sequence_index}"
            self.serial_worker.send_command(command)
            self.add_status_message(f"Playing sequence {sequence_index}")
        except ValueError:
            QMessageBox.warning(self, "Error", "Invalid sequence format")

    def delete_selected_sequence(self):
        """Delete the selected sequence"""
        if not self.serial_worker:
            QMessageBox.warning(self, "Error", "Not connected to Arduino")
            return

        current_item = self.sequence_list.currentItem()
        if not current_item:
            QMessageBox.warning(self, "Error", "Please select a sequence to delete")
            return

        # Extract sequence index from item text
        sequence_text = current_item.text()
        try:
            sequence_index = int(sequence_text.split(":")[0])
            command = f"DELETE_SEQUENCE:{sequence_index}"
            self.serial_worker.send_command(command)
            self.add_status_message(f"Deleted sequence {sequence_index}")
            self.refresh_sequence_list()
        except ValueError:
            QMessageBox.warning(self, "Error", "Invalid sequence format")

    def refresh_sequence_list(self):
        """Refresh the sequence list from Arduino"""
        if self.serial_worker:
            self.serial_worker.send_command("LIST_SEQUENCES")

    def save_sequence_to_file(self):
        """Save sequences to a JSON file"""
        file_name, _ = QFileDialog.getSaveFileName(self, "Save Sequences", "", "JSON Files (*.json)")
        if file_name:
            try:
                import json
                sequences_data = {}

                # Get current sequences from Arduino
                # For now, we'll create a placeholder structure
                # In a full implementation, we'd need to retrieve sequence data from Arduino
                sequences_data = {
                    "info": "Sequence data would be retrieved from Arduino",
                    "sequences": []
                }

                with open(file_name, 'w') as f:
                    json.dump(sequences_data, f, indent=2)

                self.add_status_message(f"Sequences saved to {file_name}")
                QMessageBox.information(self, "Success", f"Sequences saved to {file_name}")

            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save sequences: {str(e)}")

    def load_sequence_from_file(self):
        """Load sequences from a JSON file"""
        file_name, _ = QFileDialog.getOpenFileName(self, "Load Sequences", "", "JSON Files (*.json)")
        if file_name:
            try:
                import json
                with open(file_name, 'r') as f:
                    sequences_data = json.load(f)

                self.add_status_message(f"Sequences loaded from {file_name}")
                QMessageBox.information(self, "Success", f"Sequences loaded from {file_name}\n\nNote: Sequences need to be uploaded to Arduino for execution.")

            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to load sequences: {str(e)}")

    def closeEvent(self, event):
        """Handle application close"""
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker.wait()
        event.accept()

def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")  # Modern look

    # Dark theme palette
    palette = QPalette()
    palette.setColor(QPalette.ColorRole.Window, QColor(53, 53, 53))
    palette.setColor(QPalette.ColorRole.WindowText, Qt.GlobalColor.white)
    palette.setColor(QPalette.ColorRole.Base, QColor(25, 25, 25))
    palette.setColor(QPalette.ColorRole.AlternateBase, QColor(53, 53, 53))
    palette.setColor(QPalette.ColorRole.ToolTipBase, Qt.GlobalColor.white)
    palette.setColor(QPalette.ColorRole.ToolTipText, Qt.GlobalColor.white)
    palette.setColor(QPalette.ColorRole.Text, Qt.GlobalColor.white)
    palette.setColor(QPalette.ColorRole.Button, QColor(53, 53, 53))
    palette.setColor(QPalette.ColorRole.ButtonText, Qt.GlobalColor.white)
    palette.setColor(QPalette.ColorRole.BrightText, Qt.GlobalColor.red)
    palette.setColor(QPalette.ColorRole.Link, QColor(42, 130, 218))
    palette.setColor(QPalette.ColorRole.Highlight, QColor(42, 130, 218))
    palette.setColor(QPalette.ColorRole.HighlightedText, Qt.GlobalColor.black)
    app.setPalette(palette)

    window = ArmControlGUI()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
