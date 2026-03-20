#!/usr/bin/env python3
"""
6DOF Robot Arm Control GUI
Python Qt6 interface for controlling Arduino-based 6DOF robot arm
"""

import sys
import json
import time
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QSlider, QPushButton, QGroupBox, QTextEdit, QComboBox,
    QMessageBox, QGridLayout, QFrame, QFileDialog, QListWidget,
    QLineEdit, QSpinBox, QShortcut
)
from PyQt6.QtCore import Qt, QTimer, QThread, pyqtSignal
from PyQt6.QtGui import QFont, QPalette, QColor, QKeySequence, QTextCursor


JOINT_NAMES = [
    "Base (J1)", "Shoulder (J2)", "Elbow (J3)",
    "Wrist Rot (J4)", "Wrist Bend (J5)", "Gripper (J6)"
]
JOINT_LIMITS = [(0, 180), (30, 150), (0, 180), (0, 180), (0, 180), (90, 180)]
HOME_POSITIONS = [92, 85, 45, 108, 80, 152]
NUM_JOINTS = 6
MAX_SEQUENCES = 2
SLIDER_DEBOUNCE_MS = 80
RECONNECT_CHECK_MS = 3000
STATUS_POLL_MS = 1000
MAX_LOG_LINES = 200


class SerialWorker(QThread):
    data_received = pyqtSignal(str)
    connection_lost = pyqtSignal()

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
                try:
                    if self.serial_conn.in_waiting:
                        data = self.serial_conn.readline().decode("utf-8").strip()
                        if data:
                            self.data_received.emit(data)
                    time.sleep(0.01)
                except (serial.SerialException, OSError):
                    self.connection_lost.emit()
                    break
        except Exception as e:
            self.connection_lost.emit()

    def stop(self):
        self.running = False
        try:
            if self.serial_conn and self.serial_conn.is_open:
                self.serial_conn.close()
        except Exception:
            pass

    def send_command(self, command):
        if not self.serial_conn or not self.serial_conn.is_open:
            return False
        try:
            self.serial_conn.write(f"{command}\n".encode())
            self.serial_conn.flush()
            return True
        except (serial.SerialException, OSError):
            self.connection_lost.emit()
            return False

    @property
    def is_connected(self):
        return self.serial_conn is not None and self.serial_conn.is_open and self.running


class ArmControlGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.serial_worker = None
        self.current_positions = list(HOME_POSITIONS)
        self.is_recording = False
        self.current_sequence_index = 0
        self._last_connected_port = None

        self._pending_joint_commands = {}
        self._debounce_timer = QTimer()
        self._debounce_timer.setSingleShot(True)
        self._debounce_timer.timeout.connect(self._flush_joint_commands)

        self._init_ui()
        self._init_shortcuts()
        self._init_timers()

    # ---- UI Construction ----

    def _init_ui(self):
        self.setWindowTitle("6DOF Robot Arm Control")
        self.setGeometry(100, 100, 820, 640)

        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)

        main_layout.addWidget(self._build_joint_panel())
        main_layout.addWidget(self._build_right_panel())

        self.statusBar().showMessage("Ready - Not connected")

    def _init_shortcuts(self):
        QShortcut(QKeySequence("Escape"), self, self.emergency_stop)
        QShortcut(QKeySequence("Ctrl+H"), self, lambda: self._send_preset("HOME"))
        QShortcut(QKeySequence("Ctrl+R"), self, self.toggle_recording)
        QShortcut(QKeySequence("Ctrl+Shift+C"), self, self.toggle_connection)

    def _init_timers(self):
        self._status_timer = QTimer()
        self._status_timer.timeout.connect(self.request_status)
        self._status_timer.start(STATUS_POLL_MS)

        self._reconnect_timer = QTimer()
        self._reconnect_timer.timeout.connect(self._check_reconnect)

    def _build_joint_panel(self):
        group = QGroupBox("Joint Control")
        layout = QVBoxLayout(group)

        self.joint_sliders = []
        self.joint_value_labels = []

        for i, name in enumerate(JOINT_NAMES):
            joint_layout = QVBoxLayout()

            label = QLabel(name)
            label.setFont(QFont("Arial", 10, QFont.Weight.Bold))
            joint_layout.addWidget(label)

            slider_row = QHBoxLayout()

            min_label = QLabel(f"{JOINT_LIMITS[i][0]}")
            min_label.setFixedWidth(28)
            min_label.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
            slider_row.addWidget(min_label)

            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setRange(JOINT_LIMITS[i][0], JOINT_LIMITS[i][1])
            slider.setValue(self.current_positions[i])
            slider.setTickPosition(QSlider.TickPosition.TicksBelow)
            slider.setTickInterval(30)
            slider.valueChanged.connect(lambda val, idx=i: self._on_slider_changed(idx, val))
            slider_row.addWidget(slider)

            value_label = QLabel(f"{self.current_positions[i]}\u00b0")
            value_label.setFixedWidth(42)
            value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            slider_row.addWidget(value_label)

            max_label = QLabel(f"{JOINT_LIMITS[i][1]}")
            max_label.setFixedWidth(28)
            max_label.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            slider_row.addWidget(max_label)

            joint_layout.addLayout(slider_row)

            if i < NUM_JOINTS - 1:
                sep = QFrame()
                sep.setFrameShape(QFrame.Shape.HLine)
                sep.setFrameShadow(QFrame.Shadow.Sunken)
                joint_layout.addWidget(sep)

            layout.addLayout(joint_layout)
            self.joint_sliders.append(slider)
            self.joint_value_labels.append(value_label)

        return group

    def _build_right_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)

        # Connection
        conn_group = QGroupBox("Connection  [Ctrl+Shift+C]")
        conn_layout = QVBoxLayout(conn_group)
        port_row = QHBoxLayout()
        port_row.addWidget(QLabel("Port:"))
        self.port_combo = QComboBox()
        self._refresh_ports()
        port_row.addWidget(self.port_combo)
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        port_row.addWidget(self.connect_btn)
        refresh_btn = QPushButton("Scan")
        refresh_btn.setFixedWidth(50)
        refresh_btn.clicked.connect(self._refresh_ports)
        port_row.addWidget(refresh_btn)
        conn_layout.addLayout(port_row)
        layout.addWidget(conn_group)

        # Speed
        speed_group = QGroupBox("Movement Speed")
        speed_layout = QHBoxLayout(speed_group)
        speed_layout.addWidget(QLabel("Delay:"))
        self.speed_spinbox = QSpinBox()
        self.speed_spinbox.setRange(5, 200)
        self.speed_spinbox.setValue(15)
        self.speed_spinbox.setSuffix(" ms")
        speed_layout.addWidget(self.speed_spinbox)
        set_speed_btn = QPushButton("Set")
        set_speed_btn.clicked.connect(self._set_speed)
        speed_layout.addWidget(set_speed_btn)
        layout.addWidget(speed_group)

        # Presets
        preset_group = QGroupBox("Presets  [Ctrl+H = Home]")
        preset_layout = QGridLayout(preset_group)
        presets = [("Home", "HOME"), ("Min", "MIN"), ("Max", "MAX"), ("Wave", "WAVE")]
        for idx, (name, cmd) in enumerate(presets):
            btn = QPushButton(name)
            btn.clicked.connect(lambda _, c=cmd: self._send_preset(c))
            preset_layout.addWidget(btn, idx // 2, idx % 2)
        layout.addWidget(preset_group)

        # Sequences
        seq_group = QGroupBox("Sequences  [Ctrl+R = Record]")
        seq_layout = QVBoxLayout(seq_group)

        rec_row = QHBoxLayout()
        self.sequence_name_edit = QLineEdit()
        self.sequence_name_edit.setPlaceholderText("Sequence name")
        rec_row.addWidget(self.sequence_name_edit)
        self.record_btn = QPushButton("Start Recording")
        self.record_btn.clicked.connect(self.toggle_recording)
        self.record_btn.setStyleSheet("QPushButton { background-color: #e67e22; color: white; }")
        rec_row.addWidget(self.record_btn)
        seq_layout.addLayout(rec_row)

        self.sequence_list = QListWidget()
        self.sequence_list.setMaximumHeight(100)
        seq_layout.addWidget(self.sequence_list)

        btn_row = QHBoxLayout()
        for text, slot in [
            ("Refresh", self._refresh_sequences),
            ("Play", self._play_sequence),
            ("Delete", self._delete_sequence),
        ]:
            btn = QPushButton(text)
            btn.clicked.connect(slot)
            btn_row.addWidget(btn)
        seq_layout.addLayout(btn_row)

        file_row = QHBoxLayout()
        save_btn = QPushButton("Save to File")
        save_btn.clicked.connect(self._save_sequences_to_file)
        file_row.addWidget(save_btn)
        load_btn = QPushButton("Load from File")
        load_btn.clicked.connect(self._load_sequences_from_file)
        file_row.addWidget(load_btn)
        seq_layout.addLayout(file_row)

        layout.addWidget(seq_group)

        # Safety
        safety_group = QGroupBox("Safety  [Esc = Stop]")
        safety_layout = QVBoxLayout(safety_group)
        self.emergency_btn = QPushButton("EMERGENCY STOP")
        self.emergency_btn.setStyleSheet(
            "QPushButton { background-color: #c0392b; color: white; font-weight: bold; font-size: 13px; padding: 6px; }"
        )
        self.emergency_btn.clicked.connect(self.emergency_stop)
        safety_layout.addWidget(self.emergency_btn)
        status_btn = QPushButton("Get Status")
        status_btn.clicked.connect(self.request_status)
        safety_layout.addWidget(status_btn)
        layout.addWidget(safety_group)

        # Log
        log_group = QGroupBox("Log")
        log_layout = QVBoxLayout(log_group)
        self.status_text = QTextEdit()
        self.status_text.setMaximumHeight(150)
        self.status_text.setReadOnly(True)
        log_layout.addWidget(self.status_text)
        layout.addWidget(log_group)

        return panel

    # ---- Connection ----

    def toggle_connection(self):
        if self.serial_worker and self.serial_worker.is_connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port_text = self.port_combo.currentText()
        if not port_text:
            QMessageBox.warning(self, "Error", "No serial port selected")
            return
        port = port_text.split(" - ")[0]
        try:
            self.serial_worker = SerialWorker(port)
            self.serial_worker.data_received.connect(self._on_data_received)
            self.serial_worker.connection_lost.connect(self._on_connection_lost)
            self.serial_worker.start()
            self._last_connected_port = port
            self.connect_btn.setText("Disconnect")
            self.statusBar().showMessage(f"Connected to {port}")
            self._log(f"Connected to {port}")
            self._reconnect_timer.stop()
            QTimer.singleShot(500, self._refresh_sequences)
        except Exception as e:
            QMessageBox.critical(self, "Connection Error", str(e))

    def _disconnect(self):
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker.wait(2000)
            self.serial_worker = None
        self.connect_btn.setText("Connect")
        self.statusBar().showMessage("Disconnected")
        self._log("Disconnected")

    def _on_connection_lost(self):
        self._log("Connection lost - will attempt reconnect")
        self.serial_worker = None
        self.connect_btn.setText("Connect")
        self.statusBar().showMessage("Connection lost")
        self._reconnect_timer.start(RECONNECT_CHECK_MS)

    def _check_reconnect(self):
        if self._last_connected_port is None:
            return
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if self._last_connected_port in ports:
            self._log(f"Port {self._last_connected_port} reappeared, reconnecting...")
            self._reconnect_timer.stop()
            for i in range(self.port_combo.count()):
                if self.port_combo.itemText(i).startswith(self._last_connected_port):
                    self.port_combo.setCurrentIndex(i)
                    break
            self._connect()

    def _refresh_ports(self):
        self.port_combo.clear()
        for p in serial.tools.list_ports.comports():
            self.port_combo.addItem(f"{p.device} - {p.description}")

    # ---- Send Helper ----

    def _send(self, command, silent=False):
        if not self.serial_worker or not self.serial_worker.is_connected:
            if not silent:
                self.statusBar().showMessage("Not connected")
            return False
        return self.serial_worker.send_command(command)

    # ---- Slider Debouncing ----

    def _on_slider_changed(self, joint_index, value):
        self.joint_value_labels[joint_index].setText(f"{value}\u00b0")
        self._pending_joint_commands[joint_index] = value
        self._debounce_timer.start(SLIDER_DEBOUNCE_MS)

    def _flush_joint_commands(self):
        if not self.serial_worker or not self.serial_worker.is_connected:
            self._pending_joint_commands.clear()
            return
        for idx, val in self._pending_joint_commands.items():
            self._send(f"J{idx + 1}:{val}", silent=True)
        self._pending_joint_commands.clear()

    # ---- Commands ----

    def _send_preset(self, cmd):
        if self._send(cmd):
            self._log(f"Sent: {cmd}")

    def _set_speed(self):
        speed = self.speed_spinbox.value()
        if self._send(f"SET_SPEED:{speed}"):
            self._log(f"Speed set to {speed}ms")

    def emergency_stop(self):
        if self._send("STOP"):
            self._log("EMERGENCY STOP")
        orig = self.emergency_btn.styleSheet()
        self.emergency_btn.setStyleSheet(
            "QPushButton { background-color: #7b241c; color: white; font-weight: bold; font-size: 13px; padding: 6px; }"
        )
        QTimer.singleShot(1500, lambda: self.emergency_btn.setStyleSheet(orig))

    def request_status(self):
        self._send("STATUS", silent=True)

    # ---- Data Handling ----

    def _on_data_received(self, data):
        self._log(f"RX: {data}")

        if data.startswith("OK:"):
            self._parse_positions(data[3:])
        elif data.startswith("SEQUENCE:"):
            pass
        elif data.startswith("ERROR:"):
            self.statusBar().showMessage(f"Error: {data[6:]}")
        elif self._is_sequence_entry(data):
            self.sequence_list.addItem(data)

    @staticmethod
    def _is_sequence_entry(data):
        parts = data.split(":", 1)
        return len(parts) == 2 and parts[0].isdigit() and len(parts[1]) > 0

    def _parse_positions(self, payload):
        try:
            for pair in payload.split(","):
                if ":" not in pair:
                    continue
                joint_part, angle_str = pair.split(":")
                idx = int(joint_part[1:]) - 1
                angle = int(angle_str)
                if 0 <= idx < NUM_JOINTS:
                    self.current_positions[idx] = angle
                    self.joint_sliders[idx].blockSignals(True)
                    self.joint_sliders[idx].setValue(angle)
                    self.joint_sliders[idx].blockSignals(False)
                    self.joint_value_labels[idx].setText(f"{angle}\u00b0")
        except (ValueError, IndexError):
            pass

    # ---- Sequences ----

    def toggle_recording(self):
        if not self.serial_worker or not self.serial_worker.is_connected:
            QMessageBox.warning(self, "Error", "Not connected")
            return

        if not self.is_recording:
            name = self.sequence_name_edit.text().strip()
            if not name:
                QMessageBox.warning(self, "Error", "Enter a sequence name")
                return
            idx = self._next_sequence_slot()
            if idx is None:
                QMessageBox.warning(
                    self, "Error",
                    f"All {MAX_SEQUENCES} sequence slots are full. Delete one first."
                )
                return
            if self._send(f"RECORD_START:{idx}:{name}"):
                self.is_recording = True
                self.current_sequence_index = idx
                self.record_btn.setText("Stop Recording")
                self.record_btn.setStyleSheet("QPushButton { background-color: #c0392b; color: white; }")
                self._log(f"Recording to slot {idx}: {name}")
        else:
            if self._send("RECORD_STOP"):
                self.is_recording = False
                self.record_btn.setText("Start Recording")
                self.record_btn.setStyleSheet("QPushButton { background-color: #e67e22; color: white; }")
                self._log("Recording stopped")
                self._refresh_sequences()

    def _next_sequence_slot(self):
        used = set()
        for i in range(self.sequence_list.count()):
            text = self.sequence_list.item(i).text()
            try:
                used.add(int(text.split(":")[0]))
            except ValueError:
                pass
        for slot in range(MAX_SEQUENCES):
            if slot not in used:
                return slot
        return None

    def _play_sequence(self):
        idx = self._selected_sequence_index()
        if idx is not None and self._send(f"PLAY_SEQUENCE:{idx}"):
            self._log(f"Playing sequence {idx}")

    def _delete_sequence(self):
        idx = self._selected_sequence_index()
        if idx is not None and self._send(f"DELETE_SEQUENCE:{idx}"):
            self._log(f"Deleted sequence {idx}")
            self._refresh_sequences()

    def _refresh_sequences(self):
        if self._send("LIST_SEQUENCES", silent=True):
            self.sequence_list.clear()

    def _selected_sequence_index(self):
        item = self.sequence_list.currentItem()
        if not item:
            QMessageBox.warning(self, "Error", "Select a sequence first")
            return None
        try:
            return int(item.text().split(":")[0])
        except ValueError:
            QMessageBox.warning(self, "Error", "Invalid sequence format")
            return None

    def _save_sequences_to_file(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Sequences", "", "JSON (*.json)")
        if not path:
            return
        data = {
            "positions": {
                f"J{i+1}": self.current_positions[i] for i in range(NUM_JOINTS)
            },
            "speed_ms": self.speed_spinbox.value(),
            "sequences": [],
        }
        for i in range(self.sequence_list.count()):
            data["sequences"].append(self.sequence_list.item(i).text())
        try:
            with open(path, "w") as f:
                json.dump(data, f, indent=2)
            self._log(f"Saved to {path}")
        except OSError as e:
            QMessageBox.critical(self, "Error", str(e))

    def _load_sequences_from_file(self):
        path, _ = QFileDialog.getOpenFileName(self, "Load Sequences", "", "JSON (*.json)")
        if not path:
            return
        try:
            with open(path, "r") as f:
                data = json.load(f)

            if "speed_ms" in data:
                self.speed_spinbox.setValue(data["speed_ms"])

            all_sent = True
            if "positions" in data:
                for key, angle in data["positions"].items():
                    if not self._send(f"{key}:{angle}"):
                        all_sent = False
                        break
                    time.sleep(0.05)

            if all_sent:
                self._log(f"Loaded from {path}")
            else:
                self._log(f"Partial load from {path} (connection lost)")
        except (OSError, json.JSONDecodeError, KeyError) as e:
            QMessageBox.critical(self, "Error", str(e))

    # ---- Logging ----

    def _log(self, message):
        ts = time.strftime("%H:%M:%S")
        self.status_text.append(f"[{ts}] {message}")
        doc = self.status_text.document()
        while doc.blockCount() > MAX_LOG_LINES:
            cursor = QTextCursor(doc.begin())
            cursor.select(QTextCursor.SelectionType.BlockUnderCursor)
            cursor.movePosition(QTextCursor.MoveOperation.NextBlock, QTextCursor.MoveMode.KeepAnchor)
            cursor.removeSelectedText()

    # ---- Cleanup ----

    def closeEvent(self, event):
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker.wait(2000)
        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

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
