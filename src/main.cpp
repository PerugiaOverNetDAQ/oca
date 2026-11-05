#include <signal.h>
#include <unistd.h>
#include <iostream>

#include "anyoption.h"

#include "daqserver.h"
#include "utility.h"
#include "daqConfig.h"
#include "makaClient.h"

//!OCA configurations
daqConfig::configParams daqConf;

daqserver* daqsrv = nullptr;
makaClient* maka  = nullptr;

int StatusOn=0;
void PrintStatus(int dummy) {
  printf("%s) Status %d", __METHOD_NAME__, dummy);
  StatusOn=1;
  return;
}

int ControlOn=1;
void StopRun(int dummy) {
  printf("%s) Stopping %d", __METHOD_NAME__, dummy);
  ControlOn=0;
  daqsrv->StopListening();
  return;
}

struct commandLineOptions
{
  string ocaCfg;
  string paperoCfg;
  int verbose;
};

/*

*/
int commandLineParser(int argc, char *argv[], commandLineOptions &clOpt, vector<string> & args) {
  AnyOption *opt = new AnyOption();

  /* Set preferences  */
  // opt->noPOSIX(); /* do not check for POSIX style character options */
  opt->setVerbose(); /* print warnings about unknown options */
  opt->autoUsagePrint(true); /* print usage for bad options */

  /* Set the usage/help   */
  opt->addUsage("Usage: ");
  opt->addUsage("");
  opt->addUsage(" -h  --help		Prints this help ");
  opt->addUsage(" -v  --verbose <level>	Verbose level");
  opt->addUsage(" -p  --papero <path>	PAPERO config file");
  opt->addUsage(" -o  --oca <path>	OCA config file");
  opt->addUsage("");

  /* Set the option strings/characters */

  /* Options will be checked only on the command line and not in
   * option/resource file */
  opt->setCommandFlag("help", 'h'); /* a flag (takes no argument), supporting long and short form */
  opt->setCommandOption("verbose", 'v');  /* an option (takes an argument), supporting only long form */
  opt->setCommandOption("papero", 'p');
  opt->setCommandOption("oca", 'o');

  /* Process the commandline and resource file */

  /* go through the command line and get the options  */
  opt->processCommandArgs(argc, argv);

  /* print usage if no options */
  //if (!opt->hasOptions()) {
  //  opt->printUsage();
  //  delete opt;
  //  exit(1);
  //}

  /* Get the values */
  if (opt->getFlag("help") || opt->getFlag('h'))
  {
    opt->printUsage();
    exit(0);
  }
  if (opt->getValue('v') != NULL || opt->getValue("verbose") != NULL)
  {
    clOpt.verbose = stoi(opt->getValue('v'));
  }
  else
  {
    clOpt.verbose = 0;
  }
  if (opt->getValue('p') != NULL || opt->getValue("papero") != NULL)
  {
    clOpt.paperoCfg = opt->getValue('p');
  }
  else
  {
    clOpt.paperoCfg = "./config/papero.cfg";
  }

  if (opt->getValue('o') != NULL || opt->getValue("oca") != NULL)
  {
    clOpt.ocaCfg = opt->getValue('o');
  }
  else
  {
    clOpt.ocaCfg = "./config/oca.cfg";
  }
  
  /* Get the actual arguments after the options */
  for (int i = 0; i < opt->getArgc(); i++) {
    cout << opt->getArgv(i) << endl;
    args.push_back(opt->getArgv(i));
  }

  /* Done */
  delete opt;
  return 0;
}


int main(int argc, char *argv[]) {
  std::cout<<"---------------------------------------------------------------------"<<std::endl;
  std::cout<<"OCA hash="<<GIT_HASH<<", time="<<COMPILE_TIME<<", branch="<<GIT_BRANCH<<std::endl;
  std::cout<<"---------------------------------------------------------------------"<<std::endl;
  
  //Parse command line options
  vector<string> args;
  commandLineOptions opt;
  commandLineParser(argc, argv, opt, args);

  //Read OCA config file and get the parameters
  daqConfig daqConfig(opt.ocaCfg);
  daqConf = daqConfig.getParams();

  //Setup MAKA client side
  maka = new makaClient(daqConf.makaIpAddr.c_str(), daqConf.makaPort, opt.verbose,
                          daqConf.makaCmdLen);

  //Setup OCA client side
  //This must be done before `signal` handling
  //to avoid NULL daqsrv when StopRun function is invoked
  daqsrv = new daqserver(daqConf.portClient, opt.verbose, opt.paperoCfg);
  //Create and configure detectors
  daqsrv->SetUpConfigClients();

  //Wait for clients and servers to be ready
  sleep(5);

  //FIXME: this is not really working: it is killed as a standard CTRL-C
  //the param sent to StopRun is SIGTERM itself and we need that StopRun accepts a param even if cannot use it
  signal(SIGTERM,StopRun); //killing the PID of the process we call the function StopRun that exits the program in the right way
  signal(SIGINT,StopRun); // sending 'CTRL_C' the program exits in the right way
  signal(SIGQUIT,PrintStatus); //sending 'CTRL \' we print the numbers of taken events

  //When used like this, daqsrv always listen commands from the client and passes them to the detectors
  //FIXME:  Use the daqConf.listenClient for standalone OCA
  //        The best solution would be a daqmaster class that inherits from
  //        daqserver (or vicevers) and which is not a server
  daqsrv->ListenCmd();

  if (daqsrv) delete daqsrv;

  printf("%s) Exiting...\n", __METHOD_NAME__);

  return 0;
}
