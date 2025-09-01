/***********************  Filename: dp_isr.c  ********************************/
/* ========================================================================= */
/*                                                                           */
/* 0000  000   000  00000 0  000  0   0 0 0000                               */
/* 0   0 0  0 0   0 0     0 0   0 0   0 0 0   0                              */
/* 0   0 0  0 0   0 0     0 0     0   0 0 0   0      Einsteinstrae 6        */
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
#include <stdio.h>
#include "platform.h"
#include "main.h"  /* Para HAL_GPIO_WritePin y definiciones GPIO */
#include "dp_inc.h"  /* Para definiciones de estructuras PROFIBUS */
#include "dp_if.h"   /* Para definiciones de estructuras SSA */
#include "DpCfg.h"   /* Para ASIC_RAM_LENGTH y otras constantes del VPC3+ */
#include "DpAppl.h"  /* Para sDpAppl */

// Declaraciones de funciones estáticas (antes de su uso)
static uint8_t dp_isr_validate_and_process_cfg(uint8_t bCfgLength, uint8_t* pbCfgData);
static uint8_t dp_isr_process_extended_cfg(uint8_t bCfgLength, uint8_t* pbCfgData);

/*---------------------------------------------------------------------------*/
/* validation constants                                                      */
/*---------------------------------------------------------------------------*/
#define MAX_SAFE_CFG_LENGTH     ((uint8_t)50)      // Maximum safe CFG length before rejecting
#define MAX_SAFE_PRM_LENGTH     ((uint8_t)50)      // Maximum safe PRM length before rejecting
#define EXPECTED_CFG_LENGTH     ((uint8_t)2)       // Expected CFG length for this application
#define MAX_DEBUG_PRINT_BYTES   ((uint8_t)50)      // Maximum bytes to print in debug messages

// Ring buffer para logging del ISR (sin printf)
#define ISR_LOG_BUFFER_SIZE     128  // Reducido para evitar overflow en uint8_t
#define ISR_LOG_ENTRY_SIZE      64

// Constantes para el buffer de diagnóstico del VPC3+
#define DIAG_BUFFER_AVAILABLE   0x00  // Buffer de diagnóstico disponible según manual VPC3+

typedef struct {
    uint8_t buffer[ISR_LOG_BUFFER_SIZE][ISR_LOG_ENTRY_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t overflow;
} isr_log_buffer_t;

static isr_log_buffer_t isr_log_buffer = {0};

// Función para agregar entrada al log del ISR (sin printf)
static inline void isr_log_add(const char* prefix, const char* message, uint32_t value) {
    // Usar bitwise AND en lugar de módulo (%) para seguridad en interrupciones (re-entrancy).
    // Esto solo funciona porque ISR_LOG_BUFFER_SIZE es una potencia de 2 (128).
    uint8_t next_head = (isr_log_buffer.head + 1) & (ISR_LOG_BUFFER_SIZE - 1);

    if (next_head != isr_log_buffer.tail) {
        uint8_t* entry = isr_log_buffer.buffer[isr_log_buffer.head];
        uint8_t len = 0;
        
        // Copiar prefijo
        while (*prefix && len < ISR_LOG_ENTRY_SIZE - 1) {
            entry[len++] = *prefix++;
        }
        
        // Copiar mensaje
        while (*message && len < ISR_LOG_ENTRY_SIZE - 1) {
            entry[len++] = *message++;
        }
        
        // Agregar valor si es necesario
        if (value != 0xFFFFFFFF) {
            entry[len++] = ' ';
            entry[len++] = '0';
            entry[len++] = 'x';
            
            // Convertir valor a hex (simplificado)
            uint8_t nibble;
            for (int i = 7; i >= 0; i--) {
                nibble = (value >> (i * 4)) & 0x0F;
                if (nibble != 0 || i == 0) {
                    entry[len++] = (nibble < 10) ? '0' + nibble : 'A' + (nibble - 10);
                }
            }
        }
        
        entry[len] = '\0';
        isr_log_buffer.head = next_head;
    } else {
        isr_log_buffer.overflow = 1;
    }
}

/*---------------------------------------------------------------------------*/
/* Funciones para acceder al buffer de log desde el main loop               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Obtiene el número de entradas disponibles en el buffer de log
 * @return Número de entradas disponibles para leer
 */
uint8_t isr_log_get_available_entries(void) {
    if (isr_log_buffer.head >= isr_log_buffer.tail) {
        return isr_log_buffer.head - isr_log_buffer.tail;
    } else {
        return ISR_LOG_BUFFER_SIZE - isr_log_buffer.tail + isr_log_buffer.head;
    }
}

/**
 * @brief Lee la siguiente entrada del buffer de log
 * @param buffer Buffer de destino para la entrada
 * @param buffer_size Tamaño del buffer de destino
 * @return 1 si se leyó una entrada, 0 si no hay entradas disponibles
 */
uint8_t isr_log_read_entry(char* buffer, uint8_t buffer_size) {
    if (isr_log_buffer.tail == isr_log_buffer.head) {
        return 0; // No hay entradas disponibles
    }
    
    uint8_t* entry = isr_log_buffer.buffer[isr_log_buffer.tail];
    uint8_t len = 0;
    
    // Copiar entrada al buffer de destino
    while (*entry && len < buffer_size - 1) {
        buffer[len++] = *entry++;
    }
    buffer[len] = '\0';
    
    // Avanzar el tail
    isr_log_buffer.tail = (isr_log_buffer.tail + 1) % ISR_LOG_BUFFER_SIZE;
    
    return 1;
}

/**
 * @brief Verifica si hay overflow en el buffer de log
 * @return 1 si hubo overflow, 0 en caso contrario
 */
uint8_t isr_log_has_overflow(void) {
    return isr_log_buffer.overflow;
}

/**
 * @brief Limpia el flag de overflow del buffer de log
 */
void isr_log_clear_overflow(void) {
    isr_log_buffer.overflow = 0;
}

/**
 * @brief Obtiene estadísticas del buffer de log
 * @param total_entries Puntero para almacenar el total de entradas
 * @param available_entries Puntero para almacenar entradas disponibles
 * @param overflow Puntero para almacenar estado de overflow
 */
void isr_log_get_stats(uint8_t* total_entries, uint8_t* available_entries, uint8_t* overflow) {
    if (total_entries) *total_entries = ISR_LOG_BUFFER_SIZE;
    if (available_entries) *available_entries = isr_log_get_available_entries();
    if (overflow) *overflow = isr_log_buffer.overflow;
}

// Macros para logging del ISR
#define ISR_LOG(prefix, msg)           isr_log_add(prefix, msg, 0xFFFFFFFF)
#define ISR_LOG_VAL(prefix, msg, val)  isr_log_add(prefix, msg, val)

#if VPC3_SERIAL_MODE
   #define MakeWord( Hi, Lo ) ( (uint16_t)( ( ( ( (uint8_t)( Hi ) ) << 8 ) | ( (uint8_t)( Lo ) ) ) ) )
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
      
      // Log para verificar la máscara de software (usando ring buffer)
      uint16_t events = pDpSystem->wPollInterruptEvent;
      ISR_LOG_VAL("[VPC3_Poll]", "Eventos hardware leidos", events);
      ISR_LOG_VAL("[VPC3_Poll]", "Mascara de software aplicada", pDpSystem->wPollInterruptMask);
      
      // Decodificar eventos específicos para análisis (sin printf)
      if (events & 0x0001) ISR_LOG("[VPC3_Poll]", "MAC_RESET/CLOCK_SYNC (0x0001)");
      if (events & 0x0002) ISR_LOG("[VPC3_Poll]", "GO_LEAVE_DATA_EX (0x0002)");
      if (events & 0x0004) ISR_LOG("[VPC3_Poll]", "BAUDRATE_DETECT (0x0004)");
      if (events & 0x0008) ISR_LOG("[VPC3_Poll]", "WD_DP_MODE_TIMEOUT (0x0008)");
      if (events & 0x0010) ISR_LOG("[VPC3_Poll]", "USER_TIMER_CLOCK (0x0010)");
      if (events & 0x0020) ISR_LOG("[VPC3_Poll]", "DXB_LINK_ERROR (0x0020)");
      if (events & 0x0040) ISR_LOG("[VPC3_Poll]", "NEW_EXT_PRM_DATA (0x0040)");
      if (events & 0x0080) ISR_LOG("[VPC3_Poll]", "DXB_OUT (0x0080)");
      if (events & 0x0100) ISR_LOG("[VPC3_Poll]", "NEW_GC_COMMAND (0x0100)");
      if (events & 0x0200) ISR_LOG("[VPC3_Poll]", "NEW_SSA_DATA (0x0200)");
      if (events & 0x0400) ISR_LOG("[VPC3_Poll]", "NEW_CFG_DATA (0x0400)");
      if (events & 0x0800) ISR_LOG("[VPC3_Poll]", "NEW_PRM_DATA (0x0800)");
      if (events & 0x1000) ISR_LOG("[VPC3_Poll]", "DIAG_BUFFER_CHANGED (0x1000)");
      if (events & 0x2000) ISR_LOG("[VPC3_Poll]", "DX_OUT (0x2000)");
      if (events & 0x4000) ISR_LOG("[VPC3_Poll]", "POLL_END_IND (0x4000)");
      if (events & 0x8000) ISR_LOG("[VPC3_Poll]", "FDL_IND (0x8000)");
      
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
            ISR_LOG("[dp_isr]", "=== IND_DIAG_BUFFER_CHANGED DETECTADO ===");
            ISR_LOG("[dp_isr]", "=== ETAPA 0: Solicitud de Diagnostico (Slave_Diag) recibida del maestro ===");
            ISR_LOG_VAL("[dp_isr]", "TIMESTAMP", HAL_GetTick());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L antes del evento", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H antes del evento", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "DP_STATE antes del evento", VPC3_GET_DP_STATE());
            ISR_LOG_VAL("[dp_isr]", "MODE_REG_2 antes del evento", VPC3_GetModeReg2Shadow());
            ISR_LOG_VAL("[dp_isr]", "ESTADO ACTUAL", VPC3_GET_DP_STATE());
            
            // Análisis del buffer de diagnóstico
            uint8_t diag_buffer_sm = Vpc3Read(0x0E); // Diag buffer state machine
            ISR_LOG_VAL("[dp_isr]", "Diag Buffer State Machine", diag_buffer_sm);
            
            if (diag_buffer_sm == DIAG_BUFFER_AVAILABLE) {
                ISR_LOG("[dp_isr]", "Buffer de diagnóstico disponible");
                
                ISR_LOG("[dp_isr]", "Llamando DpDiag_IsrDiagBufferChanged...");
                DpDiag_IsrDiagBufferChanged();
                
                // RESTAURAR: Validación crítica de punteros (si la función existe)
                #ifdef VPC3_ValidateSegmentPointers
                ISR_LOG("[dp_isr]", "Verificando integridad de punteros después del evento...");
                if (VPC3_ValidateSegmentPointers() != DP_OK) {
                    ISR_LOG("[dp_isr]", "PUNTEROS CORRUPTOS DETECTADOS! Intentando recuperación...");
                } else {
                    ISR_LOG("[dp_isr]", "Punteros de segmentos validados correctamente");
                }
                #else
                ISR_LOG("[dp_isr]", "Verificación de punteros de segmentos (función no disponible)");
                #endif
                
                ISR_LOG_VAL("[dp_isr]", "STATUS_L después del evento", VPC3_GET_STATUS_L());
                ISR_LOG_VAL("[dp_isr]", "STATUS_H después del evento", VPC3_GET_STATUS_H());
                ISR_LOG_VAL("[dp_isr]", "DP_STATE después del evento", VPC3_GET_DP_STATE());
                ISR_LOG("[dp_isr]", "=== FIN IND_DIAG_BUFFER_CHANGED ===");
            }
            
            // CRÍTICO: Restaurar confirmación de evento
            VPC3_POLL_CON_IND_DIAG_BUFFER_CHANGED();
         } /* if( VPC3_POLL_IND_DIAG_BUFFER_CHANGED() ) */
         
         /*------------------------------------------------------------------*/
         /* IND_NEW_PRM_DATA - RESTAURAR BUCLE DO-WHILE                     */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_PRM_DATA() )
         {
            ISR_LOG("[dp_isr]", "=== ETAPA 1: Trama de Parametrizacion (Set_Param) recibida ===");
            ISR_LOG_VAL("[dp_isr]", "TIMESTAMP", HAL_GetTick());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L antes de procesar PRM", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H antes de procesar PRM", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "ESTADO ACTUAL", VPC3_GET_DP_STATE());
            ISR_LOG("[dp_isr]", "(Deberia ser WAIT_PRM)");
            
            uint8_t bPrmLength;
            #if DP_INTERRUPT_MASK_8BIT == 0
               VPC3_POLL_CON_IND_NEW_PRM_DATA();
            #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */
            
            // RESTAURAR: Bucle do-while para manejo de conflictos
            bResult = VPC3_PRM_FINISHED;
            do
            {
                uint8_t* prmBufPtr;
                VPC3_ADR prmAddr;
                
                bPrmLength = VPC3_GET_PRM_LEN();
                ISR_LOG_VAL("[dp_isr]", "PRM Length", bPrmLength);
                
                // Validación de seguridad ANTES de copiar los datos
                prmBufPtr = VPC3_GET_PRM_BUF_PTR();
                prmAddr = (VPC3_ADR)(uint32_t)prmBufPtr;
                
                ISR_LOG_VAL("[dp_isr]", "PRM Buffer Ptr", (unsigned int)prmBufPtr);
                ISR_LOG_VAL("[dp_isr]", "Addr", prmAddr);
                
                // Validar que la dirección está dentro del rango válido
                if (prmAddr >= ASIC_RAM_LENGTH || ((uint32_t)prmAddr + bPrmLength) > ASIC_RAM_LENGTH || bPrmLength > HELP_BUFSIZE) {
                    ISR_LOG("[dp_isr]", "ERROR: Puntero o longitud PRM corrupta!");
                    ISR_LOG_VAL("[dp_isr]", "Addr", prmAddr);
                    ISR_LOG_VAL("[dp_isr]", "Length", bPrmLength);
                    ISR_LOG("[dp_isr]", "Rechazando trama PRM para evitar LECTURA ILEGAL");
                    bResult = VPC3_SET_PRM_DATA_NOT_OK();
                    break; // Salir del bucle do-while
                }
                
                // Copia segura de los datos a un buffer local
                ISR_LOG("[dp_isr]", "Validaciones PRM exitosas - procediendo con copia segura");
                CopyFromVpc3_((MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], prmBufPtr, bPrmLength);
                
                // Llamar a la función de validación lógica
                ISR_LOG_VAL("[dp_isr]", "Validando parametros", bPrmLength);
                if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK )
                {
                   #if REDUNDANCY
                      #if DP_MSAC_C1
                         if( VPC3_GET_PRM_LEN() != PRM_CMD_LENGTH )
                         {
                            MSAC_C1_CheckIndNewPrmData( (MEM_STRUC_PRM_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength );
                         }
                      #endif /* #if DP_MSAC_C1 */
                   #else
                      #if DP_MSAC_C1
                         MSAC_C1_CheckIndNewPrmData( (MEM_STRUC_PRM_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength );
                      #endif /* #if DP_MSAC_C1 */
                   #endif /* #if REDUNDANCY */
                   bResult = VPC3_SET_PRM_DATA_OK();
                   ISR_LOG("[dp_isr]", "-> RESULTADO: Parametros ACEPTADOS");
                }
                else
                {
                   bResult = VPC3_SET_PRM_DATA_NOT_OK();
                   ISR_LOG("[dp_isr]", "-> RESULTADO: Parametros RECHAZADOS");
                }
            }
            while( bResult == VPC3_PRM_CONFLICT ); // RESTAURADO: Bucle de reintentos
         } /* if( VPC3_POLL_IND_NEW_PRM_DATA() ) */
         
         /*------------------------------------------------------------------*/
         /* check config data , application specific!                        */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_NEW_CFG_DATA() )
         {
            ISR_LOG("[dp_isr]", "=== ETAPA 2: Trama de Configuracion (Check_Cfg) recibida ===");
            ISR_LOG_VAL("[dp_isr]", "TIMESTAMP", HAL_GetTick());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L antes de procesar CFG", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H antes de procesar CFG", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "ESTADO ACTUAL", VPC3_GET_DP_STATE());
            ISR_LOG("[dp_isr]", "(Deberia ser WAIT_CFG)");
            
            uint8_t bCfgLength;
            #if DP_INTERRUPT_MASK_8BIT == 0
               VPC3_POLL_CON_IND_NEW_CFG_DATA();
            #endif /* #if DP_INTERRUPT_MASK_8BIT == 0 */
            
            bResult = VPC3_CFG_FINISHED;
            do
            {
               uint8_t* cfgBufPtr;
               VPC3_ADR cfgAddr;
               
               bCfgLength = VPC3_GET_CFG_LEN();
               ISR_LOG_VAL("[dp_isr]", "CFG Length", bCfgLength);
               
               // Validación crítica: Verificar puntero CFG antes de leer
               cfgBufPtr = VPC3_GET_CFG_BUF_PTR();
               cfgAddr = (VPC3_ADR)(uint32_t)cfgBufPtr;
               
               ISR_LOG_VAL("[dp_isr]", "CFG Buffer Ptr", (unsigned int)cfgBufPtr);
               ISR_LOG_VAL("[dp_isr]", "Addr", cfgAddr);
               
               // Validaciones de seguridad
               if (cfgAddr >= ASIC_RAM_LENGTH || (cfgAddr + bCfgLength) > ASIC_RAM_LENGTH || bCfgLength > HELP_BUFSIZE) {
                   ISR_LOG("[dp_isr]", "ERROR: Puntero CFG corrupto o longitud inválida!");
                   ISR_LOG_VAL("[dp_isr]", "Addr", cfgAddr);
                   ISR_LOG_VAL("[dp_isr]", "Length", bCfgLength);
                   ISR_LOG("[dp_isr]", "Rechazando trama CFG corrupta");
                   bResult = VPC3_SET_CFG_DATA_NOT_OK();
                   break;
               }
               
               ISR_LOG("[dp_isr]", "Validaciones CFG exitosas - procediendo con copia segura");
               CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], cfgBufPtr, bCfgLength );
               
               ISR_LOG_VAL("[dp_isr]", "Validando configuracion", bCfgLength);
               switch( DpCfg_ChkNewCfgData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) )
               {
                  case DP_CFG_OK:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgOk();
                     #endif /* #if DP_MSAC_C1 */
                     bResult = VPC3_SET_CFG_DATA_OK();
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion ACEPTADA (sin cambios)");
                     break;
                  }
                  
                  case DP_CFG_FAULT:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgNotOk();
                     #endif /* #if DP_MSAC_C1 */
                     bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion RECHAZADA");
                     break;
                  }
                  
                  case DP_CFG_UPDATE:
                  {
                     /* Calculate the length of the input and output using the configuration bytes */
                     if( DP_OK != VPC3_CalculateInpOutpLength( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) )
                     {
                        #if DP_MSAC_C1
                           MSAC_C1_DoCfgNotOk();
                        #endif /* #if DP_MSAC_C1 */
                        bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     }
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
                        ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion ACEPTADA (con actualizacion de I/O)");
                     }
                     break;
                  }
                  
                  default:
                  {
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion con estado desconocido");
                     bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     break;
                  }
               }
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
         /* IND_USER_TIMER_CLOCK                                             */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_USER_TIMER_CLOCK() )
         {
            VPC3_CON_IND_USER_TIMER_CLOCK();
         } /* if( VPC3_POLL_IND_USER_TIMER_CLOCK() ) */
         
         /*------------------------------------------------------------------*/
         /* IND_GO_LEAVE_DATA_EX                                             */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_GO_LEAVE_DATA_EX() )
         {
            ISR_LOG("[dp_isr]", "=== IND_GO_LEAVE_DATA_EX DETECTADO ===");
            uint8_t state_before = VPC3_GET_DP_STATE();
            ISR_LOG("[dp_isr]", "=== ETAPA 3: Orden de Cambio de Estado recibida ===");
            ISR_LOG_VAL("[dp_isr]", "TIMESTAMP", HAL_GetTick());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L antes del evento", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H antes del evento", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "DP_STATE antes del evento", VPC3_GET_DP_STATE());
            ISR_LOG_VAL("[dp_isr]", "MODE_REG_2 antes del evento", VPC3_GetModeReg2Shadow());
            ISR_LOG_VAL("[dp_isr]", "ESTADO ANTES del cambio", state_before);
            
            // Análisis del estado de comunicación
            uint8_t status_l = VPC3_GET_STATUS_L();
            uint8_t actual_dp_state = (status_l & 0x60) >> 5;
            ISR_LOG_VAL("[dp_isr]", "Estado real calculado de STATUS_L", actual_dp_state);
            
            if (actual_dp_state == DATA_EX) {
                ISR_LOG("[dp_isr]", "  Estado real es DATA_EX - posible evento falso");
            } else {
                ISR_LOG("[dp_isr]", "  Estado real NO es DATA_EX - evento válido");
            }
            
            ISR_LOG("[dp_isr]", "IND_GO_LEAVE_DATA_EX detectado - llamando DpAppl_IsrGoLeaveDataExchange");
            ISR_LOG_VAL("[dp_isr]", "VPC3_GET_DP_STATE()", VPC3_GET_DP_STATE());
            
            #if DP_MSAC_C1
               MSAC_C1_LeaveDx();
            #endif /* #if DP_MSAC_C1 */
            DpAppl_IsrGoLeaveDataExchange( VPC3_GET_DP_STATE() );
            uint8_t state_after = VPC3_GET_DP_STATE();
            ISR_LOG_VAL("[dp_isr]", "ESTADO DESPUES del cambio", state_after);
            if (state_before != DATA_EX && state_after == DATA_EX)
            {
                ISR_LOG("[dp_isr]", "-> RESULTADO: Transicion a DataExchange EXITOSA");
            }
            else if (state_before == DATA_EX && state_after != DATA_EX)
            {
                 ISR_LOG("[dp_isr]", "-> RESULTADO: Se ha salido de DataExchange");
            }
            VPC3_CON_IND_GO_LEAVE_DATA_EX();
            ISR_LOG_VAL("[dp_isr]", "STATUS_L después del evento", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H después del evento", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "DP_STATE después del evento", VPC3_GET_DP_STATE());
            ISR_LOG("[dp_isr]", "IND_GO_LEAVE_DATA_EX procesado");
            ISR_LOG("[dp_isr]", "=== FIN IND_GO_LEAVE_DATA_EX ===");
            ISR_LOG("[dp_isr]", "=== FIN ETAPA 3 ===");
         } /* if( VPC3_POLL_IND_GO_LEAVE_DATA_EX() ) */
         
         /*------------------------------------------------------------------*/
         /* IND_DX_OUT                                                       */
         /*------------------------------------------------------------------*/
         if( VPC3_POLL_IND_DX_OUT() )
         {
            ISR_LOG("[dp_isr]", "--- ALERTA: IND_DX_OUT detectado INESPERADAMENTE via polling ---");
            ISR_LOG("[dp_isr]", "DEBUG: IND_DX_OUT detectado - procesando Data Exchange Output");
            ISR_LOG_VAL("[dp_isr]", "STATUS antes de DpAppl_IsrDxOut L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS antes de DpAppl_IsrDxOut H", VPC3_GET_STATUS_H());
            
            #if DP_MSAC_C1
               MSAC_C1_CheckIndDxOut();
            #endif /* #if DP_MSAC_C1 */
            DpAppl_IsrDxOut();
            ISR_LOG_VAL("[dp_isr]", "STATUS después de DpAppl_IsrDxOut L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS después de DpAppl_IsrDxOut H", VPC3_GET_STATUS_H());
            VPC3_CON_IND_DX_OUT();
            ISR_LOG_VAL("[dp_isr]", "STATUS después de VPC3_CON_IND_DX_OUT L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS después de VPC3_CON_IND_DX_OUT H", VPC3_GET_STATUS_H());
            ISR_LOG("[dp_isr]", "DEBUG: IND_DX_OUT procesado");
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



// Función para validar y procesar configuraciones de diferentes longitudes
// NOTA: Con la implementación actual (Opción A), esta función solo se usa para diagnóstico
// ya que DpCfg_ChkNewCfgData() retorna DP_CFG_OK para configuraciones extendidas válidas
static uint8_t dp_isr_validate_and_process_cfg(uint8_t bCfgLength, uint8_t* pbCfgData) {
    // Validación básica de longitud
    if (bCfgLength == 0 || bCfgLength > MAX_SAFE_CFG_LENGTH) {
        ISR_LOG("[CFG_VALIDATE]", "Longitud CFG inválida");
        return DP_CFG_FAULT;
    }
    
    // Si es exactamente 2 bytes, verificar si es la configuración esperada
    if (bCfgLength == EXPECTED_CFG_LENGTH) {
        if (pbCfgData[0] == 0x20 && pbCfgData[1] == 0x10) {
            ISR_LOG("[CFG_VALIDATE]", "CFG estándar 2 bytes aceptada");
            return DP_CFG_OK;
        } else {
            ISR_LOG("[CFG_VALIDATE]", "CFG 2 bytes con contenido inesperado");
            return DP_CFG_FAULT;
        }
    }
    
    // Si es más larga, verificar si comienza con 0x20 0x10
    if (bCfgLength > EXPECTED_CFG_LENGTH) {
        if (pbCfgData[0] == 0x20 && pbCfgData[1] == 0x10) {
            ISR_LOG("[CFG_VALIDATE]", "CFG extendida con header válido - DP_CFG_UPDATE");
            return DP_CFG_UPDATE;  // Solo para diagnóstico (la decisión final está en DpCfg_ChkNewCfgData)
        } else {
            ISR_LOG("[CFG_VALIDATE]", "CFG larga sin header válido - rechazada");
            return DP_CFG_FAULT;
        }
    }
    
    return DP_CFG_FAULT;
}

// Función para procesar configuración extendida (SD2 con PDU de 1...246 bytes)
static uint8_t dp_isr_process_extended_cfg(uint8_t bCfgLength, uint8_t* pbCfgData) {
    // Los primeros 2 bytes ya fueron validados (0x20 0x10)
    (void)pbCfgData; // Evitar warning de parámetro no usado
    uint8_t extended_length = bCfgLength - 2;
    
    ISR_LOG_VAL("[CFG_EXTENDED]", "Procesando configuración extendida", extended_length);
    
    // Aquí podrías implementar el parsing del formato SD2
    // Por ahora, solo logueamos y aceptamos
    if (extended_length > 0) {
        ISR_LOG("[CFG_EXTENDED]", "Configuración extendida procesada exitosamente");
        return DP_CFG_OK;
    }
    
    return DP_CFG_OK;
}

// Función para leer el log del ISR desde el main loop (no ISR)
uint8_t dp_isr_read_log(char* buffer, uint8_t max_length) {
    uint8_t bytes_read = 0;
    
    while (isr_log_buffer.tail != isr_log_buffer.head && bytes_read < max_length - 1) {
        uint8_t* entry = isr_log_buffer.buffer[isr_log_buffer.tail];
        uint8_t entry_len = strlen((char*)entry);
        
        if (bytes_read + entry_len + 1 < max_length) {
            strcpy(&buffer[bytes_read], (char*)entry);
            bytes_read += entry_len;
            buffer[bytes_read++] = '\n';
        } else {
            break;
        }
        
        isr_log_buffer.tail = (isr_log_buffer.tail + 1) % ISR_LOG_BUFFER_SIZE;
    }
    
    buffer[bytes_read] = '\0';
    return bytes_read;
}

// Función para limpiar el log del ISR
void dp_isr_clear_log(void) {
    isr_log_buffer.head = 0;
    isr_log_buffer.tail = 0;
    isr_log_buffer.overflow = 0;
}

// Función para verificar si hay overflow en el log
uint8_t dp_isr_log_overflow(void) {
    return isr_log_buffer.overflow;
}

// *** FUNCIONES DE LOGGING ESPECÍFICAS PARA DpCfg (ISR-safe) ***
// Wrappers simples que llaman directamente a isr_log_add
void dp_cfg_log_message(const char* prefix, const char* message) {
    isr_log_add(prefix, message, 0xFFFFFFFF);
}

void dp_cfg_log_value(const char* prefix, const char* message, uint32_t value) {
    isr_log_add(prefix, message, value);
}

void dp_cfg_log_hex_dump(const char* prefix, const char* message, const uint8_t* data, uint8_t length) {
    // Crear mensaje con prefijo y longitud
    char log_entry[ISR_LOG_ENTRY_SIZE];
    int offset = snprintf(log_entry, sizeof(log_entry), "[%s] %s (%d bytes): ", prefix, message, length);
    
    // Agregar bytes hex en grupos de 4 para evitar truncamiento
    for (uint8_t i = 0; i < length && offset < (int)sizeof(log_entry) - 8; i++) {
        offset += snprintf(log_entry + offset, sizeof(log_entry) - offset, "0x%02X ", data[i]);
    }
    
    // Usar isr_log_add con el mensaje completo
    isr_log_add(log_entry, "", 0xFFFFFFFF);
}

/*---------------------------------------------------------------------------*/
/* Función para leer el estado real del ASIC sin máscaras                    */
/*---------------------------------------------------------------------------*/
static uint8_t dp_isr_get_real_state(void)
{
    uint8_t status_l = VPC3_GET_STATUS_L();
    uint8_t real_state;
    
    // Mapeo directo según manual VPC3+ (sin máscaras)
    switch (status_l) {
        case 0x00:
            real_state = 0x00; // OFFLINE
            break;
        case 0x91:
            real_state = 0x01; // PASSIVE_IDLE
            break;
        case 0x04:
            real_state = 0x02; // WAIT_PRM
            break;
        case 0x08:
            real_state = 0x03; // WAIT_CFG
            break;
        case 0x0C:
            real_state = 0x04; // WAIT_CFG
            break;
        case 0x10:
            real_state = 0x05; // DATA_EX
            break;
        default:
            real_state = 0xFF; // DESCONOCIDO
            break;
    }
    
    ISR_LOG_VAL("[dp_isr]", "Estado real del ASIC (STATUS_L directo)", status_l);
    ISR_LOG_VAL("[dp_isr]", "Estado mapeado", real_state);
    
    return real_state;
}

/*---------------------------------------------------------------------------*/
/* function: dp_isr                                                           */
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

/**
 * @brief  Función que el EXTI-Callback invoca.
 *         En modo polling repite VPC3_Poll, en modo interrupt repite VPC3_Isr.
 */
void dp_isr(void)
{
   /* Añadimos un log antes y después de la llamada crítica para detectar cuelgues. */
   ISR_LOG("[dp_isr]", "Procesando eventos del chip...");

#if (VPC3_SERIAL_MODE == 0)
    VPC3_Isr();
#else
    VPC3_Poll();
#endif

   ISR_LOG("[dp_isr]", "Procesamiento de eventos finalizado.");
}

