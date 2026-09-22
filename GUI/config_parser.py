import configparser
import os
from pathlib import Path

# Dynamic resolution of project root directory and default configuration paths
ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_OCA_PATH = str(ROOT_DIR / "config" / "oca.cfg")
DEFAULT_PAPERO_PATH = str(ROOT_DIR / "config" / "papero.cfg")


def get_config_parser():
    """
    Initializes and returns a ConfigParser instance with case-sensitive option support.
    """
    parser = configparser.ConfigParser()
    # Prevent ConfigParser from automatically converting keys to lowercase
    parser.optionxform = str
    return parser

def load_oca_config(filepath=DEFAULT_OCA_PATH):
    """
    Reads the OCA configuration file. Returns default values if the file is missing.
    """
    config = get_config_parser()  

    data = {
        # GUI-managed fields
        "oca_ip": "",
        "maka_dir": "",
        "write_file": False,
        "send_om": False,
        "om_prescaler": 0,

        # C++ Backend extra fields
        "listenClient": False,
        "portClient": 0,
        "clientCmdLen": 0,
        "makaPort": 0,
        "makaCmdLen": 0,
        "calMode": False,
        "intTrigEn": False
    }
    
    # Security check
    if not os.path.exists(filepath):
        return data  
        
    try:
        config.read(filepath)
        if "GLOBAL" in config:
            sec = config["GLOBAL"]
            # GUI-managed fields (with fallback handling for backward compatibility)
            data["oca_ip"] = sec.get("oca_ip", sec.get("makaIpAddr", ""))
            data["maka_dir"] = sec.get("maka_dir", sec.get("dataFolder", ""))
            data["write_file"] = sec.getboolean("write_file", sec.getboolean("makaSendToFile", False))
            data["send_om"] = sec.getboolean("send_om", sec.getboolean("makaSendToOm", False))
            data["om_prescaler"] = sec.getint("om_prescaler", sec.getint("makaOmPreScale", 0))
            
            # C++ Backend extra fields
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
    Serializes OCA configuration data to an INI file under the [GLOBAL] section.
    """
    dir_name = os.path.dirname(filepath)
    if dir_name:
        os.makedirs(dir_name, exist_ok=True)

    #Loading pre-existing data to avoid losing manually written C++ fields
    current_data = load_oca_config(filepath)

    #Update only the data collected by the GUI (data)
    current_data.update(data)
        
    config = get_config_parser() 
    
    #current_data instead of data to use the combined values
    config["GLOBAL"] = {
        # GUI-managed fields
        "oca_ip": str(current_data.get("oca_ip", "")),
        "maka_dir": str(current_data.get("maka_dir", "")),
        "write_file": str(current_data.get("write_file", False)),
        "send_om": str(current_data.get("send_om", False)),
        "om_prescaler": str(current_data.get("om_prescaler", 0)),

        # C++ Backend extra fields
        "listenClient": str(current_data.get("listenClient", False)),
        "portClient": str(current_data.get("portClient", 0)),
        "clientCmdLen": str(current_data.get("clientCmdLen", 0)),
        "makaPort": str(current_data.get("makaPort", 0)),
        "makaCmdLen": str(current_data.get("makaCmdLen", 0)),
        "calMode": str(current_data.get("calMode", False)),
        "intTrigEn": str(current_data.get("intTrigEn", False))
    }
    
    with open(filepath, "w") as configfile:
        config.write(configfile)

def load_papero_config(filepath=DEFAULT_PAPERO_PATH):
    """
    Loads configuration settings for all 10 PAPERO detector modules.
    """
    config = get_config_parser() 
    data = []

    # Initialize structure for 10 modules
    for i in range(10):
        data.append({
            # GUI-managed fields
            "enable": False, "ip": "", "send_maka": False, "trigger": 0, "test_mode": False, "test_channel": 0,
            "bias0": 0.0, "bias1": 0.0, 
            
            # C++ Backend extra fields 
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
                # GUI-managed fields
                data[i]["enable"] = sec.getboolean("enable", sec.getboolean("makaEnable", False))
                data[i]["ip"] = sec.get("ip", sec.get("ipAddr", ""))
                data[i]["send_maka"] = sec.getboolean("send_maka", False)
                data[i]["trigger"] = sec.getint("trigger", sec.getint("intTrigPeriod", 0))
                data[i]["test_mode"] = sec.getboolean("test_mode", sec.getboolean("testUnitEn", False))
                data[i]["test_channel"] = sec.getint("test_channel", sec.getint("chTest", 0))
                data[i]["bias0"] = sec.getfloat("bias0", 0.0)
                data[i]["bias1"] = sec.getfloat("bias1", 0.0)
                
                # C++ Backend extra fields
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
    Serializes PAPERO modules configuration to an INI file under [PAPERO_N] sections.
    """
    dir_name = os.path.dirname(filepath)
    if dir_name:
        os.makedirs(dir_name, exist_ok=True)

    #Loading the existing configuration of the 10 PAPERI
    current_list = load_papero_config(filepath)

    #Row-by-row data merge
    for i in range(10):
        if i < len(data_list):
            current_list[i].update(data_list[i])
        
    config = get_config_parser() 
    #current_data instead of data to use the combined values
    for i, current_data in enumerate(current_list):
        section_name = f"PAPERO_{i+1}"
        config[section_name] = {
            # GUI-managed fields
            "enable": str(current_data.get("enable", False)),
            "ip": str(current_data.get("ip", "")),
            "trigger": str(current_data.get("trigger", 0)),
            "test_mode": str(current_data.get("test_mode", False)),
            "test_channel": str(current_data.get("test_channel", 0)),
            "bias0": str(current_data.get("bias0", 0.0)),
            "bias1": str(current_data.get("bias1", 0.0)),
            
            # C++ Backend extra fields
            "id": str(current_data.get("id", i + 1)),
            "tcpPort": str(current_data.get("tcpPort", 0)),
            "cmdLen": str(current_data.get("cmdLen", 0)),
            "testUnitCfg": str(current_data.get("testUnitCfg", 0)),
            "hkEn": str(current_data.get("hkEn", False)),
            "dataEn": str(current_data.get("dataEn", False)),
            "pktLen": str(current_data.get("pktLen", 0)),
            "feClkDiv": str(current_data.get("feClkDiv", 0)),
            "feClkDuty": str(current_data.get("feClkDuty", 0)),
            "adcClkDiv": str(current_data.get("adcClkDiv", 0)),
            "adcClkDuty": str(current_data.get("adcClkDuty", 0)),
            "trig2Hold": str(current_data.get("trig2Hold", 0)),
            "adcFast": str(current_data.get("adcFast", False)),
            "busyLen": str(current_data.get("busyLen", 0)),
            "adcDelay": str(current_data.get("adcDelay", 0)),
            "ideTest": str(current_data.get("ideTest", False))
        }
        
    with open(filepath, "w") as configfile:
        config.write(configfile)