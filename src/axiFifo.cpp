#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>


#include "utility.h"
#include "axiFifo.h"


axiFifo::axiFifo(void* virtualBase, uint32_t address, uint32_t csr, uint32_t aEmptyThr, uint32_t aFullThr, uint8_t interruptEn){
  //Base Data and CST addresses: the shifts are in units of bytes
  addr = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + address) & (unsigned long)(HW_REGS_MASK)));
  CsrAddr = (uint32_t*)((unsigned long)virtualBase + ((unsigned long)(ALT_LWFPGASLVS_OFST + csr) & (unsigned long)(HW_REGS_MASK)));
  // The shifts are in units of 4-bytes
  UsedwAddr = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_LEVEL_REG;
  StatusAddr = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_STATUS_REG;

  AFullReg = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_ALMOSTFULL_REG;
  AEmptyReg = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_ALMOSTEMPTY_REG;
  EventReg = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_EVENT_REG;
  iEnableReg = CsrAddr + (unsigned long)ALTERA_AVALON_FIFO_IENABLE_REG;

  init(aEmptyThr, aFullThr, interruptEn);
}

void axiFifo::init (uint32_t aEmptyThr, uint32_t aFullThr, uint8_t interruptEn) {
  *AEmptyReg	= aEmptyThr;
  *AFullReg	  = aFullThr;
  *iEnableReg = interruptEn & ALTERA_AVALON_FIFO_IENABLE_ALL_MSK;
  *EventReg		= ALTERA_AVALON_FIFO_EVENT_ALL_MSK;
  return;
};

int axiFifo::write(uint32_t* data){
  if (getFull()) {
    return -1;
  }
  *addr = *data;
  return 4;
};

int axiFifo::writeChunck(uint32_t* data, int len){
  if (getFull()) {//FIXME: Should check the A-Full before writing
    return -1;
  }
  
  for (int i=0; i<len; i++){
    *addr = data[i];
  }
  return len;
};

int axiFifo::read(uint32_t* data){
  if (getEmpty()) {
    return -1;
  }
  *data = *addr;
  return 4;
};

int axiFifo::readChunk(uint32_t* data, int lenChunk, bool flush){
  int len = lenChunk;
  uint32_t actualLevel = getUsedw();

  if (flush == true) {
    len = (uint32_t)actualLevel;
  }

  if ((uint32_t)len > actualLevel) {
    //if(baseAddr.verbose > 3){
    //  printf("%s) Tried to read %d, while fifo level was %d\n", __METHOD_NAME__, _len, (int)(actualLevel));
    //  uint32_t regContent;
    //  ReadReg(21, &regContent);
    //  printf("Register 21: %08x\n", regContent);
    //  ReadReg(22, &regContent);
    //  printf("Register 22: %08x\n", regContent);
    //}
    return (-1);
  } else {
    for (int i=0; i<len; i++){
      data[i] = *addr;
    }
  };
  return len;
  
};

void axiFifo::Status() {
	printf("%s)\n Full: %u\t AFull: %u\nEmpty: %u\tAEmpty: %u\nUsedw: %u\nSettings:\nAFull: %u\tAEmpty: %u\n", __METHOD_NAME__,
	        getUsedw(), getFull(), getEmpty(), getAFull(), getAEmpty(),
          *AFullReg, *AEmptyReg);
};