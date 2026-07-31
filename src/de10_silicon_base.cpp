#include "de10_silicon_base.h"
#include "paperoProtocol.h"
#include "utility.h"
#include <unistd.h>

uint32_t okVal = 0xb01af1ca;
uint32_t badVal = 0x000cacca;

de10_silicon_base::de10_silicon_base(std::string address, uint32_t port, paperoConfig::configParams* params, int _calMode, int _intTrig, int verb):tcpclient(address.c_str(), (int)port, verb){
  uint32_t cmdLenReply = 1;

  
  //Copy the parameters from the config file
  detId         = params->id  & 0x0000FFFF;
  cmdlenght     = params->cmdLen;
  testUnitCfg   = (uint32_t)params->testUnitCfg & 0x00000003;
  testUnitEn    = (uint32_t)params->testUnitEn & 0x00000001;
  hkEn          = (uint32_t)params->hkEn & 0x00000001;
  dataEn        = (uint32_t)params->dataEn & 0x00000001;
  pktLen        = params->pktLen;
  intTrigPeriod = params->intTrigPeriod & 0xFFFFFFF8;
  feClkDuty     = (uint32_t)params->feClkDuty & 0x0000FFFF;
  feClkDiv      = (uint32_t)params->feClkDiv & 0x0000FFFF;
  adcClkDuty    = (uint32_t)params->adcClkDuty & 0x0000FFFF;
  adcClkDiv     = (uint32_t)params->adcClkDiv & 0x0000FFFF;
  trig2Hold     = (uint32_t)params->trig2Hold & 0x0000FFFF;
  adcFast       = (uint32_t)params->adcFast & 0x00000001;
  calEn         = (uint32_t)_calMode & 0x00000001;
  intTrigEn     = (uint32_t)_intTrig & 0x00000001;
  busyLen       = (uint32_t)params->busyLen & 0x0000FFFF;
  adcDelay      = (uint32_t)params->adcDelay & 0x0000FFFF;
  bias0         = params->bias0;
  bias1         = params->bias1;
  ideTest       = (uint32_t)params->ideTest & 0x00000001;
  chTest        = (uint32_t)params->chTest & 0x000000FF;
  daqMode       = params->daqMode & 0x00000003;
  lth           = params->lth;
  hth           = params->hth;
  // Preserve the historical default, then let daqserver override all policy
  // bits explicitly for every run-control command.
  eventEnable   = calEn == 0 ? 1u : 0u;
  autoCalib     = 0u;
  saveCalib     = calEn == 1 ? 1u : 0u;
  applyThresholds = 1u;

  //Send command length and set it with the loopback value
  //Cannot use specific function since it is the first time setting the length
  SendInt(cmdlenght);
  ReceiveInt(cmdLenReply);
  //Set cmd lenght with the detector reply (and check if they are the same)
  if ((uint32_t)cmdlenght != cmdLenReply){
    printf("%s) Detector has command length %d (requested: %d)\n",
            __METHOD_NAME__, cmdlenght, cmdLenReply);
    exit(1);
  }
  cmdlenght=cmdLenReply;//in number of char
  printf("%s) Set Cmd Lenght to %d\n", __METHOD_NAME__, cmdLenReply);  

  //
  ConfigureTestUnit(testUnitCfg);
  SetIntTriggerPeriod(intTrigPeriod);
  SetCalibrationMode(calEn);
  SelectTrigger(intTrigEn);
  SetTrig2Hold(trig2Hold);
  SetConfigBias();

  //Make sure system is NOT running
  SetMode(0);

  if (verbosity>0) {
    printf("%s) de10 silicon created\n", __METHOD_NAME__);
  }
}

//--------------------------------------------------------------

de10_silicon_base::~de10_silicon_base(){
}

//--------------------------------------------------------------

int de10_silicon_base::checkReply(const char* msg){
  uint32_t reply = 0;
  
  ReceiveInt(reply);
  if (reply!=okVal) {
    printf("%s) %s: ko\n", __METHOD_NAME__, msg);
    return 1;
  }
  
  return 0;
}

void de10_silicon_base::SetCmdLenght(int lenght) {
  uint32_t reply = 0;

  if (SendCmd("setCmdLength")==0) {
    SendInt((uint32_t)lenght);
  }

  ReceiveInt(reply);
  //let's set as cmd lenght not the one passed but the one received back (hoping they are equal)
  cmdlenght=reply;//in number of char
  printf("%s) Set Cmd Lenght to %d\n", __METHOD_NAME__, reply);

  return;
}

//--------------------------------------------------------------
int de10_silicon_base::readReg(int regAddr, uint32_t &regCont){

  int ret=0;
  if (SendCmd("readReg")==0) {
    SendInt((uint32_t)regAddr);
  }
  else {
    ret = 1;
  }
  
  if (ReceiveInt(regCont)<=0) ret = 1;

  if (verbosity>0) {
    printf("%s) Read: %d\n", __METHOD_NAME__, regCont);
  }
  
  return ret;
}

int de10_silicon_base::writeReg(int regAddr, uint32_t regCont){

  int ret=0;
  if (SendCmd("writeReg")==0) {
    SendInt((uint32_t)regAddr);
    SendInt((uint32_t)regCont);
  }
  else {
    ret = 1;
  }
  
  uint32_t reply = 0;
  ReceiveInt(reply);
  if (reply!=0) ret = 1;
  
  return ret;
}

int de10_silicon_base::updateReg(int regAddr, uint32_t regCont, uint32_t mask){

  int ret=0;
  uint32_t regContOld = 0;
  if (readReg(regAddr, regContOld)) {
    ret = 1;
  } else {
    regCont = (regCont & mask) | (regContOld & ~mask);
  }
  
  if (writeReg(regAddr, regCont)) {
    ret = 1;
  }

  return ret;
}

//FIX ME: use the proper functions or the 2D array to retrieve configurations
int de10_silicon_base::Init() {
  int ret=0;
  uint32_t regContent = 1;

  if (verbosity>0) {
    printf("%s) initializing (reset everything)\n", __METHOD_NAME__);
  }
  
  if (SendCmd("init")==0) {
    //Register 1
    regContent = (testUnitCfg << 8) | (hkEn << 6) \
      | (testUnitEn << 1) | dataEn;
    SendInt(regContent);
    
    //Register 2
    regContent = intTrigPeriod | (calEn<<1) | intTrigEn;
    SendInt(regContent);
    
    //Register 3
    regContent = detId;
    SendInt(regContent);
    
    //Register 4
    regContent = pktLen;
    SendInt(regContent);
    
    //Register 5
    regContent = (feClkDuty << 16) | feClkDiv;
    SendInt(regContent);
    
    //Register 6
    regContent = (adcClkDuty << 16) | adcClkDiv;
    SendInt(regContent);
    
    //Register 7
    regContent  = adcFast << 31 | ideTest << 24 | chTest << 16 | trig2Hold;
    SendInt(regContent);

    //Register 8
    regContent  = busyLen << 16 | adcDelay;
    SendInt(regContent);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Initializing");
  
  return ret;
}

int de10_silicon_base::SetTrig2Hold(uint32_t delayIn){
  int ret=0;
  trig2Hold = (delayIn & 0x0000FFFF);
  if (SendCmd("setDelay")==0) {
    SendInt(trig2Hold);
  }
  else {
    ret = 1;
  }

  ret += checkReply("Setting Delay");

  return ret;
}

int de10_silicon_base::SetMode(uint8_t modeIn) {
  int ret=0;

  if (modeIn == 0u) {
    mode = paperoProtocol::kStopCommand;
    if (SendCmd("stopAcquisition") != 0) {
      ret = 1;
    }
  }
  else {
    const uint32_t thresholds = paperoProtocol::PackThresholds(lth, hth);
    mode = paperoProtocol::BuildAcquisitionCommand(
      eventEnable != 0u,
      calEn != 0u,
      autoCalib != 0u,
      saveCalib != 0u,
      applyThresholds != 0u,
      daqMode);

    // PAPERO samples REG11 when REG0.RUN_REQUEST rises. Send both values to
    // the HPS in one command so it can preserve REG11 -> REG0 write order.
    if (SendCmd("startAcquisition") == 0) {
      SendInt(thresholds);
      SendInt(mode);
    }
    else {
      ret = 1;
    }
  }
  
  ret += checkReply("Setting Mode");
  return ret;
}

int de10_silicon_base::GetEventNumber() {
  int ret=0;
  uint32_t exttrigcount = 0;
  uint32_t inttrigcount = 0;

  if (SendCmd("getEventNumber")==0) {
    ReceiveInt(exttrigcount);
    ReceiveInt(inttrigcount);
  }
  else {
    ret = 1;
  }
  
  if (verbosity>0) {
    printf("%s) Event number: %d %d\n", __METHOD_NAME__, exttrigcount, inttrigcount);
  }
  return ret;
}

int de10_silicon_base::EventReset() {
  int ret = 0;
  if (SendCmd("eventReset")!=0) ret = 1;
  
  ret += checkReply("Resetting events (reinitialize)");

  return ret;
}

int de10_silicon_base::GetCalibrationValid(bool& valid) {
  uint32_t status = 0;
  const int ret = readReg(paperoProtocol::kCalibrationStatusRegister, status);
  valid = ret == 0 &&
          (status & paperoProtocol::kCalibrationValidMask) != 0u;
  if (verbosity > 0) {
    printf("%s) CAL_VALID=%u (status=%08x)\n", __METHOD_NAME__,
           valid ? 1u : 0u, status);
  }
  return ret;
}

int de10_silicon_base::GetRunIdle(bool& idle) {
  uint32_t status = 0;
  const int ret = readReg(paperoProtocol::kCalibrationStatusRegister, status);
  idle = ret == 0 && (status & paperoProtocol::kRunIdleMask) != 0u;
  if (verbosity > 1) {
    printf("%s) RUN_IDLE=%u (status=%08x)\n", __METHOD_NAME__,
           idle ? 1u : 0u, status);
  }
  return ret;
}

void de10_silicon_base::AskEvent(){
  SendCmd("getEvent");
}

//FIX ME: this doesn't reply as the others (i.e. 0=OK), but with the size it did read 
int de10_silicon_base::GetEvent(std::vector<uint32_t>& evt, uint32_t& evtLen){
  //Get the event from HPS and loop here until all data are read
  uint32_t evtRead = 0;
  ReceiveInt(evtLen);//in int units
  //  printf("%s) Event Lenght = %u\n", __METHOD_NAME__, evtLen);
  if (evt.size()<evtLen) evt.resize(evtLen);
  evtLen*=sizeof(uint32_t);//in byte units
  while (evtRead < evtLen) {
    //    printf("%s) %d %d %d\n", __METHOD_NAME__, evtRead/sizeof(uint32_t), evtLen, evtRead);
    evtRead += Receive(&evt[evtRead/sizeof(uint32_t)], evtLen-evtRead);
  }

  // if (evtLen) {
  //   printf("%s) %d %d %d\n", __METHOD_NAME__, evtRead/sizeof(uint32_t), evtLen, evtRead);
  //   printf("%s) Length: %d\n",__METHOD_NAME__, evtLen);
  //   for (uint32_t jj=0; jj<(evtLen/4); jj++) {
  //     printf("%s)******%d %08x\n",__METHOD_NAME__, jj, evt[jj]);
  //   }
  //  }
  return evtRead;
}

//TO DO: there will be another method, in future to really calibrate: put in cal mode, start the trigger, stop the calibration and let the system compute pedestals, sigmas, etc...
int de10_silicon_base::SetCalibrationMode(uint32_t calEnIn){
  int ret = 0;
  calEn = calEnIn & 0x00000001;
  if (SendCmd("calibrate")==0){
    SendInt(calEn<<1);
  }
  else {
    ret = 1;
  }

  ret += checkReply("Setting calibration mode");
  
  return ret;
}

int de10_silicon_base::WriteCalibPar(){
  //char readBack[LEN]="";
  //client_send("WriteCalibPar");
  //client_receive(readBack);
  printf("%s) FIX ME: do not yet implemented in HPS\n", __METHOD_NAME__);
  return 1;
}

int de10_silicon_base::SaveCalibrations(){
  //char readBack[LEN]="";
  //client_send("SaveCalibrations");
  //client_receive(readBack);
  printf("%s) FIX ME: do not yet implemented in HPS\n", __METHOD_NAME__);
  return 1;
}

int de10_silicon_base::SetIntTriggerPeriod(uint32_t intTrigPeriodIn){
  int ret=0;
  intTrigPeriod = intTrigPeriodIn & 0xFFFFFFF8;
  if (SendCmd("intTrigPeriod")==0) {
    SendInt(intTrigPeriod);
  }
  else {
    ret = 1;
  }

  ret += checkReply("Setting Internal Trigger Period");

  return ret;
}

int de10_silicon_base::SelectTrigger(uint32_t intTrigEnIn){
  int ret=0;
  intTrigEn = intTrigEnIn & 0x00000001;
  if (SendCmd("selectTrigger")==0) {
    SendInt(intTrigEn);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Selecting Trigger");

  return ret;
}

int de10_silicon_base::ConfigureTestUnit(uint32_t testUnitEnIn){
  int ret=0;
  testUnitEn = testUnitEnIn & 0x00000001;
  if (SendCmd("configTestUnit")==0) {
    SendInt(testUnitEn<<1);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Configuring Test Unit");

  return ret;
}

int de10_silicon_base::SetFeClk(uint32_t _feClkDuty, uint32_t _feClkDiv){
  int ret=0;
  feClkDuty = _feClkDuty & 0x0000FFFF;
  feClkDiv  = _feClkDiv  & 0x0000FFFF;
  if (SendCmd("setFeClk")==0) {
    SendInt((feClkDuty << 16) | feClkDiv);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Setting FE clk");

  return ret;
}

int de10_silicon_base::SetAdcClk(uint32_t _adcClkDuty, uint32_t _adcClkDiv){
  int ret=0;
  adcClkDuty = _adcClkDuty & 0x0000FFFF;
  adcClkDiv  = _adcClkDiv  & 0x0000FFFF;
  if (SendCmd("setAdcClk")==0) {
    SendInt((adcClkDuty << 16) | adcClkDiv);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Setting ADC clk");

  return ret;
}

int de10_silicon_base::SetIdeTest(uint32_t _ideTest, uint32_t _chTest){
  int ret=0;
  ideTest = _ideTest & 0x00000001;
  chTest  = _chTest & 0x000000FF;
  if (SendCmd("setIdeTest")==0) {
    SendInt(ideTest << 24 | chTest << 16);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Setting IDE1140 Test Mode");

  return ret;
}

int de10_silicon_base::SetAdcFast(uint32_t _adcFast){
  int ret=0;
  adcFast = _adcFast & 0x00000001;
  if (SendCmd("setAdcFast")==0) {
    SendInt(adcFast << 31);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Setting ADC Fast Mode");

  return ret;
}

int de10_silicon_base::SetBusyLen(uint32_t _busyLen){
  int ret=0;
  busyLen = _busyLen & 0x0000FFFF;
  if (SendCmd("setBusyLen")==0) {
    SendInt(busyLen << 16);
  }
  else {
    ret = 1;
  }
  
  ret += checkReply("Setting Busy Length");

  return ret;
}

int de10_silicon_base::SetAdcDelay(uint32_t _adcDelay){
  int ret=0;
  adcDelay = _adcDelay & 0x0000FFFF;
  if (SendCmd("setAdcDelay")==0) {
    SendInt(adcDelay);
  }
  else {
    ret = 1;
  }

  ret += checkReply("Setting ADC Delay");
  
  return ret;
}

int de10_silicon_base::SetConfigBias(){
  return SetBias(bias0, bias1);
}

int de10_silicon_base::SetBias(string _bias0, string _bias1){
  int ret=0;
  
  if (SendCmd("setBias")==0) {
    Send(_bias0.c_str());
    Send(_bias1.c_str());
  }
  else {
    ret = 1;
  }

  ret += checkReply("Setting Biases");
  
  return ret;
}

int de10_silicon_base::GetBiasCurr(float& _curr0, float& _curr1, uint8_t& _flags){
  int ret=0;
  
  if (SendCmd("getBiasCurr")==0) {
    Receive(&_curr0, sizeof(_curr0));
    Receive(&_curr1, sizeof(_curr1));
    Receive(&_flags, sizeof(_flags));
  }
  else {
    ret = 1;
  }

  ret += checkReply("Bias Currents Read");
  
  return ret;
}

int de10_silicon_base::runStart() {
  int ret = 0;
  if (SendCmd("runStart")!=0) {
    ret = 1;
  }
  ret += checkReply("Starting Run");
  return ret;
}

int de10_silicon_base::runStop() {
  int ret = 0;

  if (SendCmd("runStop")!=0) {
    ret = 1;
  }
  
  ret += checkReply("Stopping Run");
  
  return ret;
}
