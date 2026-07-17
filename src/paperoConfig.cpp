/*!
  @file paperoConfig.cpp
  @copydoc paperoConfig.h
*/

#include "paperoConfig.h"

void paperoConfig::readConfigFromFile(const string& filePath)
{
  ifstream is;
  openInputFile(filePath, is);
  config(is);
  is.close();
}

paperoConfig::configParams* paperoConfig::getParams(int det)
{
  if ((uint32_t)det > conf.size())
  {
    cout << __METHOD_NAME__ << ") [ERR] Detector " << det <<
                        " not in config list. Returning first element." << endl;
    return conf[0];
  }
  return conf[det];
};

paperoConfig::vectorParam paperoConfig::getParams()
{
  return conf;
}

void paperoConfig::dump()
{
  cout << "Dumping configuration map..." << endl;
  for (uint32_t jj = 0; jj < conf.size(); jj++)
  {
    cout << endl << "Detector " << jj << ": " << endl;
    conf[jj]->dump();
  }
}

void paperoConfig::openInputFile(const string& filePath, ifstream& inFile)
{
  cout << "From file " << filePath << " read ";
  inFile.open(filePath);
  if (not(inFile)) {
    cout << " could not open file " << filePath << ". Abort." << endl;
    exit(1);
  }
}

int paperoConfig::config(istream& is)
{
  int linesRead = 0;
  configParams* tempBuffer = nullptr;

  for (string line; getline(is, line); ) {
    linesRead++;

    while (line.length() > 0 && (line[line.length() - 1] == '\r' || line[line.length() - 1] == '\n')) {
      line.erase(line.length() - 1, 1);
    }

    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }

    if (line[0] == '[' && line[line.length() - 1] == ']') {
      tempBuffer = new configParams;
      conf.push_back(tempBuffer);

      size_t underscorePos = line.find('_');
      if (underscorePos != string::npos) {
        string idStr = line.substr(underscorePos + 1, line.length() - underscorePos - 2);
        stringstream ss(idStr);
        ss >> tempBuffer->id;
      }
      continue;
    }

    size_t pos = line.find('=');
    if (pos != string::npos && tempBuffer != nullptr) {
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

      if (key == "enable" || key == "makaEnable")          readOption<bool>(tempBuffer->makaEnable, value);
      else if (key == "ip" || key == "ipAddr")              readOption<string>(tempBuffer->ipAddr, value);
      else if (key == "trigger" || key == "intTrigPeriod")  readOption<uint32_t>(tempBuffer->intTrigPeriod, value);
      else if (key == "test_mode" || key == "testUnitEn")   readOption<bool>(tempBuffer->testUnitEn, value);
      else if (key == "test_channel" || key == "chTest")    readOption<uint16_t>(tempBuffer->chTest, value);
      else if (key == "id")            readOption<uint32_t>(tempBuffer->id, value);
      else if (key == "tcpPort")       readOption<int>(tempBuffer->tcpPort, value);
      else if (key == "cmdLen")        readOption<int>(tempBuffer->cmdLen, value);
      else if (key == "testUnitCfg")   readOption<uint8_t>(tempBuffer->testUnitCfg, value);
      else if (key == "hkEn")          readOption<bool>(tempBuffer->hkEn, value);
      else if (key == "dataEn")        readOption<bool>(tempBuffer->dataEn, value);
      else if (key == "pktLen")        readOption<uint32_t>(tempBuffer->pktLen, value);
      else if (key == "feClkDiv")      readOption<uint16_t>(tempBuffer->feClkDiv, value);
      else if (key == "feClkDuty")     readOption<uint16_t>(tempBuffer->feClkDuty, value);
      else if (key == "adcClkDiv")     readOption<uint16_t>(tempBuffer->adcClkDiv, value);
      else if (key == "adcClkDuty")    readOption<uint16_t>(tempBuffer->adcClkDuty, value);
      else if (key == "trig2Hold")     readOption<uint16_t>(tempBuffer->trig2Hold, value);
      else if (key == "adcFast")       readOption<bool>(tempBuffer->adcFast, value);
      else if (key == "busyLen")       readOption<uint16_t>(tempBuffer->busyLen, value);
      else if (key == "adcDelay")      readOption<uint16_t>(tempBuffer->adcDelay, value);
      else if (key == "ideTest")       readOption<bool>(tempBuffer->ideTest, value);
    }
  }

  cout << linesRead << " lines." << endl;
  return linesRead;
}