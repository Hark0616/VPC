/***********************  Filename: dp_isr.c  ********************************/
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
/* Function:       interrupt service routine for VPC3+                       */
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

   - function prototypes
   - data structures
   - internal functions

*/
/*****************************************************************************/
/* include hierarchy */
#include <string.h>
#include "platform.h"
#include "main.h"  /* Para HAL_GPIO_WritePin y definiciones GPIO */
#include <stdio.h>
#include "dp_inc.h"  /* Para definiciones de estructuras PROFIBUS */
#include "dp_if.h"   /* Para definiciones de estructuras SSA */


#if VPC3_SERIAL_MODE
   #define MakeWord( Hi, Lo ) ( (uint16_t)( ( ( (uint8_t)( Hi ) ) << 8 ) | ( (uint8_t)( Lo ) ) ) )
#endif /* #if VPC3_SERIAL_MODE */


/*---------------------------------------------------------------------------*/
/* function: VPC3_Poll                                                       */
/*---------------------------------------------------------------------------*/
#if VPC3_SERIAL_MODE
uint16_t VPC3_Poll( void )
{
volatile uint8_t bResult;

   #if DP_INTERRUPT_MASK_8BIT == 0

      #if VPC3_SERIAL_MODE

         pDpSystem->wPollInterruptEvent = MakeWord( Vpc3Read( bVpc3RwIntReqReg_H ), Vpc3Read( bVpc3RwIntReqReg_L ) );

      #else

         CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->wPollInterruptEvent, ((uint8_t *)(VPC3_ADR)( Vpc3AsicAddress )), 2 );

         #if BIG_ENDIAN
            Swap16( &pDpSystem->wPollInterruptEvent );
         #endif /* #if BIG_ENDIAN */

      #endif /* #if VPC3_SERIAL_MODE */

      // Log para verificar la máscara de software
      printf("DEBUG: [VPC3_Poll] Eventos hardware leidos: 0x%04X, Mascara de software aplicada: 0x%04X\n",
             pDpSystem->wPollInterruptEvent, pDpSystem->wPollInterruptMask);

      pDpSystem->wPollInterruptEvent &= pDpSystem->wPollInterruptMask;

      if( pDpSystem->wPollInterruptEvent > 0 )
      {

   #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */

         #if( DP_TIMESTAMP == 0 )
            /*---------------------------------------------------------------*/
            /* IND_MAC_RESET                                                 */
            /*---------------------------------------------------------------*/
            if( VPC3_POLL_IND_MAC_RESET() )
            {
               DpAppl_MacReset();

               VPC3_CON_IND_MAC_RESET();
            } /* if( VPC3_POLL_IND_MAC_RESET() ) */
         #endif /* #if( DP_TIMESTAMP == 0 ) */

         /*------------------------------------------------------------------*/
         /* IND_DIAG_BUF_CHANGED                                             */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_DIAG_BUFFER_CHANGED() )
         {
            DpDiag_IsrDiagBufferChanged();

            VPC3_POLL_CON_IND_DIAG_BUFFER_CHANGED();
         } /* if( VPC3_POLL_IND_DIAG_BUFFER_CHANGED() ) */

         /*------------------------------------------------------------------*/
         /* IND_NEW_PRM_DATA                                                 */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_PRM_DATA() )
         {
            printf("🎯 [dp_isr] === PRM RECIBIDO DEL MASTER ===\r\n");
            printf("🎯 [dp_isr] TIMESTAMP: %lu ms\r\n", HAL_GetTick());
            printf("🎯 [dp_isr] STATUS_L antes de procesar PRM: 0x%02X\r\n", VPC3_GET_STATUS_L());
            printf("🎯 [dp_isr] STATUS_H antes de procesar PRM: 0x%02X\r\n", VPC3_GET_STATUS_H());
            
            uint8_t bPrmLength;

            #if DP_INTERRUPT_MASK_8BIT == 0
               VPC3_POLL_CON_IND_NEW_PRM_DATA();
            #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */

            bResult = VPC3_PRM_FINISHED;

            do
            {
                               bPrmLength = VPC3_GET_PRM_LEN();

                // --- VALIDACIÓN CRÍTICA: Verificar puntero PRM antes de leer ---
                VPC3_UNSIGNED8_PTR prmBufPtr = VPC3_GET_PRM_BUF_PTR();
                VPC3_ADR prmAddr = (VPC3_ADR)prmBufPtr;
                
                printf("DEBUG: [dp_isr] PRM Buffer Ptr: 0x%08X, Addr: 0x%04X\r\n", (unsigned int)prmBufPtr, prmAddr);
                
                // Validar que la dirección está dentro del rango válido
                if (prmAddr >= ASIC_RAM_LENGTH) {
                    printf("🚨 [dp_isr] ERROR: Puntero PRM corrupto! Addr=0x%04X >= ASIC_RAM_LENGTH=0x%04X\r\n", 
                           prmAddr, ASIC_RAM_LENGTH);
                    printf("🚨 [dp_isr] Rechazando trama PRM corrupta para evitar LECTURA ILEGAL\r\n");
                    bResult = VPC3_SET_PRM_DATA_NOT_OK();
                    break; // Salir del bucle do-while
                }
                
                // Validar que la longitud es razonable
                if (bPrmLength > 50) {
                    printf("🚨 [dp_isr] ERROR: Longitud PRM sospechosa (%d bytes). Rechazando.\r\n", bPrmLength);
                    bResult = VPC3_SET_PRM_DATA_NOT_OK();
                    break; // Salir del bucle do-while
                }

                CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], prmBufPtr, bPrmLength );

               if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK )
               {
                  #if REDUNDANCY
                     #if DP_MSAC_C1
                        if( VPC3_GET_PRM_LEN() != PRM_CMD_LENGTH )
                        {
                           MSAC_C1_CheckIndNewPrmData( (MEM_STRUC_PRM_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength );
                        } /* if( VPC3_GET_PRM_LEN() != PRM_CMD_LENGTH ) */
                     #endif /* #if DP_MSAC_C1 */
                  #else
                     #if DP_MSAC_C1
                        MSAC_C1_CheckIndNewPrmData( (MEM_STRUC_PRM_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength );
                     #endif /* #if DP_MSAC_C1 */
                  #endif /* #if REDUNDANCY */

                  bResult = VPC3_SET_PRM_DATA_OK();
               } /* if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK ) */
               else
               {
                  bResult = VPC3_SET_PRM_DATA_NOT_OK();
               } /* else of if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK ) */
            }
            while( bResult == VPC3_PRM_CONFLICT );
         } /* if( VPC3_POLL_IND_NEW_PRM_DATA() ) */

         /*------------------------------------------------------------------*/
         /* check config data , application specific!                        */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_CFG_DATA() )
         {
            printf("🎯 [dp_isr] === CFG RECIBIDO DEL MASTER ===\r\n");
            printf("🎯 [dp_isr] TIMESTAMP: %lu ms\r\n", HAL_GetTick());
            printf("🎯 [dp_isr] STATUS_L antes de procesar CFG: 0x%02X\r\n", VPC3_GET_STATUS_L());
            printf("🎯 [dp_isr] STATUS_H antes de procesar CFG: 0x%02X\r\n", VPC3_GET_STATUS_H());
            
            uint8_t bCfgLength;

            #if DP_INTERRUPT_MASK_8BIT == 0
               VPC3_POLL_CON_IND_NEW_CFG_DATA();
            #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */

            bResult = VPC3_CFG_FINISHED;

            do
            {
               bCfgLength = VPC3_GET_CFG_LEN();
               
               // --- VALIDACIÓN CRÍTICA: Verificar puntero CFG antes de leer ---
               VPC3_UNSIGNED8_PTR cfgBufPtr = VPC3_GET_CFG_BUF_PTR();
               VPC3_ADR cfgAddr = (VPC3_ADR)cfgBufPtr;
               
               printf("DEBUG: [dp_isr] CFG Buffer Ptr: 0x%08X, Addr: 0x%04X\r\n", (unsigned int)cfgBufPtr, cfgAddr);
               
               // Validar que la dirección está dentro del rango válido
               if (cfgAddr >= ASIC_RAM_LENGTH) {
                   printf("🚨 [dp_isr] ERROR: Puntero CFG corrupto! Addr=0x%04X >= ASIC_RAM_LENGTH=0x%04X\r\n", 
                          cfgAddr, ASIC_RAM_LENGTH);
                   printf("🚨 [dp_isr] Rechazando trama CFG corrupta para evitar LECTURA ILEGAL\r\n");
                   bResult = VPC3_SET_CFG_DATA_NOT_OK();
                   break; // Salir del bucle do-while
               }
               
               // Validar que la longitud es razonable
               if (bCfgLength > 50) {
                   printf("🚨 [dp_isr] ERROR: Longitud CFG sospechosa (%d bytes). Rechazando.\r\n", bCfgLength);
                   bResult = VPC3_SET_CFG_DATA_NOT_OK();
                   break; // Salir del bucle do-while
               }
               
               CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], cfgBufPtr, bCfgLength );

               printf("DEBUG: [dp_isr] Contenido de abPrmCfgSsaHelpBuffer JUSTO ANTES de DpCfg_ChkNewCfgData (%d bytes): ", bCfgLength);
               
                               // Protección contra tramas CFG corruptas
                if (bCfgLength > 10) {
                    printf("⚠️ [dp_isr] ADVERTENCIA: Trama CFG sospechosamente larga (%d bytes). Esperado: 2 bytes (1 OUT + 1 IN).\r\n", bCfgLength);
                    printf("⚠️ [dp_isr] Posible problema en configuración del PLC o GSD.\r\n");
                    if (bCfgLength > 50) {
                        printf("⚠️ [dp_isr] Limitando a primeros 50 bytes para evitar corrupción.\r\n");
                        bCfgLength = 50;
                    }
                }
               
               for(int k=0; k<bCfgLength && k<50; k++) {
                  printf("0x%02X ", pDpSystem->abPrmCfgSsaHelpBuffer[k]);
               }
               printf("\r\n");

               switch( DpCfg_ChkNewCfgData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) )
               {
                  case DP_CFG_OK:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgOk();
                     #endif /* #if DP_MSAC_C1 */

                     bResult = VPC3_SET_CFG_DATA_OK();
                     break;
                  } /* case DP_CFG_OK: */

                  case DP_CFG_FAULT:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgNotOk();
                     #endif /* #if DP_MSAC_C1 */

                     bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     break;
                  } /* case DP_CFG_FAULT: */

                  case DP_CFG_UPDATE:
                  {
                     /* Calculate the length of the input and output using the configuration bytes */
                     if( DP_OK != VPC3_CalculateInpOutpLength( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) )
                     {
                        #if DP_MSAC_C1
                           MSAC_C1_DoCfgNotOk();
                        #endif /* #if DP_MSAC_C1 */

                        bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     } /* if( DP_OK != VPC3_CalculateInpOutpLength( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) ) */
                     else
                     {
                        /* set IO-Length */
                        VPC3_SetIoDataLength();

                        #if DP_MSAC_C1
                           MSAC_C1_DoCfgOk();
                        #endif /* #if DP_MSAC_C1 */

                        VPC3_SET_READ_CFG_LEN( bCfgLength );
                        VPC3_UPDATE_CFG_BUFFER();

                        bResult = VPC3_SET_CFG_DATA_OK();
                     } /* else of if( DP_OK != VPC3_CalculateInpOutpLength( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) ) */
                     break;
                  } /* case DP_CFG_UPDATE: */

                  default:
                  {
                     break;
                  } /* default: */
               } /* switch( DpCfg_ChkNewCfgData( pCfgData, bCfgLength ) ) */
            }
            while( bResult == VPC3_CFG_CONFLICT );
         } /* if( VPC3_POLL_IND_NEW_CFG_DATA() ) */

         /*------------------------------------------------------------------*/
         /* IND_WD_DP_TIMEOUT                                                */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_WD_DP_MODE_TIMEOUT() )
         {
            DpAppl_IsrNewWdDpTimeout();

            VPC3_CON_IND_WD_DP_MODE_TIMEOUT();
         } /* if( VPC3_POLL_IND_WD_DP_MODE_TIMEOUT() ) */

         /*------------------------------------------------------------------*/
         /* IND_GO_LEAVE_DATA_EX                                             */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_GO_LEAVE_DATA_EX() )
         {
            printf("🚨 [dp_isr] === IND_GO_LEAVE_DATA_EX DETECTADO ===\r\n");
            printf("🚨 [dp_isr] TIMESTAMP: %lu ms\r\n", HAL_GetTick());
            printf("🚨 [dp_isr] STATUS_L antes del evento: 0x%02X\r\n", VPC3_GET_STATUS_L());
            printf("🚨 [dp_isr] STATUS_H antes del evento: 0x%02X\r\n", VPC3_GET_STATUS_H());
            printf("🚨 [dp_isr] DP_STATE antes del evento: 0x%02X\r\n", VPC3_GET_DP_STATE());
            printf("🚨 [dp_isr] MODE_REG_2 antes del evento: 0x%02X\r\n", VPC3_GetModeReg2Shadow());
            
            // Análisis del estado de comunicación
            uint8_t status_l = VPC3_GET_STATUS_L();
            uint8_t actual_dp_state = (status_l & 0x60) >> 5;
            printf("🚨 [dp_isr] Estado real calculado de STATUS_L: 0x%02X\r\n", actual_dp_state);
            
            if (actual_dp_state == DATA_EX) {
                printf("🚨 [dp_isr] ⚠️ Estado real es DATA_EX - posible evento falso\r\n");
            } else {
                printf("🚨 [dp_isr] ✅ Estado real NO es DATA_EX - evento válido\r\n");
            }
            
            printf("🚨 [dp_isr] IND_GO_LEAVE_DATA_EX detectado - llamando DpAppl_IsrGoLeaveDataExchange\r\n");
            printf("🚨 [dp_isr] VPC3_GET_DP_STATE() = 0x%02X\r\n", VPC3_GET_DP_STATE());
            
            #if DP_MSAC_C1
               MSAC_C1_LeaveDx();
            #endif /* #if DP_MSAC_C1 */

            DpAppl_IsrGoLeaveDataExchange( VPC3_GET_DP_STATE() );

            VPC3_CON_IND_GO_LEAVE_DATA_EX();
            printf("🚨 [dp_isr] STATUS_L después del evento: 0x%02X\r\n", VPC3_GET_STATUS_L());
            printf("🚨 [dp_isr] STATUS_H después del evento: 0x%02X\r\n", VPC3_GET_STATUS_H());
            printf("🚨 [dp_isr] DP_STATE después del evento: 0x%02X\r\n", VPC3_GET_DP_STATE());
            printf("🚨 [dp_isr] IND_GO_LEAVE_DATA_EX procesado\r\n");
            printf("🚨 [dp_isr] === FIN IND_GO_LEAVE_DATA_EX ===\r\n");
         } /* if( VPC3_POLL_IND_GO_LEAVE_DATA_EX() ) */

         /*------------------------------------------------------------------*/
         /* IND_DX_OUT                                                       */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_DX_OUT() )
         {
            // ESTE MENSAJE NO DEBERÍA APARECER NUNCA
            printf("--- ALERTA: [dp_isr] IND_DX_OUT detectado INESPERADAMENTE via polling. La condición de carrera puede persistir. ---\n");

            printf("DEBUG: [dp_isr] IND_DX_OUT detectado - procesando Data Exchange Output\n");
            printf("DEBUG: [dp_isr] STATUS antes de DpAppl_IsrDxOut: L=0x%02X, H=0x%02X\n", 
                   VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
            printf("DEBUG: [dp_isr] STATUS_L antes: 0x%02X (esperado DATA_EX=0x45)\n", VPC3_GET_STATUS_L());
            printf("DEBUG: [dp_isr] STATUS_H antes: 0x%02X (esperado 0xE3)\n", VPC3_GET_STATUS_H());
            
            #if DP_MSAC_C1
               MSAC_C1_CheckIndDxOut();
            #endif /* #if DP_MSAC_C1 */

            DpAppl_IsrDxOut();
            printf("DEBUG: [dp_isr] STATUS después de DpAppl_IsrDxOut: L=0x%02X, H=0x%02X\n", 
                   VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
            printf("DEBUG: [dp_isr] STATUS_L después de DpAppl_IsrDxOut: 0x%02X\n", VPC3_GET_STATUS_L());
            printf("DEBUG: [dp_isr] STATUS_H después de DpAppl_IsrDxOut: 0x%02X\n", VPC3_GET_STATUS_H());

            VPC3_CON_IND_DX_OUT();
            printf("DEBUG: [dp_isr] STATUS después de VPC3_CON_IND_DX_OUT: L=0x%02X, H=0x%02X\n", 
                   VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
            printf("DEBUG: [dp_isr] STATUS_L después de VPC3_CON_IND_DX_OUT: 0x%02X\n", VPC3_GET_STATUS_L());
            printf("DEBUG: [dp_isr] STATUS_H después de VPC3_CON_IND_DX_OUT: 0x%02X\n", VPC3_GET_STATUS_H());
            printf("DEBUG: [dp_isr] IND_DX_OUT procesado\n");
         } /* if( VPC3_POLL_IND_DX_OUT() ) */

         /*------------------------------------------------------------------*/
         /* IND_NEW_GC_COMMAND                                               */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_GC_COMMAND() )
         {
            DpAppl_IsrNewGlobalControlCommand( VPC3_GET_GC_COMMAND() );

            VPC3_CON_IND_NEW_GC_COMMAND();
         } /* if( VPC3_POLL_IND_NEW_GC_COMMAND() ) */

         /*------------------------------------------------------------------*/
         /* IND_NEW_SSA_DATA                                                 */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_SSA_DATA() )
         {
            CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], VPC3_GET_SSA_BUF_PTR(), 4 );
            DpAppl_IsrNewSetSlaveAddress( (MEM_STRUC_SSA_BLOCK_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0] );

            bResult = VPC3_FREE_SSA_BUF();

            VPC3_CON_IND_NEW_SSA_DATA();
         } /* if( VPC3_POLL_IND_NEW_SSA_DATA() ) */

         /*------------------------------------------------------------------*/
         /* IND_BAUDRATE_DETECT                                              */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_BAUDRATE_DETECT() )
         {
            #if DP_MSAC_C2
               MSAC_C2_SetTimeoutIsr();
            #endif /* #if DP_MSAC_C2 */

            DpAppl_IsrBaudrateDetect();

            VPC3_CON_IND_BAUDRATE_DETECT();
         } /* if( VPC3_POLL_IND_BAUDRATE_DETECT() ) */

         #if DP_SUBSCRIBER

            /*---------------------------------------------------------------*/
            /* IND_DXB_OUT                                                   */
            /*---------------------------------------------------------------*/
            if( VPC3_POLL_IND_DXB_OUT() )
            {
               DpAppl_IsrDxbOut();

               VPC3_CON_IND_DXB_OUT();
            } /* if( VPC3_POLL_IND_DXB_OUT() ) */

            /*---------------------------------------------------------------*/
            /* IND_DXB_LINK_ERROR                                            */
            /*---------------------------------------------------------------*/
            if( VPC3_POLL_IND_DXB_LINK_ERROR() )
            {
               DpAppl_IsrDxbLinkError();

               VPC3_CON_IND_DXB_LINK_ERROR();
            } /* if( VPC3_POLL_IND_DXB_LINK_ERROR() ) */

         #endif /* #if DP_SUBSCRIBER */

         #if DP_FDL

            /*---------------------------------------------------------------*/
            /* IND_POLL_END                                                  */
            /*---------------------------------------------------------------*/
            if( VPC3_POLL_IND_POLL_END_IND() )
            {
               VPC3_CON_IND_POLL_END_IND();

               FDL_PollEndIsr();
            } /* if( VPC3_POLL_IND_POLL_END_IND() ) */

            /*---------------------------------------------------------------*/
            /* IND_REQ_PDU                                                   */
            /*---------------------------------------------------------------*/
            if( VPC3_POLL_IND_FDL_IND() )
            {
               VPC3_CON_IND_FDL_IND();
               FDL_IndicationIsr();
            } /* if( VPC3_POLL_IND_FDL_IND() ) */

         #endif /* #if DP_FDL */

         #if DP_INTERRUPT_MASK_8BIT == 0

            #if VPC3_SERIAL_MODE

               Vpc3Write( bVpc3WoIntAck_L, (uint8_t)(pDpSystem->wPollInterruptEvent & 0xFF) );
               Vpc3Write( bVpc3WoIntAck_H, (uint8_t)(pDpSystem->wPollInterruptEvent >> 8) );

            #else

               #if BIG_ENDIAN
                  Swap16( &pDpSystem->wPollInterruptEvent );
               #endif /* #if BIG_ENDIAN */

               CopyToVpc3_( ((uint8_t *)(VPC3_ADR)( Vpc3AsicAddress + 0x02 )), (MEM_UNSIGNED8_PTR)&pDpSystem->wPollInterruptEvent, 2 );

            #endif /* #if VPC3_SERIAL_MODE */

            pDpSystem->wPollInterruptEvent = 0;

         #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */

   #if DP_INTERRUPT_MASK_8BIT == 0

      } /* if( pDpSystem->wPollInterruptEvent > 0 ) */

   #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */
   return pDpSystem->wPollInterruptEvent;
} /* uint16_t VPC3_Poll( void ) */

#endif /* #if VPC3_SERIAL_MODE */


/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

/**
 * @brief  Función que el EXTI-Callback invoca.
 *         En modo polling repite VPC3_Poll, en modo interrupt repite VPC3_Isr.
 */
void dp_isr(void)
{
   // --- LOGGING COMPLETO DE ACTIVACIÓN ---
   static uint32_t dp_isr_call_count = 0;
   dp_isr_call_count++;
   
   printf("🔍 [dp_isr] === LLAMADA #%lu ===\r\n", dp_isr_call_count);
   printf("🔍 [dp_isr] TIMESTAMP: %lu ms\r\n", HAL_GetTick());
   printf("🔍 [dp_isr] STACK TRACE - Llamada desde:\r\n");
   printf("🔍 [dp_isr] - Función: dp_isr\r\n");
   printf("🔍 [dp_isr] - Archivo: ../Core/Src/dp_isr.c\r\n");
   printf("🔍 [dp_isr] - Línea: %d\r\n", __LINE__);
   
   // Verificar si es llamada por polling o interrupción
   #if (VPC3_SERIAL_MODE == 0)
      printf("🔍 [dp_isr] MODO: INTERRUPCIÓN (VPC3_Isr)\r\n");
   #else
      printf("🔍 [dp_isr] MODO: POLLING (VPC3_Poll)\r\n");
   #endif
   
   printf("🔍 [dp_isr] INICIO - STATUS_L=0x%02X, STATUS_H=0x%02X, DP_STATE=0x%02X\r\n",
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H(), VPC3_GET_DP_STATE());
   printf("🔍 [dp_isr] INICIO - Verificando si STATUS_L=0x45 (DATA_EX) y STATUS_H=0xE3\r\n");
   
   // --- CRITICAL: Check for MODE_REG_2 corruption during interrupt ---
  uint8_t mode_reg2 = VPC3_GetModeReg2Shadow();
   printf("🔍 [dp_isr] MODE_REG_2 actual: 0x%02X (esperado: 0x05)\r\n", mode_reg2);
   
   if (mode_reg2 != 0x05) {
      printf("⚠️ [dp_isr] MODE_REG_2 corruption detected during interrupt: 0x%02X (expected 0x05)\r\n", mode_reg2);
      // Try to recover immediately
      if (VPC3_ForceModeReg2() == 0) {
         printf("✅ [dp_isr] MODE_REG_2 recovered during interrupt\r\n");
      } else {
         printf("❌ [dp_isr] Failed to recover MODE_REG_2 during interrupt\r\n");
      }
   }
   
   // Detectar corrupción de registros
   if (VPC3_GET_STATUS_L() == 0x26 && VPC3_GET_STATUS_H() == 0xB7) {
      printf("🔍 [dp_isr] ⚠️ CORRUPCIÓN DETECTADA AL INICIO - STATUS_L=0x26, STATUS_H=0xB7\r\n");
   }
   
   // Detectar transición de DATA_EX a corrupción
   static uint8_t last_status_l = 0xFF;
   static uint8_t last_status_h = 0xFF;
   uint8_t current_status_l = VPC3_GET_STATUS_L();
   uint8_t current_status_h = VPC3_GET_STATUS_H();
   
   if (last_status_l == 0x45 && last_status_h == 0xE3 && 
       (current_status_l != 0x45 || current_status_h != 0xE3)) {
      printf("🔍 [dp_isr] ⚠️ TRANSICIÓN DETECTADA: STATUS_L=0x45->0x%02X, STATUS_H=0xE3->0x%02X\r\n", 
             current_status_l, current_status_h);
   }
   
   last_status_l = current_status_l;
   last_status_h = current_status_h;
   
   printf("🔍 [dp_isr] ANTES de VPC3_Poll/VPC3_Isr - STATUS_L=0x%02X, STATUS_H=0x%02X\r\n",
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
   
#if (VPC3_SERIAL_MODE == 0)
    VPC3_Isr();
#else
    VPC3_Poll();
#endif

   printf("🔍 [dp_isr] DESPUÉS de VPC3_Poll/VPC3_Isr - STATUS_L=0x%02X, STATUS_H=0x%02X\r\n",
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
   printf("🔍 [dp_isr] FIN - STATUS_L=0x%02X, STATUS_H=0x%02X, DP_STATE=0x%02X\r\n",
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H(), VPC3_GET_DP_STATE());
   printf("🔍 [dp_isr] FIN - Verificando si STATUS_L=0x45 (DATA_EX) y STATUS_H=0xE3\r\n");
   printf("🔍 [dp_isr] === FIN LLAMADA #%lu ===\r\n", dp_isr_call_count);
}

