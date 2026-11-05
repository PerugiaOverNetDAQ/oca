#include <cstdio>
//#include <stdlib.h>
//#include <string.h>
#include <unistd.h>
//#include <thread>
//#include <netinet/in.h>
#include <fcntl.h>
//#include <sys/poll.h>
//#include <sys/ioctl.h>
#include <sys/mman.h>
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include <iostream>
#include <thread>

#include "hps_0.h"
#include "user_avalon_fifo_regs.h"
#include "fpgaDriver.h"
#include "hpsDataServer.h"
#include "hpsServer.h"

fpgaDriver*    fpga           = nullptr;
hpsDataServer* hpsDataStream  = nullptr;
hpsServer*     hpsSrv         = nullptr;

void closePapero(){
  if (hpsDataStream) delete hpsDataStream;
  if (hpsSrv) delete hpsSrv;
  if (fpga) delete fpga;
}

int main(int argc, char *argv[]){
  std::cout<<"hash="<<GIT_HASH<<", time="<<COMPILE_TIME<<", branch="<<GIT_BRANCH<<std::endl;
  
  if (argc < 4) {
    printf("Usage:\n\t%s <socket port> <verbosity level> <MAKA connection enable>\n", argv[0]);
    return 0;
  }
  int sockPort = atoi(argv[1]);
  int verbosityIn = atoi(argv[2]);
  int makaEn = atoi(argv[3]);

  //---- Debug of int/double/bool dimensions -----------------------------------
  /* sleep(3); */

  /* int pippoint[2] = { 10, 11}; */
  /* double pippodouble[2] = { 4.3, 5.4}; */
  /* bool pippobool[2] {true, false}; */
  /* printf("int: %p %p\n", &pippoint[0], &pippoint[1]); */
  /* printf("double: %p %p\n", &pippodouble[0], &pippodouble[1]); */
  /* printf("bool: %p %p\n", &pippobool[0], &pippobool[1]); */

  /* printf("%d %d\n", *(&pippoint[0]), *(&pippoint[0]+1)); */
  /* printf("%d %d\n", *(&pippoint[0]), *((int*)(((char*)&pippoint[0])+1))); */
  //----------------------------------------------------------------------------
  
  //Instantiate the FPGA driver as a global variable
  fpga = new fpgaDriver(verbosityIn);
  uint32_t piumone = 0;
  fpga->ReadReg(rPIUMONE, &piumone);
  printf("\n/*--- GateWare SHA: %08x ----------------------*/\n", fpga->getkGwV());
  printf("/*--- Piumone (it must be 0xC1A0C1A0): %08x ---*/\n\n", piumone);

  //Setup the data-stream socket with consecutive port (wrt to config socket)
  if (makaEn > 0) {
    printf("Creating a TCP Server Socket for Data Stream...\n");
    hpsDataStream = new hpsDataServer(sockPort+1, verbosityIn);
  }

  //Connect to the configuration socket and loop forever to receive commands
  printf("Creating a TCP Server Socket for Configuration...\n");
  hpsSrv = new hpsServer(sockPort, verbosityIn);

  //Accept client connections and receive commands, until client is closed
  while (1) {
    hpsSrv->ListenCmd();
  }

  //Everything done, cleanly close PAPERO
  closePapero();

  return 0;
}
