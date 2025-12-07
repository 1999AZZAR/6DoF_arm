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
    QMessageBox, QGridLayout, QFrame
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

        # Preset positions
        preset_group = QGroupBox("Preset Positions")
        preset_layout = QGridLayout(preset_group)

        presets = [
            ("Home", "HOME"),
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
            elif preset == "WAVE":
                # Fun wave motion
                commands = ["J4:60", "J4:120", "J4:60", "J4:120", "J4:90"]
                for cmd in commands:
                    self.serial_worker.send_command(cmd)
                    time.sleep(0.5)
                self.add_status_message("Sent: Wave sequence")
            else:
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
