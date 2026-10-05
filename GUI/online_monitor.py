import sys
import socket
import threading
import numpy as np

from PySide6.QtWidgets import (
    QApplication, QWidget, QMainWindow, QVBoxLayout, QHBoxLayout,
    QFormLayout, QLabel, QLineEdit, QSpinBox, QComboBox, QPushButton,
    QGroupBox, QMessageBox
)
from PySide6.QtCore import Qt, Signal, QObject

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

# Default constants
UDP_IP   = "127.0.0.1" # Hardware IP
UDP_PORT = 8890        # UDP socket listening port
BUF_SIZE = 65535       # Maximum receive buffer size

EVENT_START   = 0xfa4af1ca  # Event start identifier
QUADDER_START = 0xbaba1a9a  # QUADDER data start identifier
QUADDER_END   = 0x0bedface  # QUADDER data end identifier


# =========================================================================
# DECODING AND REORDERING FUNCTIONS
# =========================================================================
def reorder(v):
    """Reorder ADC channels from multiplexer to match physical GPIO connector layout."""
    # Safety check on the length of the input vector
    if len(v) != 1792:
        print(f"ERROR: reorder() expects 1792 elements, got {len(v)}")
        return [0] * 1792
    
    reordered = [0] * 1792
    j = 0
    # Sequential mapping of hardware ADCs
    order = [12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1]

    # Reconstruction of the logical sequence of channels
    for ch in range(128):
        for adc in order:
            write_idx = (adc * 128 + ch) % 1792
            reordered[write_idx] = v[j]
            j += 1

    # Swap channels 0-895 and 896-1791 to match physical GPIO connector layout    
    temp = reordered[0:896]
    reordered[0:896] = reordered[896:1792]
    reordered[896:1792] = temp

    return reordered


def decode_quadder(words):
    """Decode the raw data from the quadder."""
    channels = []
    for w in words:
        ch_low  = (w & 0xFFFF)         
        ch_high = ((w >> 16) & 0xFFFF)  
        channels.append(ch_low)
        channels.append(ch_high)
    return channels


# =========================================================================
# CLASS FOR MANAGING QT SIGNALS
# =========================================================================
class UdpWorkerSignals(QObject):
    """ 
    Qt signal container to send data from the network thread to the main GUI thread. 
    """
    data_received = Signal(np.ndarray, int)
    error_occurred = Signal(str)             


# =========================================================================
# ONLINE MONITOR WINDOW (EVENT VIEWER)
# # =========================================================================
class EventViewer(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.setWindowTitle("HERD Online Monitor")
        self.resize(900, 600)
        
        # Initialization of thread state and signals
        self.udp_running = False
        self.udp_thread = None
        self.udp_stop_event = threading.Event()  # Stop flag for the thread
        self.signals = UdpWorkerSignals()
        
        # Connecting signals to the main window
        self.signals.data_received.connect(self.update_plot)
        self.signals.error_occurred.connect(self.handle_udp_error)
        
        self.udp_event_id = 0
        
        # Main window layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # --- Control bar ---
        control_group = QGroupBox("Controllo Socket UDP e Rivelatori")
        control_layout = QHBoxLayout(control_group)
        
        control_layout.addWidget(QLabel("IP UDP:"))
        self.ip_input = QLineEdit(UDP_IP)
        self.ip_input.setFixedWidth(110)
        control_layout.addWidget(self.ip_input)
        
        control_layout.addWidget(QLabel("Porta UDP:"))
        self.port_sb = QSpinBox()
        self.port_sb.setRange(1, 65535)
        self.port_sb.setValue(UDP_PORT)
        control_layout.addWidget(self.port_sb)
        
        self.udp_btn = QPushButton("Avvia UDP")
        self.udp_btn.setStyleSheet("font-weight: bold; background-color: #2ecc71; color: white;")
        self.udp_btn.clicked.connect(self.toggle_udp)
        control_layout.addWidget(self.udp_btn)
        
        control_layout.addStretch()
        
        # QUADDER Selection (0-9)
        control_layout.addWidget(QLabel("Rivelatore (QUADDER):"))
        self.udp_select_quadder = QComboBox()
        for q in range(10):
            self.udp_select_quadder.addItem(f"QUADDER {q}", q)
        control_layout.addWidget(self.udp_select_quadder)
        
        main_layout.addWidget(control_group)
        
        # --- Plot ---
        self.figure = Figure(figsize=(8, 5), dpi=100)
        self.canvas = FigureCanvas(self.figure)
        self.ax = self.figure.add_subplot(111)
        
        self.ax.set_title("Online Monitor (QUADDER 0)")
        self.ax.set_xlabel("Canale ADC (0 - 1791)")
        self.ax.set_ylabel("Valore ADC")
        self.ax.set_xlim(0, 1792)
        self.ax.set_ylim(0, 4096)
        self.ax.grid(True, linestyle="--", alpha=0.5)
        
        # Initialize flat plot line
        self.line, = self.ax.plot(
            np.zeros(1792),
            color="#2980b9",
            linestyle="None",
            marker="o",
            markersize=2,
        )
        main_layout.addWidget(self.canvas)

    # ----------------- UDP THREAD CONTROL -----------------
    # Handle UDP stream
    def toggle_udp(self):
        """UDP socket startup and shutdown phases."""
        if self.udp_running:
            self.stop_udp()
        else:
            self.start_udp()

    def start_udp(self):
        """Start the UDP reception thread."""
        if self.udp_running:
            return
                
        self.udp_running = True
        self.udp_stop_event.clear()
        
        # Interface control status update
        self.udp_btn.setText("Arresta UDP")
        self.udp_btn.setStyleSheet("font-weight: bold; background-color: #e74c3c; color: white;")
        self.ip_input.setEnabled(False)
        self.port_sb.setEnabled(False)
        
        # Creation and startup of the daemon thread
        self.udp_thread = threading.Thread(target=self.udp_loop, daemon=True)
        self.udp_thread.start()

    def stop_udp(self):
        """Stop the UDP receiving thread."""
        if not self.udp_running:
            return
        
        self.udp_stop_event.set()
        self.udp_running = False
        
        # Interface control status update
        self.udp_btn.setText("Avvia UDP")
        self.udp_btn.setStyleSheet("font-weight: bold; background-color: #2ecc71; color: white;")
        self.ip_input.setEnabled(True)
        self.port_sb.setEnabled(True)
        
        if self.udp_thread:
            self.udp_thread.join(timeout=1)
            self.udp_thread = None

    def handle_udp_error(self, err_msg):
        """Displays an alert window in the event of a socket error and stops reception."""
        QMessageBox.warning(self, "Errore Socket UDP", err_msg)
        self.stop_udp()

    # ----------------- UDP LOOP -----------------
    def udp_loop(self):
        """Continuous loop for reading and interpreting the UDP packet."""
        ip = self.ip_input.text().strip()
        port = self.port_sb.value()
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.bind((ip, port))
            sock.settimeout(0.2)
        except Exception as e:
            self.signals.error_occurred.emit(f"Impossibile effettuare il bind su {ip}:{port}\n{e}")
            return

        in_event = False
        in_quadder = False
        words_read = 0
        num_quadders = 0
        quadder_read = 0
        quadder_words = []

        # Read UDP packets
        while not self.udp_stop_event.is_set():
            try:
                data, _ = sock.recvfrom(BUF_SIZE) # Buffered read to get full event data
            except socket.timeout:
                continue
            except Exception as e:
                if not self.udp_stop_event.is_set():
                    self.signals.error_occurred.emit(f"Errore lettura socket: {e}")
                break

            n = len(data) // 4  # Number of words in the packet
            for i in range(n):
                w = int.from_bytes(data[4 * i:4 * i + 4], "little")
                words_read += 1

                # Search for event start
                if w == EVENT_START:
                    in_event = True
                    in_quadder = False
                    quadder_read = 0 # Reset quadder read counter
                    quadder_words.clear() # Clear quadder words buffer
                    continue

                if not in_event:
                    continue

                # Read number of quadders in the event
                if in_event and words_read == 4:
                    num_quadders = w & 0xFFF

                # Start of the selected QUADDER block
                if w == QUADDER_START:
                    quadder_read += 1
                    
                    selected_quadder = self.udp_select_quadder.currentData()
                    # Check if we are at the correct quadder number based on the dropdown selection
                    if quadder_read == selected_quadder + 1:
                        in_quadder = True
                        quadder_words.clear()
                        continue

                # End of selected QUADDER block
                if w == QUADDER_END and in_quadder and quadder_read == (self.udp_select_quadder.currentData() + 1):
                    if len(quadder_words) >= 10:
                        # Decoding, channel reordering, and conversion to NumPy array
                        channels = reorder(decode_quadder(quadder_words[10:]))
                        ch = np.array(channels[:1792], dtype=np.int32)

                        self.udp_event_id += 1
                        # Sending processed data to the GUI thread via a Qt signal
                        self.signals.data_received.emit(ch, self.udp_event_id)

                    in_event = False
                    in_quadder = False
                    quadder_words.clear()
                    continue

                # Accumulate words within the selected QUADDER
                if in_quadder and quadder_read == (self.udp_select_quadder.currentData() + 1):
                    quadder_words.append(w)

        sock.close()

    # ----------------- GUI GRAPHICAL UPDATE -----------------
    def update_plot(self, ch_data, event_id):
        """Update the chart values ​​on the main thread."""
        self.line.set_ydata(ch_data)
        
        # Dynamic Y-axis scaling based on the maximum value encountered
        max_val = np.max(ch_data) if len(ch_data) > 0 else 100
        self.ax.set_ylim(0, max(100, int(max_val * 1.15)))
        
        selected_q = self.udp_select_quadder.currentText()
        self.ax.set_title(f"Online Monitor - {selected_q} | Evento #{event_id}")
        self.canvas.draw_idle()

    def closeEvent(self, event):
        """Ensures the UDP thread stops when the window is closed."""
        self.stop_udp()
        event.accept()



if __name__ == "__main__":
    app = QApplication(sys.argv)
    viewer = EventViewer()
    viewer.show()
    sys.exit(app.exec())