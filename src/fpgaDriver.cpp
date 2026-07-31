#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include <iostream>
#include "hwlib.h"
#include <string.h>


#include "hps_0.h"
#include "user_avalon_fifo_regs.h"

#include "utility.h"
#include "fpgaDriver.h"
#include "axiFifo.h"
#include "paperoProtocol.h"


fpgaDriver::fpgaDriver(int verbose){
  kVerbose     = verbose;
  
  printf("Opening /dev/mem...\n");
	int fd;
	if((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return;
	}

  printf("Mapping FPGA resources...\n");
	virtualBase = mmap(NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ),
                      MAP_SHARED, fd, HW_REGS_BASE);
	if(virtualBase == MAP_FAILED) {
		printf("ERROR: mmap() failed...\n");
		close(fd);
		return;
	}
  
  //Base address of the RegisterArray address: shifts are in units of bytes
  raAddr = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + REGADDR_PIO_BASE) & (unsigned long)(HW_REGS_MASK)));
  //Base address of the RegisterArray readback: shifts are in units of bytes
  raCont = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + REGCONTENT_PIO_BASE) & (unsigned long)(HW_REGS_MASK)));

  //FIXME fetch the GW version from gitlab and not from FPGA
  ReadReg(rGW_VER, &kGwV);

  //Instantiate the three FIFOs
  confFifo = new axiFifo(virtualBase, FIFO_HPS_TO_FPGA_IN_BASE,
                          FIFO_HPS_TO_FPGA_IN_CSR_BASE, 3, 1000, 0);
  hkFifo = new axiFifo(virtualBase, FIFO_FPGA_TO_HPS_OUT_BASE,
                        FIFO_FPGA_TO_HPS_OUT_CSR_BASE, 3, 1000, 0);
  dataFifo = new axiFifo(virtualBase, FAST_FIFO_FPGA_TO_HPS_OUT_BASE,
                          FAST_FIFO_FPGA_TO_HPS_OUT_CSR_BASE,
                          paperoProtocol::kFastFifoAlmostEmpty,
                          paperoProtocol::kFastFifoAlmostFull, 0);

  if (kVerbose > 3) {
    printf("FIFO Status post init:\n");
    confFifo->Status();
    hkFifo->Status();
    dataFifo->Status();
  }

  //Stop triggers (if any) and reset FPGA
  SetMode(0);
  ResetFpga();
};

fpgaDriver::~fpgaDriver(){

};

uint8_t fpgaDriver::Parity32(uint32_t dataIn){
  uint8_t parity = 0;
  uint8_t nibbles;
  for (int ii = 0; ii < 4; ii++){
    nibbles = (dataIn >> ii*8) & 0xFF;
    parity = parity | ((Parity8(nibbles))<<ii);
  }
  return parity;
}

uint32_t fpgaDriver::CrcUpdate(uint32_t crc, const void* data, size_t data_len){
  const unsigned char* d = (const unsigned char*)data;
  unsigned int i;
  bool bit;
  unsigned char c;
  while (data_len--) {
    c = d[data_len];
    for (i = 0; i < 8; i++) {
        bit = crc & 0x80000000;
        crc = (crc << 1) | ((c >> (7 - i)) & 0x01);
        if (bit) {
            crc ^= 0x04c11db7;
        }
    }
    crc &= 0xffffffff;
  }
  return crc & 0xffffffff;
}

uint32_t fpgaDriver::CrcFinalize(uint32_t crc){
  unsigned int i;
  bool bit;
  for (i = 0; i < 32; i++) {
    bit = crc & 0x80000000;
    crc <<= 1;
    if (bit) {
        crc ^= 0x04c11db7;
    }
  }
  return crc & 0xffffffff;
}

void fpgaDriver::ReadReg(int regAddr, uint32_t* data){
	//Write the address of the register to be read
	*raAddr = regAddr;
	//Read the register content
	*data = *raCont;
  if(kVerbose > 2){
    printf("%s) ReadREG: Register addr: %d - content: %08x\n", __METHOD_NAME__, regAddr, *data);
  }
}

void fpgaDriver::SingleWriteReg(uint32_t regAddr, uint32_t regContent){
  uint32_t singleWrite[2];
  singleWrite[0] = regContent;
  singleWrite[1] = regAddr;
  WriteReg(singleWrite, 2);
}

int fpgaDriver::WriteReg(uint32_t* pktContent, int pktLen){
  uint32_t packet[pktLen+6];
  uint8_t parityMsb, parityLsb;
  uint32_t pktCrc;

  //Create the packet header
  pktCrc = CrcInit();
  packet[0] = REG_SOP;
  packet[1] = (uint32_t)pktLen+5;
  packet[2] = kGwV; //@todo fetch the GW version from github and not from FPGA
  pktCrc = CrcUpdate(pktCrc, &packet[2], sizeof(uint32_t));
  packet[3] = REG_HDR1;
  pktCrc = CrcUpdate(pktCrc, &packet[3], sizeof(uint32_t));

  //Create the packet body
  for(int ii=0; ii<pktLen; ii=ii+2){
    parityLsb = Parity32(pktContent[ii]);
    parityMsb = Parity32(pktContent[ii+1]);
    packet[ii+4] = pktContent[ii];
    packet[ii+5] = ((uint32_t)parityMsb<<28) | ((uint32_t)parityLsb<<24) | pktContent[ii+1];
    pktCrc = CrcUpdate(pktCrc, &packet[ii+4], sizeof(uint32_t));
    pktCrc = CrcUpdate(pktCrc, &packet[ii+5], sizeof(uint32_t));
  }

  //Create the packet footer
  packet[pktLen+4] = REG_EOP;
  pktCrc = CrcFinalize(pktCrc);
  packet[pktLen+5] = pktCrc;

  if (kVerbose > 3){
    printf("%s) WriteReg: Packet Content:\n", __METHOD_NAME__);
    for (int jj=0; jj<pktLen+6;jj++){
      printf("%08x\n", packet[jj]);
    }
  }

  //Send the packet
  confFifo->writeChunck(packet, pktLen+6);

  return 0;
}

void fpgaDriver::ResetFpga(){
	uint32_t data[4096];
	int flushErr=0;
	//Set to high the regArray bits of reset
	SingleWriteReg((uint32_t)rGOTO_STATE, 0x00000003);

	//Flush the FastData Fifo
	flushErr = dataFifo->readChunk(data, 0, true);
	if(kVerbose > 0) printf("%s) Flushed %d words from DATA FIFO\n", __METHOD_NAME__, flushErr);
  dataPacketPending = false;
  dataPacketLength = 0;
  flushErr = hkFifo->readChunk(data, 0, true);
	if(kVerbose > 0) printf("%s) Flushed %d words from HK FIFO\n", __METHOD_NAME__, flushErr);

	//Remove regArray reset
	SingleWriteReg((uint32_t)rGOTO_STATE, 0x00000000);
}

void fpgaDriver::ResetCounters(){
  SingleWriteReg((uint32_t)rGOTO_STATE,
                 paperoProtocol::kCounterResetBit);
  SingleWriteReg((uint32_t)rGOTO_STATE,
                 paperoProtocol::kStopCommand);
}

void fpgaDriver::InitFpga(uint32_t* regsContentIn, uint32_t opLen){
  //Configure the whole regArray (except register rGOTO_STATE)
  WriteReg(regsContentIn, opLen);
  
	//Reset the FPGA
	ResetFpga();
}

void fpgaDriver::SetDelay(uint32_t delayIn){
	uint32_t regContent;
	ReadReg(rMSD_PARAM, &regContent);
	regContent = (regContent & 0xFFFF0000) | (delayIn & 0x0000ffff);
	SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::SetMode(uint32_t modeIn){
  SingleWriteReg(rGOTO_STATE, modeIn);
}

void fpgaDriver::StartAcquisition(uint32_t command, uint32_t thresholds){
  if ((command & paperoProtocol::kThresholdBit) != 0u) {
    // PAPERO samples REG11 together with the other fields on the
    // REG0.RUN_REQUEST rising edge. Keep both writes in one ordered packet.
    uint32_t runConfig[4] = {
      thresholds,
      rTHR_PARAM,
      command | paperoProtocol::kRunRequestBit,
      rGOTO_STATE
    };
    WriteReg(runConfig, 4);
  }
  else {
    SingleWriteReg(rGOTO_STATE,
                   command | paperoProtocol::kRunRequestBit);
  }
}

void fpgaDriver::StopAcquisition(){
  SingleWriteReg(rGOTO_STATE, paperoProtocol::kStopCommand);
}

void fpgaDriver::GetEventNumber(uint32_t* extTrigCount, uint32_t* intTrigCount){
	ReadReg(rEXT_TRG_COUNT, extTrigCount);
	ReadReg(rINT_TRG_COUNT, intTrigCount);
}

void fpgaDriver::Calibrate(uint32_t calibIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (regContent & 0xFFFFFFFD) | (calibIn & 0x00000002);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::intTriggerPeriod(uint32_t periodIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (periodIn & 0xFFFFFFF0) | (regContent & 0x0000000F);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::selectTrigger(uint32_t intTrigIn){
	uint32_t regContent;
	ReadReg(rTRIGBUSY_LOGIC, &regContent);
	regContent = (regContent & 0xFFFFFFFE) | (intTrigIn & 0x00000001);
	SingleWriteReg(rTRIGBUSY_LOGIC, regContent);
}

void fpgaDriver::configureTestUnit(uint32_t tuCfg){
	uint32_t regContent;
	ReadReg(rUNITS_EN, &regContent);
	regContent = (regContent & 0xFFFFFCFD) | (tuCfg & 0x00000302);
	SingleWriteReg(rUNITS_EN, regContent);
}

void fpgaDriver::setFeClk(uint32_t _feClkParams){
  SingleWriteReg(rFE_CLK_PARAM, _feClkParams);
}

void fpgaDriver::setAdcClk(uint32_t _adcClkParams){
  SingleWriteReg(rADC_CLK_PARAM, _adcClkParams);
}

void fpgaDriver::setIdeTest(uint32_t _ideTest){
  uint32_t regContent;
  ReadReg(rMSD_PARAM, &regContent);
  regContent = (regContent & 0xFE00FFFF) | (_ideTest & 0x01FF0000);
  SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::setAdcFast(uint32_t _adcFast){
  uint32_t regContent;
  ReadReg(rMSD_PARAM, &regContent);
  regContent = (regContent & 0x7FFFFFFF) | (_adcFast & 0x80000000);
  SingleWriteReg(rMSD_PARAM, regContent);
}

void fpgaDriver::setBusyLen(uint32_t _busyLen){
  uint32_t regContent;
  ReadReg(rBUSYADC_PARAM, &regContent);
  regContent = (regContent & 0x0000FFFF) | (_busyLen & 0xFFFF0000);
  SingleWriteReg(rBUSYADC_PARAM, regContent);
}

void fpgaDriver::setAdcDelay(uint32_t _adcDelay){
  uint32_t regContent;
  ReadReg(rBUSYADC_PARAM, &regContent);
  regContent = (regContent & 0xFFFF0000) | (_adcDelay & 0x0000FFFF);
  SingleWriteReg(rBUSYADC_PARAM, regContent);
}

void fpgaDriver::biasTranslate(float biasIn, uint32_t &dacOut) {
  // LT3482: Vctrl in the range 0-1.371V corresponding to 0-90V output. If Vctrl > 1.5, Vout = 90V
  // R1 = R2*(Vo/Vctrl - 1), R1 = 1MOhm, R2 = 15kOhm
  // Vo = (R1/R2 -1)*Vctrl = 66.666667*Vctrl
  // Vctrl = Vo/66.667 = 0.015228*Vo
  // 
  // LTC1663: DAC 10-bit resolution, Vref = 2.5V
  // Vout = DAC/(2^Nbits-1)*Vref = (DAC/1023)*2.5V = 0.002444*DAC
  // DAC = Vout/Vref*(2^Nbits-1)
  //
  // Vout = Vctrl
  // DAC = Vctrl/Vref*(2^Nbits-1) = Vctrl*1023/2.5 = 409.2*Vctrl
  // Vctrl = 0.015228 Vout -> DAC = 0.015228*Vout*1023/2.5 = 6.231472*Vout
  //
  // 70V -> 1.066V -> 436 , 0x1B4; 50V -> 0.761V -> 312, 0x138;
  const float kConv = (1023/2.5)*(15e3/1e6-15e3);
  dacOut = (uint32_t)(biasIn*kConv) & 0x000003FF;
}

void fpgaDriver::biasCtrl(float bias0, float bias1) {
  uint32_t bias0Dac = 0;
  uint32_t bias1Dac = 0;
  biasTranslate(bias0, bias0Dac);
  biasTranslate(bias1, bias1Dac);

  // Write values
  uint32_t regCont = (((bias1Dac&0x000003FF)<<16) | (bias0Dac&0x000003FF));
  SingleWriteReg((uint32_t)rBIAS_PARAM, regCont);
  
  // Assert DAC write request for both Biases
  SingleWriteReg((uint32_t)rBIAS_PARAM, (0x80008000) | regCont);
  
  // Dessert DAC write request
  SingleWriteReg((uint32_t)rBIAS_PARAM, (0x00000000) | regCont);
}

void fpgaDriver::biasCurrTranslate(uint32_t _currADC, float& _currA) {
  //LT3482: Iout:Imon 5:1; Imon converted with R6
  //  Iout=(5/R6)⋅Vmon
  //LTC2312: internal reference: Vmax = 4.096 V. LSB: 1 mV
  //  ADC=Vin/LSB
  //
  //Combined: Iout=(5⋅LSB/R6)⋅ADC
  //  LSB=1 mV; R6=20 k;
  //  Iout[mA] = ADC/4000
  const float kConv = 1/4000;
  _currA = ((_currADC)&0x00000FFF)*kConv*1e-3;
}

void fpgaDriver::biasCurrRead(float& _curr0, float& _curr1, uint8_t& _flags) {
  //rBIAS_CURR_MON reg content
  //     31 - LT1663 Ack 1
  //     30 - LT1663 Err 1
  //     29 - LTC2312 Busy
  //     28 - LTC2312 Done
  //[27:16] - Bias Current 1
  //     15 - LT1663 Ack 0
  //     14 - LT1663 Err 0
  // [11:0] - Bias Current 0;

  uint32_t regContent = 0;
  //Read reg
  ReadReg(rBIAS_CURR_MON, &regContent);
  //Populate flags
  _flags = ((regContent&0xF0000000)>>24)|((regContent&0x0000C000)>>12);
  
  //Convert current and update port
  biasCurrTranslate((regContent&0x00000FFF), _curr0);
  biasCurrTranslate((regContent&0x0FFF0000)>>16, _curr1);
}

int fpgaDriver::getEvent(std::vector<uint32_t>& evt, int* evtLen){
  *evtLen = 0;

  // HEF compressed packets have a variable length.  Consume the two-word
  // header as soon as it is available, then wait for precisely the advertised
  // remainder instead of relying on a RAW-packet almost-empty threshold.
  if (!dataPacketPending) {
    if (dataFifo->getUsedw() < 2u) {
      return 0;
    }

    uint32_t sopWord = 0;
    uint32_t pktLen = 0;
    if (dataFifo->read(&sopWord) < 0 || dataFifo->read(&pktLen) < 0) {
      fprintf(stderr, "Error reading event header\n");
      return -1;
    }
    if (sopWord != DATA_SOP) {
      fprintf(stderr, "First value of event not SoP: %08x\n", sopWord);
      return -1;
    }
    if (pktLen < 12u ||
        pktLen > paperoProtocol::kHefMaxPacketLength) {
      fprintf(stderr, "Invalid HEF packet length: %u\n", pktLen);
      return -2;
    }

    dataPacketPending = true;
    dataPacketLength = pktLen;
  }

  const uint32_t remainingWords = dataPacketLength - 1u;
  if (dataFifo->getUsedw() < remainingWords) {
    if (kVerbose > 4) {
      printf("%s) Waiting for HEF packet: %u/%u words available\n",
             __METHOD_NAME__, dataFifo->getUsedw(), remainingWords);
    }
    *evtLen = 0;
    return 0;
  }

  evt.resize(dataPacketLength + 1u);
  evt[0] = DATA_SOP;
  evt[1] = dataPacketLength;
  const int wordsRead =
      dataFifo->readChunk(&evt[2], remainingWords, false);
  if (wordsRead != static_cast<int>(remainingWords)) {
    fprintf(stderr, "Error in reading event\n");
    return -3;
  }

  if (kVerbose > 4){
    printf("%s) Event:\n", __METHOD_NAME__);
    for(uint32_t i = 0; i < dataPacketLength + 1u; i++){
      printf("%08x\n", evt[i]);
    }
  }

  *evtLen = static_cast<int>(dataPacketLength + 1u);
  dataPacketPending = false;
  dataPacketLength = 0;

  return 0;
}
