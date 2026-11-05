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
  if (det < 0 || (uint32_t)det >= conf.size())
    throw out_of_range("Detector index outside PAPERO configuration");
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
  int wordsRead = 0;
  
  //Get a complete line (until \n)
  for (string line; getline(is, line); ) {
    stringstream ss(line.substr(0, line.find('#')));
    configParams params = {};
    configParams* tempBuffer = &params;
    for (string word; ss >> word; ) {

      //Pick the right word
      switch(wordsRead){
        case 0:
          readOption<uint32_t>(tempBuffer->id, word);
          break;
        case 1:
          readOption<bool>(tempBuffer->makaEnable, word);
          break;
        case 2:
          readOption<string>(tempBuffer->ipAddr, word);
          break;
        case 3:
          readOption<int>(tempBuffer->tcpPort, word);
          break;
        case 4:
          readOption<int>(tempBuffer->cmdLen, word);
          break;
        case 5:
          readOption<uint16_t>(tempBuffer->testUnitCfg, word);
          break;
        case 6:
          readOption<bool>(tempBuffer->testUnitEn, word);
          break;
        case 7:
          readOption<bool>(tempBuffer->hkEn, word);
          break;
        case 8:
          readOption<bool>(tempBuffer->dataEn, word);
          break;
        case 9:
          readOption<uint32_t>(tempBuffer->intTrigPeriod, word);
          break;
        case 10:
          readOption<uint32_t>(tempBuffer->pktLen, word);
          break;
        case 11:
          readOption<uint16_t>(tempBuffer->feClkDiv, word);
          break;
        case 12:
          readOption<uint16_t>(tempBuffer->feClkDuty, word);
          break;
        case 13:
          readOption<uint16_t>(tempBuffer->adcClkDiv, word);
          break;
        case 14:
          readOption<uint16_t>(tempBuffer->adcClkDuty, word);
          break;
        case 15:
          readOption<uint16_t>(tempBuffer->trig2Hold, word);
          break;
        case 16:
          readOption<bool>(tempBuffer->adcFast, word);
          break;
        case 17:
          readOption<uint16_t>(tempBuffer->busyLen, word);
          break;
        case 18:
          readOption<uint16_t>(tempBuffer->adcDelay, word);
          break;
        case 19:
          readOption<uint16_t>(tempBuffer->prgClkDiv, word);
          break;
        case 20:
          readOption<uint16_t>(tempBuffer->prgClkDuty, word);
          break;
        case 21:
          readOption<bool>(tempBuffer->fastorTxDisable, word);
          break;
        case 22:
          readOption<bool>(tempBuffer->debugEn, word);
          break;
        case 23:
          readOption<bool>(tempBuffer->pt1, word);
          break;
        case 24:
          readOption<bool>(tempBuffer->pt2, word);
          break;
        case 25:
          readOption<bool>(tempBuffer->gain, word);
          break;
        case 26:
          readOption<bool>(tempBuffer->polarity, word);
          break;
        case 27:
          readOption<bool>(tempBuffer->serTxDisable, word);
          break;
        case 28:
          readOption<bool>(tempBuffer->globalTestPulse, word);
          break;
        case 29:
          readOption<uint32_t>(tempBuffer->maskA, word);
          break;
        case 30:
          readOption<uint32_t>(tempBuffer->maskB, word);
          break;
        case 31:
          readOption<uint32_t>(tempBuffer->tpEnA, word);
          break;
        case 32:
          readOption<uint32_t>(tempBuffer->tpEnB, word);
          break;
        case 33:
          readOption<uint32_t>(tempBuffer->discA, word);
          break;
        case 34:
          readOption<uint32_t>(tempBuffer->discB, word);
          break;
        default:
          throw runtime_error("Too many columns in PAPERO configuration");
      }

      wordsRead++;
    }
    if (!wordsRead) continue;
    if (wordsRead != 35)
      throw runtime_error("Expected 35 columns in each PAPERO ASTRA row");
    if (params.testUnitCfg > 3 || params.pktLen < 11 || params.pktLen > 4106 ||
        params.id > 0xffff || params.tcpPort < 1 || params.tcpPort > 65534 ||
        params.cmdLen < 24 || params.cmdLen > 255 ||
        !params.feClkDiv || !params.adcClkDiv || !params.prgClkDiv)
      throw runtime_error("Invalid ASTRA detector, packet, clock or Test Unit settings");

    //Add the temporary buffer to the output map
    conf.push_back(new configParams(params));
    
    wordsRead = 0;
    linesRead++;
  }

  if (!linesRead) throw runtime_error("No detectors in PAPERO configuration");
  cout << linesRead << " lines." << endl;

  return linesRead;

}