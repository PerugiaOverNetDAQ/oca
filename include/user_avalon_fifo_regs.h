#ifndef __USER_AVALON_FIFO_REGS_H__
#define __USER_AVALON_FIFO_REGS_H__

#define HW_REGS_BASE ( ALT_STM_OFST )		// Physical base address: 0xFC000000
#define HW_REGS_SPAN ( 0x04000000 )			// Span Physical address: 64 MB
#define HW_REGS_MASK ( HW_REGS_SPAN - 1 )


// OFFSET per puntare sui registri di stato della FIFO a partire da FIFO_HPS_TO_FPGA_IN_CSR_BASE
//This definitions would work if doing the pointer arithmetics in byte units (i.e. handling char*)-------------
/* #define ALTERA_AVALON_FIFO_LEVEL_REG        (0x00000000) */
/* #define ALTERA_AVALON_FIFO_STATUS_REG       (0x00000004) */
/* #define ALTERA_AVALON_FIFO_EVENT_REG        (0x00000008) */
/* #define ALTERA_AVALON_FIFO_IENABLE_REG      (0x0000000c) */
/* #define ALTERA_AVALON_FIFO_ALMOSTFULL_REG   (0x00000010) */
/* #define ALTERA_AVALON_FIFO_ALMOSTEMPTY_REG  (0x00000014) */
//-------------------------------------------------------------------------------------------------------------
//This definitions would work if doing the pointer arithmetics in 4-byte units (i.e. handling void* or int*)---
#define ALTERA_AVALON_FIFO_LEVEL_REG        (0x00000000)
#define ALTERA_AVALON_FIFO_STATUS_REG       (0x00000001)
#define ALTERA_AVALON_FIFO_EVENT_REG        (0x00000002)
#define ALTERA_AVALON_FIFO_IENABLE_REG      (0x00000003)
#define ALTERA_AVALON_FIFO_ALMOSTFULL_REG   (0x00000004)
#define ALTERA_AVALON_FIFO_ALMOSTEMPTY_REG  (0x00000005)
//-------------------------------------------------------------------------------------------------------------

//bit dell' EVENT REGISTER
#define ALTERA_AVALON_FIFO_EVENT_F_MSK    (0x01)
#define ALTERA_AVALON_FIFO_EVENT_E_MSK    (0x02)
#define ALTERA_AVALON_FIFO_EVENT_AF_MSK   (0x04)
#define ALTERA_AVALON_FIFO_EVENT_AE_MSK   (0x08)
#define ALTERA_AVALON_FIFO_EVENT_OVF_MSK  (0x10)
#define ALTERA_AVALON_FIFO_EVENT_UDF_MSK  (0x20)
#define ALTERA_AVALON_FIFO_EVENT_ALL_MSK  (0x3F)

//bit dello STATUS REGISTER
#define ALTERA_AVALON_FIFO_STATUS_F_MSK    (0x01)
#define ALTERA_AVALON_FIFO_STATUS_E_MSK    (0x02)
#define ALTERA_AVALON_FIFO_STATUS_AF_MSK   (0x04)
#define ALTERA_AVALON_FIFO_STATUS_AE_MSK   (0x08)
#define ALTERA_AVALON_FIFO_STATUS_OVF_MSK  (0x10)
#define ALTERA_AVALON_FIFO_STATUS_UDF_MSK  (0x20)
#define ALTERA_AVALON_FIFO_STATUS_ALL_MSK  (0x3F)

//bit dell' INTERRUTPT ENABLE REGISTER
#define ALTERA_AVALON_FIFO_IENABLE_F_MSK    (0x01)
#define ALTERA_AVALON_FIFO_IENABLE_E_MSK    (0x02)
#define ALTERA_AVALON_FIFO_IENABLE_AF_MSK   (0x04)
#define ALTERA_AVALON_FIFO_IENABLE_AE_MSK   (0x08)
#define ALTERA_AVALON_FIFO_IENABLE_OVF_MSK  (0x10)
#define ALTERA_AVALON_FIFO_IENABLE_UDF_MSK  (0x20)
#define ALTERA_AVALON_FIFO_IENABLE_ALL_MSK  (0x3F)

#endif /* __USER_AVALON_FIFO_REGS_H__ */
