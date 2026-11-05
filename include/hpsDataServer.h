/*!
  @file hpsDataServer.h
  @brief Header for TCP server to transfer scientific data to merger
  @author Mattia Barbanera (mattia.barbanera@infn.it)
  @author Matteo Duranti (matteo.duranti@infn.it)
*/

#ifndef _HPSDATASERVER_H_
#define _HPSDATASERVER_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include "hwlib.h"

#include "utility.h"
#include "tcpServer.h"
#include "hpsServer.h"
#include "fpgaDriver.h"

using namespace std;

class hpsDataServer: public tcpServer {

private:
  bool kIsRunning = false; //!<Start or stop readings
  uint32_t kEvtCount; //!< Event counter of a specific run
  chrono::_V2::system_clock::time_point kStartRunTime; //!< Start time of a run
  thread kData3d; //!< Data thread handle

protected:
  /*
    Send a 'Start of run' word to the merger
    Enable triggers in FPGA
    While kIsRunning, read events from FPGA and send them to merger
    When kIsRunning=false, disable triggers in FPGA and empty FIFOs
  */
  void dataReading();

public:
  hpsDataServer(int port, int verb=0);
  ~hpsDataServer();
  
  /*
    Await connection from merger
    Spawn a thread to stream data to FPGA
  */
  void startRun();

  /*
    Set kIsRunning=false
    Join thread
    Kill connection to merger
  */
  void stopRun();

  /*
    Read one event from FPGA
    Transmit it to merger
  */
  void getSendEvt(vector<uint32_t>& evt);
  
};


#endif
