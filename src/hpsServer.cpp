#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "hwlib.h"
#include <netinet/in.h>
//#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
//#include <sys/mman.h>
#include <arpa/inet.h>
#include <vector>
#include <iostream>
#include <chrono>

#include "utility.h"
#include "hpsServer.h"

extern fpgaDriver*    fpga;
extern hpsDataServer* hpsDataStream;

hpsServer::hpsServer(int port, int verb):tcpServer(port, verb){
  kVerbosity            = verb;
  kListeningOn          = false;
  kTcpConn              = -1;
  kSockDesc             = -1;
  kPort                 = port;
  kBlocking             = true;
  
  //Reset kAddr to intended values
  memset(&kAddr, 0, sizeof(kAddr));
  kAddr.sin_family      = AF_INET;
  kAddr.sin_port        = htons(kPort);
  kAddr.sin_addr.s_addr = INADDR_ANY;

  kCmdLen = 24;
  kEvtCount = 0;

  //Setup TCP server
  Setup();
}

void hpsServer::cmdLenHandshake(){
  Rx(&kCmdLen, sizeof(kCmdLen));
  printf("%s) Updating command length to %d\n", __METHOD_NAME__, kCmdLen);
  Tx(&kCmdLen, sizeof(kCmdLen));
  return;
}

void* hpsServer::ListenCmd(){
  //Accept new connections and set command length
  AcceptConnection();
  cmdLenHandshake();

  bool ListenCmdOn = true;
  kEvtCount = 0;
  kStartRunTime = std::chrono::system_clock::now();
  int bytesRead=0;
  while(ListenCmdOn) {
    char msg[256]="";
    bytesRead = 0;

    //Read the command
    bytesRead = Rx(msg, kCmdLen);

    //Check if the read is ok and process its content
    if(bytesRead < 0) {
      if (EAGAIN==errno || EWOULDBLOCK==errno) {
        printf("%s) errno: %d\n", __METHOD_NAME__, errno);
      }
      else {
        print_error("%s) Read error: (%d)\n", __METHOD_NAME__, errno);
      }
    }
    else if (bytesRead==0) {
      ListenCmdOn=false;
      printf("%s) Client closed the connection\n", __METHOD_NAME__);
    }
    else {
      ProcessCmdReceived(msg);
    }
    bzero(msg, sizeof(msg));
  }
  return nullptr;
}

void hpsServer::ProcessCmdReceived(char* msg){
  if(strcmp(msg, "cmd=init") == 0){
    cmdReply("init");

    if (kVerbosity > 1) printf("%s) Starting init...\n", __METHOD_NAME__);
    uint32_t regsContent[16];
    uint32_t singleReg = 0;

    //TCP-Receive the whole content (apart from reg rGOTO_STATE)
    for(int ii = 0; ii < 8; ii++){
      Rx(&singleReg, sizeof(singleReg));
      regsContent[ii*2]   = singleReg;
      regsContent[ii*2+1] = (uint32_t)ii+1;
    }
    fpga->InitFpga(regsContent, 16);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=readReg") == 0){
    cmdReply("readReg");
    uint32_t regContent;
    uint32_t regAddr = 0;

    Rx(&regAddr, sizeof(regAddr));
    //printf("Send read request...\n");
    fpga->ReadReg(regAddr, &regContent);
    Tx(&regContent, sizeof(regContent));
  }
  else if(strcmp(msg, "cmd=writeReg") == 0){
    cmdReply("writeReg");
    uint32_t regContent = 0;
    uint32_t regAddr = 0;
    int ret = 1;

    Rx(&regAddr, sizeof(regAddr));
    Rx(&regContent, sizeof(regContent));
    //printf("Send read request...\n");
    fpga->SingleWriteReg(regAddr, regContent);
    Tx(&ret, sizeof(ret));
  }
  else if((strcmp(msg, "cmd=setDelay")==0)||(strcmp(msg, "cmd=overWriteDelay")==0)){
    cmdReply("setDelay");

    uint32_t delay = 0;
    Rx(&delay, sizeof(delay));
    fpga->SetDelay(delay);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setMode") == 0){
    cmdReply("setMode");

    uint32_t mode = 0;
    Rx(&mode, sizeof(mode));
    fpga->SetMode(mode);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=getEventNumber") == 0){
    cmdReply("getEventNumber");

    uint32_t extTrigCount, intTrigCount;

    fpga->GetEventNumber(&extTrigCount, &intTrigCount);

    Tx(&extTrigCount, sizeof(extTrigCount));
    Tx(&intTrigCount, sizeof(intTrigCount));
  }
  else if(strcmp(msg, "cmd=eventReset") == 0){
    cmdReply("eventReset");

    kEvtCount = 0;
    kStartRunTime = std::chrono::system_clock::now();
    fpga->EventReset();
    Tx(&kOkVal , sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=calibrate") == 0){
    cmdReply("calibrate");

    uint32_t calib = 0;
    Rx(&calib, sizeof(calib));
    fpga->Calibrate(calib);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=writeCalibPar") == 0){
    printf("%s) WriteCalibPar not supported\n", __METHOD_NAME__);
    //Tx(&kBadVal, sizeof(kBadVal));
  }
  else if(strcmp(msg, "cmd=saveCalib") == 0){
    printf("%s) SaveCalibrations not supported\n", __METHOD_NAME__);
    //Tx(&kBadVal, sizeof(kBadVal));
  }
  else if(strcmp(msg, "cmd=intTrigPeriod") == 0){
    cmdReply("intTrigPeriod");

    uint32_t period = 0;
    Rx(&period, sizeof(period));
    fpga->intTriggerPeriod(period);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=selectTrigger") == 0){
    cmdReply("selectTrigger");

    uint32_t intTrig = 0;
    Rx(&intTrig, sizeof(intTrig));
    fpga->selectTrigger(intTrig);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=configTestUnit") == 0){
    cmdReply("configTestUnit");
    
    uint32_t tuCfg = 0;
    Rx(&tuCfg, sizeof(tuCfg));
    char testUnitCfg = ((tuCfg&0x300)>>8);
    char testUnitEn  = ((tuCfg&0x2)>>1);
    fpga->configureTestUnit(tuCfg);
    if (kVerbosity > 1) {
      printf("Test unit cfg: %d - en: %d\n", testUnitCfg, testUnitEn);
    }
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setFeClk") == 0){
    cmdReply("setFeClk");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setFeClk(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setAdcClk") == 0){
    cmdReply("setAdcClk");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setAdcClk(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setIdeTest") == 0){
    cmdReply("setIdeTest");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setIdeTest(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setAdcFast") == 0){
    cmdReply("setAdcFast");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setAdcFast(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setBusyLen") == 0){
    cmdReply("setBusyLen");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setBusyLen(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=setAdcDelay") == 0){
    cmdReply("setAdcDelay");
    
    uint32_t rxUInt = 0;
    Rx(&rxUInt, sizeof(rxUInt));
    fpga->setAdcDelay(rxUInt);
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if(strcmp(msg, "cmd=getEvent") == 0){
    cmdReply("getEvent");

	  static std::vector<uint32_t> evt;//so that the size (changed inside getEvent) is not changing continuosly
	  //	std::vector<uint32_t> evt;

    int evtLen=0;

    //Get an event from FPGA
    int evtErr = fpga->getEvent(evt, &evtLen);
    if (kVerbosity > 4) printf("getEvent result: %d\n", evtErr);
    //Send the eventLen to the socket
    Tx(&evtLen, sizeof(evtLen));
    //Send the event to the socket
    if (evtLen>0) {
      Tx(evt.data(), evtLen*sizeof(uint32_t));
      kEvtCount++;
      if (kEvtCount % 1000 == 0) {
        auto evt1000 = std::chrono::system_clock::now();
        std::cout << "Event count : " << kEvtCount << " in " << std::chrono::duration_cast<std::chrono::seconds>(evt1000 - kStartRunTime).count() << " s\n";
      }
    }
      if (kVerbosity > 3) printf("%s) Event sent\n", __METHOD_NAME__);
  }
  else if (strcmp(msg, "cmd=runStart") == 0) {
    cmdReply("runStart");
    
    printf("%s) Starting run... \n", __METHOD_NAME__);

    Tx(&kOkVal, sizeof(kOkVal));
    
    //Start the run and enable data sending
    if (hpsDataStream) {
      hpsDataStream -> startRun();
    }
    //fpga->SetMode(1);
  }
  else if (strcmp(msg, "cmd=runStop") == 0) {
    cmdReply("runStop");
    
    printf("%s) Stopping run...", __METHOD_NAME__);
    //Stop the run and disable data sending
    if (hpsDataStream) {
      hpsDataStream -> stopRun();
    }
    Tx(&kOkVal, sizeof(kOkVal));
  }
  else if (strcmp(msg, "cmd=setCmdLength") == 0) {
    cmdReply("setCmdLength");
    
    cmdLenHandshake();
  }
  else if (strcmp(msg, "cmd=quit") == 0) {
    cmdReply("quit");
    
    printf("FIX ME: Close connection and socket...\n");
  }
  else {
    printf("%s) Unknown message: %s\n", __METHOD_NAME__, msg);
    Tx(&kBadVal, sizeof(kBadVal));
    Tx(&kBadVal, sizeof(kBadVal));
  }
}

void hpsServer::cmdReply(const char* cmd){
  char cmdReadBack[256]="";
  sprintf(cmdReadBack, "rcv=%s", cmd);
  Tx(cmdReadBack, kCmdLen);
}

/*
void* hpsServer::receiver_slow_control(void *args){

  int len, rc, on = 1;
  int listen_sd = -1, new_sd = -1;
  char buffer[80];
  struct sockaddr_in addr;
  int timeout;
  struct pollfd fds[200];
  int nfds = 1, current_size = 0;
  char *port = (char*)args;
  int porta = atoi(port);

  listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_sd < 0){

    perror("socket() fallita");
    exit(-1);
  }

  rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if(rc < 0){

    perror("setopt() fallita");
    close(listen_sd);
    exit(-1);
  }

  rc = ioctl(listen_sd, FIONBIO, (char *)&on);
  if(rc < 0){

    perror("ioctl() fallita");
    close(listen_sd);
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(porta);

  rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
  if(rc < 0){

    perror("bind() fallita");
    close(listen_sd);
    exit(-1);
  }

  rc = listen(listen_sd, 32);
  if(rc < 0){

    perror("listen() fallita");
    close(listen_sd);
    exit(-1);
  }

  memset(fds, 0, sizeof(fds));

  fds[0].fd = listen_sd;
  fds[0].events = POLLIN;

  timeout = (3 * 60* 1000);

  while(1){

    printf("faccio poll\n");
    rc = poll(fds, nfds, timeout);
    if(rc < 0){

      perror("errore poll()");
      close(listen_sd);
      exit(-1);
    }

    current_size = nfds;
    printf("Current size %d\n", current_size);
    if(fds[0].revents & POLLIN){

      struct sockaddr_in cliaddr;
      int addrlen = sizeof(cliaddr);
      new_sd = accept(listen_sd, (struct sockaddr *)&cliaddr, (socklen_t * __restrict__ )&addrlen);
      printf("connessione da parte di %d accettata\n", new_sd);
      for(int i = 0; i < 200; i++){

	if(fds[i].fd == 0){

	  fds[i].fd = new_sd;
	  fds[i].events = POLLIN;
	  nfds++;
	  break;
	}
      }
    }

    for(int i = 1; i < 200; i++){

      if(fds[i].fd > 0 && fds[i].revents & POLLIN){

	rc = read(fds[i].fd, buffer, sizeof(buffer));
	if(rc < 0){

	  perror("errore poll()");
	  exit(-1);
	}else if(rc == 0){

	  close(fds[i].fd);
	  fds[i].fd = -1;
	  continue;
	}

	len = rc;
	printf("[SERVER] ho ricevuto %d bytes, da %d:: %s\n", len, fds[i].fd, buffer);

	rc = write(fds[i].fd, buffer, len);
	if(rc < 0){

	  perror("errore write()");
	  exit(-1);
	}



      }
    }
  }

  pthread_exit(NULL);
}
*/