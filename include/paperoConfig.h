/*!
  @file paperoConfig.h
  @brief Read configuration parameters from a .cfg file
  @details and populate a struct for each valid line (not empty and not comment).
  The structs are collected in a map indexed with the detector ID, specified in
  the first column as a uint32_t.
 */

#ifndef _paperoConfig_H
#define _paperoConfig_H

#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>
#include <fstream>
#include <sstream>
#include <cinttypes>
#include <map>
#include <vector>
#include <regex>

#include "daqConfig.h"
#include "utility.h"

using namespace std;

//! @copydoc paperoConfig.h
class paperoConfig
{
  public:
    paperoConfig(const string& filePath){
      readConfigFromFile(filePath);
    };

    //! Struct containing the configuration parameters
    struct configParams {
      uint32_t id; //!Detector ID
      bool makaEnable; //!Send line to MAKA
      string ipAddr; //!IP address
      int tcpPort; //!TCP client port to connect to HPS
      int cmdLen; //!Command length
      uint8_t testUnitCfg; //TestUnit Cnofiguration
      bool testUnitEn; //TestUnit Enable
      bool hkEn; //!HK reader enable
      bool dataEn; //!Scientific data enable
      uint32_t intTrigPeriod; //!Internal Trigger period (in clock cycles)
      uint32_t pktLen; //!Packet Length (in case of events of fixed length)
      uint16_t feClkDuty; //!FE clock duty cycle (in clock cycles)
      uint16_t feClkDiv; //!FE clock period (in clock cycles)
      uint16_t adcClkDuty; //!ADC clock duty cycle (in clock cycles)
      uint16_t adcClkDiv; //!ADC clock period (in clock cycles)
      uint16_t trig2Hold; //!Trigger to Hold delay (in clock cycles)
      bool adcFast; //!AD7276 Fast Mode support
      uint16_t busyLen; //!Duration of extended busy
      uint16_t adcDelay; //!ADC delay (in clock cycles)
      string bias0; //!Bias value to be set for the detector 0
      string bias1; //!Bias value to be set for the detector 1
      bool ideTest; //!Test port of IDE1140
      uint16_t chTest; //!IDE1140 channel connected to CAL port

      void dump()
      {
        cout << "ID:                   " << id << endl;
        cout << "Send to MAKA:         " << makaEnable << endl;
        cout << "IP address:           " << ipAddr << endl;
        cout << "TCP port:             " << tcpPort << endl;
        cout << "Command Length:       " << cmdLen << endl;
        cout << "TestUnit cfg:         " << testUnitCfg << endl;
        cout << "TestUnit En:          " << testUnitEn << endl;
        cout << "HK En:                " << hkEn << endl;
        cout << "Data En:              " << dataEn << endl;
        cout << "Int Trig Period:      " << intTrigPeriod << endl;
        cout << "Packet Length:        " << pktLen << endl;
        cout << "FE Clk Duty:          " << feClkDuty << endl;
        cout << "FE Clk Div:           " << feClkDiv << endl;
        cout << "ADC Clk Duty:         " << adcClkDuty << endl;
        cout << "ADC Clk Div:          " << adcClkDiv << endl;
        cout << "Trig-2-Hold Delay:    " << trig2Hold << endl;
        cout << "AD7276 Fast Mode:     " << adcFast << endl;
        cout << "Extended Busy Len:    " << busyLen << endl;
        cout << "ADC delay:            " << adcDelay << endl;
        cout << "Bias 0:               " << bias0 << endl;
        cout << "Bias 1:               " << bias1 << endl;
        cout << "IDE1140 Test Port:    " << ideTest << endl;
        cout << "IDE1140 Channel Test: " << chTest << endl;
      }
    };

    typedef std::vector<configParams*> vectorParam;

    //! Open configuration file and read it in an istream
    void readConfigFromFile(const string& filePath);

    //! Retrieve parameters for a single vector entry
    configParams* getParams(int det);

    //! Retrieve parameters for all the detectors
    vectorParam getParams();

    //! Dump the content of the whole configuration map
    void dump();

  private:

    //! Configuration parameters vector
    vectorParam conf;

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

    bool isValidFixedFloat(const std::string& s) {
      // ^      : start of string
      // \d{2}  : exactly 2 digits
      // \.     : decimal point
      // \d     : exactly 1 digit
      // $      : end of string
      std::regex floatPattern(R"(^\d{2}\.\d$)");
      return std::regex_match(s, floatPattern);
    }
};

#endif //_paperoConfig_H