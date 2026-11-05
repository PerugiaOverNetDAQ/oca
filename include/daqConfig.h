/*!
  @file daqConfig.h
  @brief Read DAQ configuration parameters from a .cfg file.
 */

#ifndef _daqConfig_H
#define _daqConfig_H

#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>
#include <fstream>
#include <sstream>

using namespace std;

/*!
  Read configuration parameters from file and populate a struct
*/
class daqConfig
{
  
  public:
    daqConfig(const string& filePath){
      readConfigFromFile(filePath);
    };

    //!Definition of configuration parameters struct
    struct configParams {
      bool listenClient;      //!Listen to external TCP client
      int portClient;         //!TCP server port number
      string dataFolder; //!Data folder
      int clientCmdLen;       //!Command length w.r.t. TCP client
      bool intTrigEn;         //!Internal trigger enable
      bool calMode;           //!Calibration mode enable
      string makaIpAddr;      //!MAKA IP address
      int makaPort;           //!TCP port to connect to MAKA
      int makaCmdLen;         //!Command length w.r.t. MAKA server
      bool makaSendToFile;    //!MAKA write data to disk
      bool makaSendToOm;      //!MAKA send events to On-line Monitor
      uint32_t makaOmPreScale;//!MAKA pre-scale factor to OM

      void dump(){
        cout << "Listen to TCP client:      " << listenClient << endl;
        cout << "TCP client port:           " << portClient << endl;
        cout << "TCP client command length: " << clientCmdLen << endl;
        cout << "Path to data folder:       " << dataFolder << endl;
        cout << "Internal Trigger enable:   " << intTrigEn << endl;
        cout << "Calibration Mode:          " << calMode << endl;
        cout << "MAKA IP Address:           " << makaIpAddr << endl;
        cout << "MAKA Port:                 " << makaPort << endl;
        cout << "MAKA command length:       " << makaCmdLen << endl;
        cout << "MAKA Write to disk:        " << makaSendToFile << endl;
        cout << "MAKA Send to OM:           " << makaSendToOm << endl;
        cout << "MAKA OM PreScale:          " << makaOmPreScale << endl;
      }
    };

    //! Open configuration file and read it in an istream
    void readConfigFromFile(const string& filePath);

    //!
    configParams getParams(){return conf;};

  private:
    //! Configuration parameters
    configParams conf;

    //! Open an input file and get the streamer object
    void openInputFile(const string& filePath,
                              ifstream& inputFile);

    //! Read and apply configuration
    int config(istream& is);

    //! Generic function to read a single option
    template <typename T>
    void readOption(T& option, string is)
    { 
      if (is.substr(0,2) == "0x")
        stringstream(is) >> hex >> option;
      else
        stringstream(is) >> option;
    }

};

#endif //_daqConfig_H