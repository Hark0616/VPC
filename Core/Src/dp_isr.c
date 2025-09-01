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
    uint8_t next_head = (isr_log_buffer.head + 1) % ISR_LOG_BUFFER_SIZE;
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
      ISR_LOG("[VPC3_Poll]", (events & 0x0800) ? "events & 0x0800 activo" : "events & 0x0800 inactivo");
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
                
                // Verificar integridad de punteros después del evento
                ISR_LOG("[dp_isr]", "Verificando integridad de punteros después del evento...");
                // Comentado temporalmente - función no disponible
                // if (VPC3_GetSegmentPointer(0) == 0xFFFF || VPC3_GetSegmentPointer(1) == 0xFFFF) {
                //     ISR_LOG("[dp_isr]", "PUNTEROS CORRUPTOS DETECTADOS! Intentando recuperación...");
                //     // Intentar recuperación de punteros
                // } else {
                //     ISR_LOG("[dp_isr]", "Punteros de segmentos validados correctamente");
                // }
                ISR_LOG("[dp_isr]", "Verificación de punteros de segmentos (función no disponible)");
                
                ISR_LOG_VAL("[dp_isr]", "STATUS_L después del evento", VPC3_GET_STATUS_L());
                ISR_LOG_VAL("[dp_isr]", "STATUS_H después del evento", VPC3_GET_STATUS_H());
                ISR_LOG_VAL("[dp_isr]", "DP_STATE después del evento", VPC3_GET_DP_STATE());
                ISR_LOG("[dp_isr]", "=== FIN IND_DIAG_BUFFER_CHANGED ===");
            }
         } /* if( VPC3_POLL_IND_DIAG_BUFFER_CHANGED() ) */

         /*------------------------------------------------------------------*/
         /* IND_NEW_PRM_DATA                                                 */
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

            bResult = VPC3_PRM_FINISHED;

            do
            {
                uint8_t* prmBufPtr;
                VPC3_ADR prmAddr;
                
               bPrmLength = VPC3_GET_PRM_LEN();

                ISR_LOG_VAL("[dp_isr]", "PRM Length", bPrmLength);
                
                // --- VALIDACIÓN CRÍTICA: Verificar puntero PRM antes de leer ---
                prmBufPtr = VPC3_GET_PRM_BUF_PTR();
                prmAddr = (VPC3_ADR)(uint32_t)prmBufPtr;
                
                ISR_LOG_VAL("[dp_isr]", "PRM Buffer Ptr", (unsigned int)prmBufPtr);
                ISR_LOG_VAL("[dp_isr]", "Addr", prmAddr);
                
                // Validar que la dirección está dentro del rango válido
                if (prmAddr >= ASIC_RAM_LENGTH) {
                    ISR_LOG("[dp_isr]", "ERROR: Puntero PRM corrupto!");
                    ISR_LOG_VAL("[dp_isr]", "Addr", prmAddr);
                    ISR_LOG_VAL("[dp_isr]", "ASIC_RAM_LENGTH", ASIC_RAM_LENGTH);
                    ISR_LOG("[dp_isr]", "Rechazando trama PRM corrupta para evitar LECTURA ILEGAL");
                    VPC3_SET_PRM_DATA_NOT_OK();
                    break; // Salir del bucle do-while
                }
                
                // Validar que la longitud es razonable según manual VPC3+ (7..244 bytes)
                if (bPrmLength > HELP_BUFSIZE) {
                    ISR_LOG("[dp_isr]", "ERROR: Longitud PRM excede buffer destino");
                    ISR_LOG_VAL("[dp_isr]", "bPrmLength", bPrmLength);
                    ISR_LOG_VAL("[dp_isr]", "buffer size", HELP_BUFSIZE);
                    ISR_LOG("[dp_isr]", "Rechazando.");
                    VPC3_SET_PRM_DATA_NOT_OK();
                    break; // Salir del bucle do-while
                }
                
                // *** VALIDACIONES CRÍTICAS ADICIONALES ***
                
                // 3. Validar que la dirección final no excede el rango de memoria
                if ((prmAddr + bPrmLength) > ASIC_RAM_LENGTH) {
                    ISR_LOG("[dp_isr]", "ERROR: Rango PRM excede memoria ASIC");
                    ISR_LOG_VAL("[dp_isr]", "prmAddr + bPrmLength", prmAddr + bPrmLength);
                    ISR_LOG_VAL("[dp_isr]", "ASIC_RAM_LENGTH", ASIC_RAM_LENGTH);
                    VPC3_SET_PRM_DATA_NOT_OK();
                    break;
                }
                
                // 4. Validar que la longitud no excede el buffer de destino
                if (bPrmLength > HELP_BUFSIZE) {
                    ISR_LOG("[dp_isr]", "ERROR: Longitud PRM excede buffer destino");
                    ISR_LOG_VAL("[dp_isr]", "bPrmLength", bPrmLength);
                    ISR_LOG_VAL("[dp_isr]", "buffer size", HELP_BUFSIZE);
                    VPC3_SET_PRM_DATA_NOT_OK();
                    break;
                }
                
                // *** TODAS LAS VALIDACIONES PASARON - PROCEDER CON COPIA SEGURA ***
                ISR_LOG("[dp_isr]", "DEBUG: Validaciones PRM exitosas - procediendo con copia segura");

                CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], prmBufPtr, bPrmLength );

               ISR_LOG_VAL("[dp_isr]", "Validando parametros", bPrmLength);
               
               // *** LOGGING DETALLADO DE LA TRAMA DE PARÁMETROS ***
               ISR_LOG("[dp_isr]", "=== CONTENIDO DE LA TRAMA DE PARAMETROS ===");
               ISR_LOG_VAL("[dp_isr]", "Longitud total", bPrmLength);
               
               // Log de los primeros bytes para debugging
               if (bPrmLength >= 1) ISR_LOG_VAL("[dp_isr]", "Byte 0 (Status1)", pDpSystem->abPrmCfgSsaHelpBuffer[0]);
               if (bPrmLength >= 2) ISR_LOG_VAL("[dp_isr]", "Byte 1 (WD_Factor1)", pDpSystem->abPrmCfgSsaHelpBuffer[1]);
               if (bPrmLength >= 3) ISR_LOG_VAL("[dp_isr]", "Byte 2 (WD_Factor2)", pDpSystem->abPrmCfgSsaHelpBuffer[2]);
               if (bPrmLength >= 4) ISR_LOG_VAL("[dp_isr]", "Byte 3 (min_Tsdr)", pDpSystem->abPrmCfgSsaHelpBuffer[3]);
               if (bPrmLength >= 5) ISR_LOG_VAL("[dp_isr]", "Byte 4 (Ident_L)", pDpSystem->abPrmCfgSsaHelpBuffer[4]);
               if (bPrmLength >= 6) ISR_LOG_VAL("[dp_isr]", "Byte 5 (Ident_H)", pDpSystem->abPrmCfgSsaHelpBuffer[5]);
               if (bPrmLength >= 7) ISR_LOG_VAL("[dp_isr]", "Byte 6 (UserPrmLen)", pDpSystem->abPrmCfgSsaHelpBuffer[6]);
               
               // Verificación del IdentNumber esperado (0xADAC)
               if (bPrmLength >= 6) {
                   uint16_t receivedIdent = (pDpSystem->abPrmCfgSsaHelpBuffer[5] << 8) | pDpSystem->abPrmCfgSsaHelpBuffer[4];
                   ISR_LOG_VAL("[dp_isr]", "IdentNumber recibido", receivedIdent);
                   if (receivedIdent == 0xADAC) {
                       ISR_LOG("[dp_isr]", "✓ IdentNumber CORRECTO (0xADAC)");
                   } else {
                       ISR_LOG("[dp_isr]", "✗ IdentNumber INCORRECTO - esperado: 0xADAC");
                   }
               }
               
               ISR_LOG("[dp_isr]", "Llamando DpPrm_ChkNewPrmData...");
               
               DP_ERROR_CODE prmCheckResult = DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength );
               ISR_LOG_VAL("[dp_isr]", "DpPrm_ChkNewPrmData retorno", prmCheckResult);
               
               if( prmCheckResult == DP_OK )
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

                  // *** CRÍTICO: Confirmar PRM_OK al ASIC ***
                  ISR_LOG("[dp_isr]", "Enviando PRM_OK al ASIC...");
                  bResult = VPC3_SET_PRM_DATA_OK();
                  ISR_LOG("[dp_isr]", "PRM_OK enviado exitosamente");
                  
                  // *** CRÍTICO: Verificar transición de estado ***
                  uint8_t statusAfterPrmOk = VPC3_GET_STATUS_L();
                  ISR_LOG_VAL("[dp_isr]", "STATUS_L despues de PRM_OK", statusAfterPrmOk);
                  
                  if (statusAfterPrmOk == 0x04) {
                      ISR_LOG("[dp_isr]", "ADVERTENCIA: Sigue en WAIT_PRM - PRM_OK no fue procesado");
                  } else if (statusAfterPrmOk == 0x08) {
                      ISR_LOG("[dp_isr]", "EXITO: Transicion a WAIT_CFG confirmada");
                  } else {
                      ISR_LOG_VAL("[dp_isr]", "Estado desconocido despues de PRM_OK", statusAfterPrmOk);
                  }
                  
                  ISR_LOG("[dp_isr]", "-> RESULTADO: Parametros ACEPTADOS.");
               } /* if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK ) */
               else
               {
                  // *** CRÍTICO: Confirmar PRM_NOT_OK al ASIC ***
                  ISR_LOG("[dp_isr]", "Enviando PRM_NOT_OK al ASIC...");
                  bResult = VPC3_SET_PRM_DATA_NOT_OK();
                  ISR_LOG("[dp_isr]", "PRM_NOT_OK enviado exitosamente");
                  
                  // Verificar estado después de PRM_NOT_OK
                  uint8_t statusAfterPrmNotOk = VPC3_GET_STATUS_L();
                  ISR_LOG_VAL("[dp_isr]", "STATUS_L despues de PRM_NOT_OK", statusAfterPrmNotOk);
                  
                  ISR_LOG("[dp_isr]", "-> RESULTADO: Parametros RECHAZADOS.");
               } /* else of if( DpPrm_ChkNewPrmData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bPrmLength ) == DP_OK ) */
            }
            while( bResult == VPC3_PRM_CONFLICT );
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
               
               // --- VALIDACIÓN CRÍTICA: Verificar puntero CFG antes de leer ---
               cfgBufPtr = VPC3_GET_CFG_BUF_PTR();
                               cfgAddr = (VPC3_ADR)(uint32_t)cfgBufPtr;
               
               ISR_LOG_VAL("[dp_isr]", "CFG Buffer Ptr", (unsigned int)cfgBufPtr);
               ISR_LOG_VAL("[dp_isr]", "Addr", cfgAddr);
               
               // Validar que la dirección está dentro del rango válido
               if (cfgAddr >= ASIC_RAM_LENGTH) {
                   ISR_LOG("[dp_isr]", "ERROR: Puntero CFG corrupto!");
                   ISR_LOG_VAL("[dp_isr]", "Addr", cfgAddr);
                   ISR_LOG_VAL("[dp_isr]", "ASIC_RAM_LENGTH", ASIC_RAM_LENGTH);
                   ISR_LOG("[dp_isr]", "Rechazando trama CFG corrupta para evitar LECTURA ILEGAL");
                   VPC3_SET_CFG_DATA_NOT_OK();
                   break; // Salir del bucle do-while
               }
               
               // Validar que la longitud es razonable según manual VPC3+ (1..244 bytes)
               if (bCfgLength > HELP_BUFSIZE) {
                   ISR_LOG("[dp_isr]", "ERROR: Longitud CFG excede buffer destino");
                   ISR_LOG_VAL("[dp_isr]", "bCfgLength", bCfgLength);
                   ISR_LOG_VAL("[dp_isr]", "buffer size", HELP_BUFSIZE);
                   ISR_LOG("[dp_isr]", "Rechazando.");
                   VPC3_SET_CFG_DATA_NOT_OK();
                   break; // Salir del bucle do-while
               }
               
               // *** VALIDACIONES CRÍTICAS ADICIONALES ***
               
               // 3. Validar que la dirección final no excede el rango de memoria
               if ((cfgAddr + bCfgLength) > ASIC_RAM_LENGTH) {
                   ISR_LOG("[dp_isr]", "ERROR: Rango CFG excede memoria ASIC");
                   ISR_LOG_VAL("[dp_isr]", "cfgAddr + bCfgLength", cfgAddr + bCfgLength);
                   ISR_LOG_VAL("[dp_isr]", "ASIC_RAM_LENGTH", ASIC_RAM_LENGTH);
                   VPC3_SET_CFG_DATA_NOT_OK();
                   break;
               }
               
               // 4. Validar que la longitud no excede el buffer de destino
               if (bCfgLength > HELP_BUFSIZE) {
                   ISR_LOG("[dp_isr]", "ERROR: Longitud CFG excede buffer destino");
                   ISR_LOG_VAL("[dp_isr]", "bCfgLength", bCfgLength);
                   ISR_LOG_VAL("[dp_isr]", "buffer size", HELP_BUFSIZE);
                   VPC3_SET_CFG_DATA_NOT_OK();
                   break;
               }
               
               // *** TODAS LAS VALIDACIONES PASARON - PROCEDER CON COPIA SEGURA ***
               ISR_LOG("[dp_isr]", "Validaciones CFG exitosas - procediendo con copia segura");
               
               CopyFromVpc3_( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], cfgBufPtr, bCfgLength );
               
               // *** DIAGNÓSTICO Y LOGGING: Validación inteligente para análisis ***
               // NOTA: Esta validación es SOLO para diagnóstico. La decisión final
               // siempre la toma DpCfg_ChkNewCfgData() independientemente de este resultado.
               uint8_t cfg_validation_result = dp_isr_validate_and_process_cfg(bCfgLength, pDpSystem->abPrmCfgSsaHelpBuffer);
               
               // Procesamiento adicional para configuraciones extendidas (solo logging)
               if (cfg_validation_result == DP_CFG_UPDATE) {
                   cfg_validation_result = dp_isr_process_extended_cfg(bCfgLength, pDpSystem->abPrmCfgSsaHelpBuffer);
               }
               
               // Log del resultado de validación (solo para análisis)
               ISR_LOG_VAL("[dp_isr]", "Resultado validación CFG (diagnóstico)", cfg_validation_result);
               switch( DpCfg_ChkNewCfgData( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) )
               {
                  case DP_CFG_OK:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgOk();
                     #endif /* #if DP_MSAC_C1 */

                     // *** CRÍTICO: Sincronizar Read_Config cuando aceptamos configuraciones extendidas ***
                     // Esto asegura que si el maestro solicita Get_Cfg, siempre reciba la configuración correcta
                     // independientemente de si la última PDU recibida fue extendida o no
                     if (bCfgLength > DpApplCfgDataLength) {
                         ISR_LOG("[dp_isr]", "Configuracion extendida aceptada - sincronizando Read_Config");
                         ISR_LOG_VAL("[dp_isr]", "Longitud recibida", bCfgLength);
                         ISR_LOG_VAL("[dp_isr]", "Longitud real GSD", DpApplCfgDataLength);
                         
                         // Establecer la longitud del buffer de lectura a nuestra configuración real
                         VPC3_SET_READ_CFG_LEN(DpApplCfgDataLength);
                         
                         // Copiar solo los bytes válidos de nuestra configuración al buffer del ASIC
                         CopyToVpc3_(VPC3_GET_READ_CFG_BUF_PTR(),
                                    &sDpAppl.sCfgData.abData[0],
                                    DpApplCfgDataLength);
                         
                         // Activar la actualización del buffer de lectura
                         VPC3_UPDATE_CFG_BUFFER();
                         
                         ISR_LOG("[dp_isr]", "Read_Config sincronizado con configuración real");
                     } else {
                         ISR_LOG("[dp_isr]", "Configuracion normal - Read_Config ya está sincronizado");
                     }

                     bResult = VPC3_SET_CFG_DATA_OK();
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion ACEPTADA (sin cambios).");
                     break;
                  } /* case DP_CFG_OK: */

                  case DP_CFG_FAULT:
                  {
                     #if DP_MSAC_C1
                        MSAC_C1_DoCfgNotOk();
                     #endif /* #if DP_MSAC_C1 */

                     bResult = VPC3_SET_CFG_DATA_NOT_OK();
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion RECHAZADA.");
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
                        ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion ACEPTADA (con actualizacion de I/O).");
                     } /* else of if( DP_OK != VPC3_CalculateInpOutpLength( (MEM_UNSIGNED8_PTR)&pDpSystem->abPrmCfgSsaHelpBuffer[0], bCfgLength ) ) */
                     break;
                  } /* case DP_CFG_UPDATE: */

                  default:
                  {
                     ISR_LOG("[dp_isr]", "-> RESULTADO: Configuracion con estado desconocido.");
                     VPC3_SET_CFG_DATA_NOT_OK();
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
                ISR_LOG("[dp_isr]", "-> RESULTADO: Transicion a DataExchange EXITOSA.");
            }
            else if (state_before == DATA_EX && state_after != DATA_EX)
            {
                 ISR_LOG("[dp_isr]", "-> RESULTADO: Se ha salido de DataExchange.");
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
            // ESTE MENSAJE NO DEBERÍA APARECER NUNCA
            ISR_LOG("[dp_isr]", "--- ALERTA: IND_DX_OUT detectado INESPERADAMENTE via polling. La condición de carrera puede persistir. ---");
            
            ISR_LOG("[dp_isr]", "DEBUG: IND_DX_OUT detectado - procesando Data Exchange Output");
            ISR_LOG_VAL("[dp_isr]", "STATUS antes de DpAppl_IsrDxOut L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS antes de DpAppl_IsrDxOut H", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L antes", VPC3_GET_STATUS_L());
            ISR_LOG("[dp_isr]", "(esperado DATA_EX=0x45)");
            ISR_LOG_VAL("[dp_isr]", "STATUS_H antes", VPC3_GET_STATUS_H());
            ISR_LOG("[dp_isr]", "(esperado 0xE3)");
            
            #if DP_MSAC_C1
               MSAC_C1_CheckIndDxOut();
            #endif /* #if DP_MSAC_C1 */

            DpAppl_IsrDxOut();
            ISR_LOG_VAL("[dp_isr]", "STATUS después de DpAppl_IsrDxOut L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS después de DpAppl_IsrDxOut H", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L después de DpAppl_IsrDxOut", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H después de DpAppl_IsrDxOut", VPC3_GET_STATUS_H());

            VPC3_CON_IND_DX_OUT();
            ISR_LOG_VAL("[dp_isr]", "STATUS después de VPC3_CON_IND_DX_OUT L", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS después de VPC3_CON_IND_DX_OUT H", VPC3_GET_STATUS_H());
            ISR_LOG_VAL("[dp_isr]", "STATUS_L después de VPC3_CON_IND_DX_OUT", VPC3_GET_STATUS_L());
            ISR_LOG_VAL("[dp_isr]", "STATUS_H después de VPC3_CON_IND_DX_OUT", VPC3_GET_STATUS_H());
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
   // --- LOGGING COMPLETO DE ACTIVACIÓN ---
   static uint32_t dp_isr_call_count = 0;
   dp_isr_call_count++;
   ISR_LOG_VAL("[dp_isr]", "=== LLAMADA", dp_isr_call_count);
   ISR_LOG("[dp_isr]", "/ INTERRUPCION DEL PLC ===");
   ISR_LOG_VAL("[dp_isr]", "TIMESTAMP", HAL_GetTick());
   ISR_LOG("[dp_isr]", "STACK TRACE - Llamada desde:");
   ISR_LOG("[dp_isr]", "- Función: dp_isr");
   ISR_LOG("[dp_isr]", "- Archivo: ../Core/Src/dp_isr.c");
   ISR_LOG_VAL("[dp_isr]", "- Línea", __LINE__);
   
   // Verificar si es llamada por polling o interrupción
   #if (VPC3_SERIAL_MODE == 0)
      ISR_LOG("[dp_isr]", "MODO: INTERRUPCIÓN (VPC3_Isr)");
   #else
      ISR_LOG("[dp_isr]", "MODO: POLLING (VPC3_Poll)");
   #endif
   
   // *** MONITOREO COMPLETO DE ESTADO INICIAL ***
   uint8_t initial_status_l = VPC3_GET_STATUS_L();
   uint8_t initial_status_h = VPC3_GET_STATUS_H();
   uint8_t initial_dp_state = VPC3_GET_DP_STATE();
   
   ISR_LOG_VAL("[dp_isr]", "INICIO - STATUS_L", initial_status_l);
   ISR_LOG_VAL("[dp_isr]", "INICIO - STATUS_H", initial_status_h);
   ISR_LOG_VAL("[dp_isr]", "INICIO - DP_STATE", initial_dp_state);
   
   // *** MONITOREO DE TODAS LAS INTERRUPCIONES ACTIVAS ***
   ISR_LOG("[dp_isr]", "=== VERIFICACIÓN DE INTERRUPCIONES ACTIVAS ===");
   
   // Verificar interrupciones de diagnóstico
   if (VPC3_POLL_IND_DIAG_BUFFER_CHANGED()) {
      ISR_LOG("[dp_isr]", "✓ IND_DIAG_BUFFER_CHANGED ACTIVO");
   }
   
   // Verificar interrupciones de parámetros
   if (VPC3_POLL_IND_NEW_PRM_DATA()) {
      ISR_LOG("[dp_isr]", "✓ IND_NEW_PRM_DATA ACTIVO - ¡SET_PRM RECIBIDO!");
   }
   
   // Verificar interrupciones de configuración
   if (VPC3_POLL_IND_NEW_CFG_DATA()) {
      ISR_LOG("[dp_isr]", "✓ IND_NEW_CFG_DATA ACTIVO - ¡CHECK_CFG RECIBIDO!");
   }
   
   // Verificar interrupciones de watchdog
   if (VPC3_POLL_IND_WD_DP_MODE_TIMEOUT()) {
      ISR_LOG("[dp_isr]", "✓ IND_WD_DP_MODE_TIMEOUT ACTIVO");
   }
   
   // Verificar interrupciones de cambio de estado
   if (VPC3_POLL_IND_GO_LEAVE_DATA_EX()) {
      ISR_LOG("[dp_isr]", "✓ IND_GO_LEAVE_DATA_EX ACTIVO");
   }
   
       // Verificar interrupciones de datos de salida (comentado - función no disponible)
    // if (VPC3_POLL_IND_NEW_DOUT_DATA()) {
    //    ISR_LOG("[dp_isr]", "✓ IND_NEW_DOUT_DATA ACTIVO");
    // }
    
    // Verificar interrupciones de datos de entrada (comentado - función no disponible)
    // if (VPC3_POLL_IND_NEW_DIN_DATA()) {
    //    ISR_LOG("[dp_isr]", "✓ IND_NEW_DIN_DATA ACTIVO");
    // }
   
   // Verificar interrupciones de SSA
   if (VPC3_POLL_IND_NEW_SSA_DATA()) {
      ISR_LOG("[dp_isr]", "✓ IND_NEW_SSA_DATA ACTIVO");
   }
   
       // Verificar si no hay interrupciones activas
    if (!VPC3_POLL_IND_DIAG_BUFFER_CHANGED() && 
        !VPC3_POLL_IND_NEW_PRM_DATA() && 
        !VPC3_POLL_IND_NEW_CFG_DATA() && 
        !VPC3_POLL_IND_WD_DP_MODE_TIMEOUT() && 
        !VPC3_POLL_IND_GO_LEAVE_DATA_EX() && 
        // !VPC3_POLL_IND_NEW_DOUT_DATA() &&  // Comentado - función no disponible
        // !VPC3_POLL_IND_NEW_DIN_DATA() &&  // Comentado - función no disponible
        !VPC3_POLL_IND_NEW_SSA_DATA()) {
       ISR_LOG("[dp_isr]", "⚠ NINGUNA INTERRUPCIÓN ACTIVA DETECTADA");
    }
   
   ISR_LOG("[dp_isr]", "=== FIN VERIFICACIÓN DE INTERRUPCIONES ===");
   
   // --- CRITICAL: Check for MODE_REG_2 corruption during interrupt ---
  uint8_t mode_reg2 = VPC3_GetModeReg2Shadow();
   ISR_LOG_VAL("[dp_isr]", "MODE_REG_2 actual", mode_reg2);
    ISR_LOG("[dp_isr]", "(esperado: 0x05)");
   
   if (mode_reg2 != 0x05) {
      ISR_LOG("[dp_isr]", "MODE_REG_2 corruption detected during interrupt");
      ISR_LOG_VAL("[dp_isr]", "expected", 0x05);
      // Try to recover immediately
      if (VPC3_ForceModeReg2() == 0) {
         ISR_LOG("[dp_isr]", "MODE_REG_2 recovered during interrupt");
      } else {
         ISR_LOG("[dp_isr]", "Failed to recover MODE_REG_2 during interrupt");
      }
   }
   
   // Detectar corrupción de registros
   if (VPC3_GET_STATUS_L() == 0x26 && VPC3_GET_STATUS_H() == 0xB7) {
      ISR_LOG("[dp_isr]", "  CORRUPCIÓN DETECTADA AL INICIO - STATUS_L=0x26, STATUS_H=0xB7");
   }
   
   // Detectar transición de DATA_EX a corrupción
   static uint8_t last_status_l = 0xFF;
   static uint8_t last_status_h = 0xFF;
   uint8_t current_status_l = VPC3_GET_STATUS_L();
   uint8_t current_status_h = VPC3_GET_STATUS_H();
   
   if (last_status_l == 0x45 && last_status_h == 0xE3 && 
       (current_status_l != 0x45 || current_status_h != 0xE3)) {
      ISR_LOG("[dp_isr]", "  TRANSICIÓN DETECTADA: STATUS_L=0x45->");
      ISR_LOG_VAL("[dp_isr]", "STATUS_L nuevo", current_status_l);
      ISR_LOG("[dp_isr]", "STATUS_H=0xE3->");
      ISR_LOG_VAL("[dp_isr]", "STATUS_H nuevo", current_status_h);
   }
   
   last_status_l = current_status_l;
   last_status_h = current_status_h;
   
   ISR_LOG_VAL("[dp_isr]", "ANTES de procesar eventos - STATUS_L", VPC3_GET_STATUS_L());
   
#if (VPC3_SERIAL_MODE == 0)
    VPC3_Isr();
#else
    VPC3_Poll();
#endif

      // *** MONITOREO COMPLETO DE ESTADO FINAL ***
   uint8_t final_status_l = VPC3_GET_STATUS_L();
   uint8_t final_status_h = VPC3_GET_STATUS_H();
   uint8_t final_dp_state = VPC3_GET_DP_STATE();
   
   ISR_LOG_VAL("[dp_isr]", "DESPUÉS de procesar eventos - STATUS_L", final_status_l);
   ISR_LOG_VAL("[dp_isr]", "DESPUÉS de procesar eventos - STATUS_H", final_status_h);
   ISR_LOG_VAL("[dp_isr]", "DESPUÉS de procesar eventos - DP_STATE", final_dp_state);
   
   // *** ANÁLISIS DE CAMBIOS DE ESTADO ***
   if (initial_status_l != final_status_l || initial_status_h != final_status_h) {
      ISR_LOG("[dp_isr]", "=== CAMBIO DE ESTADO DETECTADO ===");
             // Log de cambios de estado usando formato simple
       ISR_LOG("[dp_isr]", "✓ TRANSICIÓN DETECTADA:");
       ISR_LOG_VAL("[dp_isr]", "STATUS_L cambio", initial_status_l);
       ISR_LOG_VAL("[dp_isr]", "STATUS_L nuevo", final_status_l);
       ISR_LOG_VAL("[dp_isr]", "STATUS_H cambio", initial_status_h);
       ISR_LOG_VAL("[dp_isr]", "STATUS_H nuevo", final_status_h);
       ISR_LOG_VAL("[dp_isr]", "DP_STATE cambio", initial_dp_state);
       ISR_LOG_VAL("[dp_isr]", "DP_STATE nuevo", final_dp_state);
      
      // Interpretar el cambio de estado
      if (initial_status_l == 0x45 && final_status_l == 0x08) {
         ISR_LOG("[dp_isr]", "✓ TRANSICIÓN EXITOSA: WAIT_PRM -> WAIT_CFG");
      } else if (initial_status_l == 0x08 && final_status_l == 0x10) {
         ISR_LOG("[dp_isr]", "✓ TRANSICIÓN EXITOSA: WAIT_CFG -> DATA_EX");
      } else if (initial_status_l == 0x45 && final_status_l == 0x45) {
         ISR_LOG("[dp_isr]", "⚠ ESTADO SIN CAMBIOS: Sigue en WAIT_PRM");
      } else {
         ISR_LOG("[dp_isr]", "? TRANSICIÓN DESCONOCIDA");
      }
   } else {
      ISR_LOG("[dp_isr]", "✓ ESTADO ESTABLE - Sin cambios");
   }
   
   ISR_LOG_VAL("[dp_isr]", "FIN - STATUS_L", final_status_l);
   ISR_LOG_VAL("[dp_isr]", "FIN - STATUS_H", final_status_h);
   ISR_LOG_VAL("[dp_isr]", "FIN - DP_STATE", final_dp_state);
   ISR_LOG("[dp_isr]", "FIN - Verificando si STATUS_L=0x45 (DATA_EX) y STATUS_H=0xE3");
   
       // *** CRÍTICO: Cerrar correctamente la interrupción ***
    // En modo polling no es estrictamente necesario, pero no hace daño si el macro es "no-op" seguro
    VPC3_SET_EOI();
   
   ISR_LOG_VAL("[dp_isr]", "=== FIN LLAMADA", dp_isr_call_count);
       ISR_LOG_VAL("[dp_isr]", "=== FIN INTERRUPCION", dp_isr_call_count);
}

