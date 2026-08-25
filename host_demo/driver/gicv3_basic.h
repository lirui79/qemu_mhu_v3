// ----------------------------------------------------------
// GICv3 Helper Functions for AArch64
// Header
//
// Copyright (C) Arm Limited, 2019 All rights reserved.
//
// The example code is provided to you as an aid to learning when working
// with Arm-based technology, including but not limited to programming tutorials.
// Arm hereby grants to you, subject to the terms and conditions of this Licence,
// a non-exclusive, non-transferable, non-sub-licensable, free-of-charge licence,
// to use and copy the Software solely for the purpose of demonstration and
// evaluation.
//
// You accept that the Software has not been tested by Arm therefore the Software
// is provided �as is�, without warranty of any kind, express or implied. In no
// event shall the authors or copyright holders be liable for any claim, damages
// or other liability, whether in action or contract, tort or otherwise, arising
// from, out of or in connection with the Software or the use of Software.
//
// ------------------------------------------------------------


#ifndef __gicv3_h
#define __gicv3_h

#include <stdint.h>

/**
 * GICV3 Route Mode Define
 */
#define GICV3_ROUTE_MODE_ANY             (0x80000000)       /*!< The SPI can be delivered to any connected PE that is
                                                                 participating in distribution of the interrupt group.      */
#define GICV3_ROUTE_MODE_COORDINATE      (0)                /*!< The SPI is to be delivered to the PE A.B.C.D, the affinity
                                                                 co-ordinates specified.                                    */

/**
 * GICV3 Config Define
 */
#define GICV3_CONFIG_LEVEL               (0)                /*!< level sensitive        */
#define GICV3_CONFIG_EDGE                (2)                /*!< edge triggered         */

/**
 * GICV3 Group Define
 */
#define GICV3_GROUP0                     (0)                /*!< secure group 0         */
#define GICV3_GROUP1_SECURE              (1)                /*!< secure group 1         */
#define GICV3_GROUP1_NON_SECURE          (2)                /*!< non-secure group 1     */

/**
 * GICV3 SGI Shift Define
 */
#define GICV3_SGI_AFF3_SHIFT          (48)                  /*!< affinity 3 shift       */
#define GICV3_SGI_AFF2_SHIFT          (32)                  /*!< affinity 2 shift       */
#define GICV3_SGI_AFF1_SHIFT          (16)                  /*!< affinity 1 shift       */

/**
 * GICV3 SGI Route Mode Define
 */
#define GICV3_SGI_ROUTING_ALL         ((uint64_t)1 << 40)   /*!< The interrupt is sent to all connected PEs, except
                                                                 the originating PE (self).                                         */
#define GICV3_SGI_ROUTING_LIST        (0)                   /*!< The interrupt is sent to aff3.aff2.aff1.Target List,
                                                                 where target list is encoded as 1 bit for each affinity 0
                                                                 node under aff1. This means that the interrupt can be sent
                                                                 to a maximum of 16 PEs, which might include the originating PE.    */

/**
 * GICV3 SGI Target Define
 */
#define GICV3_SGI_TARGET_CPU0         (0x0001)              /*!< target is cpu 0        */
#define GICV3_SGI_TARGET_CPU1         (0x0002)              /*!< target is cpu 1        */
#define GICV3_SGI_TARGET_CPU2         (0x0004)              /*!< target is cpu 2        */
#define GICV3_SGI_TARGET_CPU3         (0x0008)              /*!< target is cpu 3        */
#define GICV3_SGI_TARGET_CPU4         (0x0010)              /*!< target is cpu 4        */
#define GICV3_SGI_TARGET_CPU5         (0x0020)              /*!< target is cpu 5        */
#define GICV3_SGI_TARGET_CPU6         (0x0040)              /*!< target is cpu 6        */
#define GICV3_SGI_TARGET_CPU7         (0x0080)              /*!< target is cpu 7        */
#define GICV3_SGI_TARGET_CPU8         (0x0100)              /*!< target is cpu 8        */
#define GICV3_SGI_TARGET_CPU9         (0x0200)              /*!< target is cpu 9        */
#define GICV3_SGI_TARGET_CPU10        (0x0400)              /*!< target is cpu 10       */
#define GICV3_SGI_TARGET_CPU11        (0x0800)              /*!< target is cpu 11       */
#define GICV3_SGI_TARGET_CPU12        (0x1000)              /*!< target is cpu 12       */
#define GICV3_SGI_TARGET_CPU13        (0x2000)              /*!< target is cpu 13       */
#define GICV3_SGI_TARGET_CPU14        (0x4000)              /*!< target is cpu 14       */
#define GICV3_SGI_TARGET_CPU15        (0x8000)              /*!< target is cpu 15       */

/**
 * GICV3 SGI ID Define
 */
#define GICV3_SGI_ID0                 (0x0 << 24)           /*!< sgi interrupt id 0     */
#define GICV3_SGI_ID1                 (0x1 << 24)           /*!< sgi interrupt id 1     */
#define GICV3_SGI_ID2                 (0x2 << 24)           /*!< sgi interrupt id 2     */
#define GICV3_SGI_ID3                 (0x3 << 24)           /*!< sgi interrupt id 3     */
#define GICV3_SGI_ID4                 (0x4 << 24)           /*!< sgi interrupt id 4     */
#define GICV3_SGI_ID5                 (0x5 << 24)           /*!< sgi interrupt id 5     */
#define GICV3_SGI_ID6                 (0x6 << 24)           /*!< sgi interrupt id 6     */
#define GICV3_SGI_ID7                 (0x7 << 24)           /*!< sgi interrupt id 7     */
#define GICV3_SGI_ID8                 (0x8 << 24)           /*!< sgi interrupt id 8     */
#define GICV3_SGI_ID9                 (0x9 << 24)           /*!< sgi interrupt id 9     */
#define GICV3_SGI_ID10                (0xA << 24)           /*!< sgi interrupt id 10    */
#define GICV3_SGI_ID11                (0xB << 24)           /*!< sgi interrupt id 11    */
#define GICV3_SGI_ID12                (0xC << 24)           /*!< sgi interrupt id 12    */
#define GICV3_SGI_ID13                (0xD << 24)           /*!< sgi interrupt id 13    */
#define GICV3_SGI_ID14                (0xE << 24)           /*!< sgi interrupt id 14    */
#define GICV3_SGI_ID15                (0xF << 24)           /*!< sgi interrupt id 15    */

/**
 * GICV3 SGI Access Mode Define
 */
#define GICV3_SGI_NO_NS_ACCESS        (0)                   /*!< NO_NS_ACCESS           */
#define GICV3_SGI_NS_ACCESS_GROUP0    (0x1)                 /*!< NS_ACCESS_GROUP0       */
#define GICV3_SGI_NS_ACCESS_GROUP1    (0x2)                 /*!< NS_ACCESS_GROUP1       */

// ------------------------------------------------------------
// Address Functions
// ------------------------------------------------------------

//
// THESE FUNCTIONS MUST BE CALLED TO SET THE REGISTER FILE
// LOCATIONS BEFORE USING THE OTHER FUNCTIONS!
//

/**
 * @brief  Sets the address of the Distributor and Redistributors.
 * @param  dist: virtual address of the Distributor.
 * @param  rdist: virtual address of the first RD_base register page.
 */
void setGICAddr(void* dist, void* rdist);

// ------------------------------------------------------------
// Discovery function for Distributor and Redistributors
// ------------------------------------------------------------

/**
 * @brief  Returns the number of PPIs in the GICv3.1 extended range.
 * @param  rd: Redistributor number.
 * @retval Number of PPIs.
 */
uint32_t getExtPPI(uint32_t rd);


/**
 * @brief  Returns the number of SPIs in the GICv3.0 range.
 * @retval Number of SPIs.
 */
uint32_t getSPI(void);


/**
 * @brief  Returns the number of SPIs in the GICv3.1 extended SPI range.
 * @retval Number of SPIs.
 */
uint32_t getExtSPI(void);

// ------------------------------------------------------------
// Distributor Functions
// ------------------------------------------------------------

/**
 * @brief  Sets the group enable bits in the Distributor, with GICv3 mode selected.
 * @retval 0 is success, others is error.
 */
uint32_t  enableGIC(void);

// ------------------------------------------------------------
// Redistributor Functions
//
// These functions make a number of assumptions about the
// memory used for the command queue:
// * The memory is flat mapped (VA==PA)
// * The memory is either coherent or non-cacheble.
//
// ------------------------------------------------------------

/**
 * @brief  Get Redistributer number for a given affinity.
 * @param  affinity: Affinity co-ordinate of target.
 * @retval Redistributer number.
 */
uint32_t getRedistID(uint32_t affinity);


/**
 * @brief  Wakes the currently select redistributor.
 * @param  rd: Redistributor number.
 * @retval 0 is success, others is error.
 */
uint32_t wakeUpRedist(uint32_t rd);

// ----------------------------------------------------------
// SGI, PPIs and SPI configuration functions
// ----------------------------------------------------------

/**
 * @brief  Enables the INTID.
 * @param  INTID: INTID of interrupt.
 * @param  rd: Redistributor number (ignored if INTID is a SPI).
 * @retval 0 is success, others is error.
 */
uint32_t enableInt(uint32_t INTID, uint32_t rd);


/**
 * @brief  Disables the INTID.
 * @param  INTID: INTID of interrupt.
 * @param  rd: Redistributor number (ignored if INTID is a SPI).
 * @retval 0 is success, others is error.
 */
uint32_t disableInt(uint32_t INTID, uint32_t rd);


/**
 * @brief  Sets the priority of the specified INITD.
 * @param  INTID: INTID of interrupt.
 * @param  rd: Redistributor number (ignored if INTID is a SPI).
 * @param  priority: priority (8-bit unsigned value).
 * @retval 0 is success, others is error.
 */
uint32_t setIntPriority(uint32_t INTID, uint32_t rd, uint8_t priority);


/**
 * @brief  Sets the target CPUs of the specified INTID.
 * @param  INTID: INTID of interrupt (must be in the range 32 to 1019).
 * @param  mode: Routing mode， a value of @ref GICV3_Route_Mode_Define.
 * @param  affinity: Affinity co-ordinate of target.
 * @retval 0 is success, otherwise is error.
 */
uint32_t setIntRoute(uint32_t INTID, uint32_t mode, uint32_t affinity);


/**
 * @brief  Configures the INTID as edge or level sensitive.
 * @param  INTID: INTID of interrupt.
 * @param  rd: Redistributor number (ignored if INTID is a SPI).
 * @param  conf: Whether the INTID should edge or level, a value of @ref GICV3_Config_Define.
 * @retval 0 is success, otherwise is error.
 */
uint32_t setIntType(uint32_t INTID, uint32_t rd, uint32_t conf);


/**
 * @brief  Set security/group of the specified INTID.
 * @param  INTID: INTID of interrupt (ID must be less than 32).
 * @param  rd: Redistributor number (ignored if INTID is a SPI).
 * @param  group: Security/group setting, a value of @ref GICV3_Group_Define.
 * @retval 0 is success, otherwise is error.
 */
uint32_t setIntGroup(uint32_t INTID, uint32_t rd, uint32_t group);


/**
 * @brief  Sets the pending bit of the specified INT.
 * @param  INTID: INTID of interrupt (ID must be less than 1020).
 * @param  rd: Redistributor number.
 * @retval 0 is success, otherwise is error.
 */
uint32_t setIntPending(uint32_t INTID, uint32_t rd);


/**
 * @brief  Clears the pending bit of the specified INTID.
 * @param  INTID: INTID of interrupt (ID must be less than 1020).
 * @param  rd: Redistributor number.
 * @retval 0 is success, otherwise is error.
 */
uint32_t clearIntPending(uint32_t INTID, uint32_t rd);


/**
 * @brief  Clears the active bit of the specified INTID.
 * @param  ID: INTID of interrupt (ID must be less than 1020).
 * @param  rd: Redistributor number.
 * @retval 0 is success, otherwise is error.
 */
uint32_t clearIntActive(uint32_t ID, uint32_t rd);


// ------------------------------------------------------------
// CPU Interface functions
// ------------------------------------------------------------

/**
 * @brief  Enables group 0 interrupts.
 * @note   The lowest EL that access the ICC_IGRPEN0_EL1 is determined
 *         by the routine of the FIQ exception.
 */
void enableGroup0Ints(void);


/**
 * @brief  Disables group 0 interrupts.
 * @note   The lowest EL that access the ICC_IGRPEN0_EL1 is determined
 *         by the routine of the FIQ exception.
 */
void disableGroup0Ints(void);


/**
 * @brief  Enables group 1 interrupts for current security state.
 * @note   The lowest EL that access the ICC_IGRPEN1_EL1 is determined
 *         by the routine of the IRQ exception.
 */
void enableGroup1Ints(void);


/**
 * @brief  Disables group 1 interrupts for current security state.
 * @note   The lowest EL that access the ICC_IGRPEN1_EL1 is determined
 *         by the routine of the IRQ exception.
 */
void disableGroup1Ints(void);


/**
 * @brief  Enables group 1 interrupts for non-secure state.
 * @note   Used by EL3 to enable non-secure group 1 interrupts.
 */
void enableNSGroup1Ints(void);


/**
 * @brief  Disables group 1 interrupts for non-secure state.
 * @note   Used by EL3 to disable non-secure group 1 interrupts.
 */
void disableNSGroup1Ints(void);


/**
 * @brief  Returns the value of the ICC_IAR0_EL1
 * @retval Group 0 Interrupt Acknowledge.
 */
uint32_t readIARGrp0(void);


/**
 * @brief  Writes INTID to the End Of Interrupt register
 * @param  INTID: INTID of interrupt.
 */
void writeEOIGrp0(uint32_t INTID);


/**
 * @brief  Writes INTID to the De-active Interrupt register.
 * @param  INTID: INTID of interrupt.
 */
void writeDIR(uint32_t INTID);


/**
 * @brief  Returns the value of the ICC_IAR1_EL1.
 * @retval Group 1 Interrupt Acknowledge.
 */
uint32_t readIARGrp1(void);


//uint32_t readICC_CTLR_EL3(void);
//void writeEOImode_EL3(uint32_t EOIMODE_EL3);


/**
 * @brief  Writes INTID to the Aliased End Of Interrupt register.
 * @param  INTID: INTID of interrupt.
 */
void writeEOIGrp1(uint32_t INTID);


/**
 * @brief  Sets the Priority mask register for the core run on.
 * @param  priority: mask value (8-bit unsigned value).
 * @note   An interrupt must be high priority (lower numeric value) than the mask to be sent
 */
void setPriorityMask(uint32_t priority);


/**
 * @brief  Sets ICC_BPR0_EL1 for the core run on.
 * @param  priority: Binary piont value.
 */
void setBPR0(uint32_t priority);


/**
 * @brief  Sets ICC_BPR1_EL1 for the core run on.
 * @param  priority: Binary piont value.
 */
void setBPR1(uint32_t priority);


/**
 * @brief  Returns the priority of the current active interrupt.
 * @retval priority value.
 */
uint32_t getRunningPriority(void);

// ------------------------------------------------------------
// SGIs
// ------------------------------------------------------------

/**
 * @brief  Send a Group 0 SGI.
 * @param  INTID: INTID of interrupt, a value of @ref GICV3_SGI_ID_Define.
 * @param  mode: routing mode, a value of @ref GICV3_SGI_Route_Mode_Define.
 * @param  target_list: target list, combination of values of @ref GICV3_SGI_Target_Define.
 */
void sendGroup0SGI(uint32_t INTID, uint64_t mode, uint32_t target_list);


/**
 * @brief  Send a Group 1 SGI, current security state.
 * @param  INTID: INTID of interrupt, a value of @ref GICV3_SGI_ID_Define.
 * @param  mode: routing mode, a value of @ref GICV3_SGI_Route_Mode_Define.
 * @param  target_list: target list, combination of values of @ref GICV3_SGI_Target_Define.
 */
void sendGroup1SGI(uint32_t INTID, uint64_t mode, uint32_t target_list);


/**
 * @brief  Send a Group 1 SGI, other security state.
 * @param  INTID: INTID of interrupt, a value of @ref GICV3_SGI_ID_Define.
 * @param  mode: routing mode, a value of @ref GICV3_SGI_Route_Mode_Define.
 * @param  target_list: target list, combination of values of @ref GICV3_SGI_Target_Define.
 */
void sendOtherGroup1SGI(uint32_t INTID, uint64_t mode, uint32_t target_list);


/**
 * @brief  Sets non-secure state ability to generate secure group 0/1 SGIs.
 * @param  INTID: INTID of interrupt (must be 0 to 15).
 * @param  access: access mode, a value of @ref GICV3_SGI_Acess_Mode_Define
 */
void configNSAccessSGI(uint32_t INTID, unsigned access);


#endif

// ----------------------------------------------------------
// End of gicv3_basic.h
// ----------------------------------------------------------
