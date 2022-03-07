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

configParams* paperoConfig::getParams(uint32_t det)
{
  auto it = conf.find(det);
  if (it == conf.cend())
  {
    cout << __METHOD_NAME__ << ") [ERR] Detector " << det <<
                        " not in config list. Returning first element." << endl;
    return conf.cbegin()->second;
  }
  return it->second;
};

void paperoConfig::dump()
{
  cout << "Dumping configuration map..." << endl;
  for (auto it=conf.begin();it != conf.end();it++)
  {
    cout << endl << "Key " << it->first << ": " << endl;
    (it->second)->dump();
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
  int wordsRead = 0;
  bool discardLine;
  
  //Get a complete line (until \n)
  for (string line; getline(is, line); ) {
    configParams* tempBuffer = new configParams;
    stringstream ss(line);

    //Check if empty line
    discardLine = line.length() == 0;
    //Read line, word by word
    for (string word; getline(ss, word, ' '); ) {
      //Ignore comments
      if ((wordsRead == 0) & (word[0] == '#'))
      {
        discardLine = true;
        break;
      }

      //Pick the right word
      switch(wordsRead){
        case 0:
          readOption<uint32_t>(tempBuffer->id, word);
          break;
        case 1:
          readOption<string>(tempBuffer->ipAddr, word);
          break;
        case 2:
          readOption<int>(tempBuffer->tcpPort, word);
          break;
        case 3:
          readOption<uint8_t>(tempBuffer->testUnitCfg, word);
          break;
        case 4:
          readOption<bool>(tempBuffer->testUnitEn, word);
          break;
        case 5:
          readOption<bool>(tempBuffer->hkEn, word);
          break;
        case 6:
          readOption<bool>(tempBuffer->dataEn, word);
          break;
        case 7:
          readOption<bool>(tempBuffer->intTrigEn, word);
          break;
        case 8:
          readOption<uint32_t>(tempBuffer->intTrigPeriod, word);
          break;
        case 9:
          readOption<bool>(tempBuffer->calMode, word);
          break;
        case 10:
          readOption<uint32_t>(tempBuffer->pktLen, word);
          break;
        case 11:
          readOption<uint16_t>(tempBuffer->feClkDuty, word);
          break;
        case 12:
          readOption<uint16_t>(tempBuffer->feClkDiv, word);
          break;
        case 13:
          readOption<uint16_t>(tempBuffer->adcClkDuty, word);
          break;
        case 14:
          readOption<uint16_t>(tempBuffer->adcClkDiv, word);
          break;
        case 15:
          readOption<uint16_t>(tempBuffer->trig2Hold, word);
          break;
        case 16:
          readOption<bool>(tempBuffer->ideTest, word);
          break;
        case 17:
          readOption<bool>(tempBuffer->adcFast, word);
          break;
        case 18:
          readOption<uint16_t>(tempBuffer->busyLen, word);
          break;
        case 19:
          readOption<uint16_t>(tempBuffer->adcDelay, word);
          break;
        default:
          cout << __METHOD_NAME__ << ") Too many columns in config file." << endl;
          exit(1);
      }

      wordsRead++;
    }
    //Discard empty or comment lines
    if (discardLine) continue;

    //Add the temporary buffer to the output map
    conf.insert(pair<uint32_t, configParams*>(tempBuffer->id, tempBuffer));
    
    wordsRead = 0;
    linesRead++;
  }

  cout << linesRead << " lines." << endl;

  return linesRead;

}