#include "daqclient.h"
#include <stdio.h>
#include <unistd.h>

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <string>

#include "runControl.h"
#include "utility.h"

const char* addressdaq = "localhost";
const int portdaq = 10000;

int verbosity=0;

namespace {
void PrintUsage(const char* executable){
  printf("Usage:\n"
         "\t%s <cal|daq|mix|dump> <0|1|int|ext> <save|nosave> [runnum]\n"
         "\t%s dump [runnum]\n"
         "Def:\n"
         "\t%s <0|cal|1|beam|2|mix> [runnum]\n",
         executable, executable, executable);
}

bool ParseMode(const std::string& argument, run_control::RunMode& mode) {
  if (argument == "0" || argument == "cal" || argument == "CAL") {
    mode = run_control::RunMode::Cal;
    return true;
  }
  if (argument == "1" || argument == "daq" || argument == "DAQ" ||
      argument == "beam" || argument == "BEAM") {
    mode = run_control::RunMode::Daq;
    return true;
  }
  if (argument == "2" || argument == "mix" || argument == "MIX") {
    mode = run_control::RunMode::Mix;
    return true;
  }
  if (argument == "3" || argument == "dump" || argument == "DUMP") {
    mode = run_control::RunMode::Dump;
    return true;
  }
  return false;
}

bool ParseTrigger(const std::string& argument, bool& external) {
  if (argument == "0" || argument == "int" || argument == "internal") {
    external = false;
    return true;
  }
  if (argument == "1" || argument == "ext" || argument == "external") {
    external = true;
    return true;
  }
  return false;
}

bool ParseSave(const std::string& argument, bool& save) {
  if (argument == "save") {
    save = true;
    return true;
  }
  if (argument == "nosave") {
    save = false;
    return true;
  }
  return false;
}

// Old two-argument invocations keep their exact historical behavior.  DUMP is
// the only new short form because trigger source and NOSAVE have no meaning.
bool ParseLegacy(const std::string& argument, uint16_t& controlWord) {
  run_control::RunMode mode;
  if (!ParseMode(argument, mode)) {
    return false;
  }
  if (mode == run_control::RunMode::Cal) {
    controlWord = 0x0000u; // internal + save
  } else if (mode == run_control::RunMode::Daq) {
    controlWord = 0x0002u; // external + nosave
  } else if (mode == run_control::RunMode::Mix) {
    controlWord = 0x0001u; // internal + save
  } else {
    controlWord = run_control::Encode(run_control::RunMode::Dump, false, true);
  }
  return true;
}

bool ParseRunNumber(const char* argument, unsigned int& runNumber) {
  const std::string text(argument);
  // Leading zeroes remain decimal (for example 00021 -> 21).  Hexadecimal is
  // accepted only with the explicit 0x/0X prefix documented by startOCA.
  const bool hasHexPrefix = text.size() > 2 && text[0] == '0' &&
                            (text[1] == 'x' || text[1] == 'X');
  char* end = nullptr;
  const unsigned long parsed =
    std::strtoul(argument, &end, hasHexPrefix ? 16 : 10);
  if (end == argument || *end != '\0' || parsed > 0xffffu) {
    return false;
  }
  runNumber = static_cast<unsigned int>(parsed);
  return true;
}
}

int main(int argc, char *argv[]) {
  uint16_t controlWord = 0;
  int runNumberArgument = -1;

  if (argc == 2 || argc == 3) {
    if (!ParseLegacy(argv[1], controlWord)) {
      PrintUsage(argv[0]);
      return 1;
    }
    if (argc == 3) runNumberArgument = 2;
  } else if (argc == 4 || argc == 5) {
    run_control::RunMode mode;
    bool external = false;
    bool save = false;
    if (!ParseMode(argv[1], mode) ||
        !ParseTrigger(argv[2], external) ||
        !ParseSave(argv[3], save) ||
        (mode == run_control::RunMode::Dump && !save)) {
      PrintUsage(argv[0]);
      return 1;
    }
    controlWord = run_control::Encode(mode, external, save);
    if (argc == 5) runNumberArgument = 4;
  } else {
    PrintUsage(argv[0]);
    return 1;
  }

  unsigned int runnum = 21;
  if (runNumberArgument >= 0) {
    if (!ParseRunNumber(argv[runNumberArgument], runnum)) {
      printf("Invalid run number: %s (expected 0..65535)\n",
             argv[runNumberArgument]);
      return 1;
    }
  }

  //------------------------------------------------
  char readBack[LEN]="";
  static const int length=16;
  char command_string[2*length+1] = "";
  //------------------------------------------------

  if (verbosity>0) {
    printf("--------------------------------\n");
    printf("startOCA:\n");
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

  //71616b23
  //uint32_t start[4] = {0x080080FF, 0x01001500, 0x010000EE, 0x236B6171};
  // The packet is sent as four native little-endian uint32_t values.  Reorder
  // both 16-bit fields explicitly so the server sees RRRRCCCC in bytes 4..7.
  uint32_t start[4] = {
    0x080080FF,
    run_control::PackStartWord(static_cast<uint16_t>(runnum), controlWord),
    0x010000EE,
    tsReord
  };
  daq->Send((void*)start, 4*sizeof(uint32_t));
  daq->ReceiveCmdReply(readBack);//is blocking and this is wanted
  hex2string(readBack,length,command_string);
  printf("%s) Read from DAQ: %s\n", __METHOD_NAME__, command_string);
      
  return 0;
}
