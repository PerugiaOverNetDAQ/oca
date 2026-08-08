#include "utility.h"
#include <sys/time.h>
//#include <TDatime.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctime>
#include <bitset>
#include <iostream>
#include <chrono>
#include <algorithm>

#include "daqserver.h"
#include "makaClient.h"

extern makaClient* maka;

daqserver::daqserver(int port, int verb, std::string paperoCfgPath):tcpServer(port, verb){
  //Copy configuration file parameters
  kCmdLen   = daqConf.clientCmdLen;
  calibmode = daqConf.calMode;
  trigtype  = daqConf.intTrigEn;
  kdataPath = daqConf.dataFolder;

  //Read paperoConfig parameters
  paperoConfig paperoConf(paperoCfgPath);
  paperoConfVector = paperoConf.getParams();
  
  //Stop the run (if applicable) and reset
  kStart  = false;
  mode    = 0;
  addressdet.clear();
  portdet.clear();
  makaEn.clear();

}

daqserver::~daqserver(){
  if (kVerbosity>0) {
    printf("%s) destroying daqserver\n", __METHOD_NAME__);
  }

  for (uint32_t ii=0; ii<det.size(); ii++) {
    if (det.at(ii)) delete det.at(ii);
    if (kVerbosity>0) {
      printf("%s) destroying DE10 %d\n", __METHOD_NAME__, ii);
    }
  }

  return;
}

void daqserver::SetUpConfigClients(){
  //Setup detector clients
  SetListDetectors();

  //Configure MAKA client
  maka->setup(daqConf.dataFolder, makaEn, iddet, portdet, addressdet, \
              daqConf.makaSendToFile, daqConf.makaSendToOm, \
              daqConf.makaOmPreScale);

  //Start the socket
  SockStart();

  if (kVerbosity>0){
    printf("%s) DAQ Server Created\n", __METHOD_NAME__);
  }
  
  //Configure detectors
  SetDetectors();
  Init();

  return;
}

void daqserver::SetListDetectors(){
  iddet.clear();
  addressdet.clear();
  portdet.clear();
  makaEn.clear();

  for (uint32_t ii=0; ii<paperoConfVector.size(); ii++) {
    iddet.push_back(paperoConfVector[ii]->id);
    addressdet.push_back(paperoConfVector[ii]->ipAddr.data());
    portdet.push_back(paperoConfVector[ii]->tcpPort);
    makaEn.push_back(paperoConfVector[ii]->makaEnable);
  }
}

void daqserver::SetDetectors(){
  det.clear();
  for (uint32_t ii=0; ii<portdet.size(); ii++) {
    det.push_back(
      new de10_silicon_base(
        addressdet[ii],
        portdet[ii],
        paperoConfVector[ii],
        calibmode,
        trigtype,
        kVerbosity)
      );
  }
}

void daqserver::SetDetId(const char* addressde10, uint32_t _detId){
  
  for (int ii=0; ii<(int)(det.size()); ii++) {
    if (strcmp(addressde10, addressdet[ii].c_str()) == 0){
      det[ii]->SetDetId(_detId);
    }
  }
  
  return;
}

void daqserver::SetPacketLen(const char* addressde10, uint32_t _pktLen){
  
  for (int ii=0; ii<(int)(det.size()); ii++) {
    if (strcmp(addressde10, addressdet[ii].c_str()) == 0){
      det[ii]->SetPacketLen(_pktLen);
    }
  }
  
  return;
}

void daqserver::SetDetectorsCmdLenght(int detcmdlenght){

  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetCmdLenght(detcmdlenght);
  }

  return;
}

void daqserver::SetCalibrationMode(uint32_t mode){

  calibmode = mode;
  
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetCalibrationMode(calibmode);
  }
  
  return;
}

void daqserver::SetMode(uint8_t _mode){

  mode = _mode;
  
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetMode(mode);
  }
  
  return;
}
  
void daqserver::SelectTrigger(uint32_t trig){

  trigtype = trig;
  
  //Only select trigger type in the Patch-Panel DE10 (assuming it is the last one)
  //det[portdet.size()-1]->SelectTrigger(trigtype);
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SelectTrigger(trigtype);
  }

  return;
}

void daqserver::SetFeClk(uint32_t _feClkDuty, uint32_t _feClkDiv){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetFeClk(_feClkDuty, _feClkDiv);
  }
  return;
}

void daqserver::SetAdcClk(uint32_t _adcClkDuty, uint32_t _adcClkDiv){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetAdcClk(_adcClkDuty, _adcClkDiv);
  }
  return;
}

void daqserver::SetIdeTest(uint32_t _ideTest, uint32_t _chTest){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetIdeTest(_ideTest, _chTest);
  }
  return;
}

void daqserver::SetAdcFast(uint32_t _adcFast){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetAdcFast(_adcFast);
  }
  return;
}

void daqserver::SetAdcDelay(uint32_t _adcDelay){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetAdcDelay(_adcDelay);
  }
  return;
}

void daqserver::SetBusyLen(uint32_t _busyLen){
  for (int ii=0; ii<(int)(det.size()); ii++) {
    det[ii]->SetBusyLen(_busyLen);
  }
  return;
}



void daqserver::ResetBoards(){
  printf("%s) Resetting boards counters...\n", __METHOD_NAME__);
  for(auto de10 : det){
    de10->EventReset();
  }
}

int daqserver::ReplyToCmd(char* msg) {
  //Send msg to socket
  int n = write(kTcpConn, msg, kCmdLen);
  if (n < 0){
    fprintf(stderr, "%s) Error in writing to the socket\n", __METHOD_NAME__);
    return 1;
  }
  
  if (kVerbosity>1) {
    printf("%s) Sent %d bytes\n", __METHOD_NAME__, n);
  }
  
  return 0;
}

void daqserver::ListenCmd(){

  kListeningOn = true;

  while (kListeningOn){
  
    char msg[LEN];

    //Receive a command of kCmdLen numbers chars (each one in ASCII char),
    //+ 1 for the termination character
    ssize_t readret = read(kTcpConn, msg, ((kCmdLen*8)*sizeof(char)+1));

    if (readret < 0){
      //Error
      if (EAGAIN == errno || EWOULDBLOCK == errno) {
        if (kVerbosity>1) {
          printf("%s) There's nothing to read now; try again later\n", __METHOD_NAME__);
        }
      }
      else {
        print_error("%s) Read error: \n", __METHOD_NAME__);
      }
    }
    else if (readret==0){
      //Stream is over and client disconnected: wait for another connection
      AcceptConnection();
    }
    else {
      //RX ok
      ProcessCmdReceived(msg);
    }

    bzero(msg, sizeof(msg));
  }

  if (kVerbosity>0) {
    printf("%s) Stop Listening\n", __METHOD_NAME__);
  }

  return;
}

void daqserver::ProcessCmdReceived(char* msg){

  if (kVerbosity>1) {
    printf("%s) |%s| (lenght = %lu)\n", __METHOD_NAME__, msg, strlen(msg));
  }

  if(strstr(msg, "cmd=") != NULL) { //out commands: "cmd=xxxx"

    if (strcmp(msg, "cmd=Init") == 0){
      printf("%s) Init()\n", __METHOD_NAME__);
      Init();
      ReplyToCmd(msg);
    }
    else if (strcmp(msg, "cmd=Wait") == 0){//essentially for test
      printf("%s) Wait()\n", __METHOD_NAME__);
      printf("sleeping for 30s: "); fflush(stdout);
      for (int ii=0; ii<30; ii++) {
        printf("%d... ", ii); fflush(stdout);
        sleep(1);
      }
      printf("\n");
      ReplyToCmd(msg);
    }

  }
  else {//possibly a chinese command
    // command
    static const char* btcmd ="FF800008";
    static const char* start ="EE000001";
    static const char* stop  ="EE000000";
    static const char* beam  ="0002";
    static const char* cal   ="0004"; //10Hz
    static const char* calOffSpill   ="0000"; //10Hz
    static const char* mix   ="0001";

    static const int length=16;
    char command_string[2*length+1] = "";
    hex2string(msg, length, command_string);

    //    printf("%s\n", command_string);

    char cmdgroup[4][32] = {"", "", "", ""};
    for (int ii=0; ii<4; ii++) {
      strncpy(cmdgroup[ii], &command_string[ii*8], 8);
      if (kVerbosity>0) {
        printf("%s\n", cmdgroup[ii]);
      }
    }
    
    //check the command
    if (strcmp(btcmd, cmdgroup[0])==0) {//is a chinese command
      if (strcmp(start,cmdgroup[2])==0) {//start daq
	      printf("%s) Start()\n", __METHOD_NAME__);

        // ignore consecutive Start commands
        if(kStart){
          char* tempStr = "Already in START state. Ignoring last command.";
          printf("%s) %s\n", __METHOD_NAME__, tempStr);
          ReplyToCmd(tempStr);
          return;
        }

	      char runtype[32] = "";
	      strncpy(runtype, &cmdgroup[1][4], 4);
	      char sruntype[32] = "";
	      if (strcmp(beam,runtype)==0) {
	        sprintf(sruntype, "BEAM");
	        SetCalibrationMode(0);
          SelectTrigger(0);
	      }
	      else if ((strcmp(cal,runtype)==0) | (strcmp(calOffSpill,runtype)==0)) {
	        sprintf(sruntype, "CAL");
	        SetCalibrationMode(1);
          SelectTrigger(1);
	      }
        else if (strcmp(mix,runtype)==0) {
	        sprintf(sruntype, "MIX");
	        SetCalibrationMode(0);
          SelectTrigger(0);
	      }
	      else {
	        printf("%s) Not a valid run type %s\n", __METHOD_NAME__, runtype);
	        sprintf(sruntype, "????");
	      }

        char srunnum[32] = "";
        strncpy(srunnum,  &cmdgroup[1][0], 4);
        char* ptr;
        uint32_t runnum = strtol(srunnum, &ptr, 16);
        uint32_t unixtime = strtol(cmdgroup[3], &ptr, 16);
        std::time_t t = unixtime;
        if (kVerbosity>0) {
          printf("runtype=%s (-> %s), runnum=%s (%u), unixtime=%u (%s -> %s)\n", runtype, sruntype, srunnum, runnum, unixtime, cmdgroup[3], asctime(localtime(&t)));
        }
        ResetBoards();
        runStart();
        maka->runStart(sruntype, runnum, unixtime);

        printf("%s) Everything started. Enabling triggers...\n", __METHOD_NAME__);
        SetMode(1);

        //Spawn a thread to read events. Stop() will join the thread
        nEvents = 0;
        //_3d = std::thread(&daqserver::Start, this, sruntype, runnum, unixtime);
        
        kStart = true;
        ReplyToCmd(msg);
      }
      else if(strcmp(stop,cmdgroup[2])==0) {//stop daq
        printf("%s) Stop()\n", __METHOD_NAME__);
        // ignore consecutive Start commands
        if(!kStart){
          char* tempStr = "Already in STOP state. Ignoring the command.";
          printf("%s) %s\n", __METHOD_NAME__, tempStr);
          ReplyToCmd(tempStr);
          return;
        }
        uint32_t nEvts = 0;
        Stop(nEvts);
        printf("%s)        Events: %d \n", __METHOD_NAME__, nEvts);
        Tx(&nEvts, sizeof(nEvts));
        ReplyToCmd(msg);
      }
      else {
        printf("%s) not a valid sub-command: %s (%s)\n\n", __METHOD_NAME__, cmdgroup[2], command_string);
        sprintf(msg, "NOT-A-VALID-SUBCMD");
        ReplyToCmd(msg);
      }
    }
    else {
      printf("%s) not a valid command: %s\n\n", __METHOD_NAME__, command_string);
      sprintf(msg, "NOT-A-VALID-CMD");
      ReplyToCmd(msg);
    }
  }

  return;
}

void daqserver::ReadAllRegs(){

  for (int ii=0; ii<32; ii++) {
    printf("%s) Reading reg %d\n", __METHOD_NAME__, ii);
    ReadReg(ii);
  }

  return;
}

int daqserver::ReadReg(uint32_t regAddr) {
  int ret=0;
  uint32_t regCont=0; //FIX ME Shall be vector

  for (uint32_t ii=0; ii<det.size(); ii++) {
    ret |= (det.at(ii)->readReg(regAddr, regCont)<<ii);
    if (kVerbosity>-1) {
      printf("%s) Read from DE10 %d: %08x\n", __METHOD_NAME__, ii, regCont);
    }
  }
  return ret;
}

int daqserver::WriteReg(uint32_t regAddr, uint32_t regCont) {
  int ret=0;

  for (uint32_t ii=0; ii<det.size(); ii++) {
    ret |= (det.at(ii)->writeReg(regAddr, regCont)<<ii);
  }
  return ret;
}

int daqserver::updateReg(uint32_t regAddr, uint32_t regCont, uint32_t mask) {
  int ret=0;

  for (uint32_t ii=0; ii<det.size(); ii++) {
    ret |= (det.at(ii)->updateReg(regAddr, regCont, mask)<<ii);
  }
  return ret;
}

int daqserver::Init() {
  int ret = 0;

  for (uint32_t ii=0; ii<det.size(); ii++) {
    ret |= (det.at(ii)->Init())<<1;
    if (kVerbosity>0) {
      printf("%s) Init of DE10 %d\n", __METHOD_NAME__, ii);
    }
  }

  return ret;
}


//Read the events from all of the DE10 and write them in binary to the .dat file
int daqserver::recordEvents(FILE* fd) {
  
  int readRet = 0;
  int writeRet = 0;
  //std::vector<uint32_t*> evts(det.size(), "");
  uint32_t evtLen = 0;
  uint32_t evtLen_tot = 0;
  std::vector<uint32_t> evt(652);

  // FIX ME: at most 64 DE10
  std::bitset<64> replied{0};
  
  constexpr uint32_t header = 0xfa4af1ca;//FIX ME: this header must be done properly. In particular the real length (written by this master, not the one in the payload, after the SoP word) 
  bool headerWritten = false;

  // FIX ME: replace kStart with proper timeout
  do {
    for (uint32_t ii=0; ii<det.size(); ii++) {
      if(!replied[ii]){
	      det.at(ii)->AskEvent();
      }
    }    
    
    for (uint32_t ii=0; ii<det.size(); ii++) {
      if(!replied[ii]){
	      uint32_t readSingle = (det.at(ii)->GetEvent(evt, evtLen));
	      readRet += readSingle;
	      if(evtLen){
	        replied[ii] = true;
	      }    
	      evtLen_tot += evtLen;

	      // only write the header when the first board replies
	      if(replied.count() == 1 && !headerWritten){
	        ++nEvents;
	        fwrite(&header, 4, 1, fd);	  
	        headerWritten = true;
	      }
	      writeRet += fwrite(evt.data(), evtLen, 1, fd);

	      if (kVerbosity>0) {
	        printf("%s) Get event from DE10 %s\n", __METHOD_NAME__, addressdet[ii].c_str());
	        printf("  Bytes read: %d/%d\n", readSingle, evtLen);
	        printf("  Writes performed: %d/%lu\n", writeRet, det.size());
	      }
      }
    }
  } while (replied.count() && (replied.count() != det.size()) && kStart);

  //Everything is read and dumped to file
  if (evtLen_tot!=0) {
    if (readRet != (int)evtLen_tot || writeRet != (int)(det.size())) {
      printf("%s):\n", __METHOD_NAME__);
      printf("    Bytes read: %d/%u\n", readRet, evtLen_tot);
      printf("    Writes performed: %d/%d\n", writeRet, (int)(det.size()));
      return -1;
    }
  }
  else {
    if (kVerbosity>1) {
      printf("%s) total event lenght was 0\n", __METHOD_NAME__);
    }
  }
  return 0;
}

auto format_time_values = [](unsigned int val, size_t ndigits) {
    std::string sval = std::to_string(val);
    if (sval.length() < ndigits) {
        sval = std::string(ndigits - sval.length(), '0').append(sval);
    }
    return sval;
};

void daqserver::Start(char* runtype, uint32_t runnum, uint32_t unixtime) {
  //Open a file in the kdataPath folder and name it with UTC
  char dataFileName[255];

  auto format_human_date = [](uint32_t timel){
      // Construct human-readable date
      time_t time{timel};
      auto humanTime = *gmtime(&time);

      std::string dateTime;
      dateTime.append(std::to_string(humanTime.tm_year + 1900));
      dateTime.append(format_time_values(humanTime.tm_mon + 1, 2));
      dateTime.append(format_time_values(humanTime.tm_mday, 2));
      dateTime.append("_");
      dateTime.append(format_time_values(humanTime.tm_hour, 2));
      dateTime.append(format_time_values(humanTime.tm_min, 2));
      dateTime.append(format_time_values(humanTime.tm_sec, 2));

      return dateTime;
  };

  // // copy runtype and make it all UPPERCASE
  // std::string runtype_upper{runtype};
  // std::transform(begin(runtype_upper), end(runtype_upper), begin(runtype_upper), std::toupper);

  std::string humanDate = format_human_date(unixtime);
  sprintf(dataFileName,"%s/SCD_RUN%05d_%s_%s.dat", kdataPath.data(), runnum, runtype, humanDate.c_str());

  printf("%s) Opening output file: %s\n", __METHOD_NAME__, dataFileName);

  FILE* dataFileD;
  dataFileD = fopen(dataFileName,"w");
  if (dataFileD == nullptr) {
    printf("%s) Error: file %s could not be created. Do the data dir %s exist?\n", __METHOD_NAME__, dataFileName, kdataPath.data());
    return;
  }

  ResetBoards();
  SetMode(1);
  
  //Dump events to the file until Stop is received
  kStart = true;
  unsigned int lastNEvents = 0;
  using clock_type = std::chrono::system_clock;
  // using clock_type = std::chrono::high_resolution_clock;
  while(kStart) {
    usleep(200);
    auto start = clock_type::now();
    recordEvents(dataFileD);
    auto stop = clock_type::now();

    if(nEvents != lastNEvents){
      std::cout << "\rEvent " << nEvents << " last recordEvents took " << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << " us                            " << std::flush;
      lastNEvents = nEvents;
    }
  }
  std::cout << '\n';
  
  //Close the file and terminate thread
  fclose(dataFileD);
  printf("%s) File %s closed\n", __METHOD_NAME__, dataFileName);
}

void daqserver::Stop(uint32_t &_nEvts) {
  if(kStart){
    //FIX ME: metterci un while che fa N GetEvent()
    kStart = false;
    SetMode(0);
    maka->runStop(_nEvts);
    printf("%s) Events: %d \n", __METHOD_NAME__, _nEvts);
    runStop();
    sleep(10);
  }
  
  if (kVerbosity > 0) printf("%s) Run stopped succesfully\n", __METHOD_NAME__);
}

void daqserver::runStart(){
  printf("%s) Starting run on all detectors...\n", __METHOD_NAME__);
  for(auto de10 : det){
    de10->runStart();
  }
}

void daqserver::runStop(){
  printf("%s) Stopping run on all detectors...\n", __METHOD_NAME__);
  for(auto de10 : det){
    de10->runStop();
  }
}