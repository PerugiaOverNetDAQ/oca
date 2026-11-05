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
#include <stdexcept>

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
      uint16_t testUnitCfg; //TestUnit Cnofiguration
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
      bool adcFast; //! ASTRA UPDATE: register 7 bit 24
      uint16_t busyLen; //!Duration of extended busy
      uint16_t adcDelay; //!ADC delay (in clock cycles)
      uint16_t prgClkDiv; //!PRG clock period
      uint16_t prgClkDuty; //!PRG clock duty cycle
      bool fastorTxDisable; //!FastOR transmitter disable
      bool debugEn; //!Debug enable
      bool pt1; //!PT1
      bool pt2; //!PT2
      bool gain; //!Gain
      bool polarity; //!Polarity
      bool serTxDisable; //!Serializer transmitter disable
      bool globalTestPulse; //!Global test-pulse enable
      uint32_t maskA; //!Channel mask A
      uint32_t maskB; //!Channel mask B
      uint32_t tpEnA; //!Test-pulse enable A
      uint32_t tpEnB; //!Test-pulse enable B
      uint32_t discA; //!Discriminator A
      uint32_t discB; //!Discriminator B

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
        cout << "PRG clock period: " << prgClkDiv << endl;
        cout << "PRG clock duty cycle: " << prgClkDuty << endl;
        cout << "FastOR transmitter disable: " << fastorTxDisable << endl;
        cout << "Debug enable: " << debugEn << endl;
        cout << "PT1: " << pt1 << endl;
        cout << "PT2: " << pt2 << endl;
        cout << "Gain: " << gain << endl;
        cout << "Polarity: " << polarity << endl;
        cout << "Serializer transmitter disable: " << serTxDisable << endl;
        cout << "Global test-pulse enable: " << globalTestPulse << endl;
        cout << "Channel mask A: " << maskA << endl;
        cout << "Channel mask B: " << maskB << endl;
        cout << "Test-pulse enable A: " << tpEnA << endl;
        cout << "Test-pulse enable B: " << tpEnB << endl;
        cout << "Discriminator A: " << discA << endl;
        cout << "Discriminator B: " << discB << endl;
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
      stringstream value(is);
      if (is.substr(0,2) == "0x" || is.substr(0,2) == "0X")
        value >> hex;
      if (!(value >> option) || value.peek() != char_traits<char>::eof())
        throw runtime_error("Invalid PAPERO configuration value: " + is);
    }
};

#endif //_paperoConfig_H