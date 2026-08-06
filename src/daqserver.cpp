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
#include <fstream>
#include <unordered_map>

#include "daqserver.h"
#include "makaClient.h"
#include "paperoProtocol.h"
#include "runControl.h"

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
  mode    = paperoProtocol::kStopCommand;
  addressdet.clear();
  portdet.clear();
  makaEn.clear();

}

daqserver::~daqserver(){
  StopInjectionMonitor();
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

void daqserver::StopInjectionMonitor(){
  injectionMonitorStop = true;
  //Attende la chiusura solo quando il thread esiste
  if (injectionMonitor.joinable()) {
    injectionMonitor.join();
  }
}

int daqserver::ArmInjection(const std::string& path){
  //Impedisce di sostituire i dati durante un run attivo
  if (kStart) {
    fprintf(stderr, "%s) Injection can only be armed while DAQ is stopped\n",
            __METHOD_NAME__);
    return 1;
  }
  //Chiude il monitor collegato alla preparazione precedente
  StopInjectionMonitor();

  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    fprintf(stderr, "%s) Cannot open injection file %s\n", __METHOD_NAME__, path.c_str());
    return 1;
  }

  //Verifica che il file contenga un numero intero di word, altrimenti c'è un problema
  const std::streamoff byteCount = input.tellg();
  if (byteCount <= 0 || (byteCount % sizeof(uint32_t)) != 0) {
    fprintf(stderr, "%s) Invalid injection file size: %lld bytes\n", __METHOD_NAME__, static_cast<long long>(byteCount));
    return 1;
  }

  input.seekg(0);
  std::vector<uint32_t> fileWords(
    static_cast<size_t>(byteCount) / sizeof(uint32_t));

  //Carica tutto il file in RAM server DAQ (OCA)
  if (!input.read(reinterpret_cast<char*>(fileWords.data()), byteCount)) {
    fprintf(stderr, "%s) Cannot read injection file %s\n", __METHOD_NAME__, path.c_str());
    return 1;
  }

  //Associa ogni detector id alla posizione della rispettiva DE10. Ogni DE10 riceve i propri dati
  std::unordered_map<uint32_t, size_t> detectorIndex;
  //Crea un vettore di payload separato per ogni DE10
  std::vector<std::vector<uint32_t>> payloads(det.size());
  for (size_t i = 0; i < det.size(); ++i) {
    detectorIndex[det[i]->GetDetId()] = i;
  }

  //Scorre tutte le word per trovare i pacchetti PAPERO nel file MAKA
  size_t position = 0u;
  while (position + paperoProtocol::kPayloadOffset <= fileWords.size()) {
    //Avanza di una word quando la posizione corrente non contiene il SOP
    //0xBABA1A9A, parametrizzato nel file .h
    if (fileWords[position] != paperoProtocol::kDataSop) {
      ++position;
      continue;
    }

    //Ricava la dimensione completa del pacchetto dalla lunghezza dichiarata
    const uint32_t packetLength = fileWords[position+1u];
    const size_t packetWords = static_cast<size_t>(packetLength) + 1u;

    //Scarta pacchetti fuori limite incompleti o privi del trailer
    //kHefPacketLength = 896 + 12 = 908
    //kHefMaxPacketLength = 1792 + 12 = 1804, con la mixed
    //trailer 0x0BEDFACE posizione SOP+lunghezza dichiarata-1
    if (packetLength < paperoProtocol::kHefPacketLength || packetLength > paperoProtocol::kHefMaxPacketLength || position + packetWords > fileWords.size() || fileWords[position + packetLength - 1u] != paperoProtocol::kDataTrailer) {
      ++position;
      continue;
    }

    //Trigger type e detector id da header PAPERO
    const uint32_t triggerType = fileWords[position+6u] & 0xffu;
    const uint32_t detectorId = fileWords[position+4u] >> 16;
    //Cerca la DE10 configurata per il detector trovato
    const auto found = detectorIndex.find(detectorId);
    //Accetta solo dati legacy (Da Prior.Enc) destinati a una DE10 configurata
    //Se ad esempio carico un file che ha prima le tabelle di calib, grazie al trig type non le accetto
    if ((triggerType == paperoProtocol::kLegacyTrigType ||
         triggerType == paperoProtocol::kMixedTrigType) &&
        found != detectorIndex.end()) {
      auto& output = payloads[found->second];
      //Ignora gli eventi che superano la dimensione della calibrazione
      if (output.size() < paperoProtocol::kInjectionWords) {
        //Seleziona la prima word del payload legacy
        const auto first = fileWords.begin() + position + paperoProtocol::kPayloadOffset;
        //Copia un payload completo nel vettore della DE10 trovata
        output.insert(output.end(), first, first + paperoProtocol::kHefPayloadWords);
      }
    }
    //Salta direttamente alla word successiva al pacchetto valido
    position += packetWords;
  }

  //Trasferisce ogni vettore alla DE10 corrispondente
  for (size_t i = 0; i < det.size(); ++i) {
    //Calcola quanti eventi completi sono disponibili per la board
    const size_t events = payloads[i].size() / paperoProtocol::kHefPayloadWords;
    printf("%s) DE10 %zu: loaded %zu/%u legacy events from %s\n", __METHOD_NAME__, i, events, paperoProtocol::kCalibrationEvents, path.c_str());
    //Segnala che il run potrà terminare con underflow
    if (events < paperoProtocol::kCalibrationEvents) {
      fprintf(stderr, "%s) DE10 %zu has too few injected events; " "the calibration will fall back to FE data\n", __METHOD_NAME__, i);
    }

    //Invia il payload alla memoria HPS della board corrente
    //Reminder RAM DDR HPS 1 GB
    //          payload injectionWords   10,5 MiB
    //config FIFO FPGA  1024 word 4 KiB
    //FIFO di iniezione FPGA 4096 word 16 KiB, possiamo in caso ingrandirla
    if (det[i]->PrepareInjection(payloads[i]) != 0) {
      //Annulla tutte le board quando una prep fallisce
      fprintf(stderr, "%s) Cannot prepare injection on DE10 %zu\n", __METHOD_NAME__, i);
      for (auto board : det) {
        board->SetInjectionEnable(0u);
        board->CancelInjection();
      }
      injectionArmed = false;
      return 1;
    }
  }

  injectionArmed = true;
  return 0;
}

int daqserver::DisarmInjection(){
  //Impedisce la cancellazione durante un run attivo
  if (kStart) {
    fprintf(stderr, "%s) Injection can only be disarmed while DAQ is stopped\n",  __METHOD_NAME__);
    return 1;
  }
  StopInjectionMonitor();
  int ret = 0;
  //Disabilita e cancella i dati su ogni board
  for (auto board : det) {
    board->SetInjectionEnable(0u);
    ret |= board->CancelInjection();
  }
  injectionArmed = false;
  return ret;
}

void daqserver::MonitorInjection(){
  //Ripete il controllo fino alla richiesta di arresto
  while (!injectionMonitorStop) {
    bool allFinished = !det.empty();
    bool failed = false;

    //Legge e unifica lo stato di tutte le board
    for (size_t i = 0; i < det.size(); ++i) {
      uint32_t status = 0u;
      //Mantiene il monitor attivo quando una lettura fallisce
      if (det[i]->GetInjectionStatus(status) != 0) {
        allFinished = false;
        continue;
      }
      //Rileva underflow o altri errori segnalati dalla FPGA
      failed = failed || (status & paperoProtocol::kInjectionFailedMask) != 0u;
      allFinished = allFinished && (status & (paperoProtocol::kInjectionDoneMask | paperoProtocol::kInjectionFailedMask)) != 0u;
    }

    if (failed) {
      //Disabilita la sorgente di iniezione dopo un errore
      fprintf(stderr, "%s) Injection underflow: inject=1 -> inject=0; " "restarting the calibration from FE data\n", __METHOD_NAME__);
      for (auto board : det) {
        board->SetInjectionEnable(0u);
      }
      injectionArmed = false;

      //Ferma il run su tutte le board
      SetMode(0);
      //Attende lo svuotamento delle pipeline 30s prima della ripartenza
      if (WaitForBoardsRunIdle(30000) != 0 || injectionMonitorStop) {
        return;
      }
      //Cancella i buffer sugli HPS
      for (auto board : det) {
        board->CancelInjection();
      }
      //Azzera i contatori prima della nuova calibrazione
      ResetBoards();
      //Riparte usando i dati prodotti dal frontend, e non più gli injected
      if (!injectionMonitorStop) {
        SetMode(1);
      }
      return;
    }

    //Disabilita il flusso di iniezione quando tutte le board hanno finito
    if (allFinished) {
      //Rimuove il bit di iniezione dai prossimi comandi di avvio
      for (auto board : det) {
        board->SetInjectionEnable(0u);
      }
      injectionArmed = false;
      printf("%s) Injected calibration completed: inject=0\n",
             __METHOD_NAME__);
      return;
    }
    //Attende cento millisecondi prima del controllo successivo
    usleep(100000);
  }
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

void daqserver::SetEventEnable(uint32_t enable){
  for (auto de10 : det) {
    de10->SetEventEnable(enable);
  }
}

void daqserver::SetAutoCalibration(uint32_t enable){
  for (auto de10 : det) {
    de10->SetAutoCalibration(enable);
  }
}

void daqserver::SetSaveCalibration(uint32_t enable){
  for (auto de10 : det) {
    de10->SetSaveCalibration(enable);
  }
}

void daqserver::SetApplyThresholds(uint32_t enable){
  for (auto de10 : det) {
    de10->SetApplyThresholds(enable);
  }
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

int daqserver::AllCalibrationsValid(bool& allValid){
  allValid = !det.empty();
  for (uint32_t ii = 0; ii < det.size(); ++ii) {
    bool boardValid = false;
    if (det[ii]->GetCalibrationValid(boardValid) != 0) {
      fprintf(stderr, "%s) Cannot read CAL_VALID from DE10 %u\n",
              __METHOD_NAME__, ii);
      return 1;
    }
    allValid = allValid && boardValid;
    if (kVerbosity > 0) {
      printf("%s) DE10 %u calibration is %s\n", __METHOD_NAME__, ii,
             boardValid ? "valid" : "not valid");
    }
  }
  return 0;
}

int daqserver::AllBoardsRunIdle(bool& allIdle){
  allIdle = !det.empty();
  for (uint32_t ii = 0; ii < det.size(); ++ii) {
    bool boardIdle = false;
    if (det[ii]->GetRunIdle(boardIdle) != 0) {
      fprintf(stderr, "%s) Cannot read RUN_IDLE from DE10 %u\n",
              __METHOD_NAME__, ii);
      return 1;
    }
    allIdle = allIdle && boardIdle;
  }
  return 0;
}

int daqserver::WaitForBoardsRunIdle(uint32_t timeoutMs){
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
  do {
    bool allIdle = false;
    if (AllBoardsRunIdle(allIdle) != 0) {
      return 1;
    }
    if (allIdle) {
      printf("%s) All PAPERO pipelines are drained\n", __METHOD_NAME__);
      return 0;
    }

    usleep(100000);
  } while (std::chrono::steady_clock::now() < deadline);

  fprintf(stderr, "%s) Timeout waiting for PAPERO RUN_IDLE\n",
          __METHOD_NAME__);
  return 1;
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
  
    //Azzera il buffer per garantire una stringa valida
    char msg[LEN] = "";

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
      //Inserisce il terminatore dopo i byte ricevuti
      msg[std::min<ssize_t>(readret, LEN-1)] = '\0';
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

    //Riconosce il comando che arma un file locale sul server DAQ
    if (strncmp(msg, "cmd=inject=1;path=", 18) == 0){
      //Restituisce il risultato della lettura e preparazione del file
      char reply[LEN] = "";
      snprintf(reply, sizeof(reply), "%s",
               ArmInjection(msg + 18) == 0 ?
                 "INJECT-ARMED" : "INJECT-ERROR");
      ReplyToCmd(reply);
    }
    //Riconosce il comando che cancella ogni dato di iniezione preparato
    else if (strcmp(msg, "cmd=inject=0") == 0){
      //Restituisce il risultato della cancellazione sulle board
      char reply[LEN] = "";
      snprintf(reply, sizeof(reply), "%s",
               DisarmInjection() == 0 ?
                 "INJECT-DISARMED" : "INJECT-ERROR");
      ReplyToCmd(reply);
    }
    else if (strcmp(msg, "cmd=Init") == 0){
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
          char tempStr[LEN] = "Already in START state. Ignoring last command.";
          printf("%s) %s\n", __METHOD_NAME__, tempStr);
          ReplyToCmd(tempStr);
          return;
        }

	      char runControlText[5] = "";
	      strncpy(runControlText, &cmdgroup[1][4], 4);
        char* controlEnd = nullptr;
        const unsigned long parsedControl =
          strtoul(runControlText, &controlEnd, 16);
        run_control::Command runCommand;
        if (controlEnd != runControlText + 4 || parsedControl > 0xffffu ||
            !run_control::Decode(static_cast<uint16_t>(parsedControl),
                                 runCommand)) {
          char tempStr[LEN] = "NOT-A-VALID-RUN-CONTROL";
          printf("%s) Invalid run-control field %s\n", __METHOD_NAME__,
                 runControlText);
          ReplyToCmd(tempStr);
          return;
        }

        const bool dumpOnly =
          runCommand.mode == run_control::RunMode::Dump;
        bool forceCalibration =
          runCommand.mode == run_control::RunMode::Cal ||
          runCommand.mode == run_control::RunMode::Mix;
        const bool autoCalibration =
          runCommand.mode == run_control::RunMode::Daq;
        const bool eventEnable =
          runCommand.mode == run_control::RunMode::Daq ||
          runCommand.mode == run_control::RunMode::Mix;

        if (runCommand.mode == run_control::RunMode::Daq) {
          bool allValid = false;
          if (AllCalibrationsValid(allValid) != 0) {
            char tempStr[LEN] = "CALIB-STATUS-READ-ERROR";
            ReplyToCmd(tempStr);
            return;
          }
          forceCalibration = !allValid;
          if (!allValid) {
            printf("%s) At least one board is not calibrated; "
                   "calibrating all boards before DAQ\n", __METHOD_NAME__);
          }
        }

        //Usa i dati di iniezione solo quando il run richiede una calibrazione reale
        const bool useInjection = injectionArmed.load() &&
                                  forceCalibration && !dumpOnly;
        //Aggiorna il bit di iniezione nel comando di ogni board
        for (auto board : det) {
          board->SetInjectionEnable(useInjection ? 1u : 0u);
        }

        SetCalibrationMode(forceCalibration ? 1u : 0u);
        SetAutoCalibration(autoCalibration ? 1u : 0u);
        SetSaveCalibration(
          (dumpOnly || runCommand.saveCalibration) ? 1u : 0u);
        SetApplyThresholds(dumpOnly ? 0u : 1u);
        SetEventEnable(eventEnable ? 1u : 0u);
        // User protocol: 0=internal, 1=external. The detector API retains
        // its historical inverse polarity: 1=internal, 0=external.
        if (!dumpOnly) {
          SelectTrigger(runCommand.externalTrigger ? 0u : 1u);
        }

	      char sruntype[32] = "";
        snprintf(sruntype, sizeof(sruntype), "%s",
                 run_control::Name(runCommand.mode));

        char srunnum[32] = "";
        strncpy(srunnum,  &cmdgroup[1][0], 4);
        char* ptr;
        uint32_t runnum = strtol(srunnum, &ptr, 16);
        uint32_t unixtime = strtol(cmdgroup[3], &ptr, 16);
        std::time_t t = unixtime;
        if (kVerbosity>0) {
          printf("control=%s (mode=%s, trigger=%s, tables=%s), "
                 "runnum=%s (%u), unixtime=%u (%s -> %s)\n",
                 runControlText, sruntype,
                 dumpOnly ? "n/a" :
                   (runCommand.externalTrigger ? "external" : "internal"),
                 (dumpOnly || runCommand.saveCalibration) ? "save" : "nosave",
                 srunnum, runnum, unixtime, cmdgroup[3],
                 asctime(localtime(&t)));
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
        if (useInjection) {
          //Chiude un eventuale monitor terminato ma ancora associato al thread
          StopInjectionMonitor();
          injectionMonitorStop = false;
          //Avvia il controllo asincrono dopo il comando di avvio delle board
          injectionMonitor = std::thread(&daqserver::MonitorInjection, this);
        }
        ReplyToCmd(msg);
      }
      else if(strcmp(stop,cmdgroup[2])==0) {//stop daq
        printf("%s) Stop()\n", __METHOD_NAME__);
        // ignore consecutive Start commands
        if(!kStart){
          char tempStr[LEN] = "Already in STOP state. Ignoring the command.";
          printf("%s) %s\n", __METHOD_NAME__, tempStr);
          ReplyToCmd(tempStr);
          return;
        }
        uint32_t nEvts = 0;
        if (Stop(nEvts) != 0) {
          // stopOCA reads the event-count word before the textual reply.
          nEvts = 0xffffffffu;
          Tx(&nEvts, sizeof(nEvts));
          char tempStr[LEN] = "STOP-DRAIN-ERROR";
          ReplyToCmd(tempStr);
          return;
        }
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
  std::vector<uint32_t> evt(paperoProtocol::kHefPacketWords);

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

int daqserver::Stop(uint32_t &_nEvts) {
  if(kStart){
    //Ferma il monitor prima di arrestare le board
    StopInjectionMonitor();
    SetMode(0);
    if (WaitForBoardsRunIdle(30000) != 0) {
      // REG0 already blocks new triggers, but HPS senders and MAKA must stay
      // alive. Keeping kStart true permits a later STOP retry.
      fprintf(stderr, "%s) STOP incomplete; HPS and MAKA remain active\n",
              __METHOD_NAME__);
      return 1;
    }
    kStart = false;

    // HPS stopRun performs its final FIFO reads while MAKA is connected.
    runStop();
    maka->runStop(_nEvts);
    printf("%s) Events: %d \n", __METHOD_NAME__, _nEvts);
    sleep(10);
  }
  
  if (kVerbosity > 0) printf("%s) Run stopped succesfully\n", __METHOD_NAME__);
  return 0;
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
