#ifndef AXIFIFO_H
#define AXIFIFO_H

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"

#include "hps_0.h"
#include "user_avalon_fifo_regs.h"


/*!
  AXI FIFO properties and methods
  
  The pointer arithmetics below relies on the type (void*, char*, int*, etc...) of the pointers:
    - if the pointer is void* summing 1 (pointer + 1) means actually summing 4 (since the compiler knows that you have to move of 4 bytes)
    - if the pointer is char* summing 1 (pointer + 1) means actually summing 1 (since the compiler knows that you have to move of 1 bytes)
  This arithmetics must be compatible with the offset defined
*/
class axiFifo {
  protected:
    uint32_t* addr; //!< Base address (corresponds to data or q)
    uint32_t* CsrAddr; //!< CSR address
    uint32_t* UsedwAddr; //!< Used-words address
    uint32_t* StatusAddr; //!< Status address
    
    uint32_t* AFullReg; //!< Almost-full threshold register
    uint32_t* AEmptyReg; //!< Almost-empty threshold register
    uint32_t* EventReg; //!< Events register
    uint32_t* iEnableReg; //!< Interrupt-Enable register

  public:
    axiFifo(void* virtualBase, uint32_t address, uint32_t csr, uint32_t aEmptyThr, uint32_t aFullThr, uint8_t interruptEn);

    //!< Return the number of used words
    inline uint32_t getUsedw () {
      return *UsedwAddr;
    };

    //!< Return the full flag
    inline bool getFull () {
     return (*StatusAddr & ALTERA_AVALON_FIFO_STATUS_F_MSK) && 1;
    };

    //!< Return the almost-full flag
    inline bool getAFull () {
    	return (*StatusAddr & ALTERA_AVALON_FIFO_STATUS_AF_MSK) && 1;
    };

    //!< Return the empty flag
    inline bool getEmpty () {
    	return (*StatusAddr & ALTERA_AVALON_FIFO_STATUS_E_MSK) && 1;
    };

    //!< Return the almost-empty flag
    inline bool getAEmpty () {
    	return (*StatusAddr & ALTERA_AVALON_FIFO_STATUS_AE_MSK) && 1;
    };

    //!< Return the overflow flag
    inline bool getOverFlow () {
    	return (*EventReg & ALTERA_AVALON_FIFO_EVENT_OVF_MSK) && 1;
    };

    //!< Reset overflow flag
    inline void resetOverflow () {
      *EventReg &= ALTERA_AVALON_FIFO_EVENT_OVF_MSK;
    };

    /*!
      FIFO initialization: Set aFull and aEmpty thresholds, disable interrupts, and reset events register
      @param aEmptyThr Almost-empty threshold in number of words
      @param aFullThr Almost-full threshold in number of words
      @param interruptEn Interrupt enable as required by Intel
    */
    void init (uint32_t aEmptyThr, uint32_t aFullThr, uint8_t interruptEn);

    /*!
      Write single word
      @param data Data to be written
      @return -1 if error, number of written bytes
    */
    int write(uint32_t* data);

    /*!
      Write a chunk of data of length len
      @param data Data to be written
      @param len Amount of bytes to be written
      @return -1 if full, number of written bytes
    */
    int writeChunck(uint32_t* data, int len);

    /*!
      Read single word
      @param data Memory where to write
      @return -1 if empty, number of read bytes
    */
    int read(uint32_t* data);

    /*!
      Read a chunk of data of length len
      @param data Memory where to write
      @param len Amount of bytes to read
      @return -1 if words are not enough, number of read bytes
    */
    int readChunk(uint32_t* data, int lenChunk, bool flush);

    /*
      Print the FIFO status
    */
    void Status();
};

#endif
