import sys  
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QFormLayout, QGridLayout, QGroupBox, QLabel, QLineEdit,
    QCheckBox, QSpinBox, QDoubleSpinBox, QComboBox, QPushButton,
    QMessageBox
)
from PySide6.QtCore import Qt, QProcess
from PySide6.QtGui import QCloseEvent
from pathlib import Path

import config_parser

class HerdDaqWindow(QMainWindow):
    def __init__(self):
        super().__init__()  
        
        self.setWindowTitle("HERD DAQ Control Interface")
        self.resize(1000, 750)  
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # UI Setup
        self.init_global_settings(main_layout)
        self.init_papero_grid(main_layout)
        self.init_execution_panel(main_layout)
        
        # Initialize background daemon handlers and state management
        self.init_processes()
        # Sync UI with values from existing .cfg files on startup
        self.load_configuration_into_ui()

        # Flag for closing/crash management
        self.intentional_stop = False


    # =========================================================================
    # UI INITIALIZATION
    # =========================================================================
    def init_global_settings(self, parent_layout):
        """Builds the global OCA/MAKA configuration panel."""
        group_box = QGroupBox("Global Settings (OCA/MAKA)")
        form_layout = QFormLayout(group_box)
        
        self.oca_ip_input = QLineEdit()
        self.oca_ip_input.setPlaceholderText("Es. 127.0.0.1")
        self.maka_dir_input = QLineEdit()
        self.maka_dir_input.setPlaceholderText("/path/to/")
        self.write_file_cb = QCheckBox("Write to file")
        self.send_om_cb = QCheckBox("Send to OM")
        self.om_prescaler_sb = QDoubleSpinBox()
        self.om_prescaler_sb.setDecimals(0)
        self.om_prescaler_sb.setRange(0, 4294967295)
        
        form_layout.addRow("Indirizzo IPv4 OCA:", self.oca_ip_input)
        form_layout.addRow("Directory Dati MAKA:", self.maka_dir_input)
        form_layout.addRow(self.write_file_cb)
        form_layout.addRow(self.send_om_cb)
        form_layout.addRow("Prescaler OM (uint32):", self.om_prescaler_sb)
        
        parent_layout.addWidget(group_box)
        
    def init_papero_grid(self, parent_layout):
        """Builds the 10-module PAPERO parameter grid."""
        group_box = QGroupBox("PAPERO Grid Configuration")
        grid_layout = QGridLayout(group_box)
        
        headers = [
            "Abilita", "Indirizzo IPv4", "Invia a MAKA", 
            "Periodo Trigger Int.", "Bias 0", 
            "Bias 1", "Test Mode", "Canale Test"
        ]
        
        for col_idx, text in enumerate(headers):
            grid_layout.addWidget(QLabel(f"<b>{text}</b>"), 0, col_idx)
            
        self.papero_rows = []
        
        for i in range(10):
            row_widgets = {}
            row_idx = i + 1
            
            enable_cb = QCheckBox(f"PAPERO {row_idx}")
            ip_input = QLineEdit()
            ip_input.setPlaceholderText("192.168.1.x")
            send_maka_cb = QCheckBox()
            trigger_sb = QDoubleSpinBox()
            trigger_sb.setDecimals(0)
            trigger_sb.setRange(0, 4294967295)
            bias0_sb = QDoubleSpinBox()
            bias0_sb.setRange(-1000.0, 1000.0)  
            bias1_sb = QDoubleSpinBox()
            bias1_sb.setRange(-1000.0, 1000.0)
            test_mode_cb = QCheckBox()
            test_chan_sb = QSpinBox()
            test_chan_sb.setRange(0, 127)
            
            row_widgets["enable"] = enable_cb
            row_widgets["ip"] = ip_input
            row_widgets["send_maka"] = send_maka_cb
            row_widgets["trigger"] = trigger_sb
            row_widgets["bias0"] = bias0_sb
            row_widgets["bias1"] = bias1_sb
            row_widgets["test_mode"] = test_mode_cb
            row_widgets["test_channel"] = test_chan_sb
            
            grid_layout.addWidget(enable_cb, row_idx, 0)
            grid_layout.addWidget(ip_input, row_idx, 1)
            grid_layout.addWidget(send_maka_cb, row_idx, 2, Qt.AlignmentFlag.AlignCenter)
            grid_layout.addWidget(trigger_sb, row_idx, 3)
            grid_layout.addWidget(bias0_sb, row_idx, 4)
            grid_layout.addWidget(bias1_sb, row_idx, 5)
            grid_layout.addWidget(test_mode_cb, row_idx, 6, Qt.AlignmentFlag.AlignCenter)
            grid_layout.addWidget(test_chan_sb, row_idx, 7)

            # Initialize row as disabled; map enable checkbox to toggle function
            self.toggle_row_widgets(row_widgets, False)
            enable_cb.toggled.connect(lambda checked, rw=row_widgets: self.toggle_row_widgets(rw, checked))
            
            self.papero_rows.append(row_widgets)
            
        parent_layout.addWidget(group_box)
        
    def toggle_row_widgets(self, widgets, enabled):
        """Enables or disables input controls for a specific PAPERO row."""
        widgets["ip"].setEnabled(enabled)
        widgets["send_maka"].setEnabled(enabled)
        widgets["trigger"].setEnabled(enabled)
        widgets["bias0"].setEnabled(enabled)
        widgets["bias1"].setEnabled(enabled)
        widgets["test_mode"].setEnabled(enabled)
        widgets["test_channel"].setEnabled(enabled)
        
    def init_execution_panel(self, parent_layout):
        """Builds the run control panel and status badges."""
        group_box = QGroupBox("Pannello di Esecuzione")
        layout = QHBoxLayout(group_box)
        
        layout.addWidget(QLabel("Tipo di Run:"))
        self.run_type_combo = QComboBox()
        self.run_type_combo.addItems(["CAL", "BEAM"])
        layout.addWidget(self.run_type_combo)
        
        layout.addStretch()
        layout.addWidget(QLabel("MAKA:"))
        self.maka_status_lbl = QLabel(" IDLE ")
        self.maka_status_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter) 
        self.maka_status_lbl.setFixedHeight(30)
        self.maka_status_lbl.setMinimumWidth(100)
        self.maka_status_lbl.setStyleSheet("background-color: #7f8c8d; color: white; font-weight: bold; border-radius: 3px;")
        layout.addWidget(self.maka_status_lbl)
        
        layout.addWidget(QLabel("OCA:"))
        self.oca_status_lbl = QLabel(" IDLE ")
        self.oca_status_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.oca_status_lbl.setFixedHeight(30)
        self.oca_status_lbl.setMinimumWidth(100)
        self.oca_status_lbl.setStyleSheet("background-color: #7f8c8d; color: white; font-weight: bold; border-radius: 3px;")
        layout.addWidget(self.oca_status_lbl)
        layout.addStretch()
        
        self.start_btn = QPushButton("START")
        self.start_btn.setMinimumWidth(100)
        self.stop_btn = QPushButton("STOP")
        self.stop_btn.setMinimumWidth(100)
        
        self.start_btn.clicked.connect(self.on_start_clicked)
        
        layout.addWidget(self.start_btn)
        layout.addWidget(self.stop_btn)
        
        parent_layout.addWidget(group_box)


    # ==========================================
    # GUI CONNECTION LOGIC <-> PARSER
    # ==========================================
    def load_configuration_into_ui(self):
        """Populates UI widgets with configuration loaded from disk."""
        oca_data = config_parser.load_oca_config()
        self.oca_ip_input.setText(oca_data["oca_ip"])
        self.maka_dir_input.setText(oca_data["maka_dir"])
        self.write_file_cb.setChecked(oca_data["write_file"])
        self.send_om_cb.setChecked(oca_data["send_om"])
        self.om_prescaler_sb.setValue(oca_data["om_prescaler"])
        
        papero_data_list = config_parser.load_papero_config()
        for i, row in enumerate(self.papero_rows):
            data = papero_data_list[i]
            row["enable"].setChecked(data["enable"])
            row["ip"].setText(data["ip"])
            row["send_maka"].setChecked(data["send_maka"])
            row["trigger"].setValue(data["trigger"])
            row["bias0"].setValue(data["bias0"])
            row["bias1"].setValue(data["bias1"])
            row["test_mode"].setChecked(data["test_mode"])
            row["test_channel"].setValue(data["test_channel"])

    def dump_ui_to_files(self):
        """Extracts current UI state and writes configuration files to disk."""
        print("[INFO] Avvio serializzazione dei parametri su file...")

        # Package OCA UI data into dictionary schema expected by config_parser
        oca_data = {
            "oca_ip": self.oca_ip_input.text(),
            "maka_dir": self.maka_dir_input.text(),
            "write_file": self.write_file_cb.isChecked(),
            "send_om": self.send_om_cb.isChecked(),
            "om_prescaler": int(self.om_prescaler_sb.value())
        }
        config_parser.save_oca_config(oca_data)
        
        papero_data_list = []
        for row in self.papero_rows:
            papero_data_list.append({
                "enable": row["enable"].isChecked(),
                "ip": row["ip"].text(),
                "send_maka": row["send_maka"].isChecked(),
                "trigger": int(row["trigger"].value()),
                "bias0": row["bias0"].value(),
                "bias1": row["bias1"].value(),
                "test_mode": row["test_mode"].isChecked(),
                "test_channel": row["test_channel"].value()
            })
        config_parser.save_papero_config(papero_data_list)
        print("[SUCCESS] Parametri salvati con successo in oca.cfg e papero.cfg!")

    
    # =========================================================================
    # ASYNCHRONOUS PROCESS MANAGEMENT & CRASH HANDLING
    # =========================================================================
    def init_processes(self):
        """Instantiates QProcess objects and configures process signals."""
        self.p_maka = QProcess(self)
        self.p_oca = QProcess(self)

        root_dir = str(config_parser.ROOT_DIR)
        self.p_maka.setWorkingDirectory(root_dir)
        self.p_oca.setWorkingDirectory(root_dir)

        print("[INFO] Istanze QProcess per MAKA e OCA create correttamente.")

        # Startup sequence cascade: MAKA -> OCA
        self.p_maka.started.connect(self.start_oca_daemon)
        self.p_oca.started.connect(self.on_oca_started)

        # UI state indicators
        self.p_maka.stateChanged.connect(lambda state: self.update_status_label(self.maka_status_lbl, state))
        self.p_oca.stateChanged.connect(lambda state: self.update_status_label(self.oca_status_lbl, state))
        
        print("[INFO] Istanze QProcess create e segnali di concatenazione (MAKA->OCA) configurati.")
        self.stop_btn.clicked.connect(self.on_stop_clicked)
        
        # Crash monitoring
        self.p_maka.finished.connect(lambda exit_code, exit_status: self.handle_process_crash("MAKA", exit_status))
        self.p_oca.finished.connect(lambda exit_code, exit_status: self.handle_process_crash("OCA", exit_status))

    def start_daemons_sequence(self):
        """Initiates daemon execution starting with MAKA."""
        print("[INFO] Inizio sequenza di avvio asincrona: Lancio MAKA...")
        
        self.p_maka.start("./exe/MAKA", ["5555", "2"]) 

    def start_oca_daemon(self):
        """Triggers OCA daemon once MAKA is confirmed RUNNING."""
        print("[INFO] MAKA avviato con successo. Lancio di OCA in cascata...")
        
        self.p_oca.start("./exe/OCA", ["-v", "1"])

    def update_status_label(self, label: QLabel, state: QProcess.ProcessState):
        """Updates status badge style and text based on QProcess state."""
        if state == QProcess.ProcessState.NotRunning:
            label.setText(" IDLE / STOPPED ")
            label.setStyleSheet("background-color: #7f8c8d; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")
        elif state == QProcess.ProcessState.Starting:
            label.setText(" STARTING... ")
            label.setStyleSheet("background-color: #f39c12; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")
        elif state == QProcess.ProcessState.Running:
            label.setText(" RUNNING ")
            label.setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")
    

    # =========================================================================
    # RUN CONTROL & INTERLOCKING LOGIC
    # =========================================================================
    def set_ui_interlocked(self, locked: bool):
        """Locks or unlocks user interface controls during active runs."""
        is_enabled = not locked
        
        self.oca_ip_input.setEnabled(is_enabled)
        self.maka_dir_input.setEnabled(is_enabled)
        self.write_file_cb.setEnabled(is_enabled)
        self.send_om_cb.setEnabled(is_enabled)
        self.om_prescaler_sb.setEnabled(is_enabled)
        
        for row in self.papero_rows:
            row["enable"].setEnabled(is_enabled)
            # Re-enable sub-widgets only if the row checkbox is checked
            if not locked and row["enable"].isChecked():
                self.toggle_row_widgets(row, True)
            elif locked:
                self.toggle_row_widgets(row, False)
                
        self.run_type_combo.setEnabled(is_enabled)
        self.start_btn.setEnabled(is_enabled)

    def on_start_clicked(self):
        """Handles START sequence: locking UI, saving configs, launching processes."""
        # Reset crash monitoring state for new execution cycle
        self.intentional_stop = False
        self.set_ui_interlocked(locked=True)
        
        if not self.oca_ip_input.text():
            print("[WARNING] Indirizzo IP OCA non inserito!")
            
        self.dump_ui_to_files()
        self.start_daemons_sequence()
        
    def on_oca_started(self):
        """Executes STARTOCA command once daemons are active."""
        run_type = self.run_type_combo.currentText()
        run_arg = "0" if run_type == "CAL" else "1"
        
        print(f"[INFO] Demoni attivi. Lancio STARTOCA per run di tipo {run_type} {run_arg}")
        self.p_startoca = QProcess(self)
        self.p_startoca.setWorkingDirectory(str(config_parser.ROOT_DIR))
        self.p_startoca.start("./exe/STARTOCA", [run_arg])

    def on_stop_clicked(self):
        """Handles STOP sequence: executing STOPOCA and terminating daemons."""
        print("[INFO] Pulsante STOP premuto. Esecuzione STOPOCA e terminazione demoni...")

        # Flag set to True to prevent triggering handle_process_crash
        self.intentional_stop = True
        
        self.p_stopoca = QProcess(self)
        self.p_stopoca.setWorkingDirectory(str(config_parser.ROOT_DIR))
        self.p_stopoca.start("./exe/STOPOCA")
        
        self.p_oca.terminate()
        self.p_maka.terminate()
        
        self.set_ui_interlocked(locked=False)


    # =========================================================================
    # CRASH HANDLING & CLEAN SHUTDOWN
    # =========================================================================
    def handle_process_crash(self, process_name, exit_status):
        """Intercepts abnormal process termination and safety-locks system."""
        if self.intentional_stop:
            return

        if exit_status == QProcess.ExitStatus.CrashExit:
            print(f"[CRITICAL] Rilevato crash del processo {process_name}!")
                
            QMessageBox.critical(
                self, 
                "Errore di Sistema", 
                f"Il demone {process_name} si è interrotto in modo anomalo!\nIl sistema verrà messo in sicurezza (STOP)."
            )  
            self.on_stop_clicked()

    def closeEvent(self, event: QCloseEvent):
        """Ensures clean process termination upon window closure."""
        print("[INFO] Richiesta di chiusura GUI. Pulizia processi in corso...")

        self.intentional_stop = True
        
        if self.p_maka.state() != QProcess.ProcessState.NotRunning:
            self.p_maka.kill()
        if self.p_oca.state() != QProcess.ProcessState.NotRunning:
            self.p_oca.kill()
            
        print("[SUCCESS] Processi terminati.")
        event.accept() 
    


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = HerdDaqWindow()      
    window.show()              
    sys.exit(app.exec())        