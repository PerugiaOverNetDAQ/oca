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

int makaClient::setup(string _dataPath, vector<int> _detPorts,
                        vector<const char*> _detAddrs){
  if (SendCmd("setup")==0) {
    printf("%s) Setup sent\n", __METHOD_NAME__);
  }
  else {
    printf("%s) Error in setup\n", __METHOD_NAME__);
    return 1;
  }

  int ret = checkReply("Setting Up");
  
  return ret;

  ////#pragma pack(push,1)
  //#pragma pack(1)
  //struct setupPacket {
  //   int pktLen;
  //   int pathLen;
  //   int detNum;
  //   vector<int> ports;
  //   vector<const char*> addr;
  //   string path;
  //};
  ////#pragma pack(pop)
  ////}__attribute__((packed));
  //struct setupPacket sp;
  //uint32_t reply = 1;
  //
  //int detAddsSize = 0;
  //for (uint32_t ii=0; ii<_detAddrs.size(); ii++){
  //  detAddsSize += strlen(_detAddrs[ii]); //+1: \0 at the end
  //  printf("Length %u\n", strlen(_detAddrs[ii]));
  //}
  //
  ////Populate structure fields
  //sp.pathLen = _dataPath.length();
  ////sp.pathLen = _dataPath.capacity();
  //sp.detNum  = _detPorts.size();
  //sp.pktLen  = 3*sizeof(int) + sp.detNum*sizeof(int) + detAddsSize + sp.pathLen;
  //sp.ports   = _detPorts;
  //sp.addr    = _detAddrs;
  //sp.path    = _dataPath;
  //
  //printf("%s) Configurations to be sent:\n", __METHOD_NAME__);
  //printf("  Packet length:        %u\n",  sp.pktLen);
  //printf("  Path Length:          %u\n",  sp.pathLen);
  //printf("  Path:                 %s\n",  sp.path.c_str());
  //printf("  Number of detectors:  %u\n",  sp.detNum);
  //printf("  Size of detPorts:     %lu\n", sp.detNum*sizeof(int));
  //printf("  Size of detAddrs:     %u\n",  detAddsSize);
  //
  //
  //for (uint32_t ii=0; ii<sp.ports.size(); ii++){
  //  printf("  Detector Address %u:  %s\n", ii, sp.addr[ii]);
  //  printf("  Detector Port %u:     %u\n", ii, sp.ports[ii]);
  //}
  //printf("  Size of struct:     %u\n",  sizeof(sp));
  //
  //if (SendCmd("setup")==0) {
  //  int temp = 0;
  //  printf("%s) Setup sent\n", __METHOD_NAME__);
  //  //Tx length and configurations
  //  SendInt(sp.pktLen);
  //  temp = Send(&sp, sp.pktLen);
  //  printf("Sent %u bytes\n", temp);
  //}
  //else {
  //  return 1;
  //}
  //
  ////sprintf(msg, "%04u%04u%'-128s%", pktLen, _detNum, _dataPath);
  //
  ////Receive OkNok
  //ReceiveInt(reply);
  //if (verbosity>0) {
  //  printf("%s) reply: %s\n", __METHOD_NAME__, reply==kOkVal?"ok":"ko");
  //  return 1;
  //}
  //
  //return 0;
}

int makaClient::runStart(char* _runType, uint32_t _runNum, uint32_t _runTime){
  char msg[25];
  
  sprintf(msg, "%'- 8s%08x%08x", _runType, _runNum, _runTime);

  printf("%s) Message: |%s|%x|%x|\n", __METHOD_NAME__, _runType, _runNum, _runTime);
  printf("%s) Length: %lu - Message: |%s|\n", __METHOD_NAME__, sizeof(msg), msg);

  //Tx command
  if (SendCmd("runStart")==0) {
    printf("%s) Starting merger...\n", __METHOD_NAME__);
    //Tx configurations
    Send(msg, sizeof(char)*25);
  }
  else {
    return 1;
  }
  
  int ret = checkReply("Starting Run");
  
  return ret;
}

int makaClient::runStop(){

  //Tx command
  if (SendCmd("runStop")!=0) {
    return 1;
  }
  printf("%s) Stopping merger...\n", __METHOD_NAME__);

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