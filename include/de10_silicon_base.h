#ifndef DE10_SILICON_BASE_H
#define DE10_SILICON_BASE_H

#include <string>
#include <vector>

#include "tcpclient.h"
#include "paperoConfig.h"

class de10_silicon_base: public tcpclient {

private:
  //FIX ME: trasformare le variabili sotto in std::map in cui la key è un nome logico
  //(cosi' potremmo anche fare un metodo generico Set("pincopallino", ...)
  //e il value è una pair (indirizzo registro, contenuto)

  //Configuration variables
  uint32_t mode;
  uint32_t detId;
  uint32_t testUnitCfg;
  uint32_t hkEn;
  uint32_t testUnitEn;
  uint32_t dataEn;
  uint32_t intTrigPeriod;
  uint32_t calEn;
  uint32_t intTrigEn;
  uint32_t pktLen;
  uint32_t feClkDuty;
  uint32_t feClkDiv;
  uint32_t adcClkDuty;
  uint32_t adcClkDiv;
  uint32_t trig2Hold;
  uint32_t adcFast;
  uint32_t busyLen;
  uint32_t adcDelay;
  string   bias0;
  string   bias1;
  uint32_t ideTest;
  uint32_t chTest;

  /*
    Check if detector performed the required actions of the command sent
  */
  int checkReply(const char* msg);

public:
  ~de10_silicon_base();
  de10_silicon_base(const std::string address, uint32_t port, paperoConfig::configParams* params, int _calMode, int _intTrig, int verb=0);

  // virtual void changeText(const std::string& new_text) {};
  // virtual void sendData(std::vector<double> event) {};

  virtual void SetCmdLenght(int lenght);//overrides the mothers' one

  void SetDetId(uint32_t _detid){ detId = _detid; }//FIX ME: not effective until an Init is sent
  void SetPacketLen(uint32_t _pktLen){ pktLen = _pktLen; }//FIX ME: not effective until an Init is sent
  
  int readReg(int regAddr, uint32_t &regCont);
  int writeReg(int regAddr, uint32_t regCont);
  int updateReg(int regAddr, uint32_t regCont, uint32_t mask);
  int Init();
  int SetTrig2Hold(uint32_t delayIn);
  int SetMode(uint8_t modeIn);
  int GetEventNumber();
  int EventReset();
  void AskEvent();
  int GetEvent(std::vector<uint32_t>& evt, uint32_t& evtLen);
  int SetCalibrationMode(uint32_t calEnIn);
  int WriteCalibPar();
  int SaveCalibrations();
  int SetIntTriggerPeriod(uint32_t intTrigPeriodIn);
  int SelectTrigger(uint32_t intTrigEnIn);
  int ConfigureTestUnit(uint32_t testUnitEnIn);
  int SetFeClk(uint32_t _feClkDuty, uint32_t _feClkDiv);
  int SetAdcClk(uint32_t _adcClkDuty, uint32_t _adcClkDiv);
  int SetIdeTest(uint32_t _ideTest, uint32_t _chTest);
  int SetAdcFast(uint32_t _adcFast);
  int SetBusyLen(uint32_t _busyLen);
  int SetAdcDelay(uint32_t _adcDelay);
  int SetConfigBias();
  int SetBias(string _bias0, string _bias1);
  int runStart(); //!< Send the runStart command to PAPERO
  int runStop(); //!< Send the runStop command to PAPERO
};

#endif
