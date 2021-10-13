﻿#include "de10_silicon_base.h"
#include "utility.h"

uint32_t okVal = 0xb01af1ca;
uint32_t badVal = 0x000cacca;

de10_silicon_base::de10_silicon_base(const char *address, int port, int verb):tcpclient(address, port, verb){
  //Initialize and compute configurations
  testUnitCfg = 0;
  hkEn = 0;
  //  ConfigureTestUnit(0);
  dataEn = 1;
  //  SetIntTriggerPeriod(0x02faf080);
  //  SetCalibrationMode(0);
  //  SelectTrigger(0);
  pktLen = 0x0000028A;
  feClkDuty  = 0x00000004;
  feClkDiv   = 0x00000028;
  adcClkDuty = 0x00000004;
  adcClkDiv  = 0x00000002;
  //  SetDelay(0x00000145);
  //  SetMode(0);
  detId = 0x000000E3;

  changeText("hello");
  if (verbosity>0) {
    printf("%s) de10 silicon created\n", __METHOD_NAME__);
  }
}

//--------------------------------------------------------------

de10_silicon_base::~de10_silicon_base(){
}

//--------------------------------------------------------------
int de10_silicon_base::readReg(int regAddr, uint32_t &regCont){

  int ret=0;
  if (SendCmd("readReg")>0) {
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
  uint32_t reply = 1;

  if (verbosity>0) {
    printf("%s) initializing (reset everything)\n", __METHOD_NAME__);
  }
  
  if (SendCmd("init")>0) {
    //Register 1
    regContent = (testUnitCfg&0x00000003) << 8 | (hkEn&0x00000001) << 6 \
      | testUnitEn | (dataEn&0x00000001);
    SendInt(regContent);
    
    //Register 2
    regContent = intTrigPeriod|calEn|intTrigEn;
    SendInt(regContent);
    
    //Register 3
    regContent = detId&0x000000FF;
    SendInt(regContent);
    
    //Register 4
    regContent = pktLen;
    SendInt(regContent);
    
    //Register 5
    regContent = ((feClkDuty&0x0000FFFF)<<16) | (feClkDiv&0x0000FFFF);
    SendInt(regContent);
    
    //Register 6
    regContent = ((adcClkDuty&0x0000FFFF)<<16) | (adcClkDiv&0x0000FFFF);
    SendInt(regContent);
    
    //Register 7
    regContent  = delay;
    SendInt(regContent);
  }
  else {
    ret = 1;
  }
  
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }
  
  return ret;
}

int de10_silicon_base::SetDelay(uint32_t delayIn){
  int ret=0;
  uint32_t reply = 1;
  delay = (delayIn & 0x0000FFFF);
  if (SendCmd("setDelay")>0) {
    SendInt(delay);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }
  return ret;
}

int de10_silicon_base::SetMode(uint8_t modeIn) {
  int ret=0;
  uint32_t reply = 1;
  mode=(modeIn << 4)&0x00000010;
  if (SendCmd("setMode")>0) {
    SendInt(mode);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }  
  return ret;
}

int de10_silicon_base::GetEventNumber() {
  int ret=0;
  if (SendCmd("getEventNumber")<=0) ret = 1;
  uint32_t exttrigcount = 0;
  ReceiveInt(exttrigcount);
  uint32_t inttrigcount = 0;
  ReceiveInt(inttrigcount);
  if (verbosity>0) {
    printf("%s) event number: %d %d\n", __METHOD_NAME__, exttrigcount, inttrigcount);
  }
  return ret;
}

int de10_silicon_base::EventReset() {
  int ret = 0;
  uint32_t reply = 1;
  if (SendCmd("eventReset")<=0) ret = 1;
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) Resetting events (reinitialize): %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }  
  return ret;
}

//FIX ME: this doesn't reply as the others (i.e. 0=OK), but with the size it did read 
int de10_silicon_base::GetEvent(std::vector<uint32_t>& evt, uint32_t& evtLen){

  SendCmd("getEvent");

  //Get the event from HPS and loop here until all data are read
  uint32_t evtRead = 0;
  ReceiveInt(evtLen);//in int units
  if (evt.size()<evtLen) evt.resize(evtLen);
  evtLen*=sizeof(uint32_t);//in byte units
  while (evtRead < evtLen) {
    evtRead += Receive(&evt[evtRead], evtLen-evtRead);
  }

  return evtRead;
}

//TO DO: there will be another method, in future to really calibrate: put in cal mode, start the trigger, stop the calibration and let the system compute pedestals, sigmas, etc...
int de10_silicon_base::SetCalibrationMode(uint32_t calEnIn){
  int ret = 0;
  uint32_t reply = 1;
  calEn = (calEnIn&0x00000001)<<1;
  if (SendCmd("calibrate")>0){
    SendInt(calEn);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) Resetting events (reinitialize): %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }  
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
  uint32_t reply = 1;
  intTrigPeriod = intTrigPeriodIn&0xFFFFFFF0;
  if (SendCmd("intTrigPeriod")>0) {
    SendInt(intTrigPeriod);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }
  return ret;
}

int de10_silicon_base::SelectTrigger(uint32_t intTrigEnIn){
  int ret=0;
  uint32_t reply = 1;
  intTrigEn = intTrigEnIn&0x00000001;
  if (SendCmd("selectTrigger")>0) {
    SendInt(intTrigEn);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }
  return ret;
}

int de10_silicon_base::ConfigureTestUnit(uint32_t testUnitEnIn){
  int ret=0;
  uint32_t reply = 1;
  testUnitEn = (testUnitEnIn&0x00000001)<<1;
  if (SendCmd("configTestUnit")>0) {
    SendInt(testUnitEn);
  }
  else {
    ret = 1;
  }
  ReceiveInt(reply);
  if (verbosity>0) {
    printf("%s) reply: %s\n", __METHOD_NAME__, reply==okVal?"ok":"ko");
  }
  return ret;
}
