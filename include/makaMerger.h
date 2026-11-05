/*!
  @file makaMerger.h
  @brief Header for merger to collect data from remote detectors
  @author Mattia Barbanera (mattia.barbanera@infn.it)
*/

#ifndef MAKA_H
#define MAKA_H


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
#include <time.h>

//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <sys/poll.h>
//#include <sys/ioctl.h>

#include "utility.h"
#include "tcpServer.h"
#include "tcpclient.h"
#include "udpSocket.h"
#include "makaConfig.h"

using namespace std;

class makaMerger : public tcpServer {

  private:
    //Configuration parameters
    const uint32_t kOkVal  = 0xb01af1ca; //!< OK  answer to client
    const uint32_t kBadVal = 0x000cacca; //!< NOK Answer to client
    vector<uint32_t> kDetIds; //!<Remote detectors ports
    vector<std::string> kDetAddrs; //!<Remote detectors addresses
    vector<uint32_t> kDetPorts; //!<Remote detectors ports
    string kDataPath = "./data/"; //!<Folder path where to store data
    int kCmdLen = 24; //!<Server commands length, handshaken with the client
    thread kMerger3d; //!<Thread that hosts the merger
    bool kRunning = false; //!<Flag for run state
    uint32_t kNEvts = 0; //!<Events in a run
    uint32_t kNEvtsCal = 0; //!<Calibration events in a run
    uint32_t kNEvtsBeam = 0; //!<Physics events in a run
    //uint32_t kRunTime //!<Run time
    std::string kRunType;     //!<Run information: type
    uint32_t kRunNum;   //!<Run information: number
    uint32_t kRunTime;  //!<Run information: start time, in unix time
    bool kDataToFile; //!< Write data to file
    bool kDataToOm;   //!< Send data to On-line Monitor
    uint32_t kOmPreScale; //!< On-line Monitor Prescale factor
    configPacket* cpRx; //!<Configurations received from OCA
    startPacket*  spRx; //!<Start received from OCA

    //UDP server to on-line monitor
    std::string kUdpAddr = "127.0.0.1"; //!< UDP Server address (x.x.x.x format)
    int kUdpPort = 8890;  //!< UDP server port
    udpClient* omClient;

    //N clients for remote detectors
    vector<tcpclient*> kDet; //!<Remote detectors clients

    /*
      Write file header:
        Known word
        UNIX time of the run
        MAKA git hash
        Type [31:28], Data Version [27:16], # detectors [15:0]
        Detector ID 0 [31:16], Detector ID 1 [15:0]
        .......
    */
    int fileHeader(FILE* _dataFile);

    /*
      Create file
      While kRunning, collect data from clients
      When finished, close file
    */
    int merger();

    /*
      Create a header
      Collect data from clients
      Write data to disk
      Parasitically send data via UDP
    */
    int collector(FILE* _dataFile);

    /*
      Read an event from one detector
      First word must be the event length in uint32_t units
    */
    int getEvent(std::vector<uint32_t>& _evt, uint32_t& _evtLen, int _det);

    /*
      Handshake command length
    */
    void cmdLenHandshake();

    /*
      Process commands received 
    */
    void processCmds(char* msg);

    /*

    */
   void cmdReply(const char* cmd);


  public:
    makaMerger(int port, int verb=0, bool _net=true);
    ~makaMerger();

    /*
      Clear detector address and port lists
    */
    void clearDetLists();

    /*
      Add detector address and port to lists
    */
    void addDet(uint32_t _id, char* _addr, int _port);

    /*
      Close clients for the remote detectors
    */
    void clearDetectors();

    /*
      Create actual clients for the remote detectors
      FIXME: tcpclient connects as soon as it is created -> separate functions
    */
    void setUpDetectors();

    /*
      Start clients
      kRunning = true
      Start thread
    */
    void runStart();

    /*
      kRunning = false
      Stop thread
      Close clients
    */
    void runStop(int _sleep=0);

    /*
      Accept connections
      Loop to listen commands
    */
    void* listenCmd();

};

#endif
