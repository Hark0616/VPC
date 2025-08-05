/************************** Filename: dp_user.h ******************************/
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
/* Function:                                                                 */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Technical support:       eMail: support@profichip.com                     */
/*                          Phone: ++49-9132-744-2150                        */
/*                          Fax. : ++49-9132-744-29-2150                     */
/*                                                                           */
/*****************************************************************************/

/*! \file
     \brief Header file for user application.

*/

/*****************************************************************************/
/* contents:
*/
/*****************************************************************************/
/* reinclude protection */


#ifndef DP_APPL_H
#define DP_APPL_H

/*---------------------------------------------------------------------------*/
/* defines, structures                                                       */
/*---------------------------------------------------------------------------*/
//Software Version
#define VERSION1 '6'
#define VERSION2 '0'
#define VERSION3 '9'

/** \brief Events for PROFIBUS Application */
typedef enum
{
   eDpApplEv_Init              = 0x0000,   /**< Init value. */
   eDpApplEv_ResetBasp         = 0x0001,   /**< Enable BASP signal. */
   eDpApplEv_SetBasp           = 0x0002,   /**< Disable BASP signal. */
   eDpApplEv_IoIn              = 0x0010,   /**< Read input data. */
   eDpApplEv_IoOut             = 0x0020,   /**< Write output data. */
   eDpApplEv_Freeze            = 0x0100,   /**< Write freeze command. */
   eDpApplEv_UnFreeze          = 0x0200,   /**< Write unfreeze command. */
   eDpApplEv_Sync              = 0x0400,   /**< Write sync command. */
   eDpApplEv_UnSync            = 0x0800,   /**< Write unsync command. */
   eDpApplEv_FwUpd             = 0x1000,   /**< Firmware update command. */
   eDpApplEv_PollEepromRW      = 0x2000,   /**< Poll EEPROM Read command. */
   eDpApplEv_PollServChnRW     = 0x4000    /**< Poll service channel. */
} eDpApplEv_Flags;

typedef struct
{
   eDpApplEv_Flags            eDpApplEvent;                 /**< User application structure. */

   CFG_STRUCT                 sCfgData;                     /**< Default PROFIBUS configuration data. */

   uint8_t                    bDiagnosticAlarmSeqNr;        /**< Sequence-number for diagnostic alarms. */
   uint8_t                    bProcessAlarmSeqNr;           /**< Sequence-number for process alarms. */
   DPL_STRUC_LIST_CB          dplDiagQueue;                 /**< Double pointered list for DP-V0 diagnostics. */

   #if DP_FDL
      uint8_t                 abDpv1RwBuffer[ C1_LEN  ];    /**< Buffer for DP-V1 Read/Write service - only for testing. */

      #if DPV1_IM_SUPP

         sIM0                 sIM_0;

         #if IM_1_4_OPTIONAL_SUPP
         	uint16_t          wRevCounter;
            sIM1              sIM_1;
            sIM1              sIM_2;
            sIM1              sIM_3;
            sIM1              sIM_4;
         #endif /* #if IM_1_4_OPTIONAL_SUPP */

      #endif /* #if DPV1_IM_SUPP */
   #endif//#if DP_FDL
}DP_APPL_STRUC;

extern DP_APPL_STRUC  sDpAppl;   /**< User application structure. */

typedef struct
{
   uint8_t  bAlarm;
   uint8_t  bMode;
   uint8_t  bTimeBase;
   uint16_t wUpperLimit;
   uint16_t wLowerLimit;
} STRUC_PRM_COUNTER;

typedef struct
{
   uint8_t bLength;
   uint8_t bSlotNr;
   uint8_t bData;
}STRUC_MODULE_PRM_BLOCK;
#define MEM_STRUC_MODULE_PRM_BLOCK_PTR  STRUC_MODULE_PRM_BLOCK MEM_PTR

typedef struct
{
   uint8_t abDo8[2];
   uint8_t abDio32[4];
   uint8_t abCounter[2];
} STRUC_OUTPUT;

typedef struct
{
   uint8_t abDi8[2];
   uint8_t abDio32[4];
   uint8_t abCounter[2];
} STRUC_INPUT;

#define MaxModule ((uint8_t)0x02) //#define MaxModule ((uint8_t)0x06)

typedef struct
{
   uint8_t             bNrOfModules;
   STRUC_INPUT         sInput;
   STRUC_OUTPUT        sOutput;
   STRUC_PRM_COUNTER   sPrmCounter;
} STRUC_SYSTEM;

extern STRUC_SYSTEM      sSystem;

/*****************************************************************************/
/* reinclude-protection */


#else
    #pragma message "The header DPAPPL.H is included twice or more !"
#endif


/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2012. Confidential.                         */
/*****************************************************************************/

