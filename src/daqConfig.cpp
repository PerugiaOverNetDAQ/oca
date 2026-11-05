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
  int wordsRead = 0;
  bool discardLine;
  
  //Get a complete line (until \n)
  for (string line; getline(is, line); ) {
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
          readOption<bool>(conf.listenClient, word);
          break;
        case 1:
          readOption<int>(conf.portClient, word);
          break;
        case 2:
          readOption<int>(conf.clientCmdLen, word);
          break;
        case 3:
          readOption<string>(conf.makaIpAddr, word);
          break;
        case 4:
          readOption<int>(conf.makaPort, word);
          break;
        case 5:
          readOption<int>(conf.makaCmdLen, word);
          break;
        case 6:
          readOption<string>(conf.dataFolder, word);
          break;
        case 7:
          readOption<bool>(conf.calMode, word);
          break;
        case 8:
          readOption<bool>(conf.intTrigEn, word);
          break;
        case 9:
          readOption<bool>(conf.makaSendToFile, word);
          break;
        case 10:
          readOption<bool>(conf.makaSendToOm, word);
          break;
        case 11:
          readOption<uint32_t>(conf.makaOmPreScale, word);
          break;
        default:
          cout << __METHOD_NAME__ << ") Too many columns in config file." << endl;
          exit(1);
      }

      wordsRead++;
    }
    //Discard empty or comment lines
    if (discardLine) continue;
    
    wordsRead = 0;
    linesRead++;
  }

  cout << linesRead << " line(s)." << endl;

  return linesRead;

}