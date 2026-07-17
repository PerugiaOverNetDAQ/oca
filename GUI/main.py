import sys  
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QFormLayout, QGridLayout, QGroupBox, QLabel, QLineEdit,
    QCheckBox, QSpinBox, QDoubleSpinBox, QComboBox, QPushButton
)
from PySide6.QtCore import Qt  

# modulo personalizzato per leggere/scrivere i file
import config_parser

class HerdDaqWindow(QMainWindow):
    def __init__(self):
        super().__init__()  
        
        self.setWindowTitle("HERD DAQ Control Interface")
        self.resize(1000, 750)  
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # Costruzione dell'interfaccia grafica
        self.init_global_settings(main_layout)
        self.init_papero_grid(main_layout)
        self.init_execution_panel(main_layout)
        
        # Popolamento dei dati
        self.load_configuration_into_ui()
        
    def init_global_settings(self, parent_layout):
        group_box = QGroupBox("Global Settings (OCA/MAKA)")
        form_layout = QFormLayout(group_box)
        
        # Creazione dei widget (caselle di testo, spunte e numeri)
        self.oca_ip_input = QLineEdit()
        self.oca_ip_input.setPlaceholderText("Es. 192.168.1.10")
        self.maka_dir_input = QLineEdit()
        self.maka_dir_input.setPlaceholderText("/path/to/")
        self.write_file_cb = QCheckBox("Write to file")
        self.send_om_cb = QCheckBox("Send to OM")
        self.om_prescaler_sb = QDoubleSpinBox()
        self.om_prescaler_sb.setDecimals(0)
        self.om_prescaler_sb.setRange(0, 4294967295)
        
        # Inserimento nel layout
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
        
        # Intestazione della tabella
        for col_idx, text in enumerate(headers):
            grid_layout.addWidget(QLabel(f"<b>{text}</b>"), 0, col_idx)
            
        self.papero_rows = []
        
        # Crea le 10 righe della tabella
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
            
            # Salvataggio dei widget nel dizionario di riga
            row_widgets["enable"] = enable_cb
            row_widgets["ip"] = ip_input
            row_widgets["send_maka"] = send_maka_cb
            row_widgets["trigger"] = trigger_sb
            row_widgets["bias0"] = bias0_sb
            row_widgets["bias1"] = bias1_sb
            row_widgets["test_mode"] = test_mode_cb
            row_widgets["test_channel"] = test_chan_sb
            
            # Aggiunta al layout a griglia
            grid_layout.addWidget(enable_cb, row_idx, 0)
            grid_layout.addWidget(ip_input, row_idx, 1)
            grid_layout.addWidget(send_maka_cb, row_idx, 2, Qt.AlignmentFlag.AlignCenter)
            grid_layout.addWidget(trigger_sb, row_idx, 3)
            grid_layout.addWidget(bias0_sb, row_idx, 4)
            grid_layout.addWidget(bias1_sb, row_idx, 5)
            grid_layout.addWidget(test_mode_cb, row_idx, 6, Qt.AlignmentFlag.AlignCenter)
            grid_layout.addWidget(test_chan_sb, row_idx, 7)
            
            # Stato iniziale, la riga è spenta
            self.toggle_row_widgets(row_widgets, False)
            # Accensione/spegnimento riga
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
        
        self.start_btn = QPushButton("START")
        self.start_btn.setMinimumWidth(100)
        self.stop_btn = QPushButton("STOP")
        self.stop_btn.setMinimumWidth(100)
        
        # START salva i dati
        self.start_btn.clicked.connect(self.dump_ui_to_files)
        
        layout.addWidget(self.start_btn)
        layout.addWidget(self.stop_btn)
        
        parent_layout.addWidget(group_box)

    # ==========================================
    # LOGICA DI CONNESSIONE GUI <-> PARSER
    # ==========================================

    def load_configuration_into_ui(self):
        """
        Prende i dizionari restituiti da config_parser e 'spinge' i valori
        dentro l'interfaccia grafica usando .setText(), .setValue(), ecc.
        """
        # Carica OCA / Global
        oca_data = config_parser.load_oca_config()
        self.oca_ip_input.setText(oca_data["oca_ip"])
        self.maka_dir_input.setText(oca_data["maka_dir"])
        self.write_file_cb.setChecked(oca_data["write_file"])
        self.send_om_cb.setChecked(oca_data["send_om"])
        self.om_prescaler_sb.setValue(oca_data["om_prescaler"])
        
        # Carica la griglia dei PAPERI
        papero_data_list = config_parser.load_papero_config()
        for i, row in enumerate(self.papero_rows):
            data = papero_data_list[i]
            # Inserisce i dati letti dal file nella GUI
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
        Legge l'interfaccia grafica e crea dei dizionari. Passa poi questi dizionari
        a config_parser per scriverli fisicamente sui file .cfg.
        """
        print("[INFO] Avvio serializzazione dei parametri su file...")
        
        # Legge OCA / Global
        oca_data = {
            "oca_ip": self.oca_ip_input.text(),
            "maka_dir": self.maka_dir_input.text(),
            "write_file": self.write_file_cb.isChecked(),
            "send_om": self.send_om_cb.isChecked(),
            "om_prescaler": int(self.om_prescaler_sb.value())
        }
        config_parser.save_oca_config(oca_data) # Manda al parser
        
        # Legge la griglia dei PAPERI
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
        config_parser.save_papero_config(papero_data_list) # Manda al parser
        print("[SUCCESS] Parametri salvati con successo in oca.cfg e papero.cfg!")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = HerdDaqWindow()      
    window.show()              
    sys.exit(app.exec())        