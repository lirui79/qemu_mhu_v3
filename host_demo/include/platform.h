#ifndef __PLATFORM_H__
#define __PLATFORM_H__

/* system frequency */
#define SYS_FREQ_HZ             62500000UL
#define SYS_FREQ_KHZ            (SYS_FREQ_HZ / 1000)
#define SYS_FREQ_MHZ            (SYS_FREQ_HZ / 1000000) 

/* peripheral base addresses */
#define GICD_BASE               0x2F020000U
#define GICR_BASE               0x2F120000U
#define MHU_SND_DATA            0x2FC00000U
#define MHU_PBX_BASE            0x2FC20000U
#define MHU_REC_DATA            0x2FC30000U
#define MHU_MBX_BASE            0x2FC50000U
#define UART0_BASE              0xD0000000U

/* memory space */
#define DDR_BASE                0x81000000U
#define DDR_SIZE                0x1F000000U

/* interrupt definition */
#define IRQ_ID_PHY_TIMER        30  //PPI-14
#define IRQ_ID_MHU_PBX_COMB     78  //SPI-46
#define IRQ_ID_MHU_MBX_COMB     110 //SPI-78

#endif