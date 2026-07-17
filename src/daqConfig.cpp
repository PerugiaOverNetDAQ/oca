/*!
  @file daqConfig.cpp
  @brief DAQ configuration methods.
*/

#include "utility.h"
#include "daqConfig.h"

void daqConfig::readConfigFromFile(const string& filePath)
{
  ifstream is;
  openInputFile(filePath, is);
  config(is);
  is.close();
}


void daqConfig::openInputFile(const string& filePath, ifstream& inFile)
{
  cout << "From file " << filePath << " read ";
  inFile.open(filePath);
  if (not(inFile)) {
    cout << " could not open file " << filePath << ". Abort." << endl;
    exit(1);
  }
}


int daqConfig::config(istream& is)
{
  int linesRead = 0;
  
  for (string line; getline(is, line); ) {
    linesRead++;

    while (line.length() > 0 && (line[line.length() - 1] == '\r' || line[line.length() - 1] == '\n')) {
      line.erase(line.length() - 1, 1);
    }

    if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') {
      continue;
    }

    size_t pos = line.find('=');
    if (pos == string::npos) {
      continue;
    }

    string key = line.substr(0, pos);
    string value = line.substr(pos + 1);

    while (key.length() > 0 && (key[key.length() - 1] == ' ' || key[key.length() - 1] == '\t')) {
      key.erase(key.length() - 1, 1);
    }
    while (key.length() > 0 && (key[0] == ' ' || key[0] == '\t')) {
      key.erase(0, 1);
    }
    while (value.length() > 0 && (value[value.length() - 1] == ' ' || value[value.length() - 1] == '\t')) {
      value.erase(value.length() - 1, 1);
    }
    while (value.length() > 0 && (value[0] == ' ' || value[0] == '\t')) {
      value.erase(0, 1);
    }

    if (key == "oca_ip" || key == "makaIpAddr")           readOption<string>(conf.makaIpAddr, value);
    else if (key == "maka_dir" || key == "dataFolder")     readOption<string>(conf.dataFolder, value);
    else if (key == "write_file" || key == "makaSendToFile") readOption<bool>(conf.makaSendToFile, value);
    else if (key == "send_om" || key == "makaSendToOm")     readOption<bool>(conf.makaSendToOm, value);
    else if (key == "om_prescaler" || key == "makaOmPreScale") readOption<uint32_t>(conf.makaOmPreScale, value);
    else if (key == "listenClient")  readOption<bool>(conf.listenClient, value);
    else if (key == "portClient")    readOption<int>(conf.portClient, value);
    else if (key == "clientCmdLen")  readOption<int>(conf.clientCmdLen, value);
    else if (key == "makaPort")      readOption<int>(conf.makaPort, value);
    else if (key == "makaCmdLen")    readOption<int>(conf.makaCmdLen, value);
    else if (key == "calMode")       readOption<bool>(conf.calMode, value);
    else if (key == "intTrigEn")     readOption<bool>(conf.intTrigEn, value);
  }

  cout << linesRead << " line(s)." << endl;
  conf.dump();
  return linesRead;
}