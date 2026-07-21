#include "de10_silicon_base.h"
#include "utility.h"
#include <unistd.h>

namespace {
// PAPERO REG0 command layout.  The DAQ mode occupies REG0[25:24]; thresholds
// travel separately in REG11 and are sampled on the RUN_REQUEST rising edge.
constexpr uint32_t kRunRequestMask = 1u << 4;
constexpr uint32_t kEventEnableMask = 1u << 16;
constexpr uint32_t kForceCalibrationMask = 1u << 17;
constexpr uint32_t kThresholdValidMask = 1u << 18;
constexpr uint32_t kAutoCalibrationMask = 1u << 19;
constexpr uint32_t kSaveCalibrationMask = 1u << 20;
constexpr uint32_t kDaqModeShift = 24;
// FPGA readback registers are exposed by the HPS driver with an offset of 16.
constexpr int kCalibrationStatusRegister = 27;
constexpr uint32_t kCalibrationValidMask = 1u << 0;
constexpr uint32_t kRunIdleMask = 1u << 1;
}

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
  ideTest       = (uint32_t)params->ideTest & 0x00000001;
  chTest        = (uint32_t)params->chTest & 0x000000FF;
  daqMode       = params->daqMode & 0x00000003; // REG0[25:24].
  lth           = params->lth & 0x0000FFFF;     // REG11[15:0].
  hth           = params->hth & 0x0000FFFF;     // REG11[31:16].
  // Initial state is either the historical CAL-only or event-only mode.  The
  // DAQ server overrides this explicitly for every received run command.
  eventEnable   = calEn == 0 ? 1 : 0;
  autoCalib     = 0;
  saveCalib     = calEn == 1 ? 1 : 0;
  applyThresholds = 1;

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
  if (modeIn == 0) {
    // Clearing REG0 terminates whichever command was latched at START.
    mode = 0;
    if (SendCmd("stopAcquisition")!=0) {
      ret = 1;
    }
  }
  else {
    // CAL and EVENT_ENABLE are independent: CAL=1/EVENT=1 implements run MIX.
    // DAQ mode affects normal events only; PAPERO forces the calibration tables
    // through the LadderWrapper RAW path while calibration is active.
    const uint32_t thresholds = (hth << 16) | lth;
    mode = kRunRequestMask |
           (eventEnable == 1 ? kEventEnableMask : 0) |
           (calEn == 1 ? kForceCalibrationMask : 0) |
           (applyThresholds == 1 ? kThresholdValidMask : 0) |
           (autoCalib == 1 ? kAutoCalibrationMask : 0) |
           (saveCalib == 1 ? kSaveCalibrationMask : 0) |
           (daqMode << kDaqModeShift);

    // HPS writes REG11 before REG0 so PAPERO cannot latch stale thresholds.
    if (SendCmd("startAcquisition")==0) {
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
  
  ret += checkReply("Resetting event counters");

  return ret;
}

int de10_silicon_base::GetCalibrationValid(bool& valid) {
  uint32_t status = 0;
  const int ret = readReg(kCalibrationStatusRegister, status);
  valid = ret == 0 && (status & kCalibrationValidMask) != 0u;
  if (verbosity > 0) {
    printf("%s) CAL_VALID=%u (status=%08x)\n", __METHOD_NAME__,
           valid ? 1u : 0u, status);
  }
  return ret;
}

int de10_silicon_base::GetRunIdle(bool& idle) {
  uint32_t status = 0;
  const int ret = readReg(kCalibrationStatusRegister, status);
  idle = ret == 0 && (status & kRunIdleMask) != 0u;
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

// Cache the calibration request used by the next REG0 command and keep the
// historical TRIGBUSY calibration bit synchronized for compatible firmware.
// The actual calibration sequence starts later, atomically, in SetMode(1).
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