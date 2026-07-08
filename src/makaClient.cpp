/*!
  @file makaClient.cpp
  @brief Client to interface MAKA merger
  @author Mattia Barbanera (mattia.barbanera@infn.it)
*/

#include"makaClient.h"

makaClient::makaClient(const char *_address, int _port, int _verb, int _cmdLen) : tcpclient(_address, _port, _verb){
  printf("%s) Setting up connection with MAKA...\n",__METHOD_NAME__);
  cmdLenHandshake(_cmdLen);
  printf("... connection done.\n");
}

makaClient::~makaClient(){

}

void makaClient::cmdLenHandshake(int _cmdLen){
  uint32_t cmdLenReply;
  SendInt(_cmdLen);
  ReceiveInt(cmdLenReply);

  //Set cmd lenght with the detector reply (and check if they are the same)
  if ((uint32_t)_cmdLen != cmdLenReply){
    printf("%s) Detector has command length %d (requested: %d)\n",
            __METHOD_NAME__, _cmdLen, cmdLenReply);
    exit(1);
  }
  
  SetCmdLenght(cmdLenReply);
  
  printf("%s) Set Cmd Lenght to: %d\n", __METHOD_NAME__, cmdLenReply);  

}

int makaClient::setup(string _dataPath, vector<bool> _makaEn, vector<uint32_t> _detIds,\
                        vector<uint32_t> _detPorts, vector<string> _detAddrs,\
                        bool _dataToFile, bool _dataToOm, uint32_t _omPreScale){
  //PAPERO opens the socket on OCAport+1
  vector<uint32_t> makaIds;
  vector<uint32_t> makaPorts;
  vector<string> makaAddrs;
  for (size_t ii=0; ii<_detPorts.size(); ii++) {
    if (_makaEn[ii]) {
      makaIds.push_back(_detIds[ii]);
      makaPorts.push_back(_detPorts[ii]+1);
      makaAddrs.push_back(_detAddrs[ii]);
    }
  }


  configPacket* cp = new configPacket(makaIds, makaPorts, makaAddrs, _dataPath,\
                                      _dataToFile, _dataToOm, _omPreScale);
  
  printf("%s) Configurations to be sent:\n", __METHOD_NAME__);
  cp->dump();
  
  //Send command and packet
  if (SendCmd("setup")==0) {
    int temp = 0;
    SendInt(cp->pktLen);
    printf("%s) Setup sent\n", __METHOD_NAME__);
    //Tx configurations
    cp->ser();
    temp = Send(cp->msg, cp->pktLen);
    printf("Sent %u bytes\n", temp);
  }
  else {
    printf("%s) Error in setup\n", __METHOD_NAME__);
    return 1;
  }
  delete cp;

  int ret = checkReply("Setting Up");

  return ret;
}

int makaClient::runStart(std::string _runType, uint32_t _runNum, uint32_t _runTime){
  startPacket* sp = new startPacket(_runType, _runNum, _runTime);
  printf("%s) Configurations to be sent:\n", __METHOD_NAME__);
  sp->dump();

  //Tx command
  if (SendCmd("runStart")==0) {
    printf("%s) Starting merger...\n", __METHOD_NAME__);
    SendInt(sp->pktLen);
    //Tx configurations
    sp->ser();
    Send(sp->msg, sp->pktLen);
  }
  else {
    return 1;
    delete sp;
  }
  delete sp;
  
  int ret = checkReply("Starting Run");
  
  return ret;
}

int makaClient::runStop(uint32_t& _nEvts){

  //Tx command
  if (SendCmd("runStop")!=0) {
    return 1;
  }
  printf("%s) Stopping merger...\n", __METHOD_NAME__);

  ReceiveInt(_nEvts);
  printf("%s)        Events: %d \n", __METHOD_NAME__, _nEvts);

  int ret = checkReply("Stopping Run");
  
  return ret;
}

int makaClient::checkReply(const char* msg){
  uint32_t reply = 0;
  ReceiveInt(reply);
  if (reply!=kOkVal) {
    printf("%s) %s: ko\n", __METHOD_NAME__, msg);
    return 1;
  }
  
  return 0;
}