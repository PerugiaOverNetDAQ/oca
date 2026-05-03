/*!
  @file makaClient.h
  @brief Header for client to interface MAKA merger
  @author Mattia Barbanera (mattia.barbanera@infn.it)
*/

#ifndef MAKACLIENT_H
#define MAKACLIENT_H


#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctime>
#include <bitset>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

#include "utility.h"
#include "makaConfig.h"
#include "tcpclient.h"

using namespace std;

class makaClient : public tcpclient {

  private:
    //Configuration parameters
    const uint32_t kOkVal  = 0xb01af1ca; //!< OK  answer from server
    const uint32_t kBadVal = 0x000cacca; //!< NOK Answer from server

    /*
      Handshake command length
    */
    void cmdLenHandshake(int _cmdLen);

    /*
      Check if MAKA performed the required actions of the command sent
    */
    int checkReply(const char* msg);


  public:
    makaClient(const char *_address, int _port, int _verb, int _cmdLen);
    ~makaClient();

    /*
      Send cmd=setup
      Send configurations
    */
    int setup(string _dataPath, vector<bool> _makaEn, vector<uint32_t> _detIds,\
                vector<uint32_t> _detPorts, vector<std::string> _detAddrs,\
                bool _dataToFile, bool _dataToOm, uint32_t _omPreScale);

    /*
      Send cmd=runStart
      Send configurations
    */
    int runStart(std::string _runType, uint32_t _runNum, uint32_t _runTime);

    /*
      Send cmd=runStop
    */
    int runStop(uint32_t& _nEvts);

};

#endif
