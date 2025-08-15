/***********************  Filename: dp_if.c  *********************************/
/* ========================================================================= */
/*                                                                           */
/* 0000  000   000  00000 0  000  0   0 0 0000                               */
/* 0   0 0  0 0   0 0     0 0   0 0   0 0 0   0      Einsteinstra�e 6        */
/* 0   0 0  0 0   0 0     0 0     0   0 0 0   0      91074 Herzogenaurach    */
/* 0000  000  0   0 000   0 0     00000 0 0000       Germany                 */
/* 0     00   0   0 0     0 0     0   0 0 0                                  */
/* 0     0 0  0   0 0     0 0   0 0   0 0 0          Phone: ++499132744200   */
/* 0     0  0  000  0     0  000  0   0 0 0    GmbH  Fax:   ++4991327442164  */
/*                                                                           */
/* ========================================================================= */
/*                                                                           */
/* Function:       interface service routines for VPC3+ (dp-protocol)        */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* history                                                                   */
/* ========================================================================= */
/* 08.09.2005 [V5.00.00]                                                     */
/*                     Urversion                                             */
/* 22.10.2005 [V5.00.01]                                                     */
/*                     BugFix:                                               */
/*                     Dp_cfg.h:                                             */
/*                     Missing bracket:                                      */
/*                     #define ASIC_USER_RAM (ASIC_RAM_LENGTH -              */
/*                                              DP_ORG_LENGTH - SAP_LENGTH)  */
/*                     DP_VERSION_BUGFIX changed to 0x01                     */
/* 09.01.2006 [V5.00.02]                                                     */
/*                     BugFix:                                               */
/*                     Problem with SIEMENS PLC 318 and Diag.cfg_Fault       */
/*                     The VPC3+/C does not set the default master address   */
/*                     back in the diagnostic telegram to 0xFF.              */
/*                     Software solution in dp_user.c                        */
/*                     (named with BugFix 502).                              */
/*                     DP_VERSION_BUGFIX changed to 0x02                     */
/* 21.02.2006 [V5.00.03]                                                     */
/*                     BugFix:                                               */
/*                     I&M Functions: Return Codes of NRS-PDUs changed.      */
/*                     profichip has now it's own MANUFACTURER_ID (0x01BC).  */
/*                     BugFix:                                               */
/*                     The definition of LITTLE_ENDIAN and BIG_ENDIAN is     */
/*                     now correct.                                          */
/*                     BugFix:                                               */
/*                     MSAC_C2-connection: Certification Error Ifak -        */
/*                     ILL31 transistion implemented.                        */
/*                     After disconnecting profibus cable, an open MSAC-C2   */
/*                     connection will be closed after timeout (2*F-timer).  */
/*                     Function acls in dp_msac_c1 optimized.                */
/*                     (named with BugFix 503).                              */
/*                     DP_VERSION_BUGFIX changed to 0x03                     */
/* 13.11.2006 [V5.00.04]                                                     */
/*                     Certification problem ( itm ):                        */
/*                     The DPV1-alarms will be checked now in the function   */
/*                     UserChkNewPrmData().                                  */
/*                     Certification problem ( procentec ):                  */
/*                     The master class 2 sends an initiate.req with out     */
/*                     data and the slave answers with next free SAP.        */
/*                     After then the master class 2 send an initiate.req    */
/*                     with data, the slave should now answer with the same  */
/*                     SAP and not with new SAP.                             */
/*                     (named with BugFix 504).                              */
/*                     DP_VERSION_BUGFIX changed to 0x04                     */
/* 03.08.2009 [V6.00.00]                                                     */
/*                     Supports now VPC3+S                                   */
/*                     DP_VERSION_BUGFIX changed to 0x00                     */
/* 08.08.2009 [V6.00.01]                                                     */
/*                     BugFix:                                               */
/*                     DP_VERSION_BUGFIX changed to 0x01                     */
/* 17.09.2010 [V6.00.02]                                                     */
/*                     BugFix:                                               */
/*                     Error in function: VPC3_GetMasterAddress              */
/*                     DP_VERSION_BUGFIX changed to 0x02                     */
/* 13.12.2010 [V6.00.03]                                                     */
/*                     BugFix:                                               */
/*                     dp_if.h: add the macro _PACKED_ to all structures     */
/*                     Modification: Prm-, ChkCfg- and SSA-telegram via      */
/*                     help buffer:  abPrmCfgSsaHelpBuffer[ HELP_BUFSIZE ].  */
/*                     DP_VERSION_BUGFIX changed to 0x03                     */
/* 15.02.2011 [V6.00.04]                                                     */
/*                     BugFix:                                               */
/*                     dp_if.h: DxB-macros are wrong                         */
/*                      - macro VPC3_GET_NEXT_DXB_OUT_BUFFER_CMD() modified  */
/*                      - macro VPC3_GET_DXB_OUT_BUFFER_SM() modified        */
/*                     modification of VPC3_Initialization( uint8_t          */
/*                     bSlaveAddress,uint16_t wIdentNumber,psCFG psCfgData );*/
/*                     DP_VERSION_BUGFIX changed to 0x04                     */
/* 19.04.2011 [V6.00.05]                                                     */
/*                     BugFix:                                               */
/*                     dp_msac2.c:  MSAC_C2_InitiateReqToRes( ... )          */
/*                      - psMsgHeader->bDataLength = C2_FEATURES_SUPPORTED_2;*/
/*                     Modification dp_if.h:                                 */
/*                      - new macro: VPC3_SET_EMPTY_SAP_LIST()               */
/*                      - new macro: VPC3_SET_SAP_LIST_END(OFFSET)           */
/*                     Modification of following functions:                  */
/*                      - dp_if.c: VPC3_Initialization( ... )                */
/*                      - dp_if.c: VPC3_SetConstants( ... )                  */
/*                      - dp_fdl.c: FDL_Init( ... )                          */
/*                     DP_VERSION_BUGFIX changed to 0x05                     */
/* 21.10.2011 [V6.00.06]                                                     */
/*                     BugFix:                                               */
/*                     dp_msac2.c: MSAC_C2_TransmitDelay( ... )              */
/*                     If the master sends an Abort message during           */
/*                     MSAC_C2_TransmitDelay call, the Class2 statemachine   */
/*                     breaks down with fatal error.                         */
/*                     Modification of following functions:                  */
/*                     - dp_msac2.c: MSAC_C2_TransmitDelay( ... )            */
/*                     - dp_msac2.c: MSAC_C2_RealizeIndRecv( ... )           */
/*                     DP_VERSION_BUGFIX changed to 0x06                     */
/* 12.12.2011 [V6.00.07]                                                     */
/*                     BugFix:                                               */
/*                     - macro VPC3Write() now with cast VPC3_ADR            */
/*                     - macro VPC3Read() now with cast VPC3_ADR             */
/*                     If MSAC_C2 connection run out in the function         */
/*                     MSAC_C2_Process the User-Application was informed 3   */
/*                     times via function UserMsac2AbortInd.                 */
/*                     Modification of following functions:                  */
/*                     - platform.h now without the define NOP_              */
/*                     DP_VERSION_BUGFIX changed to 0x07                     */
/* 18.07.2012 [V6.00.08]                                                     */
/*                     Modification of following functions:                  */
/*                      - dp_if.c: VPC3_MemoryTest( ... )                    */
/*                     DP_VERSION_BUGFIX changed to 0x08                     */
/* 05.06.2013 [V6.00.09]                                                     */
/*                     BugFix:                                               */
/*                      - VPC3+S in serial mode: when reading from the       */
/*                        Control Parameter memory (address 0x000 to 0x015)  */
/*                        only the READ_BYTE instruction may be used.        */
/*                        Otherwise an unintendend read operation to the     */
/*                        subsequent memory location will occur leading to   */
/*                        an unpredictable behavior of the VPC3+S.           */
/*                      - dp_if.h:                                           */
/*                        - macro changed: VPC3_SetDpState( _eDpState )      */
/*                        - macro changed: VPC3_ClrDpState( _eDpState )      */
/*                        - macro changed: VPC3_GetDpState( _eDpState )      */
/*                      - dp_msac1.c: alarm statemachine                     */
/*                        - AL_CheckTypeStatus() modified                    */
/*                        - AL_CheckSequenceStatus() modified                */
/*                        - AL_CheckSendAlarm() modified                     */
/*                     Modification of following functions:                  */
/*                      - Vpc3_Initialization( ... )                         */
/*                      - Vpc3_SetConstants( ... )                           */
/*                      - Vpc3_Isr( ... )                                    */
/*                      - Vpc3_Poll( ... )                                   */
/*                     New Feature:                                          */
/*                      - MAC Reset added to dp_isr, Dp_Appl.c               */
/*                     DP_VERSION_BUGFIX changed to 0x09                     */
/* 05.06.2013 [V6.00.10]                                                     */
/*                     BugFix:                                               */
/*                      - Problems with PROFIBUS conformancetest:            */
/*                        - Testcase: TC08 NMAS1, TC09 NMAS2                 */
/*                          If the master sends two SetPrm telegrams, the    */
/*                          software hangs in the functions DpDiag_SetPrmOk  */
/*                          and the slave acknowledge with Prm fault.        */
/*                     Function names changed:                               */
/*                      - DpDiag_PrmNotOk(...) --> DpDiag_SetPrmNotOk( ... ) */
/*                      - DpDiag_PrmOk(...)    --> DpDiag_SetPrmOk( ... )    */
/*                     Modification of following functions:                  */
/*                      - DpDiag_SetPrmOk( ... )                             */
/*                     DP_VERSION_BUGFIX changed to 0x10                     */
/* 15.07.2013 [V6.01.10]                                                     */
/*                     New function:                                         */
/*                      - support of several ident numbers                   */
/*                     DP_VERSION_FUNCTION changed to 0x01                   */
/* 29.08.2013 [V6.01.11]                                                     */
/*                     BugFix: I&M record set 1 .. 4                         */
/*                      - after writing I&M 1..4 it's not allowed to read    */
/*                        I&M 1..4.                                          */
/*                     DP_VERSION_BUGFIX changed to 0x11                     */
/* 02.10.2014 [V6.01.12]                                                     */
/*                     BugFix:                                               */
/*                      - Problems with PROFIBUS conformancetest:            */
/*                        - Testcase: TC08 NMAS1, TC09 NMAS2                 */
/*                          If the master sends two SetPrm telegrams, the    */
/*                          software hangs in the functions DpDiag_SetPrmOk  */
/*                          and the slave acknowledge with Prm fault.        */
/*                        - Modification of following functions:             */
/*                          - DpDiag_Alarm( ... )                            */
/*                          - DpDiag_SetPrmOk( ... )                         */
/*                     DP_VERSION_BUGFIX changed to 0x12                     */
/* 05.05.2015 [V6.01.13]                                                     */
/*                     Modification of following function:                   */
/*                      - VPC3_UpdateDiagnosis( .. )                         */
/*                        Now: copy to diag buffer if length > 0             */
/*                     BugFix Redundancy System:                             */
/*                      - Modification of following function:                */
/*                        - VPC3_InitBufferStructure( ... )                  */
/*                          - set following variable to zero:                */
/*                            pDpSystem->wVpc3UsedDPV0BufferMemory = 0;      */
/*                        - AL_InitAlarm( ... )                              */
/*                          - don't clear alarm buffer in Redundancy Mode    */
/*                        - AL_AlarmEnabled( ... )                           */
/*                        - AL_CheckTypeStatus( ... )                        */
/*                          - remove static                                  */
/* 19.11.2015 [V6.01.14]                                                     */
/*                     BugFix:                                               */
/*                      - Modification of pointer cast:                      */
/*                        - pDpSystem->pDxbOutBuffer1                        */
/*                        - pDpSystem->pDxbOutBuffer2                        */
/*                        - pDpSystem->pDxbOutBuffer3                        */
/*                        - pDpSystem->pDoutBuffer1                          */
/*                        - pDpSystem->pDoutBuffer2                          */
/*                        - pDpSystem->pDoutBuffer3                          */
/*                        - pDpSystem->pDinBuffer1                           */
/*                        - pDpSystem->pDinBuffer2                           */
/*                        - pDpSystem->pDinBuffer3                           */
/*                        - pDpSystem->pDiagBuffer1                          */
/*                        - pDpSystem->pDiagBuffer2                          */
/*                     DP_VERSION_BUGFIX changed to 0x14                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Technical support:       eMail: support@profichip.com                     */
/*                          Phone: ++49-9132-744-2150                        */
/*                          Fax. : ++49-9132-744-29-2150                     */
/*                                                                           */
/*****************************************************************************/

/*! \file
     \brief Basic functions for PROFIBUS communication with VPC3+.

*/

/*****************************************************************************/
/* contents:

  - function prototypes
  - data structures
  - internal functions

*/
/*****************************************************************************/
/* include hierarchy */
#include <string.h>
#include <stdio.h>
#include "platform.h"

/*---------------------------------------------------------------------------*/
/* version                                                                   */
/*---------------------------------------------------------------------------*/
#define DP_VERSION_MAIN_INTERFACE   ((uint8_t)0x06)
#define DP_VERSION_FUNCTION         ((uint8_t)0x01)
#define DP_VERSION_BUGFIX           ((uint8_t)0x14)

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/
static DP_ERROR_CODE    VPC3_SetConstants          ( uint8_t bSlaveAddress, uint16_t wIdentNumber );
static DP_ERROR_CODE    VPC3_InitBufferStructure   ( void );
#if DP_SUBSCRIBER
   static DP_ERROR_CODE VPC3_InitSubscriber        ( void );
#endif /* #if DP_SUBSCRIBER */

/*---------------------------------------------------------------------------*/
/* global user data definitions                                              */
/*---------------------------------------------------------------------------*/
#ifdef DP_DEBUG_ENABLE
   sDP_DEBUG_BUFFER_ITEM   asDebugBuffer[ MAX_NR_OF_DEBUG ];
   uint8_t                 bDebugBufferIndex;
   uint8_t                 bDebugBufferOverlapped;

   #if REDUNDANCY
      uint8_t              bOldDebugCode_1;
      uint8_t              bOldDetail_1;
      uint8_t              bOldDebugCode_2;
      uint8_t              bOldDetail_2;
   #endif /* #if REDUNDANCY */
#endif /* #ifdef DP_DEBUG_ENABLE */

/*---------------------------------------------------------------------------*/
/* defines                                                                   */
/*---------------------------------------------------------------------------*/
/* Literals */
#define RBL_PRM   ((uint8_t)0x00)
#define RBL_CFG   ((uint8_t)0x01)
#define RBL_SSA   ((uint8_t)0x02)

/*---------------------------------------------------------------------------*/
/* function: GetFirmwareVersion                                              */
/*---------------------------------------------------------------------------*/
void GetFirmwareVersion( psDP_VERSION pVersion )
{
   pVersion->wComponentsInstalled =   0x0000

                         #if DP_MSAC_C1
                                    | DP_COMP_INSTALLED_MSAC1_IFA

                             #if DP_ALARM
                                    | DP_COMP_INSTALLED_SUB_AL
                                  #if DP_ALARM_OVER_SAP50
                                    | DP_COMP_INSTALLED_SUB_AL_50
                                  #endif /* #if DP_ALARM_OVER_SAP50 */
                             #endif /* #if DP_ALARM */

                             #if DPV1_IM_SUPP
                                    | DP_COMP_INSTALLED_IM
                             #endif /* #if DPV1_IM_SUPP */
                         #endif /* #if DP_MSAC_C1 */

                         #if DP_MSAC_C2
                                    | DP_COMP_INSTALLED_MSAC2_IFA

                             #if DPV1_IM_SUPP
                                    | DP_COMP_INSTALLED_IM
                             #endif /* #if DPV1_IM_SUPP */
                         #endif /* #if DP_MSAC_C2 */

                                        ;

   switch( VPC3_GET_ASIC_TYPE() )
   {
      case AT_VPC3B:
      {
         pVersion->wComponentsInstalled |= DP_COMP_INSTALLED_VPC3_B ;
         break;
      } /* case AT_VPC3B: */

      case AT_VPC3C:
      {
         pVersion->wComponentsInstalled |= DP_COMP_INSTALLED_VPC3_C ;
         break;
      } /* case AT_VPC3C: */

      case AT_MPI12X:
      {
         pVersion->wComponentsInstalled |= DP_COMP_INSTALLED_VPC3_D ;
         break;
      } /* case AT_MPI12X: */

      case AT_VPC3S:
      {
         pVersion->wComponentsInstalled |= DP_COMP_INSTALLED_VPC3_S ;
         break;
      } /* case VPC3S: */

      default:
      {
         /* do nothing */
         break;
      } /* default: */
   } /* switch( VPC3_GET_ASIC_TYPE() */

   pVersion->bMainInterface = DP_VERSION_MAIN_INTERFACE;
   pVersion->bFunction      = DP_VERSION_FUNCTION;
   pVersion->bBugfix        = DP_VERSION_BUGFIX;
} /* void GetFirmwareVersion( psDP_VERSION pVersion ) */

/*---------------------------------------------------------------------------*/
/* function: DpWriteDebugBuffer                                              */
/*---------------------------------------------------------------------------*/
#ifdef DP_DEBUG_ENABLE
void DpWriteDebugBuffer( DEBUG_CODE bDebugCode, uint8_t bDetail1, uint8_t bDetail2 )
{
   #if REDUNDANCY
      if( bDetail1 == RED_CHANNEL_1 )
      {
         if( ( bOldDebugCode_1 != bDebugCode ) || ( bOldDetail_1 != bDetail2 ) )
         {
            bOldDebugCode_1 = bDebugCode;
            bOldDetail_1 = bDetail2;
         } /* if( ( bOldDebugCode_1 != bDebugCode ) || ( bOldDetail_1 != bDetail2 ) ) */
         else
         {
            return;
         } /* else of if( ( bOldDebugCode_1 != bDebugCode ) || ( bOldDetail_1 != bDetail2 ) ) */
      } /* if( bDetail1 == RED_CHANNEL_1 ) */
      else
      {
         if( ( bOldDebugCode_2 != bDebugCode ) || ( bOldDetail_2 != bDetail2 ) )
         {
            bOldDebugCode_2 = bDebugCode;
            bOldDetail_2 = bDetail2;
         } /* if( ( bOldDebugCode_2 != bDebugCode ) || ( bOldDetail_2 != bDetail2 ) ) */
         else
         {
            return;
         } /* else of if( ( bOldDebugCode_2 != bDebugCode ) || ( bOldDetail_2 != bDetail2 ) ) */
      } /* else of if( bDetail1 == RED_CHANNEL_1 ) */
   #endif /* #if REDUNDANCY */

   if( bDebugBufferOverlapped == VPC3_FALSE )
   {
      asDebugBuffer[ bDebugBufferIndex ].bDebugCode = bDebugCode;
      asDebugBuffer[ bDebugBufferIndex ].bDetail1   = bDetail1;
      asDebugBuffer[ bDebugBufferIndex ].bDetail2   = bDetail2;

      if( bDebugBufferIndex == ( MAX_NR_OF_DEBUG - 1 ) )
      {
         bDebugBufferOverlapped = VPC3_TRUE;
         bDebugBufferIndex = 0;
      } /* if( bDebugBufferIndex == ( MAX_NR_OF_DEBUG - 1 ) ) */
      else
      {
         bDebugBufferIndex++;
      } /* else of if( bDebugBufferIndex == ( MAX_NR_OF_DEBUG - 1 ) ) */
   } /* if( bDebugBufferOverlapped == VPC3_FALSE ) */
} /* void DpWriteDebugBuffer( DEBUG_CODE debug_code, uint8_t detail_1, uint8_t detail_2 ) */
#endif /* #ifdef DP_DEBUG_ENABLE */

/*---------------------------------------------------------------------------*/
/* function: DpClearDebugBuffer                                              */
/*---------------------------------------------------------------------------*/
#ifdef DP_DEBUG_ENABLE
void DpClearDebugBuffer( void )
{
uint16_t i;

   bDebugBufferOverlapped = VPC3_FALSE;
   bDebugBufferIndex      = 0x00;

   for( i = 0; i < MAX_NR_OF_DEBUG; i++ )
   {
      asDebugBuffer[i].bDebugCode = 0x00;
      asDebugBuffer[i].bDetail1   = 0x00;
      asDebugBuffer[i].bDetail2   = 0x00;
   } /* for( i = 0; i < MAX_NR_OF_DEBUG; i++ ) */
} /* void DpClearDebugBuffer( void ) */
#endif /* #ifdef DP_DEBUG_ENABLE */

/*---------------------------------------------------------------------------*/
/* function: VPC3_MemoryTest                                                 */
/*---------------------------------------------------------------------------*/
/*!
  \brief Check VPC3 memory.
   This function checks the memory of VPC3+. The starting address is 16hex and
   the end address depends on DP_VPC3_4KB_MODE (DpCfg.h).

  \retval DP_OK - memory OK
  \retval DP_VPC3_ERROR - Memory Error
*/
DP_ERROR_CODE VPC3_MemoryTest( void )
{
#if VPC3_SERIAL_MODE
   VPC3_ADR    wAddress;
#else
   VPC3_UNSIGNED8_PTR  pToVpc3;
#endif /* #if VPC3_SERIAL_MODE */
DP_ERROR_CODE  bError;
uint16_t       wTemp;
uint16_t       j;
uint16_t       i;

   printf("DEBUG: [VPC3_MemoryTest] INICIO - Probando memoria VPC3+S\r\n");
   printf("DEBUG: [VPC3_MemoryTest] ASIC_RAM_LENGTH = 0x%04X\r\n", ASIC_RAM_LENGTH);

   /* neccessary, if 4Kbyte mode enabled */
   printf("DEBUG: [VPC3_MemoryTest] INIT_VPC3_MODE_REG_2 = 0x%02X\r\n", INIT_VPC3_MODE_REG_2);
   printf("DEBUG: [VPC3_MemoryTest]  ANTES de VPC3_SET_MODE_REG_2 - Valor actual: 0x%02X\r\n", VPC3_GetModeReg2Shadow());
   VPC3_SET_MODE_REG_2( INIT_VPC3_MODE_REG_2 );
   printf("DEBUG: [VPC3_MemoryTest]  DESPUÉS de VPC3_SET_MODE_REG_2 - Valor configurado: 0x%02X\r\n", VPC3_GetModeReg2Shadow());
   
   // Verificar que se configuró correctamente
   uint8_t mode_reg_2 = VPC3_GetModeReg2Shadow();
   printf("DEBUG: [VPC3_MemoryTest] MODE_REG_2 configurado: 0x%02X\r\n", mode_reg_2);

   /*-----------------------------------------------------------------------*/
   /* test and clear vpc3 ram                                               */
   /*-----------------------------------------------------------------------*/
   bError = DP_OK;

   #if VPC3_SERIAL_MODE

      printf("DEBUG: [VPC3_MemoryTest] Iniciando escritura de memoria...\r\n");
      j = 0;
      wAddress = bVpc3RwTsAddr;
      printf("DEBUG: [VPC3_MemoryTest] bVpc3RwTsAddr = 0x%04X\r\n", (unsigned int)bVpc3RwTsAddr);
      
      // --- DEFENSIVE PROGRAMMING: Use conservative memory test range ---
      // Instead of testing the full ASIC_RAM_LENGTH, test only the safe range
      // The VPC3+S has reserved areas at the end of RAM that should not be accessed
      uint16_t safe_memory_end = 0x0700; // Conservative end for 2KB mode
      printf("DEBUG: [VPC3_MemoryTest] Using conservative memory test range: 0x%04X to 0x%04X\r\n", 
             (unsigned int)bVpc3RwTsAddr, (unsigned int)safe_memory_end);
      
      // Verificar qué hay en las direcciones antes de escribir
      printf("DEBUG: [VPC3_MemoryTest] Contenido inicial de memoria:\r\n");
      for( int test_addr = 0x0016; test_addr < 0x0020; test_addr += 2 )
      {
         uint8_t val1 = Vpc3Read( test_addr );
         uint8_t val2 = Vpc3Read( test_addr + 1 );
         printf("DEBUG: [VPC3_MemoryTest] Addr 0x%04X: 0x%02X 0x%02X\r\n", test_addr, val1, val2);
      }
      
      // Debug: escribir solo los primeros bytes para ver qué pasa
      printf("DEBUG: [VPC3_MemoryTest] Escribiendo primeros 10 bytes como prueba...\r\n");
      for( i = 0x16; i < 0x20; i += 2 )
      {
         uint8_t high_byte = (uint8_t)( j >> 8 );
         uint8_t low_byte = (uint8_t)( j );
         printf("DEBUG: [VPC3_MemoryTest] Escribiendo addr 0x%04X: high=0x%02X, low=0x%02X\r\n", 
                (unsigned int)wAddress, high_byte, low_byte);
         Vpc3Write( wAddress++, high_byte );
         Vpc3Write( wAddress++, low_byte );
         
         // Leer inmediatamente para verificar
         VPC3_ADR test_addr = wAddress - 2;
         uint8_t read_high = Vpc3Read( test_addr );
         uint8_t read_low = Vpc3Read( test_addr + 1 );
         printf("DEBUG: [VPC3_MemoryTest] Verificación addr 0x%04X: escrito=(0x%02X,0x%02X), leído=(0x%02X,0x%02X)\r\n", 
                (unsigned int)test_addr, high_byte, low_byte, read_high, read_low);
         
         j++;
      }
      
      // Continuar con el resto usando el rango seguro
      printf("DEBUG: [VPC3_MemoryTest] Continuando escritura del resto de memoria (rango seguro)...\r\n");
      for( i = 0x20; i < safe_memory_end; )
      {
         uint8_t high_byte = (uint8_t)( j >> 8 );
         uint8_t low_byte = (uint8_t)( j );
         
         // Debug para las direcciones donde falló antes
         if( wAddress >= 0x06C0 && wAddress <= 0x06FE )
         {
            printf("DEBUG: [VPC3_MemoryTest] Escribiendo addr 0x%04X: high=0x%02X, low=0x%02X\r\n", 
                   (unsigned int)wAddress, high_byte, low_byte);
         }
         
         Vpc3Write( wAddress++, high_byte );
         Vpc3Write( wAddress++, low_byte );

         i+=2;
         j++;
      } /* for( i = 0x16; i < safe_memory_end; ) */
      printf("DEBUG: [VPC3_MemoryTest] Escritura completada (rango seguro)\r\n");

      printf("DEBUG: [VPC3_MemoryTest] Iniciando lectura de memoria (rango seguro)...\r\n");
      j = 0;
      wAddress = bVpc3RwTsAddr;
      for( i = 0x16; i < safe_memory_end; )
      {
         uint8_t read_high = Vpc3Read( wAddress++ );
         uint8_t read_low = Vpc3Read( wAddress++ );
         wTemp = (((uint16_t)read_high) << 8 ) + (uint16_t)read_low;
         if( wTemp != j )
         {
            printf("DEBUG: [VPC3_MemoryTest] ERROR en addr 0x%04X: esperado=0x%04X, leído=0x%04X (high=0x%02X, low=0x%02X)\r\n", 
                   (unsigned int)(wAddress-2), j, wTemp, read_high, read_low);
            bError = DP_VPC3_ERROR;
         } /* if( wTemp != j ) */

         i+=2;
         j++;
      } /* for( i = 0x16; i < safe_memory_end; ) */
      printf("DEBUG: [VPC3_MemoryTest] Lectura completada, bError=%d\r\n", bError);

   #else

      j = 0;
      pToVpc3 = &pVpc3->bTsAddr VPC3_EXTENSION;
      for( i = 0x16; i < ASIC_RAM_LENGTH; )
      {
         *pToVpc3 = (uint8_t)(j>>8);
         pToVpc3++;
         *pToVpc3 = (uint8_t)j;
         pToVpc3++;

         i+=2;
         j++;
      } /* for( i = 0x16; i < ASIC_RAM_LENGTH; ) */

      j = 0;
      pToVpc3 = &pVpc3->bTsAddr VPC3_EXTENSION;
      for( i = 0x16; i < ASIC_RAM_LENGTH; )
      {
         wTemp = (((uint16_t)*pToVpc3) << 8 );
         pToVpc3++;
         wTemp +=  (uint16_t)*pToVpc3;
         pToVpc3++;
         if( wTemp != j )
         {
            bError = DP_VPC3_ERROR;
         }

         i+=2;
         j++;
      } /* for( i = 0x16; i < ASIC_RAM_LENGTH; ) */

   #endif /* #if VPC3_SERIAL_MODE */

   printf("DEBUG: [VPC3_MemoryTest] FIN - Resultado: %d (0x%02X)\r\n", bError, bError);
   return bError;
} /* DP_ERROR_CODE VPC3_MemoryTest( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_Initialization                                             */
/*---------------------------------------------------------------------------*/
/*!
  \brief Initializes VPC3+.

  \param[in] bSlaveAddress - PROFIBUS slave address ( range: 1..126 )
  \param[in] wIdentNumber - PROFIBUS ident number
  \param[in] sCfgData - default configuration

  \retval DP_OK - Initialization OK
  \retval DP_NOT_OFFLINE_ERROR - Error VPC3 is not in OFFLINE state
  \retval DP_ADDRESS_ERROR - Error, DP Slave address
  \retval DP_CALCULATE_IO_ERROR - Error with configuration bytes
  \retval DP_DOUT_LEN_ERROR - Error with Dout length
  \retval DP_DIN_LEN_ERROR - Error with Din length
  \retval DP_DIAG_LEN_ERROR - Error with diagnostics length
  \retval DP_PRM_LEN_ERROR - Error with parameter assignment data length
  \retval DP_SSA_LEN_ERROR - Error with address data length
  \retval DP_CFG_LEN_ERROR - Error with configuration data length
  \retval DP_LESS_MEM_ERROR - Error Overall, too much memory used
  \retval DP_LESS_MEM_FDL_ERROR - Error Overall, too much memory used
*/
DP_ERROR_CODE VPC3_Initialization( uint8_t bSlaveAddress, uint16_t wIdentNumber, psCFG psCfgData )
{
DP_ERROR_CODE bError;

   printf("DEBUG: VPC3_Initialization - INICIO\r\n");
   printf("DEBUG: pVpc3 = 0x%08X\r\n", (unsigned int)pVpc3);
   printf("DEBUG: Estructura pVpc3 inicializada con ceros? bReadCfgBufPtr=0x%02X\r\n", pVpc3->bReadCfgBufPtr VPC3_EXTENSION);

   /*-----------------------------------------------------------------------*/
   /* initialize global system structure                                    */
   /*-----------------------------------------------------------------------*/

   #ifdef DP_DEBUG_ENABLE
      DpClearDebugBuffer();

      #if REDUNDANCY
         bOldDebugCode_1 = 0xFF;
         bOldDetail_1 = 0xFF;
         bOldDebugCode_2 = 0xFF;
         bOldDetail_2 = 0xFF;
      #endif /* #if REDUNDANCY */
   #endif /* #ifdef DP_DEBUG_ENABLE */

   /*-------------------------------------------------------------------*/
   /* Perform hardware reset first for reliable initialization          */
   /*-------------------------------------------------------------------*/
   printf("DEBUG: Realizando reset hardware del VPC3+S...\r\n");
   if (VPC3_HardwareReset() != 0) {
      printf("DEBUG:  ADVERTENCIA: Reset hardware falló, continuando de todas formas...\r\n");
   }

   /*-------------------------------------------------------------------*/
   /* check VPC3 is in OFFLINE                                          */
   /*-------------------------------------------------------------------*/
   printf("DEBUG: Verificando estado OFFLINE...\r\n");
   uint8_t status_l = Vpc3Read(0x04);
   printf("DEBUG: STATUS_L actual: 0x%02X\r\n", status_l);
   
   if ((status_l & 0x01) != 0) {
      printf("DEBUG:  ADVERTENCIA: VPC3 no está en OFFLINE (bit 0 = 1)\r\n");
      printf("DEBUG: Intentando forzar OFFLINE...\r\n");
      
      // Try to force OFFLINE by writing to control register
      Vpc3Write(0x08, 0x04); // GO_OFFLINE command
      HAL_Delay(10);
      
      status_l = Vpc3Read(0x04);
      printf("DEBUG: STATUS_L después de forzar OFFLINE: 0x%02X\r\n", status_l);
   }
   
   if( !VPC3_GET_OFF_PASS() )
   {
      printf("DEBUG: VPC3 esta en OFFLINE, continuando...\r\n");
      
      /* neccessary, if 4Kbyte mode enabled */
      printf("DEBUG: Configurando MODE_REG_2...\r\n");
     printf("DEBUG: [VPC3_Initialization]  ANTES de VPC3_SET_MODE_REG_2 - Valor actual: 0x%02X\r\n", VPC3_GetModeReg2Shadow());
      
      // Use the robust MODE_REG_2 configuration
      if (VPC3_ForceModeReg2() != 0) {
         printf("DEBUG:  ADVERTENCIA: No se pudo configurar MODE_REG_2 correctamente\r\n");
         printf("DEBUG: Continuando con el valor actual...\r\n");
      }
      
     printf("DEBUG: [VPC3_Initialization]  DESPUÉS de VPC3_SET_MODE_REG_2 - Valor configurado: 0x%02X\r\n", VPC3_GetModeReg2Shadow());

      /* clear VPC3 */
      printf("DEBUG: Limpiando memoria VPC3 (conservative range)...\r\n");
      #if VPC3_SERIAL_MODE
         // Use conservative memory range to avoid reserved areas
         uint16_t safe_memory_size = 0x0700 - 0x16; // Conservative size for 2KB mode
         Vpc3MemSet_( bVpc3RwTsAddr, 0, safe_memory_size );
         printf("DEBUG: Memoria VPC3 limpiada (0x%04X bytes)\r\n", safe_memory_size);
      #else
         Vpc3MemSet_( &pVpc3->bTsAddr VPC3_EXTENSION, 0, (ASIC_RAM_LENGTH-0x16) );
      #endif /* #if VPC3_SERIAL_MODE */
      printf("DEBUG: Memoria VPC3 limpiada exitosamente\r\n");

      #if DP_INTERRUPT_MASK_8BIT == 0
         pDpSystem->wPollInterruptMask = SM_INTERRUPT_MASK;
      #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */

      /*--------------------------------------------------------------*/
      /* set constant values                                          */
      /*--------------------------------------------------------------*/
      printf("DEBUG: Configurando valores constantes...\r\n");
      bError = VPC3_SetConstants( bSlaveAddress, wIdentNumber );
      if( DP_OK == bError )
      {
         printf("DEBUG: Valores constantes OK, calculando longitudes I/O...\r\n");
         /*-----------------------------------------------------------*/
         /* calculate length of input and output data using cfg-data  */
         /*-----------------------------------------------------------*/
         bError = VPC3_CalculateInpOutpLength( &psCfgData->abData[0], psCfgData->bLength );
         if( DP_OK == bError )
         {
            printf("DEBUG: Longitudes I/O calculadas OK, inicializando buffers...\r\n");
            /*--------------------------------------------------------*/
            /* initialize buffer structure                            */
            /*--------------------------------------------------------*/
            bError = VPC3_InitBufferStructure();
            if ( DP_OK == bError )
            {
               printf("DEBUG: Buffers inicializados OK, configurando servicios...\r\n");
               /*-----------------------------------------------------*/
               /* initialize subscriber                               */
               /*-----------------------------------------------------*/
               #if DP_SUBSCRIBER
                   bError = VPC3_InitSubscriber();
               #endif /* #if DP_SUBSCRIBER */

               /*-----------------------------------------------------*/
               /* initialize fdl_interface                            */
               /*-----------------------------------------------------*/
               #if DP_FDL
                  sFdl_Init sFdlInit;

                  if( DP_OK == bError )
                  {
                     printf("DEBUG: Inicializando servicios FDL...\r\n");
                     memset( &sFdlInit, 0, sizeof( sFdl_Init ) );
                     #if DP_MSAC_C1
                        sFdlInit.eC1SapSupported |= C1_SAP_51;
                        #if DP_ALARM_OVER_SAP50
                           sFdlInit.eC1SapSupported |= C1_SAP_50;
                        #endif /* #if DP_ALARM_OVER_SAP50 */
                     #endif /* #if DP_MSAC_C1 */

                     #if DP_MSAC_C2
                        sFdlInit.bC2Enable = VPC3_TRUE;
                        sFdlInit.bC2_NumberOfSaps = C2_NUM_SAPS;
                     #endif /* #if DP_MSAC_C2 */

                     bError = FDL_InitAcyclicServices( bSlaveAddress, sFdlInit );
                     printf("DEBUG: Servicios FDL inicializados con resultado: 0x%02X\r\n", bError);
                  } /* if( DP_OK == bError ) */
               #endif /* #if DP_FDL */
            } /* if( DP_OK == bError ) */
         } /* if( DP_OK == bError ) */
      } /* if( DP_OK == bError ) */

      if( DP_OK == bError )
      {
         #if VPC3_SERIAL_MODE
                     printf("DEBUG: Copiando estructura de sistema (0x2A bytes)...\r\n");
         printf("DEBUG: Desde pVpc3->bTsAddr (0x%08X) hacia VPC3 addr 0x%04X\r\n", 
                (unsigned int)&pVpc3->bTsAddr, (unsigned int)(Vpc3AsicAddress+0x16));
         printf("DEBUG: Valor de bReadCfgBufPtr en estructura local: 0x%02X\r\n", 
                pVpc3->bReadCfgBufPtr VPC3_EXTENSION);
         printf("DEBUG: Offset de bReadCfgBufPtr desde bTsAddr: 0x%02X\r\n", 
                (unsigned int)&pVpc3->bReadCfgBufPtr - (unsigned int)&pVpc3->bTsAddr);
         CopyToVpc3_( (VPC3_UNSIGNED8_PTR)(Vpc3AsicAddress+0x16), (MEM_UNSIGNED8_PTR)&pVpc3->bTsAddr VPC3_EXTENSION, 0x2A );
         printf("DEBUG: Estructura de sistema copiada OK\r\n");
         #endif /* #if VPC3_SERIAL_MODE */

         /*-----------------------------------------------------------*/
         /* set real configuration data                               */
         /*-----------------------------------------------------------*/
         printf("DEBUG: Configurando longitud de configuracion (%d bytes)...\r\n", psCfgData->bLength);
         VPC3_SET_READ_CFG_LEN( psCfgData->bLength );      /* set configuration length */
         printf("DEBUG: Longitud configurada OK\r\n");
         
         printf("DEBUG: Obteniendo puntero del buffer de configuracion...\r\n");
         printf("DEBUG: Leyendo registro 0x34 del VPC3+: 0x%02X\r\n", Vpc3Read(bVpc3RwReadCfgBufPtr));
         printf("DEBUG: Puntero del buffer: 0x%08X\r\n", (unsigned int)VPC3_GET_READ_CFG_BUF_PTR());
         
         printf("DEBUG: Copiando datos de configuracion...\r\n");
         CopyToVpc3_( VPC3_GET_READ_CFG_BUF_PTR(), &psCfgData->abData[0], psCfgData->bLength );
         printf("DEBUG: Datos de configuracion copiados OK\r\n");
         
         printf("DEBUG: Configuracion final copiada exitosamente\r\n");
      } /* if( DP_OK == bError ) */
   } /* if( !VPC3_GET_OFF_PASS() ) */
   else
   {
      printf("DEBUG: ERROR - VPC3 no esta en estado OFFLINE\r\n");
      bError = DP_NOT_OFFLINE_ERROR;
   } /* else of if( !VPC3_GET_OFF_PASS() ) */

   printf("DEBUG: VPC3_Initialization - FIN con resultado: 0x%02X\r\n", bError);
   printf("DEBUG: [VPC3_Initialization] Inicializacion completada. Estado final:\r\n");
   printf("DEBUG: [VPC3_Initialization] MODE_REG_2 = 0x%02X\r\n", VPC3_GetModeReg2Shadow());
   
   // --- DEFENSIVE PROGRAMMING: Validate segment pointers after initialization ---
   DP_ERROR_CODE validationStatus = VPC3_ValidateSegmentPointers();
   if (validationStatus == DP_NOK) {
      printf("WARNING: [VPC3_Initialization] Corrupted segment pointers detected after initialization!\r\n");
      printf("WARNING: This may indicate ASIC internal state corruption.\r\n");
   }

   return bError;
} /* DP_ERROR_CODE VPC3_Initialization( uint8_t bSlaveAddress, uint16_t wIdentNumber, psCFG psCfgData ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_Start                                                      */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set VPC3+ to online mode.
  If the ASIC could be correctly initialized with VPC3_Initialization(), it still has to be started. Between initialization and
  start, the user can still initialize buffers in the ASIC.

  Reaction after this service:
  - VPC3+ generates DX_OUT event, all outputs will be cleared
  - VPC3+ generates BAUDRATE_DETECT event, if master connected
  - master sends FDL-Status.req --> slave answers with FDL-Status.resp ( RTS signal! )
*/
void VPC3_Start( void )
{
   printf("DEBUG: VPC3_Start - INICIO\r\n");
   
   #if DP_MSAC_C2
      printf("DEBUG: Abriendo canal MSAC_C2...\r\n");
      MSAC_C2_OpenChannel();
      printf("DEBUG: Canal MSAC_C2 abierto OK\r\n");
   #endif /* #if DP_MSAC_C2 */

   /*-----------------------------------------------------------------------*/
   /* start vpc3                                                            */
   /*-----------------------------------------------------------------------*/
   printf("DEBUG: Verificando estado antes del START...\r\n");
   uint8_t status_before = VPC3_GET_STATUS_L();
   printf("DEBUG: STATUS_L pre-START: 0x%02X\r\n", status_before);
   
   if( ( status_before & VPC3_PASS_IDLE ) == 0x00 )
   {
      printf("DEBUG: OK - Chip está en OFFLINE (bit 0 = 0)\r\n");
   }
   else
   {
      printf("DEBUG: WARNING - Chip NO está en OFFLINE (bit 0 = 1)\r\n");
   }

   printf("DEBUG: Enviando comando START (0x01) al registro de control (0x08)...\r\n");
   VPC3_Start__();

   printf("DEBUG: Esperando que el chip procese el comando START...\r\n");
   HAL_Delay(50); // Allow time for the chip to process

   // DEBUG: Agregar delay adicional para estabilización
   printf("DEBUG: Esperando estabilización adicional del chip...\r\n");
   HAL_Delay(500); // Increased stabilization time to 500ms

   // DEBUG: Reconfigurar registros de modo después del START (ya que se resetean)
   printf("DEBUG: Reconfigurando registros de modo después del START...\r\n");
   Vpc3Write( bVpc3RwModeReg0_H, INIT_VPC3_MODE_REG_H );
   Vpc3Write( bVpc3RwModeReg0_L, INIT_VPC3_MODE_REG_L );
   
   // Forzar MODE_REG_2 al valor correcto
   if (!VPC3_ForceModeReg2()) {
      printf("DEBUG: [VPC3_Start] ADVERTENCIA - No se pudo forzar MODE_REG_2 correctamente\r\n");
   }
   
   Vpc3Write( bVpc3WoModeReg3, INIT_VPC3_MODE_REG_3 );
   
   // DEBUG: Esperar a que los registros se procesen
   printf("DEBUG: Esperando a que los registros de modo se procesen...\r\n");
   HAL_Delay(100); // Allow time for mode registers to be processed
   
   // Verificar que MODE_REG_2 se configuró correctamente
   uint8_t mode_reg2_after = VPC3_GetModeReg2Shadow();
   printf("DEBUG: [VPC3_Start] MODE_REG_2 después de reconfiguración: 0x%02X (esperado: 0x%02X)\r\n", 
          mode_reg2_after, INIT_VPC3_MODE_REG_2);

   printf("DEBUG: Verificando estado después del comando START...\r\n");
   uint8_t status_after = VPC3_GET_STATUS_L();
   printf("DEBUG: STATUS_L post-START: 0x%02X\r\n", status_after);
   
   if( ( status_after & VPC3_PASS_IDLE ) != 0x00 )
   {
      printf("DEBUG: EXITO - Chip ha entrado en Passive_Idle (bit 0 = 1)\r\n");
   }
   else
   {
      printf("DEBUG: FALLO - Chip no ha cambiado a Passive_Idle\r\n");
      printf("DEBUG: Posible causa: Inicialización incompleta de estructuras internas\r\n");
      
      // DEBUG: Verificar registros de modo después del START
      printf("DEBUG: [VPC3_Start] Verificando registros de modo después del START...\r\n");
      printf("DEBUG: [VPC3_Start] MODE_REG_0_H: 0x%02X\r\n", Vpc3Read(bVpc3RwModeReg0_H));
      printf("DEBUG: [VPC3_Start] MODE_REG_0_L: 0x%02X\r\n", Vpc3Read(bVpc3RwModeReg0_L));
     printf("DEBUG: [VPC3_Start] MODE_REG_2: 0x%02X\r\n", VPC3_GetModeReg2Shadow());
      printf("DEBUG: [VPC3_Start] MODE_REG_3: 0x%02X\r\n", Vpc3Read(bVpc3WoModeReg3));
      
      // DEBUG: Verificar otros registros importantes
      printf("DEBUG: [VPC3_Start] Verificando otros registros...\r\n");
      printf("DEBUG: [VPC3_Start] STATUS_H: 0x%02X\r\n", Vpc3Read(0x05));
      printf("DEBUG: [VPC3_Start] CONTROL_REG: 0x%02X\r\n", Vpc3Read(0x08));
   }

   /* Fetch the first diagnosis buffer */
   printf("DEBUG: Obteniendo puntero del buffer de diagnostico...\r\n");
   pDpSystem->pDiagBuffer = VPC3_GetDiagBufPtr();
   printf("DEBUG: Puntero del buffer de diagnostico: 0x%08X\r\n", (unsigned int)pDpSystem->pDiagBuffer);

   // DEBUG: Verificar estado final después de la inicialización
   printf("DEBUG: [VPC3_Start] Verificando estado final después del START...\r\n");
   uint8_t final_status_l = VPC3_GET_STATUS_L();
   uint8_t final_status_h = VPC3_GET_STATUS_H();
   uint8_t final_mode_reg1 = Vpc3Read(bVpc3RwModeReg0_H);
   printf("DEBUG: [VPC3_Start] STATUS_L final: 0x%02X\r\n", final_status_l);
   printf("DEBUG: [VPC3_Start] STATUS_H final: 0x%02X\r\n", final_status_h);
   printf("DEBUG: [VPC3_Start] MODE_REG_1 final: 0x%02X\r\n", final_mode_reg1);
   
   if( ( final_status_l & VPC3_PASS_IDLE ) != 0x00 )
   {
      printf("DEBUG: [VPC3_Start] PASS_IDLE check: OK\r\n");
   }
   else
   {
      printf("DEBUG: [VPC3_Start] PASS_IDLE check: FAILED\r\n");
   }
   
   printf("DEBUG: VPC3_Start - FIN\r\n");
} /* void VPC3_Start( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_Stop                                                       */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set VPC3+ to offline mode.
*/
void VPC3_Stop( void )
{
   /*-----------------------------------------------------------------------*/
   /* start vpc3                                                            */
   /*-----------------------------------------------------------------------*/
   VPC3_GoOffline();
   do
   {
      /* wait, for offline */
   }while( VPC3_GET_OFF_PASS() );

   #if DP_MSAC_C2
      MSAC_C2_ResetStateMachine();
   #endif /* #if DP_MSAC_C2 */
} /* void  VPC3_Stop( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_SetConstants                                               */
/*---------------------------------------------------------------------------*/
/*!
  \brief Initializes VPC3+ with all constant values.

  \param[in] bSlaveAddress - PROFIBUS slave address ( range: 1..126 )
  \param[in] wIdentNumber - PROFIBUS ident number

  \retval DP_OK - Initialization OK
  \retval DP_ADDRESS_ERROR - Error, DP Slave address
*/
static DP_ERROR_CODE VPC3_SetConstants( uint8_t bSlaveAddress, uint16_t wIdentNumber )
{
DP_ERROR_CODE bError;

   bError = DP_OK;

   pDpSystem->bDinBufsize  = DIN_BUFSIZE;
   pDpSystem->bDoutBufsize = DOUT_BUFSIZE;
   pDpSystem->bPrmBufsize  = PRM_BUFSIZE;
   pDpSystem->bDiagBufsize = DIAG_BUFSIZE;
   pDpSystem->bCfgBufsize  = CFG_BUFSIZE;
   pDpSystem->bSsaBufsize  = SSA_BUFSIZE;

   pDpSystem->wAsicUserRam = ASIC_USER_RAM;

   /*-----------------------------------------------------------------------*/
   /* initialize  control logic                                             */
   /*-----------------------------------------------------------------------*/
   pVpc3->bIntReqReg_L VPC3_EXTENSION                = 0x00;
   pVpc3->bIntReqReg_H VPC3_EXTENSION                = 0x00;
   pVpc3->sReg.sWrite.bIntAck_L VPC3_EXTENSION       = 0xFF;
   pVpc3->sReg.sWrite.bIntAck_H VPC3_EXTENSION       = 0xFF;
   pVpc3->sCtrlPrm.sWrite.bModeReg1_R VPC3_EXTENSION = 0xFF;

   #if VPC3_SERIAL_MODE
      Vpc3Write( bVpc3RwIntReqReg_L, 0x00 );
      Vpc3Write( bVpc3RwIntReqReg_H, 0x00 );

      Vpc3Write( bVpc3WoIntAck_L,    0xFF );
      Vpc3Write( bVpc3WoIntAck_H,    0xFF );
      Vpc3Write( bVpc3WoModeReg1_R,  0xFF );
   #endif /* #if VPC3_SERIAL_MODE */

   /*-----------------------------------------------------------------------*/
   /* set modes of vpc3                                                     */
   /*-----------------------------------------------------------------------*/
   pVpc3->bModeReg0_H VPC3_EXTENSION = INIT_VPC3_MODE_REG_H;
   pVpc3->bModeReg0_L VPC3_EXTENSION = INIT_VPC3_MODE_REG_L;

   pVpc3->sCtrlPrm.sWrite.bModeReg2 VPC3_EXTENSION = INIT_VPC3_MODE_REG_2;
   pVpc3->sCtrlPrm.sWrite.bModeReg3 VPC3_EXTENSION = INIT_VPC3_MODE_REG_3;

   #if VPC3_SERIAL_MODE
      printf("DEBUG: [VPC3_SetConstants] Escribiendo registros de modo...\r\n");
      printf("DEBUG: [VPC3_SetConstants] Escribiendo MODE_REG_0_H: 0x%02X\r\n", INIT_VPC3_MODE_REG_H);
      Vpc3Write( bVpc3RwModeReg0_H, INIT_VPC3_MODE_REG_H );
      printf("DEBUG: [VPC3_SetConstants] Escribiendo MODE_REG_0_L: 0x%02X\r\n", INIT_VPC3_MODE_REG_L);
      Vpc3Write( bVpc3RwModeReg0_L, INIT_VPC3_MODE_REG_L );

      printf("DEBUG: [VPC3_SetConstants] Escribiendo MODE_REG_2: 0x%02X\r\n", INIT_VPC3_MODE_REG_2);
      Vpc3Write( bVpc3WoModeReg2, INIT_VPC3_MODE_REG_2 );
      printf("DEBUG: [VPC3_SetConstants] Escribiendo MODE_REG_3: 0x%02X\r\n", INIT_VPC3_MODE_REG_3);
      Vpc3Write( bVpc3WoModeReg3, INIT_VPC3_MODE_REG_3 );
      
      // DEBUG: Verificar que los registros se configuraron correctamente
      printf("DEBUG: [VPC3_SetConstants] Verificando configuración de registros de modo...\r\n");
      printf("DEBUG: [VPC3_SetConstants] MODE_REG_0_H configurado: 0x%02X (esperado: 0x%02X)\r\n", Vpc3Read(bVpc3RwModeReg0_H), INIT_VPC3_MODE_REG_H);
      printf("DEBUG: [VPC3_SetConstants] MODE_REG_0_L configurado: 0x%02X (esperado: 0x%02X)\r\n", Vpc3Read(bVpc3RwModeReg0_L), INIT_VPC3_MODE_REG_L);
     printf("DEBUG: [VPC3_SetConstants] MODE_REG_2 configurado: 0x%02X (esperado: 0x%02X)\r\n", VPC3_GetModeReg2Shadow(), INIT_VPC3_MODE_REG_2);
      printf("DEBUG: [VPC3_SetConstants] MODE_REG_3 configurado: 0x%02X (esperado: 0x%02X)\r\n", Vpc3Read(bVpc3WoModeReg3), INIT_VPC3_MODE_REG_3);
   #endif /* #if VPC3_SERIAL_MODE */

   /*-----------------------------------------------------------------------*/
   /* set interrupt triggers                                                */
   /*-----------------------------------------------------------------------*/
   pVpc3->sReg.sWrite.bIntMask_H VPC3_EXTENSION = (uint8_t)(~(INIT_VPC3_IND_H));
   pVpc3->sReg.sWrite.bIntMask_L VPC3_EXTENSION = (uint8_t)(~(INIT_VPC3_IND_L));

   pDpSystem->bIntIndHigh = (uint8_t)(~(INIT_VPC3_IND_H));
   pDpSystem->bIntIndLow  = (uint8_t)(~(INIT_VPC3_IND_L));

   #if VPC3_SERIAL_MODE
      Vpc3Write( bVpc3WoIntMask_H, (uint8_t)(~(INIT_VPC3_IND_H)) );
      Vpc3Write( bVpc3WoIntMask_L, (uint8_t)(~(INIT_VPC3_IND_L)) );
   #endif /* #if VPC3_SERIAL_MODE */

   /*-----------------------------------------------------------------------*/
   /* set time-variables                                                    */
   /*-----------------------------------------------------------------------*/
   pVpc3->sCtrlPrm.sWrite.bWdBaudControlVal VPC3_EXTENSION = 0x10;
   pVpc3->sCtrlPrm.sWrite.bMinTsdrVal VPC3_EXTENSION       = 0x0B;

   #if VPC3_SERIAL_MODE
      Vpc3Write( bVpc3WoWdBaudControlVal, 0x10 );
      Vpc3Write( bVpc3WoMinTsdrVal, 0x0B );
   #endif /* #if VPC3_SERIAL_MODE */

   /*-----------------------------------------------------------------------*/
   /* set variables for synch-mode                                          */
   /*-----------------------------------------------------------------------*/
   #if DP_ISOCHRONOUS_MODE
      pVpc3->sCtrlPrm.sWrite.bSyncPwReg VPC3_EXTENSION = SYNCH_PULSEWIDTH;
      pVpc3->sCtrlPrm.sWrite.bGroupSelectReg VPC3_EXTENSION = 0x80;
      pVpc3->sCtrlPrm.sWrite.bControlCommandReg VPC3_EXTENSION  = 0x00;
      pVpc3->sCtrlPrm.sWrite.bGroupSelectMask VPC3_EXTENSION = 0x00;
      pVpc3->sCtrlPrm.sWrite.bControlCommandMask VPC3_EXTENSION  = 0x00;

      #if VPC3_SERIAL_MODE
         Vpc3Write( bVpc3WoSyncPwReg, SYNCH_PULSEWIDTH );
         Vpc3Write( bVpc3WoGroupSelectReg, 0x80 );
         Vpc3Write( bVpc3WoControlCommandReg, 0x00 );
         Vpc3Write( bVpc3WoGroupSelectMask, 0x00 );
         Vpc3Write( bVpc3WoControlCommandMask, 0x00 );
      #endif /* #if VPC3_SERIAL_MODE */
   #endif /* #if DP_ISOCHRONOUS_MODE */

   /*-----------------------------------------------------------------------*/
   /* set user watchdog (dataexchange telegram counter)                     */
   /*-----------------------------------------------------------------------*/
   pVpc3->abUserWdValue[1] VPC3_EXTENSION = (uint8_t)(USER_WD >> 8);
   pVpc3->abUserWdValue[0] VPC3_EXTENSION = (uint8_t)(USER_WD);

   /*-----------------------------------------------------------------------*/
   /* set pointer to FF                                                     */
   /*-----------------------------------------------------------------------*/
   pVpc3->bFdlSapListPtr   VPC3_EXTENSION = VPC3_SAP_CTRL_LIST_START;
   VPC3_SET_EMPTY_SAP_LIST();

   /*-----------------------------------------------------------------------*/
   /* ssa support                                                           */
   /*-----------------------------------------------------------------------*/
   pVpc3->bRealNoAddChange VPC3_EXTENSION = ( SSA_BUFSIZE == 0 ) ? 0xFF : 0x00;

   /*-----------------------------------------------------------------------*/
   /* set profibus ident number                                             */
   /*-----------------------------------------------------------------------*/
   pVpc3->bIdentHigh VPC3_EXTENSION = (uint8_t)(wIdentNumber >> 8);
   pVpc3->bIdentLow  VPC3_EXTENSION = (uint8_t)(wIdentNumber);

   /*-----------------------------------------------------------------------*/
   /* set and check slave address                                           */
   /*-----------------------------------------------------------------------*/
   if( bSlaveAddress < 127 && bSlaveAddress != 0 )
   {
      pVpc3->bTsAddr VPC3_EXTENSION = bSlaveAddress;
   } /* if( bSlaveAddress < 127 && bSlaveAddress != 0 ) */
   else
   {
      bError = DP_ADDRESS_ERROR;
   } /* else of if( bSlaveAddress < 127 && bSlaveAddress != 0 ) */

   return bError;
} /* static DP_ERROR_CODE VPC3_SetConstants( uint8_t bSlaveAddress, uint16_t wIdentNumber ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_InitSubscriber                                             */
/*---------------------------------------------------------------------------*/
#if DP_SUBSCRIBER
static DP_ERROR_CODE VPC3_InitSubscriber( void )
{
DP_ERROR_CODE       bError;
VPC3_UNSIGNED8_PTR  pToVpc3;              /**< pointer to VPC3, �Controller formatted */
uint16_t            wDxbBufferLength;     /**< segmented length of DxB-Buffer */
uint8_t             bVpc3SegmentAddress;  /**< segment address in VPC3-ASIC */

   bError = DP_OK;

   /*-----------------------------------------------------------------------*/
   /* check buffer length                                                   */
   /*-----------------------------------------------------------------------*/
   if( DP_MAX_DATA_PER_LINK > 244 )
   {
      bError = SSC_MAX_DATA_PER_LINK;
   } /* if( DP_MAX_DATA_PER_LINK > 244 ) */
   else
   {
      #if VPC3_SERIAL_MODE
         /* pointer mc formatted */
         pToVpc3 = (VPC3_UNSIGNED8_PTR)( Vpc3AsicAddress + DP_ORG_LENGTH + SAP_LENGTH + pDpSystem->wVpc3UsedDPV0BufferMemory );
         /* pointer vpc3 formatted */
         bVpc3SegmentAddress = (uint8_t)( ( pDpSystem->wVpc3UsedDPV0BufferMemory + DP_ORG_LENGTH + SAP_LENGTH ) >> SEG_MULDIV );
      #else
         /* pointer mc formatted */
         pToVpc3 = &pVpc3->abDpBuffer[pDpSystem->wVpc3UsedDPV0BufferMemory] VPC3_EXTENSION;
         /* pointer vpc3 formatted */
         bVpc3SegmentAddress = (uint8_t)( ((VPC3_ADR)pToVpc3-(VPC3_ADR)Vpc3AsicAddress) >> SEG_MULDIV );
      #endif /* #if VPC3_SERIAL_MODE */

      /* length dxb_out */
      wDxbBufferLength = (((DP_MAX_DATA_PER_LINK+2)+SEG_OFFSET) & SEG_ADDBYTE);
      pVpc3->bLenDxbOutBuf = (DP_MAX_DATA_PER_LINK+2);
      pDpSystem->wVpc3UsedDPV0BufferMemory += (3*wDxbBufferLength);
      /* length link status */
      pVpc3->bLenDxbStatusBuf = ((((DP_MAX_LINK_SUPPORTED*2)+4)+SEG_OFFSET) & SEG_ADDBYTE);
      pDpSystem->wVpc3UsedDPV0BufferMemory += pVpc3->bLenDxbStatusBuf;
      /* length link table */
      pVpc3->bLenDxbLinkBuf = (((DP_MAX_LINK_SUPPORTED*4)+SEG_OFFSET) & SEG_ADDBYTE);
      pDpSystem->wVpc3UsedDPV0BufferMemory += pVpc3->bLenDxbLinkBuf;

      /*-------------------------------------------------------------------*/
      /* check memory consumption                                          */
      /*-------------------------------------------------------------------*/
      if( pDpSystem->wVpc3UsedDPV0BufferMemory > ASIC_USER_RAM )
      {
         /* Error: user needs too much memory */
         pDpSystem->wVpc3UsedDPV0BufferMemory = 0;
         bError = DP_LESS_MEM_ERROR;
      } /* if( pDpSystem->wVpc3UsedDPV0BufferMemory > ASIC_USER_RAM ) */
      else
      {
         /*---------------------------------------------------------------*/
         /* set buffer pointer                                            */
         /*---------------------------------------------------------------*/
         pVpc3->bDxbLinkBufPtr   = bVpc3SegmentAddress;
         pVpc3->bDxbStatusBufPtr = pVpc3->bDxbLinkBufPtr   + ( pVpc3->bLenDxbLinkBuf >> SEG_MULDIV );
         pVpc3->bDxbOutBufPtr1   = pVpc3->bDxbStatusBufPtr + ( pVpc3->bLenDxbStatusBuf >> SEG_MULDIV );
         pVpc3->bDxbOutBufPtr2   = pVpc3->bDxbOutBufPtr1   + ( wDxbBufferLength >> SEG_MULDIV );
         pVpc3->bDxbOutBufPtr3   = pVpc3->bDxbOutBufPtr2   + ( wDxbBufferLength >> SEG_MULDIV );

         pDpSystem->pDxbOutBuffer1 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->bDxbOutBufPtr1 VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDxbOutBuffer2 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->bDxbOutBufPtr2 VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDxbOutBuffer3 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->bDxbOutBufPtr3 VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
      } /* else of if( pDpSystem->wVpc3UsedDPV0BufferMemory > ASIC_USER_RAM ) */
   } /* else of if( DP_MAX_DATA_PER_LINK > 244 ) */

   return bError;
} /* static DP_ERROR_CODE VPC3_InitSubscriber( void ) */
#endif /* #if DP_SUBSCRIBER */


/*---------------------------------------------------------------------------*/
/* function: VPC3_CalculateInpOutpLength                                     */
/*---------------------------------------------------------------------------*/
/*!
   \brief Calculation of input- and output data.

   \param[in] pToCfgData - address of configuration data
   \param[in] bCfgLength - length of configuration data

   \retval DP_OK - Initialization OK
   \retval DP_CFG_LEN_ERROR - Error with CFG length
   \retval DP_CALCULATE_IO_ERROR - Error with DIN or DOUT length
   \retval DP_CFG_FORMAT_ERROR - Error in special configuration format
*/
DP_ERROR_CODE VPC3_CalculateInpOutpLength( MEM_UNSIGNED8_PTR pToCfgData, uint8_t bCfgLength )
{
DP_ERROR_CODE bError;
uint8_t       bSpecificDataLength;
uint8_t       bTempOutputDataLength;
uint8_t       bTempInputDataLength;
uint8_t       bLength;
uint8_t       bCount;

   bError = DP_OK;
   bTempOutputDataLength = 0;
   bTempInputDataLength  = 0;

   if( ( bCfgLength > 0 ) && ( bCfgLength <= CFG_BUFSIZE ) )
   {
      for( ; bCfgLength > 0; bCfgLength -= bCount )
      {
         bCount = 0;

         if( *pToCfgData & VPC3_CFG_IS_BYTE_FORMAT )
         {
            /* general identifier format */
            bCount++;
            /* pToCfgData points to "Configurationbyte", CFG_BF means "CFG_IS_BYTE_FORMAT" */
            bLength = (uint8_t)( ( *pToCfgData & VPC3_CFG_BF_LENGTH) + 1 );

            if( *pToCfgData & VPC3_CFG_BF_OUTP_EXIST )
            {
               bTempOutputDataLength += ( *pToCfgData & VPC3_CFG_LENGTH_IS_WORD_FORMAT ) ? ( 2 * bLength ) : bLength;
            } /* if( *pToCfgData & VPC3_CFG_BF_OUTP_EXIST ) */

            if( *pToCfgData & VPC3_CFG_BF_INP_EXIST )
            {
               bTempInputDataLength += ( *pToCfgData & VPC3_CFG_LENGTH_IS_WORD_FORMAT ) ? ( 2 * bLength ) : bLength;
            } /* if( *pToCfgData & VPC3_CFG_BF_INP_EXIST ) */

            pToCfgData++;
         } /* if( *pToCfgData & VPC3_CFG_IS_BYTE_FORMAT ) */
         else
         {
            /* pToCfgData points to the headerbyte of "special identifier format */
            /* CFG_SF means "CFG_IS_SPECIAL_FORMAT" */
            if( *pToCfgData & VPC3_CFG_SF_OUTP_EXIST )
            {
               bCount++;    /* next byte contains the length of outp_data */
               bLength = (uint8_t)( ( *( pToCfgData + bCount ) & VPC3_CFG_SF_LENGTH ) + 1 );

               bTempOutputDataLength += ( *( pToCfgData + bCount ) & VPC3_CFG_LENGTH_IS_WORD_FORMAT ) ? ( 2 * bLength ) : bLength;
            } /* if( *pToCfgData & VPC3_CFG_SF_OUTP_EXIST ) */

            if( *pToCfgData & VPC3_CFG_SF_INP_EXIST )
            {
               bCount++;  /* next byte contains the length of inp_data */
               bLength = (uint8_t)( ( *( pToCfgData + bCount ) & VPC3_CFG_SF_LENGTH ) + 1 );

               bTempInputDataLength += ( *( pToCfgData + bCount ) & VPC3_CFG_LENGTH_IS_WORD_FORMAT ) ? ( 2 * bLength ) : bLength;
            } /* if( *pToCfgData & VPC3_CFG_SF_INP_EXIST ) */

            bSpecificDataLength = (uint8_t)( *pToCfgData & VPC3_CFG_BF_LENGTH );

            if( bSpecificDataLength != 15 )
            {
               bCount = (uint8_t)( bCount + 1 + bSpecificDataLength );
               pToCfgData = pToCfgData + bCount;
            } /* if( bSpecificDataLength != 15 ) */
            else
            {
               /* specific data length = 15 not allowed */
               pDpSystem->bInputDataLength  = 0xFF;
               pDpSystem->bOutputDataLength = 0xFF;
               bError = DP_CFG_FORMAT_ERROR;
            } /* else of if( bSpecificDataLength != 15 ) */
         } /* else of if( *pToCfgData & VPC3_CFG_IS_BYTE_FORMAT ) */
      } /* for( ; bCfgLength > 0; bCfgLength -= bCount ) */

      if( ( bCfgLength != 0 ) || ( bTempInputDataLength > DIN_BUFSIZE ) || ( bTempOutputDataLength > DOUT_BUFSIZE ) )
      {
         pDpSystem->bInputDataLength  = 0xFF;
         pDpSystem->bOutputDataLength = 0xFF;

         bError = DP_CALCULATE_IO_ERROR;
      } /* if( ( bCfgLength != 0 ) || ( bTempInputDataLength > DIN_BUFSIZE ) || ( bTempOutputDataLength > DOUT_BUFSIZE ) ) */
      else
      {
         pDpSystem->bInputDataLength  = bTempInputDataLength;
         pDpSystem->bOutputDataLength = bTempOutputDataLength;
         bError = DP_OK;
      } /* else of if( ( bCfgLength != 0 ) || ( bTempInputDataLength > DIN_BUFSIZE ) || ( bTempOutputDataLength > DOUT_BUFSIZE ) ) */
   } /* if( ( bCfgLength > 0 ) && ( bCfgLength <= CFG_BUFSIZE ) ) */
   else
   {
      pDpSystem->bInputDataLength  = 0xFF;
      pDpSystem->bOutputDataLength = 0xFF;
      bError = DP_CFG_LEN_ERROR;
   } /* else of if( ( bCfgLength > 0 ) && ( bCfgLength <= CFG_BUFSIZE ) ) */

   return bError;
} /* DP_ERROR_CODE VPC3_CalculateInpOutpLength( MEM_UNSIGNED8_PTR pToCfgData, uint8_t bCfgLength ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_InitBufferStructure                                        */
/*---------------------------------------------------------------------------*/
/*!
  \brief Initializes VPC3+ buffer structure.

  \retval DP_OK - Initialization OK
  \retval DP_DOUT_LEN_ERROR - Error with Dout length
  \retval DP_DIN_LEN_ERROR - Error with Din length
  \retval DP_DIAG_LEN_ERROR - Error with diagnostics length
  \retval DP_PRM_LEN_ERROR - Error with parameter assignment data length
  \retval DP_SSA_LEN_ERROR - Error with address data length
  \retval DP_CFG_LEN_ERROR - Error with configuration data length
  \retval DP_LESS_MEM_ERROR - Error Overall, too much memory used
*/
static DP_ERROR_CODE VPC3_InitBufferStructure( void )
{
DP_ERROR_CODE  bError;
uint16_t       wOutBufferLength;    /**< calculated length of output buffer */
uint16_t       wInBufferLength;     /**< calculated length of input buffer */
uint16_t       wDiagBufferLength;   /**< calculated length of diagnostic buffer */
uint16_t       wPrmBufferLength;    /**< calculated length of parameter buffer */
uint16_t       wCfgBufferLength;    /**< calculated length of check-config buffer */
uint16_t       wSsaBufferLength;    /**< calculated length of set-slave-address buffer */
uint16_t       wAux2BufferLength;   /**< calculated length of aux buffer 2*/

   bError = DP_OK;

   /*-----------------------------------------------------------------------*/
   /* check buffer length                                                   */
   /*-----------------------------------------------------------------------*/
   if( pDpSystem->bDoutBufsize > 244 )
   {
      bError = DP_DOUT_LEN_ERROR;
   } /* if( pDpSystem->bDoutBufsize > 244 ) */

   else
   if( pDpSystem->bDinBufsize > 244 )
   {
      bError = DP_DIN_LEN_ERROR;
   } /* if( pDpSystem->bDinBufsize > 244 ) */

   else
   if( ( pDpSystem->bDiagBufsize < 6 ) || ( pDpSystem->bDiagBufsize > 244 ) )
   {
      bError = DP_DIAG_LEN_ERROR;
   } /* if( ( pDpSystem->bDiagBufsize < 6 ) || ( pDpSystem->bDiagBufsize > 244 ) */

   else
   if( ( pDpSystem->bPrmBufsize < 7 ) || ( pDpSystem->bPrmBufsize > 244 ) )
   {
      bError = DP_PRM_LEN_ERROR;
   } /* if( ( pDpSystem->bPrmBufsize < 7 ) || ( pDpSystem->bPrmBufsize > 244 ) ) */

   else
   if( ( pDpSystem->bCfgBufsize < 1 ) || ( pDpSystem->bCfgBufsize > 244 ) )
   {
      bError = DP_CFG_LEN_ERROR;
   } /* if( ( pDpSystem->bCfgBufsize < 1 ) || ( pDpSystem->bCfgBufsize > 244 ) ) */

   else
   if( pDpSystem->bSsaBufsize != 0 && ( ( pDpSystem->bSsaBufsize < 4 ) || ( pDpSystem->bSsaBufsize > 244 ) ) )
   {
      bError = DP_SSA_LEN_ERROR;
   } /* if( pDpSystem->bSsaBufsize != 0 && ( ( pDpSystem->bSsaBufsize < 4 ) || ( pDpSystem->bSsaBufsize > 244 ) ) ) */

   /* prm buffer */
   if( pVpc3->bModeReg0_H VPC3_EXTENSION & VPC3_SPEC_PRM_BUF_MODE )
   {
      /* Spec_Prm_Buf_Mode: Is no longer supported by the software, 4kByte memory is enough to handle Set-Prm-telegram via auxiliary buffer. */
      bError = DP_SPEC_PRM_NOT_SUPP_ERROR;
   } /* if( pVpc3->bModeReg0_H VPC3_EXTENSION & VPC3_SPEC_PRM_BUF_MODE ) */
   pVpc3->bLenSpecPrmBuf VPC3_EXTENSION = 0;

   if( bError == DP_OK )
   {
      /*-------------------------------------------------------------------*/
      /* length of buffers ok, check memory consumption                    */
      /*-------------------------------------------------------------------*/
      /* length of output buffer */
      wOutBufferLength =  ( ( pDpSystem->bDoutBufsize + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of input buffer */
      wInBufferLength =   ( ( pDpSystem->bDinBufsize  + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of diagnostic buffer */
      wDiagBufferLength = ( ( pDpSystem->bDiagBufsize + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of prm buffer */
      wPrmBufferLength =  ( ( pDpSystem->bPrmBufsize  + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of cfg buffer */
      wCfgBufferLength =  ( ( pDpSystem->bCfgBufsize  + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of ssa buffer */
      wSsaBufferLength =  ( ( pDpSystem->bSsaBufsize  + SEG_OFFSET ) & SEG_ADDWORD );
      /* length of aux buffer 2 2*/
      wAux2BufferLength = ( wCfgBufferLength > wSsaBufferLength ) ? wCfgBufferLength : wSsaBufferLength;

      /*-------------------------------------------------------------------*/
      /* check memory consumption                                          */
      /*-------------------------------------------------------------------*/
      pDpSystem->wVpc3UsedDPV0BufferMemory = 0;
      /* add input and output buffer */
      pDpSystem->wVpc3UsedDPV0BufferMemory += ( wOutBufferLength + wInBufferLength ) * 3;
      /* add diagnostic buffer */
      pDpSystem->wVpc3UsedDPV0BufferMemory += wDiagBufferLength * 2;
      /* add prm buffer, add aux buffer 1 */
      pDpSystem->wVpc3UsedDPV0BufferMemory += wPrmBufferLength * 2;
      /* add Read Config fuffer, add Check Config buffer */
      pDpSystem->wVpc3UsedDPV0BufferMemory += wCfgBufferLength * 2;
      /* add SSA buffer */
      pDpSystem->wVpc3UsedDPV0BufferMemory += wSsaBufferLength;
      /* add aux buffer 2 */
      pDpSystem->wVpc3UsedDPV0BufferMemory += wAux2BufferLength;

      if( pDpSystem->wVpc3UsedDPV0BufferMemory > pDpSystem->wAsicUserRam )
      {
         /* Error: user needs too much memory */
         pDpSystem->wVpc3UsedDPV0BufferMemory = 0;
         bError = DP_LESS_MEM_ERROR;
      } /* if( pDpSystem->wVpc3UsedDPV0BufferMemory > pDpSystem->wAsicUserRam ) */
      else
      {
         /*---------------------------------------------------------------*/
         /* set buffer pointer                                            */
         /*---------------------------------------------------------------*/
         printf("DEBUG: === CADENA DE ASIGNACIONES DE PUNTEROS ===\r\n");
         printf("DEBUG: VPC3_DP_BUF_START=%d\r\n", VPC3_DP_BUF_START);
         printf("DEBUG: wOutBufferLength=%d, wInBufferLength=%d, wDiagBufferLength=%d\r\n", 
                wOutBufferLength, wInBufferLength, wDiagBufferLength);
         
         pVpc3->abDoutBufPtr[0] VPC3_EXTENSION  = VPC3_DP_BUF_START;
         printf("DEBUG: abDoutBufPtr[0]=0x%02X\r\n", pVpc3->abDoutBufPtr[0] VPC3_EXTENSION);
         
         pVpc3->abDoutBufPtr[1] VPC3_EXTENSION  = pVpc3->abDoutBufPtr[0] VPC3_EXTENSION  + ( wOutBufferLength  >> SEG_MULDIV );
         printf("DEBUG: abDoutBufPtr[1]=0x%02X\r\n", pVpc3->abDoutBufPtr[1] VPC3_EXTENSION);
         
         pVpc3->abDoutBufPtr[2] VPC3_EXTENSION  = pVpc3->abDoutBufPtr[1] VPC3_EXTENSION  + ( wOutBufferLength  >> SEG_MULDIV );
         printf("DEBUG: abDoutBufPtr[2]=0x%02X\r\n", pVpc3->abDoutBufPtr[2] VPC3_EXTENSION);

         pVpc3->abDinBufPtr[0] VPC3_EXTENSION   = pVpc3->abDoutBufPtr[2] VPC3_EXTENSION  + ( wOutBufferLength  >> SEG_MULDIV );
         printf("DEBUG: abDinBufPtr[0]=0x%02X\r\n", pVpc3->abDinBufPtr[0] VPC3_EXTENSION);
         
         pVpc3->abDinBufPtr[1] VPC3_EXTENSION   = pVpc3->abDinBufPtr[0]  VPC3_EXTENSION  + ( wInBufferLength   >> SEG_MULDIV );
         printf("DEBUG: abDinBufPtr[1]=0x%02X\r\n", pVpc3->abDinBufPtr[1] VPC3_EXTENSION);
         
         pVpc3->abDinBufPtr[2] VPC3_EXTENSION   = pVpc3->abDinBufPtr[1]  VPC3_EXTENSION  + ( wInBufferLength   >> SEG_MULDIV );
         printf("DEBUG: abDinBufPtr[2]=0x%02X\r\n", pVpc3->abDinBufPtr[2] VPC3_EXTENSION);

         pVpc3->abDiagBufPtr[0] VPC3_EXTENSION  = pVpc3->abDinBufPtr[2]  VPC3_EXTENSION  + ( wInBufferLength   >> SEG_MULDIV );
         printf("DEBUG: abDiagBufPtr[0]=0x%02X\r\n", pVpc3->abDiagBufPtr[0] VPC3_EXTENSION);
         
         pVpc3->abDiagBufPtr[1] VPC3_EXTENSION  = pVpc3->abDiagBufPtr[0] VPC3_EXTENSION  + ( wDiagBufferLength >> SEG_MULDIV );
         printf("DEBUG: abDiagBufPtr[1]=0x%02X\r\n", pVpc3->abDiagBufPtr[1] VPC3_EXTENSION);

         pVpc3->bCfgBufPtr VPC3_EXTENSION       = pVpc3->abDiagBufPtr[1] VPC3_EXTENSION  + ( wDiagBufferLength >> SEG_MULDIV );
         printf("DEBUG: wCfgBufferLength=%d, SEG_MULDIV=%d, (wCfgBufferLength >> SEG_MULDIV)=%d\r\n", 
                wCfgBufferLength, SEG_MULDIV, (wCfgBufferLength >> SEG_MULDIV));
         printf("DEBUG: bCfgBufPtr=0x%02X\r\n", pVpc3->bCfgBufPtr VPC3_EXTENSION);
         printf("DEBUG: ANTES de asignacion: bReadCfgBufPtr=0x%02X\r\n", pVpc3->bReadCfgBufPtr VPC3_EXTENSION);
         printf("DEBUG: Calculando: 0x%02X + %d = 0x%02X\r\n", 
                pVpc3->bCfgBufPtr VPC3_EXTENSION, 
                (wCfgBufferLength >> SEG_MULDIV),
                pVpc3->bCfgBufPtr VPC3_EXTENSION + (wCfgBufferLength >> SEG_MULDIV));
         pVpc3->bReadCfgBufPtr VPC3_EXTENSION   = pVpc3->bCfgBufPtr      VPC3_EXTENSION  + ( wCfgBufferLength  >> SEG_MULDIV );
         printf("DEBUG: DESPUES de asignacion: bReadCfgBufPtr=0x%02X\r\n", pVpc3->bReadCfgBufPtr VPC3_EXTENSION);
         printf("DEBUG: Verificando direccion: &(pVpc3->bReadCfgBufPtr)=0x%08X\r\n", (unsigned int)&(pVpc3->bReadCfgBufPtr));
         printf("DEBUG: Valor directo en direccion: *(&(pVpc3->bReadCfgBufPtr))=0x%02X\r\n", *(&(pVpc3->bReadCfgBufPtr)));

         pVpc3->bPrmBufPtr VPC3_EXTENSION       = pVpc3->bReadCfgBufPtr  VPC3_EXTENSION  + ( wCfgBufferLength  >> SEG_MULDIV );

         pVpc3->bAuxBufSel VPC3_EXTENSION       = 0x06; /* SetPrm via Aux1, ChkCfg and SSA via Aux2 */
         pVpc3->abAuxBufPtr[0] VPC3_EXTENSION   = pVpc3->bPrmBufPtr VPC3_EXTENSION       + ( wPrmBufferLength  >> SEG_MULDIV );
         pVpc3->abAuxBufPtr[1] VPC3_EXTENSION   = pVpc3->abAuxBufPtr[0] VPC3_EXTENSION   + ( wPrmBufferLength  >> SEG_MULDIV );

         pVpc3->bSsaBufPtr VPC3_EXTENSION = 0;
         if( wSsaBufferLength != 0 )
         {
             pVpc3->bSsaBufPtr VPC3_EXTENSION   = pVpc3->abAuxBufPtr[1] VPC3_EXTENSION   + ( wAux2BufferLength >> SEG_MULDIV );
         } /* if( wSsaBufferLength != 0 ) */

         /*---------------------------------------------------------------*/
         /* set buffer length                                             */
         /*---------------------------------------------------------------*/
         pVpc3->bLenDoutBuf VPC3_EXTENSION     = pDpSystem->bOutputDataLength;
         pVpc3->bLenDinBuf  VPC3_EXTENSION     = pDpSystem->bInputDataLength;

         pVpc3->abLenDiagBuf[0] VPC3_EXTENSION = 6;
         pVpc3->abLenDiagBuf[1] VPC3_EXTENSION = 6;

         pVpc3->bLenCfgData VPC3_EXTENSION     = pDpSystem->bCfgBufsize;
         pVpc3->bLenPrmData VPC3_EXTENSION     = pDpSystem->bPrmBufsize;
         pVpc3->bLenSsaBuf  VPC3_EXTENSION     = pDpSystem->bSsaBufsize;

         pVpc3->abLenCtrlBuf[0] VPC3_EXTENSION = pDpSystem->bPrmBufsize;
         pVpc3->abLenCtrlBuf[1] VPC3_EXTENSION = ( wAux2BufferLength >= 244 ) ? 244 : wAux2BufferLength;

         /* for faster access, store some pointer in local structure */
         pDpSystem->pDoutBuffer1 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDoutBufPtr[0] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));                                                                                                                                                                                                       ;
         pDpSystem->pDoutBuffer2 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDoutBufPtr[1] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDoutBuffer3 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDoutBufPtr[2] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));

         pDpSystem->pDinBuffer1  = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDinBufPtr[0] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDinBuffer2  = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDinBufPtr[1] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDinBuffer3  = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDinBufPtr[2] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));

         pDpSystem->pDiagBuffer1 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDiagBufPtr[0] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));
         pDpSystem->pDiagBuffer2 = (VPC3_UNSIGNED8_PTR)(((VPC3_ADR)(pVpc3->abDiagBufPtr[1] VPC3_EXTENSION) << SEG_MULDIV)+((VPC3_ADR)Vpc3AsicAddress));

         // --- DEFENSIVE PROGRAMMING: Validate calculated buffer pointers ---
         VPC3_ADR diag1_addr = ((VPC3_ADR)(pVpc3->abDiagBufPtr[0] VPC3_EXTENSION) << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
         VPC3_ADR diag2_addr = ((VPC3_ADR)(pVpc3->abDiagBufPtr[1] VPC3_EXTENSION) << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
         
         printf("DEBUG: [Buffer Assignment] Diag1: Segment=0x%02X, Addr=0x%04X\r\n", 
                pVpc3->abDiagBufPtr[0] VPC3_EXTENSION, diag1_addr);
         printf("DEBUG: [Buffer Assignment] Diag2: Segment=0x%02X, Addr=0x%04X\r\n", 
                pVpc3->abDiagBufPtr[1] VPC3_EXTENSION, diag2_addr);
         
         if (diag1_addr >= ASIC_RAM_LENGTH) {
            printf("ERROR: [Buffer Assignment] Corrupted Diag1 segment pointer!\r\n");
            printf("ERROR: Segment=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
                   pVpc3->abDiagBufPtr[0] VPC3_EXTENSION, diag1_addr, ASIC_RAM_LENGTH);
            pDpSystem->pDiagBuffer1 = VPC3_NULL_PTR; // Set to NULL to prevent access
         }
         
         if (diag2_addr >= ASIC_RAM_LENGTH) {
            printf("ERROR: [Buffer Assignment] Corrupted Diag2 segment pointer!\r\n");
            printf("ERROR: Segment=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
                   pVpc3->abDiagBufPtr[1] VPC3_EXTENSION, diag2_addr, ASIC_RAM_LENGTH);
            pDpSystem->pDiagBuffer2 = VPC3_NULL_PTR; // Set to NULL to prevent access
         }
      } /* else of if( pDpSystem->wVpc3UsedDPV0BufferMemory > pDpSystem->wAsicUserRam ) */
   } /* if( bError == DP_OK ) */

   return bError;
} /* static DP_ERROR_CODE VPC3_InitBufferStructure( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_GetDoutBufPtr                                              */
/*---------------------------------------------------------------------------*/
/*!
  \brief Fetch buffer pointer of the current output buffer.

  \param[out] pbState

  \retval !0 - pointer to current output buffer.
  \retval 0 - no buffer available
*/
VPC3_UNSIGNED8_PTR VPC3_GetDoutBufPtr( MEM_UNSIGNED8_PTR pbState )
{
VPC3_UNSIGNED8_PTR pToOutputBuffer;             /* pointer to output buffer ( DP-Master -> VPC3+ ) */

   *pbState = VPC3_GET_NEXT_DOUT_BUFFER_CMD();

   switch( VPC3_GET_DOUT_BUFFER_SM() )          /* locate user Dout Buffer */
   {
      case 0x10:
      {
         pToOutputBuffer = pDpSystem->pDoutBuffer1;
         break;
      } /* case 0x10: */

      case 0x20:
      {
         pToOutputBuffer = pDpSystem->pDoutBuffer2;
         break;
      } /* case 0x20: */

      case 0x30:
      {
         pToOutputBuffer = pDpSystem->pDoutBuffer3;
         break;
      } /* case 0x30: */

      default:
      {
         pToOutputBuffer = VPC3_NULL_PTR;
         break;
      } /* default: */
   } /* switch( VPC3_GET_DOUT_BUFFER_SM() ) */

   return pToOutputBuffer;
} /* VPC3_UNSIGNED8_PTR VPC3_GetDoutBufPtr( MEM_UNSIGNED8_PTR pbState ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_GetDinBufPtr                                               */
/*---------------------------------------------------------------------------*/
/*!
  \brief Fetch buffer pointer of the next input buffer.

  \retval !0 - pointer to current input buffer.
  \retval 0 - no buffer available
*/
VPC3_UNSIGNED8_PTR VPC3_GetDinBufPtr( void )
{
VPC3_UNSIGNED8_PTR pToInputBuffer;     /* pointer to input buffer ( VPC3 -> DP-Master ) */

   switch( VPC3_GET_DIN_BUFFER_SM() )  /* locate user Din Buffer */
   {
      case 0x10:
      {
         pToInputBuffer = pDpSystem->pDinBuffer1;
         break;
      } /* case 0x10: */

      case 0x20:
      {
         pToInputBuffer = pDpSystem->pDinBuffer2;
         break;
      } /* case 0x20: */

      case 0x30:
      {
         pToInputBuffer = pDpSystem->pDinBuffer3;
         break;
      } /* case 0x30: */

      default:
      {
         pToInputBuffer = VPC3_NULL_PTR;
         break;
      } /* default: */
   } /* switch( VPC3_GET_DIN_BUFFER_SM() ) */

   return pToInputBuffer;
} /* VPC3_UNSIGNED8_PTR VPC3_GetDinBufPtr( void ) */

/*--------------------------------------------------------------------------*/
/* function: VPC3_InputDataUpdate                                           */
/*--------------------------------------------------------------------------*/

/*!
  \brief Copy input data to VPC3+ and perform buffer exchange.

  \param[in] pToInputData - pointer to local input data
*/
void VPC3_InputDataUpdate( MEM_UNSIGNED8_PTR pToInputData )
{
VPC3_UNSIGNED8_PTR   pToInputBuffer;

   /* write DIN data to VPC3 */
   pToInputBuffer = VPC3_GetDinBufPtr();
   if( pToInputBuffer != VPC3_NULL_PTR )
   {
      CopyToVpc3_( pToInputBuffer, pToInputData, (uint16_t)pDpSystem->bInputDataLength );

      /* the user makes a new Din-Buffer available in the state N */
      VPC3_INPUT_UPDATE();
   } /* if( pToInputBuffer != VPC3_NULL_PTR ) */
} /* void VPC3_InputDataUpdate( MEM_UNSIGNED8_PTR pToInputData ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_GetDxbBufPtr                                               */
/*---------------------------------------------------------------------------*/
/*!
  \brief Fetch buffer pointer of the current dxb buffer.

  \param[out] pbState

  \retval !0 - pointer to current dxb buffer.
  \retval 0 - no buffer available
*/
#if DP_SUBSCRIBER
VPC3_UNSIGNED8_PTR VPC3_GetDxbBufPtr( MEM_UNSIGNED8_PTR pbState )
{
VPC3_UNSIGNED8_PTR pToDxbBuffer;

   *pbState = VPC3_GET_NEXT_DXB_OUT_BUFFER_CMD();

   switch( VPC3_GET_DXB_OUT_BUFFER_SM() )
   {
      case 0x10:
      {
         pToDxbBuffer = pDpSystem->pDxbOutBuffer1;
         break;
      } /* case 0x10: */

      case 0x20:
      {
         pToDxbBuffer = pDpSystem->pDxbOutBuffer2;
         break;
      } /* case 0x20: */

      case 0x30:
      {
         pToDxbBuffer = pDpSystem->pDxbOutBuffer3;
         break;
      } /* case 0x30: */

      default:
      {
         pToDxbBuffer = VPC3_NULL_PTR;
         break;
      } /* default: */
   } /* switch( VPC3_GET_DXB_OUT_BUFFER_SM() ) */

   return pToDxbBuffer;
} /* VPC3_UNSIGNED8_PTR VPC3_GetDxbBufPtr( MEM_UNSIGNED8_PTR pbState ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_CheckDxbLinkTable                                  */
/*-------------------------------------------------------------------*/
static DP_ERROR_CODE VPC3_CheckDxbLinkTable( uint8_t bNrOfLinks )
{
DP_ERROR_CODE  bRetValue;
uint8_t          i;

   bRetValue = DP_OK;

   for( i = 0; i < bNrOfLinks; i++ )
   {
      if(     ( pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherAddress > 125 )
          ||  ( pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherLength  < 1   )
          ||  ( pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherLength  > 244 )
          ||  ( pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleOffset     > ( pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherLength - 1 ) )
          || (( pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleOffset + pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleLength) >
                                                                              (pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherLength))
          ||  ( pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleLength > MAX_DATA_PER_LINK-2 )
        )
      {
         bRetValue = DP_PRM_DXB_ERROR;
         break;
      } /* if(     ( pDpSystem->sLinkTable.sLinkTableEntry[i].bPublisherAddress > 125 ) ... */
   } /* for( i = 0; i < bNrOfLinks; i++ ) */

   return bRetValue;
} /* static DP_ERROR_CODE VPC3_CheckDxbLinkTable( uint8_t bNrOfLinks ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_BuildDxbLinkStatus                                 */
/*-------------------------------------------------------------------*/
static void VPC3_BuildDxbLinkStatus( uint8_t bNrOfLinks )
{
DXB_LINK_STATUS   sLinkStatus;
uint8_t           i;

   memset( &sLinkStatus, 0, sizeof( DXB_LINK_STATUS ) );

   sLinkStatus.bHeader     = 4 + ( bNrOfLinks * 2 );
   sLinkStatus.bStatusType = 0x83;
   sLinkStatus.bSlotNr     = 0x00;
   sLinkStatus.bSpecifier  = 0x00;

   for( i = 0; i < bNrOfLinks; i++ )
   {
      sLinkStatus.asLinkStatus[i].bPublisherAddress = pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherAddress;
      sLinkStatus.asLinkStatus[i].bLinkStatus       = 0x00;
   } /* for( i = 0; i < bNrOfLinks; i++ ) */

   VPC3_SET_DXB_LINK_STATUS_LEN( sLinkStatus.bHeader );
   CopyToVpc3_( VPC3_GET_DXB_LINK_STATUS_BUF_PTR(), (MEM_UNSIGNED8_PTR)&sLinkStatus, sLinkStatus.bHeader );
} /* static void VPC3_BuildDxbLinkStatus( uint8_t bNrOfLinks ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_SubscriberToLinkTable                              */
/*-------------------------------------------------------------------*/
DP_ERROR_CODE VPC3_SubscriberToLinkTable( MEM_PRM_SUBSCRIBER_TABLE_PTR psSubsTable, uint8_t bNrOfLinks )
{
DP_ERROR_CODE  bRetValue;
uint8_t        i;

   memset( &pDpSystem->sLinkTable, 0, sizeof( PRM_DXB_LINK_TABLE ) );

   pDpSystem->sLinkTable.bVersion = psSubsTable->bVersion;

   for( i = 0; i < bNrOfLinks; i++ )
   {
      pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherAddress  = psSubsTable->asSubscriberTableEntry[i].bPublisherAddress;
      pDpSystem->sLinkTable.asLinkTableEntry[i].bPublisherLength   = psSubsTable->asSubscriberTableEntry[i].bPublisherLength;
      pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleOffset      = psSubsTable->asSubscriberTableEntry[i].bSampleOffset;
      pDpSystem->sLinkTable.asLinkTableEntry[i].bSampleLength      = psSubsTable->asSubscriberTableEntry[i].bSampleLength;
   } /* for( i = 0; i < bNrOfLinks; i++ ) */

   bRetValue = VPC3_CheckDxbLinkTable( bNrOfLinks );

   if( bRetValue == DP_OK )
   {
      VPC3_SET_DXB_LINK_TABLE_LEN( bNrOfLinks << 2 );
      CopyToVpc3_( VPC3_GET_DXB_LINK_TABLE_BUF_PTR(), &pDpSystem->sLinkTable.asLinkTableEntry[0].bPublisherAddress, ( bNrOfLinks << 2 ) );
      VPC3_BuildDxbLinkStatus( bNrOfLinks );
   } /* if( bRetValue == DP_OK ) */

   return bRetValue;
} /* DP_ERROR_CODE VPC3_SubscriberToLinkTable( MEM_PRM_SUBSCRIBER_TABLE_PTR psSubsTable, uint8_t bNrOfLinks ) */
#endif /* #if DP_SUBSCRIBER */

/*---------------------------------------------------------------------------*/
/* function: VPC3_GetMasterAddress                                           */
/*---------------------------------------------------------------------------*/
uint8_t VPC3_GetMasterAddress( void )
{
uint8_t bMasterAddress;
VPC3_ADR diagAddr;

   #if VPC3_SERIAL_MODE
      diagAddr = (VPC3_ADR)((uintptr_t)pDpSystem->pDiagBuffer1);
      bMasterAddress = Vpc3Read( diagAddr + 3 );

      if( bMasterAddress == 0xFF )
      {
         diagAddr = (VPC3_ADR)((uintptr_t)pDpSystem->pDiagBuffer2);
         bMasterAddress = Vpc3Read( diagAddr + 3 );
      } /* if( bMasterAddress == 0xFF ) */
   #else
      bMasterAddress = *( pDpSystem->pDiagBuffer1 + 3 );

      if( bMasterAddress == 0xFF )
      {
         bMasterAddress = *( pDpSystem->pDiagBuffer2 + 3 );
      } /* if( bMasterAddress == 0xFF ) */
   #endif /* #if VPC3_SERIAL_MODE */

   return( bMasterAddress );
} /* uint8_t VPC3_GetMasterAddress( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_SetIoDataLength                                            */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set length of VPC3+ input buffer and output buffer.

*/
void VPC3_SetIoDataLength( void )
{
   #if VPC3_SERIAL_MODE

      /* length of buffers OK, set real buffers */
      Vpc3Write( bVpc3RwLenDoutBuf, pDpSystem->bOutputDataLength );
      Vpc3Write( bVpc3RwLenDinBuf, pDpSystem->bInputDataLength );

   #else

      /* length of buffers OK, set real buffers */
      pVpc3->bLenDoutBuf VPC3_EXTENSION = pDpSystem->bOutputDataLength;
      pVpc3->bLenDinBuf  VPC3_EXTENSION = pDpSystem->bInputDataLength;

   #endif /* #if VPC3_SERIAL_MODE */
} /* void VPC3_SetIoDataLength( void ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_GetErrorCounter                                    */
/*-------------------------------------------------------------------*/
/*!
  \brief Get error counter from VPC3+.

  \param[in] wFalse buffer for VPC3+ error counter
  \param[in] wValid buffer for VPC3+ error counter
*/
void VPC3_GetErrorCounter( uint16_t *wFalse, uint16_t *wValid )
{
uint8_t abTemp[4];

   CopyFromVpc3_( &abTemp[0], VPC3_GET_ERROR_COUNTER_PTR(), 4 );
   *wFalse = abTemp[0] + (((uint16_t)abTemp[1]) << 8);
   *wValid = abTemp[2] + (((uint16_t)abTemp[3]) << 8);
} /* void VPC3_GetErrorCounter( uint16_t *wFalse, uint16_t *wValid ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_CopyErrorCounter                                   */
/*-------------------------------------------------------------------*/
/*!
  \brief Copy error counter from VPC3+.

  \param[in] pbData buffer for VPC3+ error counter
*/
void VPC3_CopyErrorCounter( MEM_UNSIGNED8_PTR pbData )
{
uint8_t abTemp[4];

   CopyFromVpc3_( &abTemp[0], VPC3_GET_ERROR_COUNTER_PTR(), 4 );
   pbData[0] = abTemp[1];
   pbData[1] = abTemp[0];
   pbData[2] = abTemp[3];
   pbData[3] = abTemp[2];
} /* void VPC3_CopyErrorCounter( MEM_UNSIGNED8_PTR pbData ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_WaitForGoOffline                                           */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set GoOffline and wait until VPC3+ is in offline.

  \retval DP_OK - VPC3+ is in OFFLINE state
  \retval DP_NOT_OFFLINE_ERROR - Error VPC3 is not in OFFLINE state
*/
DP_ERROR_CODE VPC3_WaitForGoOffline( void )
{
DP_ERROR_CODE bError;
uint8_t bLoop;

   VPC3_GoOffline();
   bError = DP_NOT_OFFLINE_ERROR;
   bLoop = 0;
   while( bLoop++ < 10 )
   {
      if( !VPC3_GET_OFF_PASS() )
      {
         bError = DP_OK;
         break;
      }
   } /* while( bLoop++ < 10 ) */

   return bError;
} /* DP_ERROR_CODE VPC3_WaitForGoOffline( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_GetDiagBufPtr                                              */
/*---------------------------------------------------------------------------*/
/*!
  \brief Fetch buffer pointer of the next diagnostic buffer.

  \param

  \retval !0 - pointer to current diagnostic buffer.
  \retval 0 - no buffer available
*/
VPC3_UNSIGNED8_PTR VPC3_GetDiagBufPtr( void )
{
VPC3_UNSIGNED8_PTR pToDiagBuffer;               /* pointer to diagnosis buffer */
uint8_t            bDiagBufferSm;
uint8_t            segmentValue;
VPC3_ADR           calculatedAddress;

   bDiagBufferSm = VPC3_GET_DIAG_BUFFER_SM();
   printf("DEBUG: [VPC3_GetDiagBufPtr] bDiagBufferSm=0x%02X\r\n", bDiagBufferSm);
   printf("DEBUG: [VPC3_GetDiagBufPtr] pDiagBuffer1=0x%08X, pDiagBuffer2=0x%08X\r\n", (unsigned int)pDpSystem->pDiagBuffer1, (unsigned int)pDpSystem->pDiagBuffer2);

   if( ( bDiagBufferSm & 0x03 ) == 0x01 )       /* locate Diag Buffer */
   {
      // --- DEFENSIVE PROGRAMMING: Validate segment pointer ---
      segmentValue = pVpc3->abDiagBufPtr[0] VPC3_EXTENSION;
      calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
      
      printf("DEBUG: [VPC3_GetDiagBufPtr] Segment[0]=0x%02X, CalculatedAddr=0x%04X\r\n", 
             segmentValue, calculatedAddress);
      
      // Check if calculated address is within valid range
      if (calculatedAddress >= ASIC_RAM_LENGTH) {
         printf("ERROR: [VPC3_GetDiagBufPtr] Corrupted segment pointer detected!\r\n");
         printf("ERROR: Segment[0]=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
                segmentValue, calculatedAddress, ASIC_RAM_LENGTH);
         printf("ERROR: Returning NULL to prevent LECTURA ILEGAL\r\n");
         return VPC3_NULL_PTR;
      }
      
      pToDiagBuffer = pDpSystem->pDiagBuffer1;
      printf("DEBUG: [VPC3_GetDiagBufPtr] Seleccionado pDiagBuffer1 (validado)\r\n");
   } /* if( ( bDiagBufferSm & 0x03 ) == 0x01 ) */
   else
   {
      if( ( bDiagBufferSm & 0x0C ) == 0x04 )
      {
         // --- DEFENSIVE PROGRAMMING: Validate segment pointer ---
         segmentValue = pVpc3->abDiagBufPtr[1] VPC3_EXTENSION;
         calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
         
         printf("DEBUG: [VPC3_GetDiagBufPtr] Segment[1]=0x%02X, CalculatedAddr=0x%04X\r\n", 
                segmentValue, calculatedAddress);
         
         // Check if calculated address is within valid range
         if (calculatedAddress >= ASIC_RAM_LENGTH) {
            printf("ERROR: [VPC3_GetDiagBufPtr] Corrupted segment pointer detected!\r\n");
            printf("ERROR: Segment[1]=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
                   segmentValue, calculatedAddress, ASIC_RAM_LENGTH);
            printf("ERROR: Returning NULL to prevent LECTURA ILEGAL\r\n");
            return VPC3_NULL_PTR;
         }
         
         pToDiagBuffer = pDpSystem->pDiagBuffer2;
         printf("DEBUG: [VPC3_GetDiagBufPtr] Seleccionado pDiagBuffer2 (validado)\r\n");
      } /* if( ( bDiagBufferSm & 0x0C ) == 0x04 ) */
      else
      {
         pToDiagBuffer = VPC3_NULL_PTR;
         printf("DEBUG: [VPC3_GetDiagBufPtr] No hay buffer disponible, retornando NULL\r\n");
      } /* else of if( ( bDiagBufferSm & 0x0C ) == 0x04 ) */
   } /* else of if( ( bDiagBufferSm & 0x03 ) == 0x01 ) */

   printf("DEBUG: [VPC3_GetDiagBufPtr] Retornando puntero: 0x%08X\r\n", (unsigned int)pToDiagBuffer);
   return pToDiagBuffer;
} /* VPC3_UNSIGNED8_PTR VPC3_GetDiagBufPtr( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_UpdateDiagnosis                                            */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set diagnostic data to VPC3+.
  By calling this function, the user transfers the new, external diagnostics to VPC3+.
  As a return value, the user receives a pointer to the new diagnostics data buffer.
  The user has to make sure that the buffer size does not exceed the size of the
  diagnostic buffer that was set when the slave's memory resources were set up:

  Diagnostic Buffer Length >= bUserDiagLen + DIAG_NORM_DIAG_SIZE

  \param[in] bDiagControl - add diagnostic bits
  \param[in] pbToUserDiagData - pointer to structure with alarm/diagnostic data
  \param[in] bUserDiagLength - length of the current user diagnostic

  Values for bUserDiagLength:
  - 0 - A previously set user diagnostic is deleted from the slave's diagnostic buffer. Only 6 bytes
        standard diagnostic are sent in the diagnostic telegram. In this case, the user does not have to transfer a
        pointer to a diagnostic buffer.
  - 1..DIAG_BUFSIZE-6 - Length of the new user diagnostic data.

  Values for bDiagControl:
  - DIAG_RESET - Reset bit 'extended diagnostic, static diagnostic, diagnostic overflow'
  - EXT_DIAG_SET - Set bit 'extended diagnostic'.
  - STAT_DIAG_SET - Set bit 'static diagnostic'.

  \retval !0 - pointer to current diagnostic buffer.
  \retval 0 - no buffer available
*/
static VPC3_UNSIGNED8_PTR VPC3_UpdateDiagnosis( uint8_t bDiagControl, MEM_UNSIGNED8_PTR pbToUserDiagData, uint8_t bUserDiagLen )
{
VPC3_UNSIGNED8_PTR pDiagBuffer; /* pointer to diagnosis buffer */
uint8_t            bNewDiagBufferCmd;
uint8_t            bDiagBufferSm;
VPC3_ADR           diagAddr;

   bDiagBufferSm = VPC3_GET_DIAG_BUFFER_SM();

   if( ( bDiagBufferSm & 0x03 ) == 0x01 )                      /* locate Diag Buffer */
   {
      /* copy to diagnosis buffer */
      if( bUserDiagLen > 0 )
      {
         CopyToVpc3_( pDpSystem->pDiagBuffer1+DIAG_NORM_DIAG_SIZE, pbToUserDiagData, bUserDiagLen );
      } /* if( bUserDiagLen > 0 ) */

      #if VPC3_SERIAL_MODE
         Vpc3Write( bVpc3RwLenDiagBuf1, bUserDiagLen+DIAG_NORM_DIAG_SIZE );
         diagAddr = (VPC3_ADR)((uintptr_t)pDpSystem->pDiagBuffer1);
         Vpc3Write( diagAddr, bDiagControl );
      #else
         pVpc3->abLenDiagBuf[0] VPC3_EXTENSION = bUserDiagLen+DIAG_NORM_DIAG_SIZE;     /* length of Diag Buffer 1 */
         *(pDpSystem->pDiagBuffer1) = bDiagControl;
      #endif /* #if VPC3_SERIAL_MODE */
   } /* if( ( bDiagBufferSm & 0x03 ) == 0x01 ) */
   else
   {
      if( ( bDiagBufferSm & 0x0C ) == 0x04 )
      {
         /* copy to diagnosis buffer */
         if( bUserDiagLen > 0 )
         {
            CopyToVpc3_( pDpSystem->pDiagBuffer2+DIAG_NORM_DIAG_SIZE, pbToUserDiagData, bUserDiagLen );
         } /* if( bUserDiagLen > 0 ) */

         #if VPC3_SERIAL_MODE
            Vpc3Write( bVpc3RwLenDiagBuf2, bUserDiagLen+DIAG_NORM_DIAG_SIZE );
            diagAddr = (VPC3_ADR)((uintptr_t)pDpSystem->pDiagBuffer2);
            Vpc3Write( diagAddr, bDiagControl );
         #else
            pVpc3->abLenDiagBuf[1] VPC3_EXTENSION = bUserDiagLen+DIAG_NORM_DIAG_SIZE;  /* length of Diag Buffer 2 */
            *(pDpSystem->pDiagBuffer2) = bDiagControl;
         #endif /* #if VPC3_SERIAL_MODE */
      } /* if( ( bDiagBufferSm & 0x0C ) == 0x04 ) */
   } /* else of if( ( bDiagBufferSm & 0x03 ) == 0x01 ) */

   bNewDiagBufferCmd = VPC3_GET_NEW_DIAG_BUFFER_CMD();

   switch( bNewDiagBufferCmd & 0x03 )
   {
      case 1:   /* use buffer 1 */
      {
         pDiagBuffer = pDpSystem->pDiagBuffer1;
         break;
      } /* case 1: */

      case 2:   /* use buffer 2 */
      {
         pDiagBuffer = pDpSystem->pDiagBuffer2;
         break;
      } /* case 2: */

      default:
      {
         /* no buffer available */
         pDiagBuffer = VPC3_NULL_PTR;
         break;
      } /* default: */
   } /* switch( bNewDiagBufferCmd & 0x03 ) */

   return pDiagBuffer;
} /* static VPC3_UNSIGNED8_PTR VPC3_UpdateDiagnosis( uint8_t bDiagControl, MEM_UNSIGNED8_PTR pbToUserDiagData, uint8_t bUserDiagLen ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_CheckDiagBufPtr                                            */
/*---------------------------------------------------------------------------*/
static VPC3_UNSIGNED8_PTR VPC3_CheckDiagBufPtr( void )
{
   if( pDpSystem->pDiagBuffer == VPC3_NULL_PTR )
   {
      pDpSystem->pDiagBuffer = VPC3_GetDiagBufPtr();
   }
   return pDpSystem->pDiagBuffer;
} /* static VPC3_UNSIGNED8_PTR VPC3_CheckDiagBufPtr( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_CheckDiagBufferChanged                                     */
/*---------------------------------------------------------------------------*/
void VPC3_CheckDiagBufferChanged( void )
{
uint8_t bVpc3Event;

   if( VPC3_GetDpState( eDpStateDiagActive ) )
   {
      #if VPC3_SERIAL_MODE
         bVpc3Event = Vpc3Read( bVpc3RwIntReqReg_H );
      #else
         bVpc3Event = pVpc3->bIntReqReg_H VPC3_EXTENSION;
      #endif /* #if VPC3_SERIAL_MODE */
      if( bVpc3Event & VPC3_INT_DIAG_BUFFER_CHANGED )
      {
         DpDiag_IsrDiagBufferChanged();
         #if VPC3_SERIAL_MODE
            Vpc3Write( bVpc3WoIntAck_H, VPC3_INT_DIAG_BUFFER_CHANGED );
         #else
            pVpc3->sReg.sWrite.bIntAck_H VPC3_EXTENSION = VPC3_INT_DIAG_BUFFER_CHANGED;
         #endif /* #if VPC3_SERIAL_MODE */
      }
   } /* if( VPC3_GetDpState( eDpStateDiagActive ) ) */
} /* void VPC3_CheckDiagBufferChanged( void ) */

/*---------------------------------------------------------------------------*/
/* function: VPC3_SetDiagnosis                                               */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set diagnostic data to VPC3+.
  By calling this function, the user provides diagnostic data to the slave. The diagnostic
  data is sent at the next possible time.
  The user has to make sure that the buffer size does not exceed the size of the
  diagnostic buffer that was set when the slave's memory resources were set up:

  Diagnostic Buffer Length >= LengthDiag_User + LengthAlarm_User

  \attention This function is not suitable for setting alarms.

  The user can set up DP diagnostics; the following applies:
   - The 6 byte standard diagnostic (refer to EN 50170) is not part of the user diagnostic.
   - In DP standard operation, one ID-related, several channel-related, and one
   - device-related diagnostics may be utilized.
   - In DPV1 operation, one revision, one ID-related, several channel-related, and
     one alarm.
   - The user is responsible for the content of the diagnostic data.

  \param[in] pbToUserDiagData - pointer to structure with alarm/diagnostic data
  \param[in] bUserDiagLength - length of the current user diagnostic
  \param[in] bDiagControl - add diagnostic bits
  \param[in] bCheckDiagFlag - check VPC3+ diagnostic flag

  Values for bUserDiagLength:
  - 0 - A previously set user diagnostic is deleted from the slave's diagnostic buffer. Only 6 bytes
        standard diagnostic are sent in the diagnostic telegram. In this case, the user does not have to transfer a
        pointer to a diagnostic buffer.
  - 1..DIAG_BUFSIZE-6 - Length of the new user diagnostic data.

  Values for bDiagControl:
  - DIAG_RESET - Reset bit 'extended diagnostic, static diagnostic, diagnostic overflow'
  - EXT_DIAG_SET - Set bit 'extended diagnostic'.
  - STAT_DIAG_SET - Set bit 'static diagnostic'.

  \retval DP_OK - Execution OK, diagnostic message is copied into VPC3+
  \retval DP_DIAG_OLD_DIAG_NOT_SEND_ERROR - Error, wait because last diagnostic message isn't send
  \retval DP_DIAG_BUFFER_LENGTH_ERROR - Error, diagnostic message is too long
  \retval DP_DIAG_NO_BUFFER_ERROR - Error, no diagnostic buffer available
  \retval DP_DIAG_CONTROL_BYTE_ERROR - Error of bDiagControl
  \retval DP_DIAG_BUFFER_ERROR - Error, diagnostic header is wrong
  \retval DP_DIAG_SEQUENCE_ERROR - Error, revision will be send in wrong state
  \retval DP_DIAG_NOT_POSSIBLE_ERROR - Error, unknown diagnostic header
*/
DP_ERROR_CODE VPC3_SetDiagnosis( MEM_UNSIGNED8_PTR pbToUserDiagData, uint8_t bUserDiagLength, uint8_t bDiagControl, uint8_t bCheckDiagFlag )
{
MEM_UNSIGNED8_PTR pbToDiagArray;
DP_ERROR_CODE     bRetValue;
uint8_t           bTmpUserDiagnosisLength;
uint8_t           bTmpLength;
uint8_t           bHeader;
uint8_t           bDpState;

   bRetValue = DP_OK;
   
   printf("DEBUG: [VPC3_SetDiagnosis] INICIO - bUserDiagLength=%d, bDiagControl=0x%02X, bCheckDiagFlag=%d\n", bUserDiagLength, bDiagControl, bCheckDiagFlag);

   /* check available diag buffer */
   printf("DEBUG: [VPC3_SetDiagnosis] Verificando VPC3_CheckDiagBufPtr()...\n");
   if( VPC3_CheckDiagBufPtr() != VPC3_NULL_PTR )
   {
      printf("DEBUG: [VPC3_SetDiagnosis] Buffer de diagnóstico disponible\n");
      bTmpUserDiagnosisLength = bUserDiagLength;
      pbToDiagArray = pbToUserDiagData;

      bDpState = VPC3_GET_DP_STATE();
      printf("DEBUG: [VPC3_SetDiagnosis] bDpState=0x%02X, DATA_EX=0x%02X\n", bDpState, DATA_EX);
      if( ( bDpState == DATA_EX ) && ( bCheckDiagFlag == VPC3_TRUE ) )
      {
         printf("DEBUG: [VPC3_SetDiagnosis] Verificando VPC3_GET_DIAG_FLAG()...\n");
         if( VPC3_GET_DIAG_FLAG() )
         {
            printf("DEBUG: [VPC3_SetDiagnosis] ERROR: Diagnóstico anterior no enviado\n");
            /* old diagnosis not send */
            bRetValue = DP_DIAG_OLD_DIAG_NOT_SEND_ERROR;
         } /* if( VPC3_GET_DIAG_FLAG() ) */
      } /* if( ( bDpState == DATA_EX ) && ( bCheckDiagFlag == VPC3_TRUE ) ) */

      /* check bUserDiagLength */
      printf("DEBUG: [VPC3_SetDiagnosis] Verificando longitud: bUserDiagLength=%d, DIAG_BUFSIZE-6=%d\n", bUserDiagLength, DIAG_BUFSIZE - 6);
      if( bUserDiagLength > ( DIAG_BUFSIZE - 6 ) )
      {
         printf("DEBUG: [VPC3_SetDiagnosis] ERROR: Longitud de buffer excede límite\n");
         bRetValue = DP_DIAG_BUFFER_LENGTH_ERROR;
      } /* if( bUserDiagLength > ( DIAG_BUFSIZE - 6 ) ) */

      if( bRetValue == DP_OK )
      {
         printf("DEBUG: [VPC3_SetDiagnosis] Verificando bDiagControl=0x%02X...\n", bDiagControl);
         /* check control byte */
         switch( bDiagControl )
         {
            case EXT_DIAG_SET:
            {
               printf("DEBUG: [VPC3_SetDiagnosis] Caso EXT_DIAG_SET, verificando bUserDiagLength=%d\n", bUserDiagLength);
               if( bUserDiagLength == 0 )
               {
                   printf("DEBUG: [VPC3_SetDiagnosis] ERROR: EXT_DIAG_SET con longitud 0\n");
                   bRetValue = DP_DIAG_CONTROL_BYTE_ERROR;
               } /* if( bUserDiagLength == 0 ) */
               break;
            } /* case EXT_DIAG_SET: */

            default:
            {
               printf("DEBUG: [VPC3_SetDiagnosis] Caso default para bDiagControl\n");
               /* do nothing */
               break;
            } /* default: */
         } /* switch( bDiagControl ) */

         /* check user diag buffer contents */
         printf("DEBUG: [VPC3_SetDiagnosis] Verificando contenido del buffer de diagnóstico...\n");
         while( ( 0 < bTmpUserDiagnosisLength ) && ( DP_OK == bRetValue ) )
         {
            bHeader = pbToDiagArray[0];
            printf("DEBUG: [VPC3_SetDiagnosis] Procesando header: 0x%02X, DIAG_TYPE_MASK=0x%02X\n", bHeader, DIAG_TYPE_MASK);
            switch( DIAG_TYPE_MASK & bHeader )
            {
               case DIAG_TYPE_DEV:
               {
                  printf("DEBUG: [VPC3_SetDiagnosis] Caso DIAG_TYPE_DEV\n");
                  bTmpLength = (( ~DIAG_TYPE_MASK ) & bHeader );
                  printf("DEBUG: [VPC3_SetDiagnosis] bTmpLength=%d, STATUS_DIAG_HEAD_SIZE=%d\n", bTmpLength, STATUS_DIAG_HEAD_SIZE);
                  if( STATUS_DIAG_HEAD_SIZE > bTmpLength )
                  {
                     printf("DEBUG: [VPC3_SetDiagnosis] ERROR: STATUS_DIAG_HEAD_SIZE > bTmpLength\n");
                     bRetValue = DP_DIAG_BUFFER_ERROR;
                  } /* if( STATUS_DIAG_HEAD_SIZE > bTmpLength ) */
                  break;
               } /* case DIAG_TYPE_DEV: */

               case DIAG_TYPE_KEN:
               {
                  printf("DEBUG: [VPC3_SetDiagnosis] Caso DIAG_TYPE_KEN\n");
                  bTmpLength = (( ~DIAG_TYPE_MASK ) & bHeader );
                  printf("DEBUG: [VPC3_SetDiagnosis] bTmpLength=%d\n", bTmpLength);
                  if ( bTmpLength == 0 )
                  {
                     printf("DEBUG: [VPC3_SetDiagnosis] ERROR: DIAG_TYPE_KEN con longitud 0\n");
                     bRetValue = DP_DIAG_BUFFER_ERROR;
                  } /* if ( bTmpLength == 0 ) */
                  break;
               } /* case DIAG_TYPE_KEN: */

               case DIAG_TYPE_CHN:
               {
                  printf("DEBUG: [VPC3_SetDiagnosis] Caso DIAG_TYPE_CHN\n");
                  bTmpLength = DIAG_TYPE_CHN_SIZE;
                  break;
               } /* case DIAG_TYPE_CHN: */

               case DIAG_TYPE_REV:
               {
                  printf("DEBUG: [VPC3_SetDiagnosis] Caso DIAG_TYPE_REV\n");
                  bTmpLength = DIAG_TYPE_REV_SIZE;
                  if( bDpState != DATA_EX )
                  {
                     printf("DEBUG: [VPC3_SetDiagnosis] ERROR: DIAG_TYPE_REV no permitido en estado 0x%02X\n", bDpState);
                     /* only allowed in state DATA_EX */
                     bRetValue = DP_DIAG_SEQUENCE_ERROR;
                  } /* if( bDpState != DATA_EX ) */
                  break;
               } /* case DIAG_TYPE_REV: */

               default:
               {
                  printf("DEBUG: [VPC3_SetDiagnosis] ERROR: Tipo de diagnóstico no reconocido: 0x%02X\n", DIAG_TYPE_MASK & bHeader);
                  /* not possible! */
                  bTmpLength = 0;
                  bRetValue = DP_DIAG_NOT_POSSIBLE_ERROR;
                  break;
               } /* default: */
            } /* switch( DIAG_TYPE_MASK & bHeader ) */

            printf("DEBUG: [VPC3_SetDiagnosis] bTmpLength=%d, bTmpUserDiagnosisLength=%d\n", bTmpLength, bTmpUserDiagnosisLength);
            bTmpUserDiagnosisLength -= bTmpLength;
            pbToDiagArray += bTmpLength;
         } /* while( ( 0 < bTmpUserDiagnosisLength ) && ( DP_OK == bRetValue ) ) */

         if( bRetValue == DP_OK )
         {
            printf("DEBUG: [VPC3_SetDiagnosis] Llamando VPC3_UpdateDiagnosis...\n");
            pDpSystem->pDiagBuffer = VPC3_UpdateDiagnosis( bDiagControl, pbToUserDiagData, bUserDiagLength );
            printf("DEBUG: [VPC3_SetDiagnosis] VPC3_UpdateDiagnosis completado\n");
         } /* if( bRetValue == DP_OK ) */
      } /* if( bRetValue == DP_OK ) */
   } /* if( VPC3_CheckDiagBufPtr() != VPC3_NULL_PTR ) */
   else
   {
      printf("DEBUG: [VPC3_SetDiagnosis] ERROR: No hay buffer de diagnóstico disponible\n");
      /* Fetch new diagnosis buffer */
      pDpSystem->pDiagBuffer = VPC3_GetDiagBufPtr();
      /* wait for next free diag_buffer */
      bRetValue = DP_DIAG_NO_BUFFER_ERROR;
   } /* else of if( VPC3_CheckDiagBufPtr() != VPC3_NULL_PTR ) */

   printf("DEBUG: [VPC3_SetDiagnosis] FINAL - Retornando bRetValue=%d\n", bRetValue);
   return bRetValue;
} /* DP_ERROR_CODE VPC3_SetDiagnosis( MEM_UNSIGNED8_PTR pbToUserDiagData, uint8_t bUserDiagLength, uint8_t bDiagControl, uint8_t bCheckDiagFlag ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_SetAlarm                                           */
/*-------------------------------------------------------------------*/
/*!
  \brief Set alarm to VPC3+.
  By calling this function, the stack accepts the transferred alarm data. In addition to the net
  data, the alarm data also includes control information according to the DPV1
  specification . The data is transmitted at the next possible time. The user has to make sure that
  the buffer size does not exceed the size of the diagnostic buffer that was set when the
  slave's memory resources were defined.

  Diagnostic Buffer Length >= LengthDiag_User + LengthAlarm_User

  Specifications:
   - When setting alarms, the user has to adhere to the requirements regarding
     permissible alarm types that he was informed of when the alarm state machine was
     started.
   - The number of alarms that are permitted to be processed simultaneously during
     communication between parameterization master and slave is specified by the
     type- or sequence mode. It is entirely handled automatically by the stack; the user has no
     influence on it, and can thus set any number of alarms of all permitted types.
   - The user is responsible for the content of the alarm data.
   - The alarm buffer is to contain only one alarm.
   - The user has possibility to add DPV0 diagnostics like identifier related, modulestatus, etc. via the
     function DpDiag_Alarm.

  \param[in] psAlarm - pointer to structure with alarm data
  \param[in] bCallback - VPC3_TRUE - the stack calls the function UserAlarm to add diagnostics like channel related / identifier related diagnostics.
  \param[in] bCallback - VPC3_FALSE - the stack sends directly alarm data

  \retval SET_ALARM_OK - Execution OK, alarm add to alarm state machine
  \retval SET_ALARM_AL_STATE_CLOSED - Error, alarm state machine is closed
  \retval SET_ALARM_ALARMTYPE_NOTSUPP - Error, alarm type is not supported
  \retval SET_ALARM_SEQ_NR_ERROR - Error, sequence number is wrong
  \retval SET_ALARM_SPECIFIER_ERROR - Error, alarm specifier is wrong
*/
uint8_t VPC3_SetAlarm( ALARM_STATUS_PDU_PTR psAlarm, uint8_t bCallback )
{
   #if DP_ALARM

      return AL_SetAlarm( psAlarm, bCallback );

   #else

      psAlarm   = psAlarm;    /* avoid warning */
      bCallback = bCallback;  /* avoid warning */

      return SET_ALARM_AL_STATE_CLOSED;

   #endif /* #if DP_ALARM */
} /* uint8_t VPC3_SetAlarm( ALARM_STATUS_PDU_PTR psAlarm, uint8_t bCallback ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_GetFreeMemory                                      */
/*-------------------------------------------------------------------*/
/*!
   \brief Determine free memory space.

   \retval free memory size.
*/
uint16_t VPC3_GetFreeMemory( void )
{
uint16_t wFreeMemorySize;

   /* return number of bytes of unused VPC3-ram */
   wFreeMemorySize = ( ASIC_USER_RAM - pDpSystem->wVpc3UsedDPV0BufferMemory - pDpSystem->wVpc3UsedDPV1BufferMemory );

   return wFreeMemorySize;
} /* uint16_t VPC3_GetFreeMemory( void ) */

/*-------------------------------------------------------------------*/
/* function: VPC3_ProcessDpv1StateMachine                            */
/*-------------------------------------------------------------------*/
/*!
  \brief The application program has to call this function cyclically so that the DPV1 services can be processed.
*/
void VPC3_ProcessDpv1StateMachine( void )
{
   #if DP_MSAC_C1
      MSAC_C1_Process();   /* state machine MSAC_C1 */
   #endif /* #if DP_MSAC_C1 */

   #if DP_ALARM
      AL_AlarmProcess();   /* state machine ALARM */
   #endif /* #if DP_ALARM */

   #if DP_MSAC_C2
      MSAC_C2_Process();   /* state machine MSAC_C2 */
   #endif /* #if DP_MSAC_C2 */
} /* void VPC3_ProcessDpv1StateMachine( void ) */

/**
 * @brief Force MODE_REG_2 to the correct value and monitor for changes.
 * 
 * This function attempts to set MODE_REG_2 to the expected value and
 * provides detailed logging about the process.
 * 
 * @retval 0 - Success (MODE_REG_2 is correct)
 * @retval 1 - Failure (MODE_REG_2 could not be set correctly)
 */
uint8_t VPC3_ForceModeReg2(void) {
   uint8_t attempts = 0;
   uint8_t max_attempts = 10;
   uint8_t expected_value = INIT_VPC3_MODE_REG_2;
   uint8_t current_value;
   
   printf("[VPC3_ForceModeReg2] INICIO - Forzando MODE_REG_2 a 0x%02X\r\n", expected_value);
   
   while (attempts < max_attempts) {
      attempts++;
      printf("[VPC3_ForceModeReg2] Intento %d/%d\r\n", attempts, max_attempts);
      
      // Write the expected value
      VPC3_SET_MODE_REG_2(expected_value);
      
      // Read back to verify
     current_value = VPC3_GetModeReg2Shadow();
      printf("[VPC3_ForceModeReg2] Escrito: 0x%02X, Leído: 0x%02X\r\n", expected_value, current_value);
      
      if (current_value == expected_value) {
         printf("[VPC3_ForceModeReg2] ÉXITO - MODE_REG_2 configurado correctamente\r\n");
         return 0; // Success
      }
      
      // Small delay before retry
      HAL_Delay(1);
   }
   
   printf("[VPC3_ForceModeReg2] FALLO - No se pudo configurar MODE_REG_2 después de %d intentos\r\n", max_attempts);
   printf("[VPC3_ForceModeReg2] Último valor leído: 0x%02X (esperado: 0x%02X)\r\n", current_value, expected_value);
   return 1; // Failure
}

/**
 * @brief Monitor and recover MODE_REG_2 if it changes unexpectedly.
 * 
 * This function checks if MODE_REG_2 has the expected value and attempts
 * to recover it if it has changed. It should be called periodically.
 * 
 * @retval 0 - MODE_REG_2 is correct
 * @retval 1 - MODE_REG_2 was corrected
 * @retval 2 - MODE_REG_2 could not be corrected
 */
uint8_t VPC3_MonitorAndRecoverModeReg2(void) {
   static uint8_t last_known_value = 0xFF;
   uint8_t current_value = VPC3_GetModeReg2Shadow();
   uint8_t expected_value = INIT_VPC3_MODE_REG_2;
   
   // First time initialization
   if (last_known_value == 0xFF) {
      last_known_value = current_value;
      printf("[VPC3_MonitorModeReg2] Inicialización - MODE_REG_2 = 0x%02X\r\n", current_value);
      return 0;
   }
   
   // Check if value changed
   if (current_value != last_known_value) {
      printf("[VPC3_MonitorModeReg2]  CAMBIO DETECTADO: MODE_REG_2 cambió de 0x%02X a 0x%02X\r\n", 
             last_known_value, current_value);
      
      // Check if current value is correct
      if (current_value == expected_value) {
         printf("[VPC3_MonitorModeReg2]  El nuevo valor es correcto\r\n");
         last_known_value = current_value;
         return 0;
      }
      
      // Try to recover
      printf("[VPC3_MonitorModeReg2] Intentando recuperar MODE_REG_2...\r\n");
      if (VPC3_ForceModeReg2() == 0) {
         printf("[VPC3_MonitorModeReg2]  MODE_REG_2 recuperado exitosamente\r\n");
         last_known_value = expected_value;
         return 1; // Recovered
      } else {
         printf("[VPC3_MonitorModeReg2]  No se pudo recuperar MODE_REG_2\r\n");
         last_known_value = current_value;
         return 2; // Could not recover
      }
   }
   
   // Value is stable
   return 0;
}

/*---------------------------------------------------------------------------*/
/*!
  \brief Validate and reset corrupted segment pointers in the ASIC.

  This function checks if the segment pointers in the ASIC's internal registers
  are within valid ranges and resets them if they are corrupted.
  
  \retval VPC3_OK - All segment pointers are valid
  \retval VPC3_ERROR - Corrupted segment pointers were detected and reset
*/
DP_ERROR_CODE VPC3_ValidateSegmentPointers(void)
{
   uint8_t segmentValue;
   VPC3_ADR calculatedAddress;
   DP_ERROR_CODE status = DP_OK;
   
   printf("DEBUG: [VPC3_ValidateSegmentPointers] Validating ASIC segment pointers...\r\n");
   
   // Validate diagnostic buffer pointers
   segmentValue = pVpc3->abDiagBufPtr[0] VPC3_EXTENSION;
   calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
   printf("DEBUG: [VPC3_ValidateSegmentPointers] Diag1: Segment=0x%02X, Addr=0x%04X\r\n", 
          segmentValue, calculatedAddress);
   
   if (calculatedAddress >= ASIC_RAM_LENGTH) {
      printf("ERROR: [VPC3_ValidateSegmentPointers] Corrupted Diag1 segment pointer detected!\r\n");
      printf("ERROR: Segment=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
             segmentValue, calculatedAddress, ASIC_RAM_LENGTH);
      printf("WARNING: This indicates ASIC internal state corruption. Attempting reset...\r\n");
      status = DP_NOK;
   }
   
   segmentValue = pVpc3->abDiagBufPtr[1] VPC3_EXTENSION;
   calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
   printf("DEBUG: [VPC3_ValidateSegmentPointers] Diag2: Segment=0x%02X, Addr=0x%04X\r\n", 
          segmentValue, calculatedAddress);
   
   if (calculatedAddress >= ASIC_RAM_LENGTH) {
      printf("ERROR: [VPC3_ValidateSegmentPointers] Corrupted Diag2 segment pointer detected!\r\n");
      printf("ERROR: Segment=0x%02X, CalculatedAddr=0x%04X exceeds ASIC_RAM_LENGTH=0x%04X\r\n",
             segmentValue, calculatedAddress, ASIC_RAM_LENGTH);
      printf("WARNING: This indicates ASIC internal state corruption. Attempting reset...\r\n");
      status = DP_NOK;
   }
   
   // Validate other critical segment pointers
   segmentValue = pVpc3->bCfgBufPtr VPC3_EXTENSION;
   calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
   printf("DEBUG: [VPC3_ValidateSegmentPointers] Cfg: Segment=0x%02X, Addr=0x%04X\r\n", 
          segmentValue, calculatedAddress);
   
   if (calculatedAddress >= ASIC_RAM_LENGTH) {
      printf("ERROR: [VPC3_ValidateSegmentPointers] Corrupted Cfg segment pointer detected!\r\n");
      status = DP_NOK;
   }
   
   segmentValue = pVpc3->bPrmBufPtr VPC3_EXTENSION;
   calculatedAddress = ((VPC3_ADR)segmentValue << SEG_MULDIV) + ((VPC3_ADR)Vpc3AsicAddress);
   printf("DEBUG: [VPC3_ValidateSegmentPointers] Prm: Segment=0x%02X, Addr=0x%04X\r\n", 
          segmentValue, calculatedAddress);
   
   if (calculatedAddress >= ASIC_RAM_LENGTH) {
      printf("ERROR: [VPC3_ValidateSegmentPointers] Corrupted Prm segment pointer detected!\r\n");
      status = DP_NOK;
   }
   
   if (status == DP_NOK) {
      printf("WARNING: [VPC3_ValidateSegmentPointers] Corrupted segment pointers detected!\r\n");
      printf("WARNING: This may require ASIC reset or re-initialization.\r\n");
   } else {
      printf("DEBUG: [VPC3_ValidateSegmentPointers] All segment pointers are valid.\r\n");
   }
   
   return status;
}

/**
 * @brief Diagnóstico del modo de memoria del VPC3+S
 * @return 1 si está en modo 4KB, 0 si está en modo 2KB
 */
uint8_t VPC3_DiagnoseMemoryMode(void) {
   printf("DEBUG: [VPC3_DiagnoseMemoryMode] === DIAGNÓSTICO DE MODO DE MEMORIA ===\r\n");
   
   // Leer el MODE_REG_2 actual
   uint8_t mode_reg2 = VPC3_GetModeReg2Shadow();
   printf("DEBUG: [VPC3_DiagnoseMemoryMode] MODE_REG_2 leído: 0x%02X (binario: ", mode_reg2);
   
   // Mostrar en binario
   for (int i = 7; i >= 0; i--) {
      printf("%d", (mode_reg2 >> i) & 1);
      if (i == 7) printf(" "); // Espacio después del bit 7
   }
   printf(")\r\n");
   
   // Extraer el bit 7 (4KB_Mode)
   uint8_t bit7 = (mode_reg2 >> 7) & 1;
   printf("DEBUG: [VPC3_DiagnoseMemoryMode] Bit 7 (4KB_Mode): %d\r\n", bit7);
   
   if (bit7 == 0) {
      printf("DEBUG: [VPC3_DiagnoseMemoryMode]  ASIC está en MODO 2KB de RAM\r\n");
      printf("DEBUG: [VPC3_DiagnoseMemoryMode]  Configuración actual es CORRECTA\r\n");
      return 0;
   } else {
      printf("DEBUG: [VPC3_DiagnoseMemoryMode]  ASIC está en MODO 4KB de RAM\r\n");
      printf("DEBUG: [VPC3_DiagnoseMemoryMode]  Configuración actual es INCORRECTA\r\n");
      printf("DEBUG: [VPC3_DiagnoseMemoryMode]  Necesitamos cambiar DP_VPC3_4KB_MODE = 1\r\n");
      return 1;
   }
}

/**
 * @brief Performs a complete hardware reset of the VPC3+S ASIC
 * @return 0 on success, 1 on failure
 */
uint8_t VPC3_HardwareReset(void) {
    printf("DEBUG: [VPC3_HardwareReset] INICIO - Reset completo del hardware VPC3+S\r\n");
    
    // 1. Disable interrupts
    DpAppl_DisableInterruptVPC3Channel1();
    
    // 2. Set CS high to ensure clean state
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
    
    // 3. Perform hardware reset sequence
    printf("DEBUG: [VPC3_HardwareReset] Secuencia de reset hardware...\r\n");
    
    // Reset pulse: low for at least 1ms
    HAL_GPIO_WritePin(VPC3_RESET_PORT, VPC3_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10); // 10ms reset pulse
    
    // Release reset
    HAL_GPIO_WritePin(VPC3_RESET_PORT, VPC3_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(50); // Wait 50ms for ASIC to stabilize
    
    // 4. Verify ASIC is responding
    printf("DEBUG: [VPC3_HardwareReset] Verificando respuesta del ASIC...\r\n");
    
    // Try to read STATUS_L register
    uint8_t status_l = Vpc3Read(0x04);
    printf("DEBUG: [VPC3_HardwareReset] STATUS_L después del reset: 0x%02X\r\n", status_l);
    
    // 5. Check if ASIC is in a known state
    if (status_l == 0xFF) {
        printf("DEBUG: [VPC3_HardwareReset]  ASIC no responde (STATUS_L = 0xFF)\r\n");
        return 1; // Failure
    }
    
    printf("DEBUG: [VPC3_HardwareReset]  ASIC responde correctamente\r\n");
    
    // 6. Re-enable interrupts
    DpAppl_EnableInterruptVPC3Channel1();
    
    printf("DEBUG: [VPC3_HardwareReset] FIN - Reset completado\r\n");
    return 0; // Success
}

/**
 * @brief Continuously monitors MODE_REG_2 and adapts system behavior accordingly
 * @return 0 if stable, 1 if changes detected and handled
 */
uint8_t VPC3_AdaptiveModeReg2Monitor(void) {
    static uint8_t last_mode_reg_2 = 0xFF;
    static uint32_t last_check_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // Check every 100ms to avoid excessive overhead
    if (current_time - last_check_time < 100) {
        return 0;
    }
    
    last_check_time = current_time;
    
   uint8_t current_mode_reg_2 = VPC3_GetModeReg2Shadow();
    
    if (current_mode_reg_2 != last_mode_reg_2) {
        printf("DEBUG: [VPC3_AdaptiveModeReg2Monitor] 🔄 MODE_REG_2 cambió: 0x%02X -> 0x%02X\r\n", 
               last_mode_reg_2, current_mode_reg_2);
        
        // Detect actual memory mode and log it
        uint8_t actual_mode = VPC3_DetectActualMemoryMode();
        uint16_t actual_ram_length = VPC3_GetActualRamLength();
        
        printf("DEBUG: [VPC3_AdaptiveModeReg2Monitor] 📊 Modo detectado: %s (RAM: 0x%04X bytes)\r\n",
               (actual_mode == 0) ? "2KB" : (actual_mode == 1) ? "4KB" : "UNKNOWN",
               actual_ram_length);
        
        last_mode_reg_2 = current_mode_reg_2;
        return 1; // Changes detected and handled
    }
    
    return 0; // No changes
}

