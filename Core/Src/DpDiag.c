/***************************  Filename: DpDiag.c *****************************/
/* ========================================================================= */
/*                                                                           */
/* 0000  000   000  00000 0  000  0   0 0 0000                               */
/* 0   0 0  0 0   0 0     0 0   0 0   0 0 0   0                              */
/* 0   0 0  0 0   0 0     0 0     0   0 0 0   0      Einsteinstra�e 6        */
/* 0000  000  0   0 000   0 0     00000 0 0000       91074 Herzogenaurach    */
/* 0     00   0   0 0     0 0     0   0 0 0                                  */
/* 0     0 0  0   0 0     0 0   0 0   0 0 0          Phone: ++499132744200   */
/* 0     0  0  000  0     0  000  0   0 0 0    GmbH  Fax:   ++4991327442164  */
/*                                                                           */
/* ========================================================================= */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Technical support:       eMail: support@profichip.com                     */
/*                          Phone: ++49-9132-744-2150                        */
/*                          Fax. : ++49-9132-744-29-2150                     */
/*                                                                           */
/*****************************************************************************/


/*****************************************************************************/
/* contents:


*/
/*****************************************************************************/
/** @file
 * Copyright (C) by profichip GmbH   All Rights Reserved. Confidential
 *
 * @brief Handling of PROFIBUS-DP diagnostics.
 *
 * @author Peter Fredehorst
 * @version $Rev$
 */

/* include hierarchy */
#include <string.h>
#include "platform.h"
#include "DpAppl.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* defines, structures                                                       */
/*---------------------------------------------------------------------------*/
#define MAX_DIAG_RETRY 10

// -- defines for user alarm state
#define USER_AL_STATE_CLOSED        ((uint8_t)0x00)
#define USER_AL_STATE_OPEN          ((uint8_t)0x01)

// -- defines for alarm types
#define ALARM_OVERLOAD              ((uint8_t)0x01)
#define ALARM_OVER_TEMP             ((uint8_t)0x02)
#define ALARM_POWER_LOSS            ((uint8_t)0x03)
#define ALARM_RELAY_STATUS          ((uint8_t)0x04)

// -- defines for DpDiag_Alarm - bAlarmType
#define USER_TYPE_RESET_DIAG        ((uint8_t)0xF8)   /**< reset diagnostic, set diagnostic length to 6. */
#define USER_TYPE_DPV0              ((uint8_t)0xFA)   /**< DP-V0 diagnostic. */
#define USER_TYPE_PRM_OK            ((uint8_t)0xFB)   /**< parameterization is Ok, reset diagnostic block */
#define USER_TYPE_PRM_NOK           ((uint8_t)0xFC)   /**< parameterization is not Ok, reset diagnostic block */
#define USER_TYPE_CFG_OK            ((uint8_t)0xFD)   /**< configuration is OK, send module status with Diag.StatDiag = 1 */
#define USER_TYPE_CFG_NOK           ((uint8_t)0xFE)   /**< configuration is not OK, send module status with Diag.ExtDiag = 1 */
#define USER_TYPE_APPL_RDY          ((uint8_t)0xFF)   /**< VPC3+ is in DataEx, reset Diag.StatDiag */

// -- identifier related diagnosis --------------------------------------------
typedef struct
{
   uint8_t bHeader;                                  // fix to 0x42
   uint8_t abIdentRelDiag[1];
} sIdentRelDiagnostic;
#define cSizeOfIdentifierDiagnosis                 ((uint8_t)0x02)

sIdentRelDiagnostic sIdentRelDiag;  /**< Identifier related diagnostic block. */

// -- status message data (coded as device diagnosis) -------------------------
// -- Modulstatus -------------------------------------------------------------
typedef struct
{
   uint8_t   bHeader;                    // fix to 0x06
   uint8_t   bModuleStatus;              // fix to 0x82
   uint8_t   bSlotNr;                    // fix to 0x00
   uint8_t   bSpecifier;                 // fix to 0x00
   uint8_t   abModuleStatus[2];
} sModuleStatusDiagnosis;
#define cSizeOfModuleStatusDiagnosis 6

sModuleStatusDiagnosis sModuleStatDiag;   /**< Module status diagnostic block. */

#define cSizeOfUsrDiagnosticAlarmData    ((uint8_t)0x05)
#define cSizeOfUsrProcessAlarmData       ((uint8_t)0x03)
#define cSizeOfDiagnosticAlarm           (cSizeOfAlarmHeader + cSizeOfUsrDiagnosticAlarmData)
#define cSizeOfProcessAlarm              (cSizeOfAlarmHeader + cSizeOfUsrProcessAlarmData)

#define MaxAlarmLength  ((uint8_t)cSizeOfUsrDiagnosticAlarmData) //Max (cSizeOfUsrDiagnosticAlarmData, cSizeOfUsrProcessAlarmData)

struct                      //alarm state machine data
{
   uint8_t           abBufferUsed[ ALARM_MAX_FIFO ];
   uint8_t           abUserAlarmData[ ALARM_MAX_FIFO ][ MaxAlarmLength ];
   ALARM_STATUS_PDU  asAlarmBuffers[ ALARM_MAX_FIFO ];
}sAlarm;

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* function: DpDiag_AlarmInit                                               */
/*--------------------------------------------------------------------------*/
/**
 * @brief Initializes user alarm structure.
 *
 * Set all values to zero.
 */
void DpDiag_AlarmInit( void )
{
   dpl_init_list__( &sDpAppl.dplDiagQueue );

   memset( &sAlarm.abBufferUsed[0], 0, ALARM_MAX_FIFO );
}//void DpDiag_AlarmInit( void )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_AlarmAlloc                                              */
/*--------------------------------------------------------------------------*/
/**
 * @brief Fetch alarm/diagnostic buffer.
 *
 * @retval VPC3_NULL_PTR; - no alarm/diagnostic buffer available.
 * @retval != VPC3_NULL_PTR; - alarm/diagnostic buffer
 */
ALARM_STATUS_PDU_PTR DpDiag_AlarmAlloc( void )
{
ALARM_STATUS_PDU_PTR psAlarm;
uint8_t              i;

   psAlarm = VPC3_NULL_PTR;
   for( i = 0; i < ALARM_MAX_FIFO; i++ )
   {
      if( sAlarm.abBufferUsed[i] == VPC3_FALSE )
      {
         sAlarm.abBufferUsed[i] = VPC3_TRUE;

         psAlarm = &(sAlarm.asAlarmBuffers[i]);
         psAlarm->bHeader         = 0;
         psAlarm->bAlarmType      = 0;
         psAlarm->bSlotNr         = 0;
         psAlarm->bSpecifier      = 0;
         psAlarm->bUserDiagLength = 0;
         psAlarm->pToUserDiagData = &(sAlarm.abUserAlarmData[i][0]);

         break;
      }//if( sAlarm.abBufferUsed[i] == VPC3_FALSE )
   }//for( i = 0; i < ALARM_MAX_FIFO; i++ )

   return psAlarm;
}//ALARM_STATUS_PDU_PTR DpDiag_AlarmAlloc( void )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_FreeAlarmBuffer                                          */
/*--------------------------------------------------------------------------*/
/*!
   \brief Set alarm/diagnostic buffer to state free.

   \param[in] psAlarm - alarm/diagnostic buffer
*/
void DpDiag_FreeAlarmBuffer( ALARM_STATUS_PDU_PTR psAlarm )
{
ALARM_STATUS_PDU_PTR psFindAlarm;
uint8_t              i;

   //V504
   for( i = 0; i < ALARM_MAX_FIFO; i++ )
   {
      if( sAlarm.abBufferUsed[i] == VPC3_TRUE )
      {
         psFindAlarm = &(sAlarm.asAlarmBuffers[i]);
         if( psFindAlarm == psAlarm )
         {
            sAlarm.abBufferUsed[i] = VPC3_FALSE;
            memset( psFindAlarm->pToUserDiagData, 0, MaxAlarmLength );
            memset( psFindAlarm, 0, sizeof( ALARM_STATUS_PDU ) );
            break;
         }//if( psFindAlarm == psAlarm )
      }//if( sAlarm.abBufferUsed[i] == VPC3_FALSE )
   }//for( i = 0; i < ALARM_MAX_FIFO; i++ )
}//void DpDiag_FreeAlarmBuffer( ALARM_STATUS_PDU_PTR ptr )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_Init                                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief Initializes local structures.
 *
 * This function is called during startup and with each received parameter telegram from PROFIBUS-DP master.
 */
void DpDiag_Init( void )
{
   //nothing todo in this example.
}//void DpDiag_Init( void )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_CheckExtDiagBit                                          */
/*---------------------------------------------------------------------------*/
/**
 * @brief If one entry in indentifier related dignostic block is one, we set EXT_DIAG_SET.
 *
 * @return EXT_DIAG_SET - the diagnostic message consist of an error
 * @return EXT_DIAG_RESET - no error detected
 */
static uint8_t DpDiag_CheckExtDiagBit( void )
{
uint8_t bExtDiagFlag;
uint8_t i;

   bExtDiagFlag = EXT_DIAG_RESET;

   for( i = 0; i < ( cSizeOfIdentifierDiagnosis - 1 ); i++ )
   {
      if( sIdentRelDiag.abIdentRelDiag[ i ] != 0 )
      {
         bExtDiagFlag = EXT_DIAG_SET;
         break;
      }//if( sIdentRelDiag.abIdentRelDiag[ i ] != 0 )
   }//for( i = 0; i < ( cSizeOfIdentifierDiagnosis - 1 ); i++ )

   return bExtDiagFlag;
}//static uint8_t DpDiag_CheckExtDiagBit( void )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_AddIdentRelDiagBlock                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief Add identifier related diagnostic block to local diagnostic.
 *
 * @param[in]pbToDiagBuffer - pointer to local diagnostic buffer
 *
 * @return length of identifier related diagnostic block
 */
static uint8_t DpDiag_AddIdentRelDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )
{
   memcpy( pbToDiagBuffer, &sIdentRelDiag, cSizeOfIdentifierDiagnosis );
   printf("DEBUG: [DpDiag_AddIdentRelDiagBlock] Copiando %d bytes: ", cSizeOfIdentifierDiagnosis);
   for (uint8_t i = 0; i < cSizeOfIdentifierDiagnosis; i++) {
       printf("0x%02X ", ((uint8_t*)&sIdentRelDiag)[i]);
   }
   printf("\n");
   return cSizeOfIdentifierDiagnosis;
}//static uint8_t DpDiag_AddIdentRelDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_AddModuleStatDiagBlock                                   */
/*---------------------------------------------------------------------------*/
/**
 * @brief Add module status to local diagnostic.
 *
 * @param[in]pbToDiagBuffer - pointer to local diagnostic buffer
 *
 * @return length of module status
 */
static uint8_t DpDiag_AddModuleStatDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )
{
   memcpy( pbToDiagBuffer, &sModuleStatDiag, cSizeOfModuleStatusDiagnosis );
   printf("DEBUG: [DpDiag_AddModuleStatDiagBlock] Copiando %d bytes: ", cSizeOfModuleStatusDiagnosis);
   for (uint8_t i = 0; i < cSizeOfModuleStatusDiagnosis; i++) {
       printf("0x%02X ", ((uint8_t*)&sModuleStatDiag)[i]);
   }
   printf("\n");
   return cSizeOfModuleStatusDiagnosis;
}//static uint8_t DpDiag_AddModuleStatDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_AddChannelRelDiagBlock                                   */
/*---------------------------------------------------------------------------*/
/**
 * @brief Add channel related diagnostic block to local diagnostic.
 *
 * @param[in]pbToDiagBuffer - pointer to local diagnostic buffer
 *
 * @return length of channel related diagnostic block
 */
static uint8_t DpDiag_AddChannelRelDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )
{
ROMCONST__ uint8_t abChnDiagSlot5[3] = { 0x84, 0xC1, 0x82 };  //module 5
ROMCONST__ uint8_t abChnDiagSlot6[3] = { 0x85, 0xC1, 0x82 };  //module 6
// uint8_t bNrOfChannelDiags = 0x00; // UNUSED
uint8_t bLength = 0x00;
uint8_t i;

   bLength = 0x00;
   for( i = 0; i < ( cSizeOfIdentifierDiagnosis - 1 ); i++ )
   {
      if( sIdentRelDiag.abIdentRelDiag[ i ] > 0x00 )
      {
         if( sIdentRelDiag.abIdentRelDiag[ i ] & 0x10 )
         {
            memcpy( pbToDiagBuffer+bLength, &abChnDiagSlot5[0], 3 );
            bLength += 3;
         }//if( sIdentRelDiag.abIdentRelDiag[ i ] & 0x10 )

         if( sIdentRelDiag.abIdentRelDiag[ i ] & 0x20 )
         {
            memcpy( pbToDiagBuffer+bLength, &abChnDiagSlot6[0], 3 );
            bLength += 3;
         }//if( sIdentRelDiag.abIdentRelDiag[ i ] & 0x20 )
      }//if( sIdentRelDiag.abIdentRelDiag[ i ] > 0x00 )
   }//for( i = 0; i < SliceBus_GetModuleCount(); i++ )

   return bLength;
}//static uint8_t DpDiag_AddChannelRelDiagBlock( MEM_UNSIGNED8_PTR pbToDiagBuffer )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_AddProcessAlarm                                          */
/*---------------------------------------------------------------------------*/
/**
 * @brief Add process alarm to local diagnostic.
 *
 * @param[in]pbToDiagBuffer - pointer to local diagnostic buffer
 * @param[in]psAlarm - pointer to process alarm
 *
 * @return length of process alarm
 */
static uint8_t DpDiag_AddProcessAlarm( MEM_UNSIGNED8_PTR pbToDiagBuffer, ALARM_STATUS_PDU_PTR psAlarm )
{
uint8_t bLength;

   bLength = 0x00;
   memcpy( pbToDiagBuffer+bLength, &psAlarm->bHeader, cSizeOfAlarmHeader );
   bLength += cSizeOfAlarmHeader;
   memcpy( pbToDiagBuffer+bLength, psAlarm->pToUserDiagData, psAlarm->bUserDiagLength );
   bLength += psAlarm->bUserDiagLength;

   return bLength;
}//static uint8_t DpDiag_AddProcessAlarm( MEM_UNSIGNED8_PTR pbToDiagBuffer, ALARM_STATUS_PDU_PTR psAlarm )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_AddDiagnosticAlarm                                       */
/*---------------------------------------------------------------------------*/
/**
 * @brief Add process alarm to local diagnostic.
 *
 * @param[in]pbToDiagBuffer - pointer to local diagnostic buffer
 * @param[in]psAlarm - pointer to diagnostic alarm
 *
 * @return length of diagnostic alarm
 */
static uint8_t DpDiag_AddDiagnosticAlarm( MEM_UNSIGNED8_PTR pbToDiagBuffer, ALARM_STATUS_PDU_PTR psAlarm )
{
uint8_t bLength;

   bLength = 0x00;
   memcpy( pbToDiagBuffer+bLength, &psAlarm->bHeader, cSizeOfAlarmHeader );
   bLength += cSizeOfAlarmHeader;
   memcpy( pbToDiagBuffer+bLength, psAlarm->pToUserDiagData, psAlarm->bUserDiagLength );
   bLength += psAlarm->bUserDiagLength;

   return bLength;
}//static uint8_t DpDiag_AddDiagnosticAlarm( MEM_UNSIGNED8_PTR pbToDiagBuffer, ALARM_STATUS_PDU_PTR psAlarm )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_Alarm ( is also called from alarm state machine !!!! )   */
/*---------------------------------------------------------------------------*/
/*!
  \brief Set diagnostic data to VPC3+.
  By calling this function, the user provides diagnostic data to the slave.
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

  \param[in] bAlarmType - type of alarm (application specific)
  \param[in] bSeqNr - running number, don't send same diagnostic twice!
  \param[in] psAlarm - pointer to alarm structure (see DpDiag_DemoDiagnostic)
  \param[in] bCheckDiagFlag - check VPC3+ diagnostic flag

  Values for bUserDiagLength:
  - 0 - A previously set user diagnostic is deleted from the slave's diagnostic buffer. Only 6 bytes
        standard diagnostic are sent in the diagnostic telegram. In this case, the user does not have to transfer a
        pointer to a diagnostic buffer.
  - 1..DIAG_BUFSIZE-6 - Length of the new user diagnostic data.

  Values for bExtDiagFlag:
  - DIAG_RESET - Reset bit 'extended diagnostic, static diagnostic, diagnostic overflow'
  - EXT_DIAG_SET - Set bit 'extended diagnostic'.
  - STAT_DIAG_SET - Set bit 'static diagnostic'.

  \retval DP_OK - Execution OK, diagnostic message is copied into VPc3+
  \retval DP_DIAG_OLD_DIAG_NOT_SEND_ERROR - Error, wait because last diagnostic message isn't send
  \retval DP_DIAG_BUFFER_LENGTH_ERROR - Error, diagnostic message is too long
  \retval DP_DIAG_CONTROL_BYTE_ERROR - Error, sequence number is wrong
  \retval DP_DIAG_BUFFER_ERROR - Error, diagnostic header is wrong
  \retval DP_DIAG_SEQUENCE_ERROR - Error, revision will be send in wrong state
  \retval DP_DIAG_NOT_POSSIBLE_ERROR - Error, unknown diagnostic header
*/
DP_ERROR_CODE DpDiag_Alarm( uint8_t bAlarmType, uint8_t bSeqNr, ALARM_STATUS_PDU_PTR psAlarm, uint8_t bCheckDiagFlag )
{
MEM_UNSIGNED8_PTR pbToDiagArray;
DP_ERROR_CODE     bRetValue;
uint8_t           bExtDiagFlag;
uint8_t           bDiagLength;
uint16_t          wDiagEvent;

   bRetValue = DP_NOK;
   
   printf("DEBUG: [DpDiag_Alarm] INICIO - bAlarmType=0x%02X, bSeqNr=0x%02X, bCheckDiagFlag=%d\n", bAlarmType, bSeqNr, bCheckDiagFlag);
   printf("DEBUG: [DpDiag_Alarm] Constantes: USER_TYPE_CFG_OK=0x%02X, STAT_DIAG_SET=0x%02X, EXT_DIAG_SET=0x%02X\n", USER_TYPE_CFG_OK, STAT_DIAG_SET, EXT_DIAG_SET);

   psAlarm = psAlarm; //avoid warning
   wDiagEvent = ( (uint16_t)(bAlarmType << 8) | ((uint16_t)bSeqNr) );
   printf("DEBUG: [DpDiag_Alarm] wDiagEvent calculado: 0x%04X\n", wDiagEvent);

   VPC3_CheckDiagBufferChanged();
   printf("DEBUG: [DpDiag_Alarm] VPC3_CheckDiagBufferChanged() completado\n");

   //don't send diagnostic twice! 0x10
   printf("DEBUG: [DpDiag_Alarm] Comparando wDiagEvent (0x%04X) con pDpSystem->wOldDiag (0x%04X)\n", wDiagEvent, pDpSystem->wOldDiag);
   if( wDiagEvent != pDpSystem->wOldDiag )
   {
      printf("DEBUG: [DpDiag_Alarm] wDiagEvent != wOldDiag, continuando...\n");
      printf("DEBUG: [DpDiag_Alarm] Verificando VPC3_GetDpState(eDpStateDiagActive): %d\n", VPC3_GetDpState( eDpStateDiagActive ));
      if( !( VPC3_GetDpState( eDpStateDiagActive ) ))
      {
         printf("DEBUG: [DpDiag_Alarm] Estado diagnóstico NO activo, procediendo...\n");
      	memset( &pDpSystem->abUserDiagnostic[0], 0x00, sizeof( pDpSystem->abUserDiagnostic ) );
      	pbToDiagArray = pDpSystem->abUserDiagnostic;
      	
      	bExtDiagFlag = 0x00;
      	bDiagLength = 0x00;
      	
      	printf("DEBUG: [DpDiag_Alarm] Procesando bAlarmType=0x%02X en switch...\n", bAlarmType);
      	switch( bAlarmType )
      	{
      	   case USER_TYPE_CFG_NOK:
      	   case USER_TYPE_CFG_OK:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso USER_TYPE_CFG_OK/NOK\n");
      	      bExtDiagFlag = ( bAlarmType == USER_TYPE_CFG_OK ) ? STAT_DIAG_SET : EXT_DIAG_SET;
      	      printf("DEBUG: [DpDiag_Alarm] bExtDiagFlag configurado: 0x%02X\n", bExtDiagFlag);
      	      bDiagLength += DpDiag_AddIdentRelDiagBlock( pbToDiagArray+bDiagLength );
      	      printf("DEBUG: [DpDiag_Alarm] Después de AddIdentRelDiagBlock: bDiagLength=%d\n", bDiagLength);
      	      bDiagLength += DpDiag_AddModuleStatDiagBlock( pbToDiagArray+bDiagLength );
      	      printf("DEBUG: [DpDiag_Alarm] Después de AddModuleStatDiagBlock: bDiagLength=%d\n", bDiagLength);
      	      break;
      	   }//case USER_TYPE_CFG_OK:
      	
      	   case USER_TYPE_APPL_RDY:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso USER_TYPE_APPL_RDY\n");
      	      bDiagLength += DpDiag_AddIdentRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddModuleStatDiagBlock( pbToDiagArray+bDiagLength );
      	      break;
      	   }//case USER_TYPE_CFG_OK:
      	
      	   case ALARM_TYPE_DIAGNOSTIC:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso ALARM_TYPE_DIAGNOSTIC\n");
      	      //check alarm appear/disappear?
      	      if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	      {
      	         //set entry in identifier related diagnostic block
      	         DpDiag_SetIdentRelDiagEntry( (psAlarm->bSlotNr-1) );
      	         //set entry in module status diagnostic block to FAULT
      	         DpDiag_SetModulStatusEntry( (psAlarm->bSlotNr-1), DIAG_MS_FAULT );
      	      }//if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	      else
      	      {
      	         //clear entry in identifier related diagnostic block
      	         DpDiag_ClrIdentRelDiagEntry( (psAlarm->bSlotNr-1) );
      	         //set entry in module status diagnostic block to OK
      	         DpDiag_SetModulStatusEntry( (psAlarm->bSlotNr-1), DIAG_MS_OK );
      	      }//else of if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	
      	      bExtDiagFlag |= DpDiag_CheckExtDiagBit();
      	      bDiagLength += DpDiag_AddIdentRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddModuleStatDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddChannelRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddDiagnosticAlarm( pbToDiagArray+bDiagLength, psAlarm );
      	      break;
      	   }//ALARM_TYPE_DIAGNOSTIC
      	
      	   case ALARM_TYPE_PROCESS:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso ALARM_TYPE_PROCESS\n");
      	      bExtDiagFlag |= DpDiag_CheckExtDiagBit();
      	      bDiagLength += DpDiag_AddIdentRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddModuleStatDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddChannelRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddProcessAlarm( pbToDiagArray+bDiagLength, psAlarm );
      	      break;
      	   }//case ALARM_TYPE_PROCESS:
      	
      	   case STATUS_MESSAGE:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso STATUS_MESSAGE\n");
      	      //DP-V0 diagnostic
      	      //check alarm appear/disappear?
      	      if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	      {
      	         //set entry in identifier related diagnostic block
      	         DpDiag_SetIdentRelDiagEntry( (psAlarm->bSlotNr-1) );
      	         //set entry in module status diagnostic block to FAULT
      	         DpDiag_SetModulStatusEntry( (psAlarm->bSlotNr-1), DIAG_MS_FAULT );
      	      }//if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	      else
      	      {
      	         //clear entry in identifier related diagnostic block
      	         DpDiag_ClrIdentRelDiagEntry( (psAlarm->bSlotNr-1) );
      	         //set entry in module status diagnostic block to OK
      	         DpDiag_SetModulStatusEntry( (psAlarm->bSlotNr-1), DIAG_MS_OK );
      	      }//else of if( (psAlarm->bSpecifier & SPEC_MASK) == SPEC_APPEARS )
      	
      	      bExtDiagFlag |= DpDiag_CheckExtDiagBit();
      	      bDiagLength += DpDiag_AddIdentRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddModuleStatDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddChannelRelDiagBlock( pbToDiagArray+bDiagLength );
      	      bDiagLength += DpDiag_AddDiagnosticAlarm( pbToDiagArray+bDiagLength, psAlarm );
      	      break;
      	   }//case STATUS_MESSAGE:
      	
      	   case USER_TYPE_RESET_DIAG:
      	   case USER_TYPE_PRM_NOK:
      	   case USER_TYPE_PRM_OK:
      	   default:
      	   {
      	      printf("DEBUG: [DpDiag_Alarm] Caso default/otros\n");
      	      bExtDiagFlag = 0x00;
      	      bDiagLength = 0x00;
      	      break;
      	   }//default:
      	}//switch( bAlarmType )
      	
      	printf("DEBUG: [DpDiag_Alarm] Después del switch: bDiagLength=%d, bExtDiagFlag=0x%02X\n", bDiagLength, bExtDiagFlag);
      	VPC3_SetDpState( eDpStateDiagActive );
      	printf("DEBUG: [DpDiag_Alarm] Estado diagnóstico activado\n");
      	
      	printf("DEBUG: [DpDiag_Alarm] Llamando VPC3_SetDiagnosis con bDiagLength=%d, bExtDiagFlag=0x%02X, bCheckDiagFlag=%d\n", bDiagLength, bExtDiagFlag, bCheckDiagFlag);
      	
      	// DEBUG: Mostrar contenido del buffer de diagnóstico
      	printf("DEBUG: [DpDiag_Alarm] Contenido del buffer de diagnóstico (%d bytes): ", bDiagLength);
      	for (uint8_t i = 0; i < bDiagLength && i < 16; i++) {
      	    printf("0x%02X ", pDpSystem->abUserDiagnostic[i]);
      	}
      	printf("\n");
      	
      	bRetValue = VPC3_SetDiagnosis( pDpSystem->abUserDiagnostic, bDiagLength, bExtDiagFlag, bCheckDiagFlag );
      	printf("DEBUG: [DpDiag_Alarm] VPC3_SetDiagnosis retornó: %d\n", bRetValue);
      	
      	if( bRetValue == DP_OK )
      	{
      	   printf("DEBUG: [DpDiag_Alarm] VPC3_SetDiagnosis exitoso, actualizando wOldDiag\n");
      	   pDpSystem->wOldDiag = wDiagEvent;
      	}//if( bRetValue == DP_OK )
      	else
      	{
      	   printf("DEBUG: [DpDiag_Alarm] VPC3_SetDiagnosis falló, limpiando estado diagnóstico\n");
      	   VPC3_ClrDpState( eDpStateDiagActive );
      	}//else of if( bRetValue == DP_OK )
   	}//if( !( VPC3_GetDpState( eDpStateDiagActive ) ))
      else
      {
         printf("DEBUG: [DpDiag_Alarm] Estado diagnóstico YA activo, retornando DP_DIAG_ACTIVE_DIAG\n");
         bRetValue = DP_DIAG_ACTIVE_DIAG;
      }//else of if( !( VPC3_GetDpState( eDpStateDiagActive ) ))
   }//if( wDiagEvent != pDpSystem->wOldDiag )
   else
   {
      printf("DEBUG: [DpDiag_Alarm] wDiagEvent == wOldDiag, retornando DP_DIAG_SAME_DIAG\n");
      bRetValue = DP_DIAG_SAME_DIAG;
   }//else if( wDiagEvent != pDpSystem->wOldDiag )

   printf("DEBUG: [DpDiag_Alarm] FINAL - Retornando bRetValue=%d\n", bRetValue);
   return bRetValue;
}//DP_ERROR_CODE DpDiag_Alarm( uint8_t bAlarmType, uint8_t bSeqNr, ALARM_STATUS_PDU_PTR psAlarm, uint8_t bCheckDiagFlag )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_SetPrmOk                                                */
/*--------------------------------------------------------------------------*/
/**
 * @brief Set User specific diagnostic, if SetPrm is OK.
 * In this demo, the last user specific diagnostic will be cleared!
 *
 * @return DP_OK - diagnostic is set
 * @return DP_DIAG_SAME_DIAG - no change necessary
 * @return other - diagnostic is not resettet yet, try again
 */
DP_ERROR_CODE DpDiag_SetPrmOk( DP_ERROR_CODE ePrmError )
{
DP_ERROR_CODE bRetValue;
DP_ERROR_CODE bDiagRetValue;
uint8_t bLoop;

   bLoop = 0;
   bRetValue = DP_PRM_RETRY_ERROR;
   while( bLoop++ < MAX_DIAG_RETRY )
   {
      bDiagRetValue = DpDiag_Alarm( USER_TYPE_PRM_OK, 0, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE );
      if( ( bDiagRetValue == DP_OK ) || ( bDiagRetValue == DP_DIAG_SAME_DIAG ) )
      {
         bRetValue = ePrmError;
         break;
      }
   }

   return bRetValue;
}//DP_ERROR_CODE DpDiag_SetPrmOk( DP_ERROR_CODE ePrmError )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_SetPrmNotOk                                             */
/*--------------------------------------------------------------------------*/
/**
 * @brief Set User specific diagnostic, if SetPrm is not OK.
 * In this demo, the last user specific diagnostic will be cleared!
 *
 * @return VPC3_TRUE - diagnostic is set
 * @return VPC3_FALSE - diagnostic is not resettet yet, try again
 */
DP_ERROR_CODE DpDiag_SetPrmNotOk( DP_ERROR_CODE ePrmError )
{
DP_ERROR_CODE bRetValue;

uint8_t bLoop;

   bLoop = 0;
   bRetValue = DP_PRM_RETRY_ERROR;
   while( bLoop++ < MAX_DIAG_RETRY )
   {
      if( DpDiag_Alarm( USER_TYPE_PRM_NOK, 0, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE ) == DP_OK )
      {
         bRetValue = ePrmError;
         break;
      }
   }

   return bRetValue;
}//DP_ERROR_CODE DpDiag_SetPrmNotOk( DP_ERROR_CODE ePrmError )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_SetCfgOk                                                */
/*--------------------------------------------------------------------------*/
/**
 * @brief Reset static diagnostic bit.
 *
 * @return VPC3_TRUE - the static diagnostic is resettet
 * @return VPC3_FALSE - the static diagnostic is not resettet yet, try again
 */
E_DP_CFG_ERROR DpDiag_SetCfgOk( E_DP_CFG_ERROR eCfgError )
{
E_DP_CFG_ERROR eRetValue;
uint8_t bLoop;

   bLoop = 0;
   
   //eRetValue = DP_CFG_FAULT;
   eRetValue = eCfgError;  // Preserve the original configuration result
   printf("DEBUG: [DpDiag_SetCfgOk] Entrando con eCfgError=%d\n", eCfgError);
   printf("DEBUG: [DpDiag_SetCfgOk] eRetValue inicializado como eCfgError=%d\n", eRetValue);
   printf("DEBUG: [DpDiag_SetCfgOk] MAX_DIAG_RETRY=%d\n", MAX_DIAG_RETRY);
   
   while( bLoop++ < MAX_DIAG_RETRY )
   {
      printf("DEBUG: [DpDiag_SetCfgOk] Intento %d: Llamando DpDiag_Alarm...\n", bLoop);
      printf("DEBUG: [DpDiag_SetCfgOk] Parámetros: USER_TYPE_CFG_OK=%d, 0x22, VPC3_NULL_PTR, VPC3_FALSE\n", USER_TYPE_CFG_OK);
      
      if( DpDiag_Alarm( USER_TYPE_CFG_OK, 0x22, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE ) == DP_OK )
      {
         printf("DEBUG: [DpDiag_SetCfgOk] DpDiag_Alarm exitoso en intento %d\n", bLoop);
         printf("DEBUG: [DpDiag_SetCfgOk] eRetValue ANTES del break: %d\n", eRetValue);
         break;  // Success - keep the original eCfgError value
      }
      printf("DEBUG: [DpDiag_SetCfgOk] DpDiag_Alarm falló en intento %d\n", bLoop);
      printf("DEBUG: [DpDiag_SetCfgOk] eRetValue después del fallo: %d\n", eRetValue);
   }
   
   printf("DEBUG: [DpDiag_SetCfgOk] Bucle terminado. bLoop=%d\n", bLoop);
   printf("DEBUG: [DpDiag_SetCfgOk] eRetValue FINAL: %d\n", eRetValue);
   printf("DEBUG: [DpDiag_SetCfgOk] eCfgError original: %d\n", eCfgError);
   printf("DEBUG: [DpDiag_SetCfgOk] ¿Debería retornar eCfgError en lugar de eRetValue? %s\n", (eCfgError != eRetValue) ? "SÍ" : "NO");
   
   // If all retries failed, still return the original configuration result
   // The diagnostic alarm failure should not override the configuration validation
   printf("DEBUG: [DpDiag_SetCfgOk] Retornando eRetValue=%d\n", eRetValue);

   return eRetValue;
}//E_DP_CFG_ERROR DpDiag_SetCfgOk( E_DP_CFG_ERROR eCfgError )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_SetCfgNotOk                                             */
/*--------------------------------------------------------------------------*/
/**
 * @brief Set static diagnostic bit.
 *
 * @return VPC3_TRUE - the static diagnostic bit is set.
 * @return VPC3_FALSE - the static diagnostic bit is not set yet, try again
 */
uint8_t DpDiag_SetCfgNotOk( void )
{
   if( DpDiag_Alarm( USER_TYPE_CFG_NOK,  0x23, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE ) == DP_OK )
   {
      return VPC3_TRUE;
   }

   return VPC3_FALSE;
}//uint8_t DpDiag_SetCfgNotOk( void )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_ResetStatDiag                                           */
/*--------------------------------------------------------------------------*/
/**
 * @brief Reset static diagnostic bit.
 *
 * @return VPC3_TRUE - the static diagnostic bit is resettet
 * @return VPC3_FALSE - the static diagnostic bit is not resettet yet, try again
 */
uint8_t DpDiag_ResetStatDiag( void )
{
   if( DpDiag_Alarm( USER_TYPE_APPL_RDY, 0, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE ) == DP_OK )
   {
      return VPC3_TRUE;
   }
   return VPC3_FALSE;
}//uint8_t DpDiag_ResetStatDiag( void )

/*---------------------------------------------------------------------------*/
/* function: UserResetDiagnosticBuffer                                       */
/*---------------------------------------------------------------------------*/
/**
 * @brief Reset diagnostic buffer.
 */
void DpDiag_ResetDiagnosticBuffer( void )
{
   do
   {
      //fetch new diagnosis buffer
      pDpSystem->pDiagBuffer = VPC3_GetDiagBufPtr();
   }
   while( pDpSystem->pDiagBuffer == VPC3_NULL_PTR );

   //clear diagnostic buffer
   DpDiag_Alarm( USER_TYPE_RESET_DIAG, USER_TYPE_RESET_DIAG, (ALARM_STATUS_PDU_PTR) VPC3_NULL_PTR, VPC3_FALSE );
}//void DpDiag_ResetDiagnosticBuffer( void )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_IsrDiagBufferChanged                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+ has
 * exchanged the diagnostic buffers, and made the old buffer available again to the user.
 */
void DpDiag_IsrDiagBufferChanged( void )
{
   // diagnosis buffer has been changed
   VPC3_ClrDpState( eDpStateDiagActive );
   // Fetch new diagnosis buffer
   pDpSystem->pDiagBuffer = VPC3_GetDiagBufPtr();
}//void DpDiag_IsrDiagBufferChanged( void )


#if DP_ALARM
/*--------------------------------------------------------------------------*/
/* function: DpDiag_AlarmAckReq (called by DPV1)                             */
/*--------------------------------------------------------------------------*/
/**
 * @brief The firmware calls this functon if a Class1 master acknowledge an alarm.
 *
 * @param[in] psAlarm - pointer to alarm structure
 */
void DpDiag_AlarmAckReq( ALARM_STATUS_PDU_PTR psAlarm )
{
   // alarm_ack from master received
   // todo: do your alarm-acknowledge handling here
   DpDiag_FreeAlarmBuffer( psAlarm );
}//void DpDiag_AlarmAckReq( ALARM_STATUS_PDU_PTR psAlarm )
#endif//#if DP_ALARM

/*--------------------------------------------------------------------------*/
/* function: DpDiag_DemoDiagnostic                                          */
/*--------------------------------------------------------------------------*/
uint8_t DpDiag_DemoDiagnostic( uint8_t bDiagNr, uint8_t bModuleNr )
{
ROMCONST__ uint8_t abDiagnosticAlarmAppears[5]    = { 0x01,0x07,0x0F,0x67,0x33 };
ROMCONST__ uint8_t abDiagnosticAlarmDisappears[5] = { 0x01,0x00,0x0F,0x00,0x00 };
ROMCONST__ uint8_t abProcessAlarmUpperLimit[3]    = { 0x02,0x01,0x00 };
ROMCONST__ uint8_t abProcessAlarmLowerLimit[3]    = { 0x02,0x00,0x01 };

ALARM_STATUS_PDU_PTR psAlarm;
uint8_t bRetValue;
uint8_t bCallback;

   bRetValue = VPC3_TRUE;

   psAlarm = DpDiag_AlarmAlloc();
   if( psAlarm != VPC3_NULL_PTR )
   {
      bCallback = VPC3_FALSE;
      switch( bDiagNr )
      {
         case 1:
         case 3:
         {
            #ifdef RS232_SERIO
                print_string("\r\nDiag: ");
                print_hexbyte( bDiagNr );
            #endif//#ifdef RS232_SERIO
            sDpAppl.bDiagnosticAlarmSeqNr = (sDpAppl.bDiagnosticAlarmSeqNr + 1) & (MAX_SEQ_NR - 1);
            psAlarm->bHeader              = cSizeOfDiagnosticAlarm;
            psAlarm->bAlarmType           = ( pDpSystem->eDPV1 == DPV1_MODE ) ? ALARM_TYPE_DIAGNOSTIC : STATUS_MESSAGE;
            psAlarm->bSlotNr              = bModuleNr;
            psAlarm->bSpecifier           = SPEC_APPEARS + (sDpAppl.bDiagnosticAlarmSeqNr << SPEC_SEQ_START);
            psAlarm->bUserDiagLength      = cSizeOfUsrDiagnosticAlarmData;
            memcpy( psAlarm->pToUserDiagData, &abDiagnosticAlarmAppears[0], cSizeOfUsrDiagnosticAlarmData );
            //The alarm state machine calls the function Useralarm again.
            //The user can can add to the alarm e.g. module status or channel related diagnostic.
            bCallback = VPC3_TRUE;

            dpl_put_blk_to_list_end__( &sDpAppl.dplDiagQueue, &psAlarm->dplListHead );
            break;
         }//case 1:

         case 2:
         case 4:
         {
            #ifdef RS232_SERIO
                print_string("\r\nDiag: ");
                print_hexbyte( bDiagNr );
            #endif//#ifdef RS232_SERIO
            sDpAppl.bDiagnosticAlarmSeqNr = (sDpAppl.bDiagnosticAlarmSeqNr + 1) & (MAX_SEQ_NR - 1);
            psAlarm->bHeader              = cSizeOfDiagnosticAlarm;
            psAlarm->bAlarmType           = ( pDpSystem->eDPV1 == DPV1_MODE ) ? ALARM_TYPE_DIAGNOSTIC : STATUS_MESSAGE;
            psAlarm->bSlotNr              = bModuleNr;
            psAlarm->bSpecifier           = SPEC_DISAPPEARS + (sDpAppl.bDiagnosticAlarmSeqNr << SPEC_SEQ_START);
            psAlarm->bUserDiagLength      = cSizeOfUsrDiagnosticAlarmData;
            memcpy( psAlarm->pToUserDiagData, &abDiagnosticAlarmDisappears[0], cSizeOfUsrDiagnosticAlarmData );
            //The alarm state machine calls the function Useralarm again.
            //The user can can add to the alarm e.g. module status or channel related diagnostic.
            bCallback = VPC3_TRUE;

            dpl_put_blk_to_list_end__( &sDpAppl.dplDiagQueue, &psAlarm->dplListHead );
            break;
         }//case 2:

         case 5:
         {
            #ifdef RS232_SERIO
                print_string("\r\nDiag: ");
                print_hexbyte( bDiagNr );
            #endif//#ifdef RS232_SERIO
            sDpAppl.bProcessAlarmSeqNr = (sDpAppl.bProcessAlarmSeqNr + 1) & (MAX_SEQ_NR - 1);
            psAlarm->bHeader           = cSizeOfProcessAlarm;
            psAlarm->bAlarmType        = ( pDpSystem->eDPV1 == DPV1_MODE ) ? ALARM_TYPE_PROCESS : STATUS_MESSAGE;
            psAlarm->bSlotNr           = bModuleNr;
            psAlarm->bSpecifier        = SPEC_GENERAL + (sDpAppl.bProcessAlarmSeqNr << SPEC_SEQ_START);
            psAlarm->bUserDiagLength   = cSizeOfUsrProcessAlarmData;
            memcpy( psAlarm->pToUserDiagData, &abProcessAlarmUpperLimit[0], cSizeOfUsrProcessAlarmData );
            //The alarm state machine calls the function Useralarm again.
            //The user can can add to the alarm e.g. module status or channel related diagnostic.
            bCallback = VPC3_TRUE;

            dpl_put_blk_to_list_end__( &sDpAppl.dplDiagQueue, &psAlarm->dplListHead );
            break;
         }//case 5:

         case 6:
         {
            #ifdef RS232_SERIO
                print_string("\r\nDiag: ");
                print_hexbyte( bDiagNr );
            #endif//#ifdef RS232_SERIO
            sDpAppl.bProcessAlarmSeqNr = (sDpAppl.bProcessAlarmSeqNr + 1) & (MAX_SEQ_NR - 1);
            psAlarm->bHeader           = cSizeOfProcessAlarm;
            psAlarm->bAlarmType        = ( pDpSystem->eDPV1 == DPV1_MODE ) ? ALARM_TYPE_PROCESS : STATUS_MESSAGE;
            psAlarm->bSlotNr           = bModuleNr;
            psAlarm->bSpecifier        = SPEC_GENERAL + (sDpAppl.bProcessAlarmSeqNr << SPEC_SEQ_START);
            psAlarm->bUserDiagLength   = cSizeOfUsrProcessAlarmData;
            memcpy( psAlarm->pToUserDiagData, &abProcessAlarmLowerLimit[0], cSizeOfUsrProcessAlarmData );
            //The alarm state machine calls the function Useralarm again.
            //The user can can add to the alarm e.g. module status or channel related diagnostic.
            bCallback = VPC3_TRUE;

            dpl_put_blk_to_list_end__( &sDpAppl.dplDiagQueue, &psAlarm->dplListHead );
            break;
         }//case 6:

         default:
         {
            DpDiag_FreeAlarmBuffer( psAlarm );
            break;
         }//default:
      }//switch( bDiagNr )
   }//if( psAlarm != VPC3_NULL_PTR )

   (void)bCallback; // Suppress unused variable warning
   return bRetValue;
}//uint8_t DpDiag_DemoDiagnostic( uint8_t bDiagNr, uint8_t bModuleNr )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_SetDefIdentRelDiag                                       */
/*---------------------------------------------------------------------------*/
/**
 * @brief Set identifier related diagnostic block to default values.
 */
void DpDiag_SetDefIdentRelDiag( void )
{
uint8_t abIdentRelDiag[] = { 0x42,0x3F };

   memcpy( &sIdentRelDiag.bHeader, &abIdentRelDiag, cSizeOfIdentifierDiagnosis );
}//void DpDiag_SetDefIdentRelDiag( void )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_SetIdentRelDiagEntry                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief Set identifier related diagnostic of module on SliceBus.
 *
 * @param[in] bModNr - module number
 */
void DpDiag_SetIdentRelDiagEntry( uint8_t bModNr )
{
   sIdentRelDiag.abIdentRelDiag[ bModNr/8 ] |= (DIAG_IR_FAULT << (bModNr%8));
}//void DpDiag_SetIdentRelDiagEntry( uint8_t bModNr )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_ClrIdentRelDiagEntry                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief Clear identifier related diagnostic of module on SliceBus.
 *
 * @param[in] bModNr - module number
 */
void DpDiag_ClrIdentRelDiagEntry( uint8_t bModNr )
{
   sIdentRelDiag.abIdentRelDiag[ bModNr/8 ] &= ~(DIAG_IR_FAULT << (bModNr%8));
}//void DpDiag_ClrIdentRelDiagEntry( uint8_t bModNr )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_SetDefModuleStatDiag                                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief Set moduls status diagnostic block to default values.
 */
void DpDiag_SetDefModuleStatDiag( void )
{
uint8_t abModStatDiag[] = { 0x06,0x82,0x00,0x00,0xAA,0x0A };

   memcpy( &sModuleStatDiag.bHeader, &abModStatDiag, cSizeOfModuleStatusDiagnosis );
}//void DpDiag_SetDefModuleStatDiag( void )

/*---------------------------------------------------------------------------*/
/* function: DpDiag_SetModulStatusEntry                                      */
/*---------------------------------------------------------------------------*/
/**
 * @brief Set status of diagnostic block Module-Status.
 *
 * @param[in] bModNr - number of module
 * @param[in] eMs - module status type
 */
void DpDiag_SetModulStatusEntry( uint8_t bModNr, E_DP_DIAG_MS eMs )
{
   sModuleStatDiag.abModuleStatus[ bModNr/4 ] &= ~(DIAG_MS_NO_MODULE << ((bModNr%4)*2));
   sModuleStatDiag.abModuleStatus[ bModNr/4 ] |= (eMs << ((bModNr%4)*2));
}//void DpDiag_SetModulStatusEntry( uint8_t bModNr, E_DP_DIAG_MS eMs )

/*--------------------------------------------------------------------------*/
/* function: DpDiag_CheckDpv0Diagnosis                                      */
/*--------------------------------------------------------------------------*/
void DpDiag_CheckDpv0Diagnosis( void )
{
   // Check for overload alarm
   if (sSystem.sInput.abDi8[0] & 0x01) {
       DpDiag_Alarm(USER_TYPE_DPV0, ALARM_OVERLOAD, NULL, VPC3_TRUE);
   }

   // Check for over temperature alarm
   if (sSystem.sInput.abDi8[0] & 0x02) {
       DpDiag_Alarm(USER_TYPE_DPV0, ALARM_OVER_TEMP, NULL, VPC3_TRUE);
   }

   // Check for power loss alarm
   if (sSystem.sInput.abDi8[0] & 0x04) {
       DpDiag_Alarm(USER_TYPE_DPV0, ALARM_POWER_LOSS, NULL, VPC3_TRUE);
   }

   // Check for relay status alarm
   if (sSystem.sInput.abDi8[0] & 0x08) {
       DpDiag_Alarm(USER_TYPE_DPV0, ALARM_RELAY_STATUS, NULL, VPC3_TRUE);
   }
}

/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

