import sys  
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QFormLayout, QGridLayout, QGroupBox, QLabel, QLineEdit,
    QCheckBox, QSpinBox, QDoubleSpinBox, QComboBox, QPushButton
)
from PySide6.QtCore import Qt, QProcess
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
        
        self.init_global_settings(main_layout)
        self.init_papero_grid(main_layout)
        self.init_execution_panel(main_layout)

        self.init_processes()
        
        self.load_configuration_into_ui()

    def update_status_label(self, label: QLabel, state: QProcess.ProcessState):

        if state == QProcess.ProcessState.NotRunning:
            label.setText(" IDLE / STOPPED ")
            label.setStyleSheet("background-color: #7f8c8d; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")
        elif state == QProcess.ProcessState.Starting:
            label.setText(" STARTING... ")
            label.setStyleSheet("background-color: #f39c12; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")
        elif state == QProcess.ProcessState.Running:
            label.setText(" RUNNING ")
            label.setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold; border-radius: 3px; padding: 3px 6px;")

    def init_processes(self):
        """
        Istanziazione dei due oggetti QProcess distinti per MAKA e OCA.
        """
        self.p_maka = QProcess(self)
        self.p_oca = QProcess(self)

        root_dir = str(config_parser.ROOT_DIR)
        
        self.p_maka.setWorkingDirectory(root_dir)
        self.p_oca.setWorkingDirectory(root_dir)

        print("[INFO] Istanze QProcess per MAKA e OCA create correttamente.")

        # MAKA -> OCA.
        self.p_maka.started.connect(self.start_oca_daemon)

        self.p_maka.stateChanged.connect(lambda state: self.update_status_label(self.maka_status_lbl, state))
        self.p_oca.stateChanged.connect(lambda state: self.update_status_label(self.oca_status_lbl, state))
        
        print("[INFO] Istanze QProcess create e segnali di concatenazione (MAKA->OCA) configurati.")

    def start_daemons_sequence(self):
        """
        Innesca la sequenza di avvio automatica partendo da MAKA.
        """
        print("[INFO] Inizio sequenza di avvio asincrona: Lancio MAKA...")
        
        self.p_maka.start("./exe/MAKA", ["5555", "2"]) 

    def start_oca_daemon(self):
        """
        Slot asincrono chiamato da Qt non appena MAKA è effettivamente in RUNNING.
        """
        print("[INFO] MAKA avviato con successo. Lancio di OCA in cascata...")
        
        self.p_oca.start("./exe/OCA", ["-v", "1"])


    def init_global_settings(self, parent_layout):
        group_box = QGroupBox("Global Settings (OCA/MAKA)")
        form_layout = QFormLayout(group_box)
        
        self.oca_ip_input = QLineEdit()
        self.oca_ip_input.setPlaceholderText("Es. 192.168.1.10")
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
            
    
            self.toggle_row_widgets(row_widgets, False)
            enable_cb.toggled.connect(lambda checked, rw=row_widgets: self.toggle_row_widgets(rw, checked))
            
            self.papero_rows.append(row_widgets)
            
        parent_layout.addWidget(group_box)
        
    def toggle_row_widgets(self, widgets, enabled):
        """Abilita o disabilita l'interazione con i widget della riga"""
        widgets["ip"].setEnabled(enabled)
        widgets["send_maka"].setEnabled(enabled)
        widgets["trigger"].setEnabled(enabled)
        widgets["bias0"].setEnabled(enabled)
        widgets["bias1"].setEnabled(enabled)
        widgets["test_mode"].setEnabled(enabled)
        widgets["test_channel"].setEnabled(enabled)
        
    def init_execution_panel(self, parent_layout):
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

    def on_start_clicked(self):
        """
        Macro-funzione associata al pulsante START.
        """
        self.dump_ui_to_files()
        
        self.start_daemons_sequence()


    # ==========================================
    # LOGICA DI CONNESSIONE GUI <-> PARSER
    # ==========================================

    def load_configuration_into_ui(self):
        """
        Prende i dizionari restituiti da config_parser e inserisce i valori
        dentro l'interfaccia grafica usando .setText(), .setValue(), ecc.
        """
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
        """
        Legge l'interfaccia grafica e crea dei dizionari passa poi questi dizionari
        a config_parser per scriverli fisicamente sui file .cfg.
        """
        print("[INFO] Avvio serializzazione dei parametri su file...")
        
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


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = HerdDaqWindow()      
    window.show()              
    sys.exit(app.exec())        