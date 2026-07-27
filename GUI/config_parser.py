import configparser
import os
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_OCA_PATH = str(ROOT_DIR / "config" / "oca.cfg")
DEFAULT_PAPERO_PATH = str(ROOT_DIR / "config" / "papero.cfg")


def get_config_parser():
    parser = configparser.ConfigParser()
    parser.optionxform = str
    return parser


def load_oca_config(filepath=DEFAULT_OCA_PATH):
    """
    Carica la configurazione di OCA.
    Legge il file oca.cfg e restituisce un dizionario con i parametri globali.
    Se il file non esiste, restituisce un dizionario con valori di default sicuri.
    """

    config = get_config_parser()  

    data = {
        "oca_ip": "",
        "maka_dir": "",
        "write_file": False,
        "send_om": False,
        "om_prescaler": 0,
        
        "listenClient": False,
        "portClient": 0,
        "clientCmdLen": 0,
        "makaPort": 0,
        "makaCmdLen": 0,
        "calMode": False,
        "intTrigEn": False
    }
    
    if not os.path.exists(filepath):
        return data  
        
    try:
        config.read(filepath)
        if "GLOBAL" in config:
            sec = config["GLOBAL"]
            data["oca_ip"] = sec.get("oca_ip", sec.get("makaIpAddr", ""))
            data["maka_dir"] = sec.get("maka_dir", sec.get("dataFolder", ""))
            data["write_file"] = sec.getboolean("write_file", sec.getboolean("makaSendToFile", False))
            data["send_om"] = sec.getboolean("send_om", sec.getboolean("makaSendToOm", False))
            data["om_prescaler"] = sec.getint("om_prescaler", sec.getint("makaOmPreScale", 0))
            
            data["listenClient"] = sec.getboolean("listenClient", False)
            data["portClient"] = sec.getint("portClient", 0)
            data["clientCmdLen"] = sec.getint("clientCmdLen", 0)
            data["makaPort"] = sec.getint("makaPort", 0)
            data["makaCmdLen"] = sec.getint("makaCmdLen", 0)
            data["calMode"] = sec.getboolean("calMode", False)
            data["intTrigEn"] = sec.getboolean("intTrigEn", False)
    except Exception as e:
        print(f"[ERROR] Errore nel parsing di {filepath}: {e}")
        
    return data

def save_oca_config(data, filepath=DEFAULT_OCA_PATH):
    """
    Salva il file oca.cfg. Se il main non passa i campi extra, 
    vengono inseriti automaticamente con valori di default.
    """
    dir_name = os.path.dirname(filepath)
    if dir_name:
        os.makedirs(dir_name, exist_ok=True)
        
    config = get_config_parser() 

    config["GLOBAL"] = {
        "oca_ip": str(data.get("oca_ip", "")),
        "maka_dir": str(data.get("maka_dir", "")),
        "write_file": str(data.get("write_file", False)),
        "send_om": str(data.get("send_om", False)),
        "om_prescaler": str(data.get("om_prescaler", 0)),
        
        "listenClient": str(data.get("listenClient", False)),
        "portClient": str(data.get("portClient", 0)),
        "clientCmdLen": str(data.get("clientCmdLen", 0)),
        "makaPort": str(data.get("makaPort", 0)),
        "makaCmdLen": str(data.get("makaCmdLen", 0)),
        "calMode": str(data.get("calMode", False)),
        "intTrigEn": str(data.get("intTrigEn", False))
    }
    
    with open(filepath, "w") as configfile:
        config.write(configfile)

def load_papero_config(filepath=DEFAULT_PAPERO_PATH):
    """
    Carica la configurazione dei 10 moduli PAPERO includendo tutti i 21 campi
    """
    config = get_config_parser() 
    data = []

    for i in range(10):
        data.append({
            "enable": False, "ip": "", "send_maka": False, "trigger": 0, "test_mode": False, "test_channel": 0,
            "bias0": 0.0, "bias1": 0.0, 
            
            "id": i + 1, "tcpPort": 0, "cmdLen": 0, "testUnitCfg": 0,
            "hkEn": False, "dataEn": False, "pktLen": 0, "feClkDiv": 0,
            "feClkDuty": 0, "adcClkDiv": 0, "adcClkDuty": 0, "trig2Hold": 0,
            "adcFast": False, "busyLen": 0, "adcDelay": 0, "ideTest": False
        })
        
    if not os.path.exists(filepath):
        return data
        
    try:
        config.read(filepath)
        for i in range(10):
            section_name = f"PAPERO_{i+1}"
            if section_name in config:
                sec = config[section_name]
                data[i]["enable"] = sec.getboolean("enable", sec.getboolean("makaEnable", False))
                data[i]["ip"] = sec.get("ip", sec.get("ipAddr", ""))
                data[i]["send_maka"] = sec.getboolean("send_maka", False)
                data[i]["trigger"] = sec.getint("trigger", sec.getint("intTrigPeriod", 0))
                data[i]["test_mode"] = sec.getboolean("test_mode", sec.getboolean("testUnitEn", False))
                data[i]["test_channel"] = sec.getint("test_channel", sec.getint("chTest", 0))
                data[i]["bias0"] = sec.getfloat("bias0", 0.0)
                data[i]["bias1"] = sec.getfloat("bias1", 0.0)
                
                data[i]["id"] = sec.getint("id", i + 1)
                data[i]["tcpPort"] = sec.getint("tcpPort", 0)
                data[i]["cmdLen"] = sec.getint("cmdLen", 0)
                data[i]["testUnitCfg"] = sec.getint("testUnitCfg", 0)
                data[i]["hkEn"] = sec.getboolean("hkEn", False)
                data[i]["dataEn"] = sec.getboolean("dataEn", False)
                data[i]["pktLen"] = sec.getint("pktLen", 0)
                data[i]["feClkDiv"] = sec.getint("feClkDiv", 0)
                data[i]["feClkDuty"] = sec.getint("feClkDuty", 0)
                data[i]["adcClkDiv"] = sec.getint("adcClkDiv", 0)
                data[i]["adcClkDuty"] = sec.getint("adcClkDuty", 0)
                data[i]["trig2Hold"] = sec.getint("trig2Hold", 0)
                data[i]["adcFast"] = sec.getboolean("adcFast", False)
                data[i]["busyLen"] = sec.getint("busyLen", 0)
                data[i]["adcDelay"] = sec.getint("adcDelay", 0)
                data[i]["ideTest"] = sec.getboolean("ideTest", False)
    except Exception as e:
        print(f"[ERROR] Errore nel parsing di {filepath}: {e}")
        
    return data

def save_papero_config(data_list, filepath=DEFAULT_PAPERO_PATH):
    """
    Salva il file papero.cfg strutturando le sezioni con tutti i 21 parametri
    richiesti dal C++ (iniettando i default se non presenti nel dizionario della GUI).
    """
    dir_name = os.path.dirname(filepath)
    if dir_name:
        os.makedirs(dir_name, exist_ok=True)
        
    config = get_config_parser() 
    for i, data in enumerate(data_list):
        section_name = f"PAPERO_{i+1}"
        config[section_name] = {
            "enable": str(data.get("enable", False)),
            "ip": str(data.get("ip", "")),
            "trigger": str(data.get("trigger", 0)),
            "test_mode": str(data.get("test_mode", False)),
            "test_channel": str(data.get("test_channel", 0)),
            "bias0": str(data.get("bias0", 0.0)),
            "bias1": str(data.get("bias1", 0.0)),
            
            "id": str(data.get("id", i + 1)),
            "tcpPort": str(data.get("tcpPort", 0)),
            "cmdLen": str(data.get("cmdLen", 0)),
            "testUnitCfg": str(data.get("testUnitCfg", 0)),
            "hkEn": str(data.get("hkEn", False)),
            "dataEn": str(data.get("dataEn", False)),
            "pktLen": str(data.get("pktLen", 0)),
            "feClkDiv": str(data.get("feClkDiv", 0)),
            "feClkDuty": str(data.get("feClkDuty", 0)),
            "adcClkDiv": str(data.get("adcClkDiv", 0)),
            "adcClkDuty": str(data.get("adcClkDuty", 0)),
            "trig2Hold": str(data.get("trig2Hold", 0)),
            "adcFast": str(data.get("adcFast", False)),
            "busyLen": str(data.get("busyLen", 0)),
            "adcDelay": str(data.get("adcDelay", 0)),
            "ideTest": str(data.get("ideTest", False))
        }
        
    with open(filepath, "w") as configfile:
        config.write(configfile)