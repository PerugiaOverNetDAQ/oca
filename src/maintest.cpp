#include "daqclient.h"
#include <stdio.h>
#include <unistd.h>

#include "utility.h"

const char* addressdaq = "localhost";
const int portdaq = 9999;

int verbosity=0;

int main(int argc, char *argv[]) {


  //------------------------------------------------
  char readBack[LEN]="";
  static const int length=16;
  char* command_string=new char[2*length];
  //------------------------------------------------
  
  if (verbosity>0) {
    printf("--------------------------------\n");
    printf("testOCA:\n");
    printf("--------------------------------\n");
  }
  
  daqclient* daq = new daqclient(addressdaq, portdaq, verbosity);
  daq->SetCmdLenght(32);
  
  daq->SendCmd("Init");
  //  daq->Receive(readBack, 32);//is blocking and this is wanted
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, readBack);

  sleep(3);

  daq->SendCmd("Wait");
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, readBack);

  // sleep(3);
  
  //  const char* start ="FF80000800000000EE00000100000000";
  //  uint32_t start[4] = {0xFF800008, 0x00000000, 0xEE000001, 0x00000000};
  uint32_t start[4] = {0x080080FF, 0x00000000, 0x010000EE, 0x00000000};
  daq->Send((void*)start, 4*sizeof(uint32_t));
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  hex2string(readBack,length,command_string);
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, command_string);
  
  sleep(30);

  //  const char* stop  ="FF80000800000000EE00000000000000";
  //  uint32_t stop[4] = {0xFF800008, 0x00000000, 0xEE000000, 0x00000000};
  uint32_t stop[4] = {0x080080FF, 0x00000000, 0x000000EE, 0x00000000};
  daq->Send((void*)stop, 4*sizeof(uint32_t));
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  hex2string(readBack,length,command_string);
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, command_string);
  
  sleep(3);
  
  daq->SendCmd("fava");
  
  return 0;
}
