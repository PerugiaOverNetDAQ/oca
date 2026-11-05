#ifndef _HPSSERVER_H_
#define _HPSSERVER_H_

#include <iostream>
#include <chrono>

#include "tcpServer.h"
#include "fpgaDriver.h"
#include "hpsDataServer.h"


class hpsServer: public tcpServer {

private:
  const uint32_t kOkVal  = 0xb01af1ca; //!< Answer to client if everything ok
  const uint32_t kBadVal = 0x000cacca; //!< Answer to client if something nok
  int kCmdLen; //!< Commands length, to be handshaked with the client
  uint32_t kEvtCount; //!< Event counter of a specific run
  std::chrono::_V2::system_clock::time_point kStartRunTime; //!< Start time of a run

  /*
    Standard reply procedure to commands
  */
  void cmdReply(const char* cmd);

public:
  hpsServer(int port, int verb=0);

  /*!
    Handshaking to set the same command length of the client
  */
  void cmdLenHandshake();
  
  /*!
    Listen to incoming commands
  */
  void* ListenCmd();

  /*!
    Process commands
  */
  void ProcessCmdReceived(char* msg);
  
};


#endif
