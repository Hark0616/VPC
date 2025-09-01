#include <string.h>
#include "platform.h"
#include "DpAppl.h"
#include "dp_if.h"   /* Para funciones de logging dp_cfg_log_* */
#include <stdio.h>
#include <stdint.h>

volatile uint8_t g_chk_cfg_flag = 0;

/*---------------------------------------------------------------------------*/
/* defines, structures                                                       */
/*---------------------------------------------------------------------------*/
enum
{
    eDpCfgEntryLength = 0x01   /**< Length of configuration data from SliceBus standard modules. */
    //eDpCfgEntryLength = 0x04   /**< Length of configuration data from SliceBus standard modules. */
};

#define DpApplCfgDataLength ((uint8_t)0x02) // Total de 2 bytes para la configuración de dos módulos

// Tamaño máximo para la copia local de configuración (seguridad y futuras expansiones)
#define LOCAL_CFG_MAX 64 // Aumentado para mayor seguridad y futuras expansiones
// Alternativa: usar HELP_BUFSIZE si se necesita más espacio
const uint8_t DpApplDefCfg[DpApplCfgDataLength] = {
    0x20,   // ID del módulo "DO 1 Byte" del GSD
    0x10    // ID del módulo "DI 1 Byte" del GSD
};
/*#define DpApplCfgDataLength ((uint8_t)0x08)
const uint8_t DpApplDefCfg[DpApplCfgDataLength] = {
    0x82,0x00,0x00,0x22,   // OUT 2 bytes  Id 0x22
    0x42,0x00,0x00,0x24    // IN  2 bytes  Id 0x24
};*/

/*---------------------------------------------------------------------------*/
/* local user data definitions                                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/
/*!
  \brief Init profibus configuration.
*/
void DpCfg_Init( void )
{
   //todo:
   sDpAppl.sCfgData.bLength = DpApplCfgDataLength; // length of configuration data
   memcpy( &sDpAppl.sCfgData.abData[0], &DpApplDefCfg[0], sDpAppl.sCfgData.bLength );
   dp_cfg_log_hex_dump("DpCfg_Init", "sDpAppl.sCfgData.abData inicializado", sDpAppl.sCfgData.abData, sDpAppl.sCfgData.bLength);

       // *** CRÍTICO: Programar el buffer de configuración del ASIC desde el arranque ***
    // Esto asegura que si el maestro solicita Get_Cfg, siempre reciba la configuración correcta
    // independientemente de si la última PDU recibida fue extendida o no
    VPC3_SET_READ_CFG_LEN(DpApplCfgDataLength);
    
    // *** CORRECCIÓN CRÍTICA: Copiar datos al buffer del ASIC y luego activar la actualización ***
    // 1. Establecer la longitud del buffer de lectura
    // 2. Copiar los datos de configuración al buffer del ASIC
    // 3. Activar la actualización del buffer (sin argumentos)
    CopyToVpc3_( VPC3_GET_READ_CFG_BUF_PTR(),
                  &sDpAppl.sCfgData.abData[0],
                  DpApplCfgDataLength );
    VPC3_UPDATE_CFG_BUFFER();   // Sin argumentos - solo activa el flag de actualización
   
   // *** CRÍTICO: Programar longitudes I/O desde el arranque ***
   // Esto asegura que bInputDataLength=1 y bOutputDataLength=1 estén configurados en el ASIC
   // antes de que lleguen datos cíclicos, evitando saltos a WAIT_PRM
   if (VPC3_CalculateInpOutpLength((MEM_UNSIGNED8_PTR)DpApplDefCfg, DpApplCfgDataLength) == DP_OK) {
       VPC3_SetIoDataLength();
       dp_cfg_log_message("DpCfg_Init", "Longitudes I/O programadas en ASIC");
       dp_cfg_log_value("DpCfg_Init", "Input length", 1);  // 1 byte por módulo DI
       dp_cfg_log_value("DpCfg_Init", "Output length", 1); // 1 byte por módulo DO
   } else {
       dp_cfg_log_message("DpCfg_Init", "ERROR: Fallo al calcular longitudes I/O");
   }
   
   dp_cfg_log_message("DpCfg_Init", "Buffer de configuración del ASIC programado");
   dp_cfg_log_value("DpCfg_Init", "Longitud programada en ASIC", DpApplCfgDataLength);

}//void DpCfg_Init( void )

/*---------------------------------------------------------------------------*/
/* function: DpCfg_ChkNewCfgData                                             */
/*---------------------------------------------------------------------------*/
/**
 * @brief Checking configuration data.
 * The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+
 * has received a Check_Cfg message and has made the data available in the Cfg buffer.
 *
 * The user has to program the function for checking the received configuration data.
 *
 * @param[in] pbCfgData - address of check configuration data
 * @param[in] bCfgLength - length of configuration data
 *
 * @return see E_DP_CFG_ERROR @see E_DP_CFG_ERROR
 */
E_DP_CFG_ERROR DpCfg_ChkNewCfgData(MEM_UNSIGNED8_PTR pbCfgData, uint8_t bCfgLength)
{
    // --- PASO 1: Copia local para evitar sobreescritura (Tu excelente idea) ---
    uint8_t localCfgBuffer[LOCAL_CFG_MAX];
    uint8_t localLen = (bCfgLength < LOCAL_CFG_MAX) ? bCfgLength : LOCAL_CFG_MAX;
    memcpy(localCfgBuffer, pbCfgData, localLen);

    g_chk_cfg_flag = 1;
    E_DP_CFG_ERROR eRetValue;
    uint8_t        bNrOfCheckModules;
    uint8_t        bRealCfgLength;
    uint8_t        i;
    MEM_UNSIGNED8_PTR pReceivedDataStart = localCfgBuffer; // Usar la copia local segura

    // DEBUGGING: Log del contenido del buffer recibido (usando copia local)
    dp_cfg_log_value("ChkCfg", "Longitud recibida", bCfgLength);
    dp_cfg_log_hex_dump("ChkCfg", "Contenido (copia local)", pReceivedDataStart, localLen);

    bRealCfgLength = sSystem.bNrOfModules * eDpCfgEntryLength;

    DpDiag_SetDefIdentRelDiag();
    DpDiag_SetDefModuleStatDiag();

    eRetValue = DP_CFG_OK;
    
    // *** NUEVA LÓGICA: Tolerar configuraciones extendidas ***
    if( bCfgLength < bRealCfgLength )
    {
       // Configuración incompleta - marcar FAULT
       bNrOfCheckModules = ( bCfgLength / eDpCfgEntryLength );
       for( i = bNrOfCheckModules; i < sSystem.bNrOfModules; i++ )
       {
          DpDiag_SetIdentRelDiagEntry( i );
          DpDiag_SetModulStatusEntry( i, DIAG_MS_NO_MODULE );
       }
       eRetValue = DP_CFG_FAULT;
    }
    else if( bCfgLength > bRealCfgLength )
    {
       // Configuración extendida - usar solo los bytes necesarios
       bNrOfCheckModules = sSystem.bNrOfModules;
       // Comparar solo los primeros bRealCfgLength bytes
       // Los bytes extra se ignoran (pueden ser información adicional del PLC)
       // *** CAMBIO CRÍTICO: Retornar OK en lugar de UPDATE para evitar VPC3_CalculateInpOutpLength ***
       eRetValue = DP_CFG_OK;
    }
    else
    {
       // Longitud exacta - comportamiento normal
       bNrOfCheckModules = sSystem.bNrOfModules;
       eRetValue = DP_CFG_OK;
    }

    for( i = 0; i < bNrOfCheckModules; i++ )
    {
       // Usa la copia local segura y accede por índice
       if( memcmp( &sDpAppl.sCfgData.abData[i*eDpCfgEntryLength], &pReceivedDataStart[i*eDpCfgEntryLength], eDpCfgEntryLength ) == 0 )
       {
          DpDiag_ClrIdentRelDiagEntry( i );
          DpDiag_SetModulStatusEntry( i, DIAG_MS_OK );
       }
       else
       {
          eRetValue = DP_CFG_FAULT;
       }
    }

    if( ( eRetValue == DP_CFG_OK ) || ( eRetValue == DP_CFG_UPDATE ) )
    {
       eRetValue = DpDiag_SetCfgOk( eRetValue );
       if( eRetValue != DP_CFG_FAULT )
       {
          VPC3_SetDpState( eDpStateCfgOkStatDiag );
       }
    }
    else
    {
       DpDiag_SetCfgNotOk();
    }

    DpDiag_AlarmInit();

    if (eRetValue == DP_CFG_OK) {
        if (bCfgLength > bRealCfgLength) {
            dp_cfg_log_message("ChkCfg", "EVENT: Chk_Cfg extendido aceptado como OK");
            dp_cfg_log_value("ChkCfg", "Longitud esperada vs recibida", bRealCfgLength);
            dp_cfg_log_value("ChkCfg", "Longitud recibida", bCfgLength);
            dp_cfg_log_message("ChkCfg", "Configuración extendida procesada como OK (bytes extra ignorados)");
        } else {
            dp_cfg_log_message("ChkCfg", "EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)");
        }
        dp_cfg_log_value("ChkCfg", "eRetValue", eRetValue);
    } else if (eRetValue == DP_CFG_FAULT) {
        dp_cfg_log_message("ChkCfg", "EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT)");
        dp_cfg_log_value("ChkCfg", "Longitud esperada vs recibida", bRealCfgLength);
        dp_cfg_log_value("ChkCfg", "Longitud recibida", bCfgLength);
        dp_cfg_log_value("ChkCfg", "pbCfgData ptr", (uint32_t)pbCfgData);
        dp_cfg_log_value("ChkCfg", "eRetValue", eRetValue);
    } else if (eRetValue == DP_CFG_UPDATE) {
        dp_cfg_log_message("ChkCfg", "EVENT: Chk_Cfg extendido aceptado (DP_CFG_UPDATE)");
        dp_cfg_log_value("ChkCfg", "Longitud esperada vs recibida", bRealCfgLength);
        dp_cfg_log_value("ChkCfg", "Longitud recibida", bCfgLength);
        dp_cfg_log_message("ChkCfg", "Configuración extendida - usando solo los primeros bytes");
        dp_cfg_log_value("ChkCfg", "eRetValue", eRetValue);
    }

    // Log de la configuración local esperada
    dp_cfg_log_hex_dump("ChkCfg", "Configuración esperada local", sDpAppl.sCfgData.abData, bRealCfgLength);
    
    dp_cfg_log_value("ChkCfg", "Finalizando con eRetValue", eRetValue);
    return eRetValue;
}

/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

