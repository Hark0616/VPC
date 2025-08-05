#include <string.h>
#include "platform.h"
#include "DpAppl.h"
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
const uint8_t DpApplDefCfg[DpApplCfgDataLength] = {
    0x20,   // ID del módulo "DI 1 Byte" del GSD
    0x10    // ID del módulo "DO 1 Byte" del GSD (REVISAR POR QUE DEBERIA SER 0xC0)
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
   //insert your real configuration data here
   sDpAppl.sCfgData.bLength = DpApplCfgDataLength; // length of configuration data
   memcpy( &sDpAppl.sCfgData.abData[0], &DpApplDefCfg[0], sDpAppl.sCfgData.bLength );
   printf("DEBUG: [DpCfg_Init] sDpAppl.sCfgData.abData inicializado: ");
   for(int k=0; k<sDpAppl.sCfgData.bLength; k++) {
       printf("0x%02X ", sDpAppl.sCfgData.abData[k]);
   }
   printf("\r\n");

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
    #define LOCAL_CFG_MAX 16 // Un tamaño máximo seguro para la copia local
    uint8_t localCfgBuffer[LOCAL_CFG_MAX];
    uint8_t localLen = (bCfgLength < LOCAL_CFG_MAX) ? bCfgLength : LOCAL_CFG_MAX;
    memcpy(localCfgBuffer, pbCfgData, localLen);

    g_chk_cfg_flag = 1;
    E_DP_CFG_ERROR eRetValue;
    uint8_t        bNrOfCheckModules;
    uint8_t        bRealCfgLength;
    uint8_t        i;
    MEM_UNSIGNED8_PTR pReceivedDataStart = localCfgBuffer; // Usar la copia local segura

    // DEBUGGING: Imprime el contenido completo del buffer recibido (usando copia local)
    printf("DEBUG: Chk_Cfg recibido. Longitud: %d bytes. Contenido (copia local): ", bCfgLength);
    for (i = 0; i < localLen; i++) {
        printf("0x%02X ", pReceivedDataStart[i]);
    }
    printf("\r\n");

    bRealCfgLength = sSystem.bNrOfModules * eDpCfgEntryLength;

    DpDiag_SetDefIdentRelDiag();
    DpDiag_SetDefModuleStatDiag();

    bNrOfCheckModules = sSystem.bNrOfModules;

    eRetValue = DP_CFG_OK;
    if( bCfgLength != bRealCfgLength )
    {
       if( bCfgLength < bRealCfgLength )
       {
          bNrOfCheckModules = ( bCfgLength / eDpCfgEntryLength );
          for( i = bNrOfCheckModules; i < sSystem.bNrOfModules; i++ )
          {
             DpDiag_SetIdentRelDiagEntry( i );
             DpDiag_SetModulStatusEntry( i, DIAG_MS_NO_MODULE );
          }
       }
       eRetValue = DP_CFG_FAULT;
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
        printf("EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)\r\n");
        printf("DEBUG: [ChkCfg] dentro del if OK eRetValue=%d\n", eRetValue);
    } else if (eRetValue == DP_CFG_FAULT) {
        printf("EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: %d, recibida: %d\r\n", bRealCfgLength, bCfgLength);
        printf("DEBUG: [ChkCfg] bRealCfgLength=%d\n", bRealCfgLength);
        printf("DEBUG: [ChkCfg] pbCfgData=%p\n", pbCfgData);
        printf("DEBUG: [ChkCfg] dentro del if FAULT eRetValue=%d\n", eRetValue);
    } else if (eRetValue == DP_CFG_UPDATE) {
        printf("EVENT: Chk_Cfg requiere actualización (DP_CFG_UPDATE)\r\n");
        printf("DEBUG: [ChkCfg] dentro del if UPDATE eRetValue=%d\n", eRetValue);
    }

    // Imprime la configuración local esperada
    printf("DEBUG: Configuración esperada local (%d bytes): ", bRealCfgLength);
    for (i = 0; i < bRealCfgLength; i++) {
        printf("0x%02X ", sDpAppl.sCfgData.abData[i]);
    }
    printf("\r\n");

    printf("DEBUG: [ChkCfg] Finalizando con eRetValue=%d\n", eRetValue);
    return eRetValue;
}

/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

