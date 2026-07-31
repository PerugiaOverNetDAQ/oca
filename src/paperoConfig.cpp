/*!
  @file paperoConfig.cpp
  @copydoc paperoConfig.h
*/

#include "paperoConfig.h"
#include "paperoProtocol.h"

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

  //Get a complete line (until \n)
  for (string line; getline(is, line); ) {
    configParams* tempBuffer = new configParams{};
    tempBuffer->daqMode = 0;
    tempBuffer->lth = paperoProtocol::kDefaultLowThreshold;
    tempBuffer->hth = paperoProtocol::kDefaultHighThreshold;
    stringstream ss(line);
    int wordsRead = 0;
    bool discardLine = line.find_first_not_of(" \t\r") == string::npos;
    bool commandFieldParseError = false;

    // Read whitespace-separated fields. This also accepts tabs and repeated
    // spaces, unlike getline(..., ' '), which produced empty columns.
    for (string word; ss >> word; ) {
      // Ignore complete comment lines and allow trailing inline comments.
      if (word[0] == '#') {
        discardLine = wordsRead == 0;
        break;
      }

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
          readOption<uint8_t>(tempBuffer->testUnitCfg, word);
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
          readOption<string>(tempBuffer->bias0, word);
          break;
        case 20:
          readOption<string>(tempBuffer->bias1, word);
          break;
        case 21:
          readOption<bool>(tempBuffer->ideTest, word);
          break;
        case 22:
          readOption<uint16_t>(tempBuffer->chTest, word);
          break;
        // Optional trailing PAPERO command fields. HEF has two bias columns,
        // therefore these follow Test Channel at indices 23..25.
        case 23:
          commandFieldParseError |=
              not readOption<uint32_t>(tempBuffer->daqMode, word);
          break;
        case 24:
          commandFieldParseError |=
              not readOption<uint16_t>(tempBuffer->lth, word);
          break;
        case 25:
          commandFieldParseError |=
              not readOption<uint16_t>(tempBuffer->hth, word);
          break;
        default:
          cout << __METHOD_NAME__ << ") Too many columns in config file." << endl;
          exit(1);
      }

      wordsRead++;
    }
    //Discard empty or comment lines
    if (discardLine) {
      delete tempBuffer;
      continue;
    }

    // Keep existing 23-column HEF files valid, but reject partially specified
    // DAQ settings so a missing threshold cannot silently use a default.
    if (wordsRead != 23 and wordsRead != 26) {
      cout << __METHOD_NAME__ << ") Expected 23 legacy columns or 26 "
           << "columns including DAQ mode, LTH and HTH; got "
           << wordsRead << ". Abort." << endl;
      delete tempBuffer;
      exit(1);
    }

    if (commandFieldParseError) {
      cout << __METHOD_NAME__ << ") Invalid DAQ mode or threshold value. Abort."
           << endl;
      delete tempBuffer;
      exit(1);
    }

    if (not (isValidFixedFloat(tempBuffer->bias0) and isValidFixedFloat(tempBuffer->bias1))) {
      cout << __METHOD_NAME__ << ") Bias values should be in the format XX.X. Abort." << endl;
      delete tempBuffer;
      exit(1);
    }

    if (tempBuffer->daqMode > 3) {
      cout << __METHOD_NAME__ << ") DAQ mode must be in the range 0..3. Abort." << endl;
      delete tempBuffer;
      exit(1);
    }

    //Add the temporary buffer to the output map
    conf.push_back(tempBuffer);
    
    linesRead++;
  }

  cout << linesRead << " lines." << endl;

  return linesRead;

}
