import configparser
import os

def load_oca_config(filepath="oca/config/oca.cfg"):
    """
    Legge il file oca.cfg e restituisce un dizionario con i parametri globali.
    Se il file non esiste, restituisce un dizionario con valori di default sicuri.
    """
    config = configparser.ConfigParser()
    
    # Definiamo i valori di default. Se il programma parte per la prima volta
    # e il file non esiste, l'interfaccia non andrà in crash ma userà questi.
    data = {
        "oca_ip": "",
        "maka_dir": "",
        "write_file": False,
        "send_om": False,
        "om_prescaler": 0
    }
    
    # Controllo di sicurezza
    if not os.path.exists(filepath):
        return data  
        
    try:
        # Legge il file. configparser decodifica automaticamente la struttura INI
        config.read(filepath)
        if "GLOBAL" in config:
            sec = config["GLOBAL"]
            # Estraiamo i valori castandoli nel tipo corretto (stringa, booleano, intero)
            data["oca_ip"] = sec.get("oca_ip", "")
            data["maka_dir"] = sec.get("maka_dir", "")
            data["write_file"] = sec.getboolean("write_file", False)
            data["send_om"] = sec.getboolean("send_om", False)
            data["om_prescaler"] = sec.getint("om_prescaler", 0)
    except Exception as e:
        print(f"[ERROR] Errore nel parsing di {filepath}: {e}")
        
    return data

def save_oca_config(data, filepath="oca/config/oca.cfg"):
    """
    Riceve un dizionario 'data' e lo serializza scrivendolo nel file oca.cfg.
    """
    config = configparser.ConfigParser()
    
    # Crea la sezione [GLOBAL] tipica dei file INI
    config["GLOBAL"] = {
        "oca_ip": str(data.get("oca_ip", "")),
        "maka_dir": str(data.get("maka_dir", "")),
        "write_file": str(data.get("write_file", False)),
        "send_om": str(data.get("send_om", False)),
        "om_prescaler": str(data.get("om_prescaler", 0))
    }
    
    # Apre il file in modalità scrittura ("w") e ci riversa i dati
    with open(filepath, "w") as configfile:
        config.write(configfile)

def load_papero_config(filepath="oca/config/papero.cfg"):
    """
    Legge il file papero.cfg e restituisce una lista contenente 10 dizionari 
    (uno per ogni possibile modulo PAPERO).
    """
    config = configparser.ConfigParser()
    data = []
    
    # Inizializza la lista con 10 strutture vuote/di default
    for i in range(10):
        data.append({
            "enable": False, "ip": "", "send_maka": False,
            "trigger": 0, "bias0": 0.0, "bias1": 0.0,
            "test_mode": False, "test_channel": 0
        })
        
    if not os.path.exists(filepath):
        return data
        
    try:
        config.read(filepath)
        # Cerca nel file le sezioni da [PAPERO_1] a [PAPERO_10]
        for i in range(10):
            section_name = f"PAPERO_{i+1}"
            if section_name in config:
                sec = config[section_name]
                # Sovrascrive i default con i valori trovati nel file
                data[i]["enable"] = sec.getboolean("enable", False)
                data[i]["ip"] = sec.get("ip", "")
                data[i]["send_maka"] = sec.getboolean("send_maka", False)
                data[i]["trigger"] = sec.getint("trigger", 0)
                data[i]["bias0"] = sec.getfloat("bias0", 0.0)
                data[i]["bias1"] = sec.getfloat("bias1", 0.0)
                data[i]["test_mode"] = sec.getboolean("test_mode", False)
                data[i]["test_channel"] = sec.getint("test_channel", 0)
    except Exception as e:
        print(f"[ERROR] Errore nel parsing di {filepath}: {e}")
        
    return data

def save_papero_config(data_list, filepath="oca/config/papero.cfg"):
    """
    Riceve una lista di 10 dizionari e li salva nel file papero.cfg creando
    10 sezioni distinte.
    """
    config = configparser.ConfigParser()
    
    for i, data in enumerate(data_list):
        section_name = f"PAPERO_{i+1}"
        config[section_name] = {
            "enable": str(data.get("enable", False)),
            "ip": str(data.get("ip", "")),
            "send_maka": str(data.get("send_maka", False)),
            "trigger": str(data.get("trigger", 0)),
            "bias0": str(data.get("bias0", 0.0)),
            "bias1": str(data.get("bias1", 0.0)),
            "test_mode": str(data.get("test_mode", False)),
            "test_channel": str(data.get("test_channel", 0))
        }
        
    with open(filepath, "w") as configfile:
        config.write(configfile)