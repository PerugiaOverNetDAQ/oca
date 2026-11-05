#include <signal.h>
#include "makaMerger.h"

makaMerger* maka = nullptr;
bool runnin = true;

void closeMaka(int signum){
  int killWait = 5;
  printf("\n%s) Killing MAKA in %u seconds...\n", __METHOD_NAME__, killWait);
  runnin = false;
  if(maka!=nullptr){
    maka->runStop(killWait);
    delete maka;
  }
  printf("... buondi\n");
  exit(signum);
}

int main(int argc, char *argv[]){
  std::cout<<"hash="<<GIT_HASH<<", time="<<COMPILE_TIME<<", branch="<<GIT_BRANCH<<std::endl;
  
  if (argc < 3) {
    printf("Usage:\n\tMAKA <socket port> <verbosity level>\n");
    return 0;
  }
  int sockPort = atoi(argv[1]);
  int verbosityIn = atoi(argv[2]);

  signal(SIGINT, closeMaka);

  printf("Creating a TCP Server Socket for configuration from OCA...\n");
  maka = new makaMerger(sockPort, verbosityIn);

  
  //Accept client connections and receive commands, until client is closed
  while (runnin) {
    maka->listenCmd();
  }

  //Everything done, cleanly close PAPERO
  closeMaka(0);

  return 0;
}
