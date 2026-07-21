#ifndef DAQSERVER_H
#define DAQSERVER_H

#include <vector>
#include <thread>

#include "tcpServer.h"
#include "de10_silicon_base.h"

#include "daqConfig.h"
#include "paperoConfig.h"

extern daqConfig::configParams daqConf;

class daqserver: public tcpServer {

protected:
  int kCmdLen;    //!< Length of incoming TCP commands 

private:
  std::vector<std::string> addressdet;
  std::vector<uint32_t> portdet;
  std::vector<uint32_t> iddet;
  std::vector<bool> makaEn;
  std::vector<de10_silicon_base*> det;
  std::string kdataPath = "./data/"; //!< Data file path
  volatile bool kStart; //!< Start event recording
  std::thread _3d;//!< Thread handle
  int calibmode;  //!< Calibration enable
  int mode;       //!< '1': Run, '0': Stop
  int trigtype;   //!< Trigger Type: if '1', internal
  unsigned int nEvents = 0; //!< Acquired event number
  paperoConfig::vectorParam paperoConfVector; //!< Papero configuration vector
  
  /*!
    Send a reply to received commands
  */
  int ReplyToCmd(char* msg);
  
  /*!
    Printing the message received from the client(s)
  */
  void ProcessCmdReceived(char* msg);
  
  int recordEvents(FILE* fd);
  
public:
  ~daqserver();
  daqserver(int port, int verb, std::string paperoCfgPath);

  /*!
    Setup clients to configure the detectors
  */
  void SetUpConfigClients();

  /*!
    Define the length of the receiving commands
  */
  void SetCmdLenght(int lenght) {
    kCmdLen = lenght;
  }

  /*!
    Create lists for detector IP addresses and ports
  */
  void SetListDetectors();

  /*!
    Create list for detector objects
  */
  void SetDetectors();

  void SetDetId(const char* addressde10, uint32_t _detId);
  void SetPacketLen(const char* addressde10, uint32_t _pktLen);

  void SetDetectorsCmdLenght(int detcmdlenght);

  void SetCalibrationMode(uint32_t mode);
  //!< Enable/disable normal events independently from the calibration request.
  void SetEventEnable(uint32_t enable);
  //!< Enable DAQ's valid-RAM check inside PAPERO.
  void SetAutoCalibration(uint32_t enable);
  //!< Enable/disable calibration table packets independently from computation.
  void SetSaveCalibration(uint32_t enable);
  //!< Enable the one-shot LTH/HTH update for this run command.
  void SetApplyThresholds(uint32_t enable);
  //!< Stop (0) or latch and start (>0) the command assembled for every board.
  void SetMode(uint8_t mode);
  //!< Select external (0) or internally generated (1) triggers on every board.
  void SelectTrigger(uint32_t trig);

  void SetFeClk(uint32_t _feClkDuty, uint32_t _feClkDiv);
  void SetAdcClk(uint32_t _adcClkDuty, uint32_t _adcClkDiv);
  void SetIdeTest(uint32_t _ideTest, uint32_t _chTest);
  void SetAdcFast(uint32_t _adcFast);
  void SetBusyLen(uint32_t _busyLen);
  void SetAdcDelay(uint32_t _adcDelay);

  void ResetBoards();
  //!< Read CAL_VALID on every board; false is not an error, failed I/O is.
  int AllCalibrationsValid(bool& allValid);
  //!< Read the firmware STOP/drain acknowledgement on every board.
  int AllBoardsRunIdle(bool& allIdle);
  //!< Poll RUN_IDLE while HPS senders and MAKA are still active.
  int WaitForBoardsRunIdle(uint32_t timeoutMs);

  void ReadAllRegs();
  
  int ReadReg(uint32_t regAddr);
  int Init();
  void Start(char* runtype, uint32_t runnum, uint32_t unixtime);
  //!< Return nonzero without closing HPS/MAKA if the FPGA drain is not proven.
  int Stop(uint32_t& _nEvts);

  /*!
    Receive commands and call the appropriate function
  */
  void ListenCmd();

  void runStart(); //!< Send runStart to all detectors
  void runStop(); //!< Send runStop to all detectors

};

#endif
