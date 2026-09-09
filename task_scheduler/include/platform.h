#ifndef __PLATFORM_H__
#define __PLATFORM_H__

/* system frequency */
#define SYS_FREQ_HZ             62500000UL
#define SYS_FREQ_KHZ            (SYS_FREQ_HZ / 1000)
#define SYS_FREQ_MHZ            (SYS_FREQ_HZ / 1000000) 

/* peripheral base addresses */
#define GICD_BASE               0x2F000000U
#define GICR_BASE               0x2F100000U
#define MHU_SND_DATA            0x2FC00000U
#define MHU_PBX_BASE            0x2FC10000U
#define MHU_REC_DATA            0x2FC30000U
#define MHU_MBX_BASE            0x2FC40000U
#define UART0_BASE              0x2FC60000U
#define UART1_BASE              0x2FC61000U
#define TS_GEN_BASE             0x2FC62000U
#define TS_GEN_REG_BASE         0x2FC63000U
#define DMA_BASE                0x2FC64000U

/* memory space */
#define TCM_BASE                0x00018000U
#define TCM_SIZE                0x00008000U
#define DDR_BASE                0x80000000U
#define DDR_SIZE                0x20000000U

/* interrupt definition */
#define IRQ_ID_PHY_TIMER        30  //PPI-14
#define IRQ_ID_DMA_COMB         32  //SPI-0
#define IRQ_ID_DMA_CH_0         33  //SPI-1
#define IRQ_ID_DMA_CH_1         34  //SPI-2
#define IRQ_ID_DMA_CH_2         35  //SPI-3
#define IRQ_ID_DMA_CH_3         36  //SPI-4
#define IRQ_ID_DMA_CH_4         37  //SPI-5
#define IRQ_ID_DMA_CH_5         38  //SPI-6
#define IRQ_ID_DMA_CH_6         39  //SPI-7
#define IRQ_ID_DMA_CH_7         40  //SPI-8
#define IRQ_ID_DMA_COMMON       49  //SPI-17
#define IRQ_ID_MHU_PBX_COMB     78  //SPI-46
#define IRQ_ID_MHU_MBX_COMB     110 //SPI-78

#endif
