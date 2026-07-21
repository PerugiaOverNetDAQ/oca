#ifndef _FPGA_DRIVER_H_
#define _FPGA_DRIVER_H_

//config fifo constants
#define REG_SOP  0xAA55CA5A
#define REG_HDR1 0x4EADE500
#define REG_EOP  0xBADC0FEE

//Scientific data constants
#define DATA_SOP 0xBABA1A9A

//Register array constants
#define rGOTO_STATE       0
#define rUNITS_EN         1
#define rTRIGBUSY_LOGIC   2
#define rDET_ID           3
#define rPKT_LEN          4
#define rFE_CLK_PARAM     5
#define rADC_CLK_PARAM    6
#define rMSD_PARAM        7
#define rBUSYADC_PARAM    8
#define rTHR_PARAM        11
#define rGW_VER           16
#define rINT_TS_MSB       17
#define rINT_TS_LSB       18
#define rEXT_TS_MSB       19
#define rEXT_TS_LSB       20
#define rWARNING          21
#define rBUSY             22
#define rEXT_TRG_COUNT    23
#define rINT_TRG_COUNT    24
#define rFDI_FIFO_NUMWORD 25
#define rCALIB_STATUS     27
#define rPIUMONE          31

// REG0 bits shared with paperoPackage.vhd.  REG0 is a level-sensitive command:
// PAPERO latches the remaining fields on RUN_REQUEST 0->1 and stops on 1->0.
#define REG0_COUNTER_RESET_MASK  (1u << 1)
#define REG0_RUN_REQUEST_MASK    (1u << 4)
#define REG0_THRESHOLD_VALID_MASK (1u << 18)

// rCALIB_STATUS fields (FPGA readback register 11 + HPS offset 16).
#define CALIB_STATUS_VALID_MASK   (1u << 0)
#define CALIB_STATUS_RUN_IDLE_MASK (1u << 1)

#include <inttypes.h>
#include <vector>
#include "axiFifo.h"

/*!
  Driver to the interface to the FPGA
  Interface to the FIFOs and registers of the FPGA
*/
class fpgaDriver {
  protected:
    int       kVerbose; //!< Verbosity level
    void*     virtualBase; //!< Base of the virtual address
    uint32_t* raAddr; //!< Register to address the register array
    uint32_t* raCont; //!< Content of the register array addressed register
    
    uint32_t kGwV; //!< Gateware version

    //!< Compute the parity of an incoming unsigned 8-bit (gcc specific)
    inline bool Parity8(uint8_t dataIn){
      return __builtin_parity(dataIn&0x000000FF);
    };


  public:
    axiFifo* confFifo = nullptr;
    axiFifo* hkFifo   = nullptr;
    axiFifo* dataFifo = nullptr;

    fpgaDriver(int verbose);
    ~fpgaDriver();

    uint32_t getkGwV(){return kGwV;};

    //!< Compute 4 parity bits for a 32-bit unsigned
    uint8_t Parity32(uint32_t dataIn);

    //!< Initialize the CRC-32 to the start value
    uint32_t CrcInit(){
      return 0x46af6449;
    }

    //!< Update the CRC-32 (polynomial representation: 0x04C11DB7)
    uint32_t CrcUpdate(uint32_t crc, const void *data, size_t data_len);

    //!< Finalize CRC-32
    uint32_t CrcFinalize(uint32_t crc);

    //!< Read a register of the register array from the corresponding PIO_core
    void ReadReg(int regAddr, uint32_t *data);

    //!< Write a single register of the array via the CONFIG FIFO
    void SingleWriteReg(uint32_t regAddr, uint32_t regContent);

    //!< Write multiple registers of the array via the CONFIG FIFO
    int WriteReg(uint32_t * pktContent, int pktLen);

    //!< Reset the FPGA modules
    void ResetFpga();

    //!< Reset only acquisition counters; do not flush transport or calibration data
    void ResetCounters();
    
    //!< Initialize the register array and reset the FPGA modules
    void InitFpga(uint32_t* regsContentIn, uint32_t opLen);
    
    /*!
      Set the wait time, in number of clock cycles, between trigger and hold deassertion for the IDE1140
    */
    void SetDelay(uint32_t delayIn);
    
    //!< Configure the FPGA mode: Stop (0), Run (1)
    void SetMode(uint32_t modeIn);

    /*!< Write REG11 first, then assert REG0.RUN_REQUEST with the complete
         command in the same ordered configuration-FIFO transaction. */
    void StartAcquisition(uint32_t command, uint32_t thresholds);

    //!< Clear REG0, producing the RUN_REQUEST falling edge that stops the command.
    void StopAcquisition();
    
    //!< Retrieve the internal and external trigger counters
    void GetEventNumber(uint32_t* extTrigCount, uint32_t* intTrigCount);
    
    //!< Reset event counters without resetting calibration memories
    void EventReset(){ResetCounters();};
    
    //!< Activate the calibration mode
    void Calibrate(uint32_t calibIn);
    
    //!< Update the internal trigger period (without changing other configs)
    void intTriggerPeriod(uint32_t periodIn);
    
    //!< Enable/Disable the internal trigger
    void selectTrigger(uint32_t intTrigIn);
    
    //!< Configure and enable/disable the test unit
    void configureTestUnit(uint32_t tuCfg);

    //!< Configure FE clock
    void setFeClk(uint32_t _feClkParams);
    
    //!< Configure ADC clock
    void setAdcClk(uint32_t _adcClkParams);

    //!< Configure IDE1140 Test port
    void setIdeTest(uint32_t _ideTest);

    //!< Configure AD7276 Fast Mode
    void setAdcFast(uint32_t _adcFast);

    //!< Configure Artificial Busy Lenght
    void setBusyLen(uint32_t _busyLen);

    //!< Configure delay between FE falling edge and AD conversion start
    void setAdcDelay(uint32_t _adcDelay);

    //!< Receive one event from the FastDATA FIFO
    int getEvent(std::vector<uint32_t>& evt, int* evtLen);
};

#endif