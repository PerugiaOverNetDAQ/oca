#include "daqclient.h"
#include <stdio.h>
#include <unistd.h>

#include <ctime>
#include <iostream>

#include "utility.h"

const char* addressdaq = "localhost";
const int portdaq = 10000;

int verbosity=0;

int main(int argc, char *argv[]) {


  //------------------------------------------------
  char readBack[LEN]="";
  static const int length=16;
  char* command_string=new char[2*length+1];
  //------------------------------------------------
  
  if (verbosity>0) {
    printf("--------------------------------\n");
    printf("stopOCA:\n");
    printf("--------------------------------\n");
  }
  
  daqclient* daq = new daqclient(addressdaq, portdaq, verbosity);
  daq->SetCmdLenght(32);

  std::time_t timeNow = std::time(nullptr);
  uint32_t ts = (uint32_t)timeNow;
  uint32_t tsReord = (ts & 0x000000FF) << 24 | (ts & 0x0000FF00) << 8 | (ts & 0x00FF0000) >> 8 | (ts & 0xFF000000) >> 24;
  //std::cout << std::hex << timeNow << ": ts " << ts << " reordered " << tsReord << std::endl;

  std::cout << std::asctime(std::gmtime(&timeNow))
            << timeNow << " seconds since the Epoch\n";
  
  //uint32_t stop[4] = {0x080080FF, 0x01001500, 0x000000EE, 0x5B8D6161};
  uint32_t stop[4] = {0x080080FF, 0x01001500, 0x000000EE, tsReord};
  daq->Send((void*)stop, 4*sizeof(uint32_t));
  uint32_t nEvts = 0;
  daq->ReceiveInt(nEvts);
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  hex2string(readBack,length,command_string);

  // The server preserves the historical count-then-reply wire order even
  // when RUN_IDLE cannot be proven.  UINT32_MAX marks that fail-safe path.
  if (nEvts == 0xffffffffu) {
    fprintf(stderr, "%s) Stop failed: %s\n", __METHOD_NAME__, command_string);
    return 1;
  }

  printf("%s)        Events: %d \n", __METHOD_NAME__, nEvts);
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, command_string);
  
  return 0;
}
