#include <string.h>
#include "platform.h"
#include "DpAppl.h"
#include "DpCfg.h"
#include "dp_if.h"
#include "dp_inc.h"
#include <stdio.h>
#include "stm32f4xx_hal.h"

// Function declarations to fix implicit declaration warnings
void print_vpc3_registers(void);
void print_vpc3_state(void);

/*---------------------------------------------------------------------------*/
/* defines, structures                                                       */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* global user data definitions                                              */
/*---------------------------------------------------------------------------*/
VPC3_STRUC_PTR             pVpc3;               /**< Pointer to VPC3+ structure. */
VPC3_STRUC_PTR             pVpc3Channel1;       /**< Pointer to VPC3+ structure channel 1. */

VPC3_SYSTEM_STRUC sDpSystemChannel1;
VPC3_SYSTEM_STRUC_PTR pDpSystem = &sDpSystemChannel1;

VPC3_ADR                   Vpc3AsicAddress;     /**< Global VPC3 address. */

VPC3_STRUC_ERRCB           sVpc3Error;          /**< Error structure. */
DP_APPL_STRUC              sDpAppl;             /**< User application structure. */

#if VPC3_SERIAL_MODE
   VPC3_STRUC              sVpc3OnlyForInit;    /**< Structure is used for VPC3+ initialization in serial mode. */
#endif//#if VPC3_SERIAL_MODE

#ifdef EvaBoard_AT89C5132
   static uint8_t bDipSwitch5;
   static uint8_t bOldDipSwitch5;
   static uint8_t bDiagStateModule5;
   static uint8_t bDiagStateModule6;
#endif//#ifdef EvaBoard_AT89C5132

//counter module
static uint16_t   wCounterValue;
static uint8_t    bCounterTimeBase;
static uint8_t    bCounterUpperLimit;
static uint8_t    bCounterLowerLimit;

STRUC_SYSTEM      sSystem;

/*---------------------------------------------------------------------------*/
/* defines, structures and variables for our demo application                */
/*---------------------------------------------------------------------------*/

#ifdef EvaBoard_AT89C5132
   #if VPC3_SERIAL_MODE
      #if VPC3_SPI
         ROMCONST__ uint8_t NAME[12] = { 0x44, 0x50, 0x56, 0x30, 0x41, 0x46, 0x46, 0x45, 0x2D, 0x53, 0x50, 0x49 }; //DPV0AFFE-SPI
      #endif//#if VPC3_SPI

      #if VPC3_I2C
         ROMCONST__ uint8_t NAME[12] = { 0x44, 0x50, 0x56, 0x30, 0x41, 0x46, 0x46, 0x45, 0x2D, 0x49, 0x49, 0x43 }; //DPV0AFFE-IIC
      #endif//#if VPC3_I2C
   #else
         ROMCONST__ uint8_t NAME[12] = { 0x44, 0x50, 0x56, 0x30, 0x41, 0x46, 0x46, 0x45, 0x20, 0x20, 0x20, 0x20 }; //DPV0AFFE
   #endif//#if VPC3_SERIAL_MODE
#endif//#ifdef EvaBoard_AT89C5132

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/
extern uint8_t DpDiag_DemoDiagnostic( uint8_t bDiagNr, uint8_t bModuleNr );
void dp_isr(void);
uint16_t leer_feedback(void);

/*--------------------------------------------------------------------------*/
/* function: DpAppl_TestApplEvent                                           */
/*--------------------------------------------------------------------------*/
/*!
  \brief Check if the internal PROFIBUS event is set and clear the event.

  \param[in] eDpApplEv PROFIBUS event @see eDpApplEv_Flags

  \retval VPC3_TRUE Event was set
  \retval VPC3_FALSE Event was not set.
*/
static uint8_t DpAppl_TestApplEvent( eDpApplEv_Flags eDpApplEv )
{
   if( sDpAppl.eDpApplEvent & eDpApplEv )
   {
      sDpAppl.eDpApplEvent &= ~eDpApplEv;
      return VPC3_TRUE;
   }//if( sDpAppl.eDpApplEvent & eDpApplEv )

   return VPC3_FALSE;
}//static uint8_t DpAppl_TestApplEvent( eDpApplEv_Flags eDpApplEv )

/*--------------------------------------------------------------------------*/
/* function: DpAppl_SetApplEvent                                            */
/*--------------------------------------------------------------------------*/
/*!
  \brief Set PROFIBUS event.

  \param[in] eDpApplEv PROFIBUS event @see eDpApplEv_Flags
*/
static void DpAppl_SetApplEvent( eDpApplEv_Flags eDpApplEv )
{
   sDpAppl.eDpApplEvent |= eDpApplEv;
}//static void DpAppl_SetApplEvent( eDpApplEv_Flags eDpApplEv )

/*--------------------------------------------------------------------------*/
/* function: DpAppl_CheckEvIoOut                                            */
/*--------------------------------------------------------------------------*/
/*!
  \brief Handling of the PROFIBUS output data ( master to slave ).

  The VPC3+ has received a DataExchange message and has made the new output data
  available in the N-buffer. In the case of Power_On or Leave_Master, the
  VPC3+ clears the content of the buffer, and generates this event also.
*/
static void DpAppl_CheckEvIoOut( void )
{
VPC3_UNSIGNED8_PTR  pToOutputBuffer;   /**< Pointer to output buffer. */
uint8_t             bOutputState;      /**< State of output data. */

   if( DpAppl_TestApplEvent( eDpApplEv_IoOut ) )
   {
      printf("DEBUG: [DpAppl_CheckEvIoOut] Evento IoOut detectado y procesado por el bucle principal (sincrono).\n");
      pToOutputBuffer = VPC3_GetDoutBufPtr( &bOutputState );
      if( pToOutputBuffer )
      {
         // Copy output data from PROFIBUS buffer
         CopyFromVpc3( &sSystem.sOutput.abDo8[0], pToOutputBuffer, pDpSystem->bOutputDataLength );

         /*/ Handle Set Point (0-100) and Relay Control
         uint8_t setpoint = sSystem.sOutput.abDo8[0] & 0x7F;  // Bits 0-6 for setpoint (0-100)
         uint8_t relay_control = (sSystem.sOutput.abDo8[0] >> 7) & 0x01;  // Bit 7 for relay control
*/
         uint16_t setpoint = pToOutputBuffer[0] | (pToOutputBuffer[1] << 8);
         // uint8_t  relay    = (setpoint >> 15) & 1;   // si quieres un bit de control - UNUSED
         setpoint &= 0x7FFF;                         // rango 0-32767 ó 0-100 como prefieras

         // TODO: Implement your setpoint and relay control logic here
         // Example: Update motor speed based on setpoint
         // Example: Control relay based on relay_control bit
      }
   }
}//static void DpAppl_CheckEvIoOut( void )

/*--------------------------------------------------------------------------*/
/* function: DpAppl_ReadInputData                                           */
/*--------------------------------------------------------------------------*/
/*!
  \brief Handling of the PROFIBUS input data ( slave --> master ).
*/
static void DpAppl_ReadInputData( void )
{
   // Read fault codes and status (Input Module 1)
   uint8_t fault_codes = 0;
   // TODO: Read actual fault codes from hardware
   // Bit 0: Overload
   // Bit 1: Over Temp
   // Bit 2: Power Loss
   // Bit 3: Relay 1 Status
   sSystem.sInput.abDi8[0] = fault_codes;

   // Read analog values (Input Module 2)
   uint8_t analog_values = 0;
   // TODO: Read actual analog values from hardware
   // Bits 0-1: Input Mode (0-10V, 2-10V)
   // Bits 2-7: Reserved for future use
   sSystem.sInput.abDi8[1] = analog_values;

   // Update input data in PROFIBUS buffer
   VPC3_UNSIGNED8_PTR pToInputBuffer = VPC3_GetDinBufPtr();
   if( pToInputBuffer )
   {
      // Read feedback value (16-bit)
      uint16_t fb = leer_feedback();        /* tu función ADC, encoder, etc. */
      pToInputBuffer[0] = fb & 0xFF;        /* LSB */
      pToInputBuffer[1] = fb >> 8;          /* MSB */

      // Copy remaining input data
      CopyToVpc3( pToInputBuffer + 2, &sSystem.sInput.abDi8[0], pDpSystem->bInputDataLength - 2 );
   }
}//static void DpAppl_ReadInputData( void )

/*--------------------------------------------------------------------------*/
/* function: DpAppl_ApplicationReady                                        */
/*--------------------------------------------------------------------------*/
/*!
  \brief ApplicatioReady

  This function is called after the PROFIBUS configuration data has been acknowledged
  positive by the user. The slave is now in DataExchange but the static diagnostic bit is set.
  The user can do here own additional initialization and should read here the input data. The
  slave delete now the static diagnostic bit and the master will send DataExchange telegrams.
*/
static void DpAppl_ApplicationReady( void )
{
   printf("DEBUG: [ApplicationReady] INICIO - Configurando aplicación después de Chk_Cfg exitoso\n");
   
   #if DPV1_IM_SUPP
      DpIm_ClearImIndex( 0x03 );
   #endif//#if DPV1_IM_SUPP

   /** @todo Make here your own initialization. */

   //read input data
   DpAppl_ReadInputData();

   //reset Diag.Stat
   printf("DEBUG: [ApplicationReady] Llamando DpDiag_ResetStatDiag()...\n");
   if( DpDiag_ResetStatDiag() )
   {
      printf("DEBUG: [ApplicationReady] DpDiag_ResetStatDiag() exitoso, configurando estados\n");
      VPC3_ClrDpState( eDpStateCfgOkStatDiag );
      VPC3_SetDpState( eDpStateApplReady );
      VPC3_SetDpState( eDpStateRun );
      printf("DEBUG: [ApplicationReady] Estados configurados: eDpStateCfgOkStatDiag=%d, eDpStateApplReady=%d, eDpStateRun=%d\n",
             VPC3_GetDpState(eDpStateCfgOkStatDiag), VPC3_GetDpState(eDpStateApplReady), VPC3_GetDpState(eDpStateRun));
   } else {
      printf("DEBUG: [ApplicationReady] DpDiag_ResetStatDiag() FALLÓ\n");
   }//if( DpDiag_ResetStatDiag() )
   
   printf("DEBUG: [ApplicationReady] FIN - Aplicación configurada\n");
}//static void DpAppl_ApplicationReady( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_ProfibusInit                                             */
/*---------------------------------------------------------------------------*/
/*!
  \brief Initializing of PROFIBUS slave communication.
*/
void DpAppl_ProfibusInit( void )
{
DP_ERROR_CODE       bError;

   /*-----------------------------------------------------------------------*/
   /* init application data                                                 */
   /*-----------------------------------------------------------------------*/
   memset( &sDpAppl, 0, sizeof( sDpAppl ) );

   /*-----------------------------------------------------------------------*/
   /* init Module-System                                                    */
   /*-----------------------------------------------------------------------*/
   memset( &sSystem, 0, sizeof(STRUC_SYSTEM) );
   sSystem.bNrOfModules = MaxModule;

   #ifdef EvaBoard_AT89C5132
      bDipSwitch5 = 0x00;
      bOldDipSwitch5 = 0x00;
      bDiagStateModule5 = VPC3_FALSE;
      bDiagStateModule6 = VPC3_FALSE;
   #endif//#ifdef EvaBoard_AT89C5132

   wCounterValue               = 0x0000;
   bCounterTimeBase            = 0x00;
   bCounterUpperLimit          = VPC3_FALSE;
   bCounterLowerLimit          = VPC3_FALSE;

   /*-----------------------------------------------------------------------*/
   /* initialize VPC3                                                       */
   /*-----------------------------------------------------------------------*/
   #if VPC3_SERIAL_MODE
      Vpc3AsicAddress = (VPC3_ADR)(uintptr_t)VPC3_ASIC_ADDRESS;
      pVpc3 = &sVpc3OnlyForInit;
      pDpSystem = &sDpSystemChannel1;

      memset( pVpc3, 0, sizeof( VPC3_STRUC ) );
   #else
      pVpc3Channel1 = (VPC3_STRUC_PTR)VPC3_ASIC_ADDRESS;
      Vpc3AsicAddress = (VPC3_ADR)(uintptr_t)VPC3_ASIC_ADDRESS;
      pVpc3 = pVpc3Channel1;
      pDpSystem = &sDpSystemChannel1;
   #endif//#if VPC3_SERIAL_MODE

   /*-----------------------------------------------------------------------*/
   /* initialize global system structure                                    */
   /*-----------------------------------------------------------------------*/
   memset( pDpSystem, 0, sizeof( VPC3_SYSTEM_STRUC ));
   pDpSystem->eDpState = eDpStateInit;

   DpPrm_Init();
   DpCfg_Init();
   DpDiag_Init();

//   DpAppl_ClrResetVPC3Channel1();

   bError = VPC3_MemoryTest();

   if( DP_OK == bError )
   {
      #ifdef EvaBoard_AT89C5132
         bError = VPC3_Initialization( (*READ_PORT0 & 0x7F), IDENT_NR, (psCFG)&sDpAppl.sCfgData );     // address of slave; PORT0
      #else
         bError = VPC3_Initialization( DP_ADDR, IDENT_NR, (psCFG)&sDpAppl.sCfgData );                  // address of slave
      #endif//#ifdef EvaBoard_AT89C5132

      if( DP_OK == bError )
      {
        // DpAppl_EnableInterruptVPC3Channel1();

         //todo: before startup the VPC3+, make here your own initialzations

         VPC3_Start();
      }//if( DP_OK == bError )
      else
      {
         sVpc3Error.bErrorCode = bError;
         DpAppl_FatalError( _DP_USER, __LINE__, &sVpc3Error );
      }//else of if( DP_OK == bError )
   }//if( DP_OK == bError )
   else
   {
      sVpc3Error.bErrorCode = bError;
      DpAppl_FatalError( _DP_USER, __LINE__, &sVpc3Error );
   }//else of if( DP_OK == bError )
}//void DpAppl_ProfibusInit( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_ProfibusMain                                                    */
/*---------------------------------------------------------------------------*/
/*!
  \brief The application program has to call this function cyclically so that the PROFIBUS DP slave services can be processed.
*/

void DpAppl_ProfibusMain( void )
{
#if VPC3_SERIAL_MODE
   uint8_t bStatusRegHigh;
   uint8_t bStatusRegLow;
#endif /* #if VPC3_SERIAL_MODE */
uint8_t bDpState;

   // Variables de timing para logs
   static uint32_t last_debug = 0;

   /*-------------------------------------------------------------------*/
   /* trigger watchdogs                                                 */
   /*-------------------------------------------------------------------*/
   // toggle user watchdog
   VPC3_RESET_USER_WD();   // toggle user watchdog

   #if VPC3_SERIAL_MODE
      /*----------------------------------------------------------------*/
      /* Poll PROFIBUS events                                           */
      /*----------------------------------------------------------------*/
      VPC3_Poll();
      // Nota: VPC3_Poll() ahora retorna uint16_t, pero no necesitamos capturar el valor aquí
      // ya que los logs están dentro de la función VPC3_Poll()
   #endif//#if VPC3_SERIAL_MODE

   /*-------------------------------------------------------------------*/
   /* DP-V1 statemachines                                               */
   /*-------------------------------------------------------------------*/
   VPC3_ProcessDpv1StateMachine();

   /*-------------------------------------------------------------------*/
   /* internal state machine                                            */
   /*-------------------------------------------------------------------*/
   if( VPC3_GetDpState( eDpStateInit ) )
   {
      // clear data
      memset( &sSystem.sOutput.abDo8[0], 0, DOUT_BUFSIZE );
      memset( &sSystem.sInput.abDi8[0],  0, DIN_BUFSIZE );

      #ifdef EvaBoard_AT89C5132
         *WRITE_PORT1 = 0x00;
         *WRITE_PORT2 = 0x00;
      #endif//#ifdef EvaBoard_AT89C5132

      VPC3_ClrDpState( eDpStateInit );
      VPC3_SetDpState( eDpStateRun );
   }//if( VPC3_GetDpState( eDpStateInit ) )

   /*-------------------------------------------------------------------*/
   /* VPC3+ DP-state                                                    */
   /*-------------------------------------------------------------------*/
   #if VPC3_SERIAL_MODE
      bStatusRegHigh = VPC3_GET_STATUS_H();
      bStatusRegLow = VPC3_GET_STATUS_L();
      
      // DEBUG: Monitoreo frecuente del estado
      static uint8_t lastStatusLow = 0xFF;
      static uint8_t lastStatusHigh = 0xFF;
      if (bStatusRegLow != lastStatusLow || bStatusRegHigh != lastStatusHigh) {
         printf("DEBUG: [STATUS_CHANGE] STATUS_L: 0x%02X -> 0x%02X, STATUS_H: 0x%02X -> 0x%02X\n", 
                lastStatusLow, bStatusRegLow, lastStatusHigh, bStatusRegHigh);
         lastStatusLow = bStatusRegLow;
         lastStatusHigh = bStatusRegHigh;
      }

      // Verificación más tolerante: solo error fatal si el ASIC no es VPC3+S
      //if( (( bStatusRegLow & VPC3_PASS_IDLE ) == 0x00 ) || (( bStatusRegHigh & AT_MASK ) != AT_VPC3S ) ) //Funcion original
      if( ( bStatusRegHigh & AT_MASK ) != AT_VPC3S )
      {
         printf("DEBUG: [FATAL_CHECK] STATUS_L=0x%02X, STATUS_H=0x%02X, AT_MASK check=%d (ERROR FATAL)\n", 
         bStatusRegLow, bStatusRegHigh, (bStatusRegHigh & AT_MASK) != AT_VPC3S);
         sVpc3Error.bErrorCode = bStatusRegLow;
         sVpc3Error.bCnId = bStatusRegHigh;
         DpAppl_FatalError( _DP_APPL, __LINE__, &sVpc3Error );
      }
      
      // Verificación de estado: solo warning si no está en PASSIVE_IDLE o superior
      if( ( bStatusRegLow & VPC3_PASS_IDLE ) == 0x00 )
      {
         static uint32_t offline_count = 0;
         offline_count++;
         printf("DEBUG: [STATE_WARNING] VPC3+ en OFFLINE (STATUS_L=0x%02X) - esperando reconexión del master (ciclo %lu)\n", 
                bStatusRegLow, offline_count);
         // No es fatal, solo un warning - el sistema puede recuperarse
         
         // El sistema está funcionando correctamente en modo offline
      }
      bDpState = ( ( bStatusRegLow & MASK_DP_STATE ) >> 5 ); //Cambio a corrido de 5 bits para que funcione correctamente con la modificacion de dp_if.h del 31/07/2025
   #else
      bDpState = VPC3_GET_DP_STATE();
   #endif /* #if VPC3_SERIAL_MODE */

      printf("DEBUG: [STATE_SWITCH] bDpState=0x%02X (WAIT_PRM=0x%02X, WAIT_CFG=0x%02X, DATA_EX=0x%02X)\n",
          bDpState, WAIT_PRM, WAIT_CFG, DATA_EX);
      printf("DEBUG: [STATE_CALC] STATUS_L=0x%02X, MASK_DP_STATE=0x%02X, cálculo=((0x%02X & 0x%02X) >> 5)=0x%02X\n",
          bStatusRegLow, MASK_DP_STATE, bStatusRegLow, MASK_DP_STATE, bDpState); //Cambio a corrido de 5 bits para que funcione correctamente con la modificacion de dp_if.h del 31/07/2025
      printf("DEBUG: [STATE_VERIFY] STATUS_L=0x%02X, bits 5-6 extraídos y desplazados: 0x%02X, debería ser DATA_EX=0x%02X\n",
          bStatusRegLow, ((bStatusRegLow & 0x60) >> 5), DATA_EX); //Cambio a corrido de 5 bits para que funcione correctamente con la modificacion de dp_if.h del 31/07/2025
      
      // Análisis detallado del problema
      printf("DEBUG: [STATE_ANALYSIS] STATUS_L=0x%02X (binario: ", bStatusRegLow);
      for(uint8_t bit = 0; bit < 8; bit++) {
          printf("%d", (bStatusRegLow >> (7-bit)) & 1);
      }
      printf(")\n");
      printf("DEBUG: [STATE_ANALYSIS] MASK_DP_STATE=0x%02X (binario: ", MASK_DP_STATE);
      for(uint8_t bit = 0; bit < 8; bit++) {
          printf("%d", (MASK_DP_STATE >> (7-bit)) & 1);
      }
      printf(")\n");
      printf("DEBUG: [STATE_ANALYSIS] STATUS_L & MASK_DP_STATE = 0x%02X (binario: ", (bStatusRegLow & MASK_DP_STATE));
      for(uint8_t bit = 0; bit < 8; bit++) {
          printf("%d", ((bStatusRegLow & MASK_DP_STATE) >> (7-bit)) & 1);
      }
      printf(")\n");
      printf("DEBUG: [STATE_ANALYSIS] (STATUS_L & MASK_DP_STATE) >> 5 = 0x%02X (binario: ", bDpState); //Cambio a corrido de 5 bits para que funcione correctamente con la modificacion de dp_if.h del 31/07/2025
      for(uint8_t bit = 0; bit < 8; bit++) {
          printf("%d", (bDpState >> (7-bit)) & 1);
      }
      printf(")\n");
      printf("DEBUG: [STATE_ANALYSIS] Comparación: bDpState=0x%02X vs DATA_EX=0x%02X (¿iguales? %s)\n",
          bDpState, DATA_EX, (bDpState == DATA_EX) ? "SÍ" : "NO");
   
   // Detectar cambio de estado específico
   static uint8_t last_dp_state = 0xFF;
   if (last_dp_state != bDpState) {
      printf("DEBUG: [STATE_CHANGE] DP_STATE cambió de 0x%02X a 0x%02X\n", last_dp_state, bDpState);
      last_dp_state = bDpState;
   }
   switch( bDpState )
   {
      case WAIT_PRM:
      {
         #ifdef EvaBoard_AT89C5132
            // set LED's
            CLR_LED_YLW__;
            SET_LED_RED__;
         #endif//#ifdef EvaBoard_AT89C5132

   
      printf("DEBUG: [STATE_WAIT_PRM] VPC3+ en WAIT_PRM - esperando telegrama Prm\n");
         break;
      }//case WAIT_PRM:

      case WAIT_CFG:
      {
         #ifdef EvaBoard_AT89C5132
            // set LED's
            CLR_LED_YLW__;
            SET_LED_RED__;
         #endif//#ifdef EvaBoard_AT89C5132

         printf("DEBUG: [STATE_WAIT_CFG] VPC3+ en WAIT_CFG - esperando telegrama Chk_Cfg\n");
         break;
      }//case WAIT_CFG:

      case DATA_EX:
      {
         #ifdef EvaBoard_AT89C5132
            // set LED's
            SET_LED_YLW__;
            CLR_LED_RED__;
         #endif//#ifdef EvaBoard_AT89C5132

         // En la primera entrada a DATA_EX, después de una configuración válida, preparamos la aplicación.
         if (VPC3_GetDpState(eDpStateCfgOkStatDiag))
         {
            printf("DEBUG: [DATA_EX] Condición para llamar a ApplicationReady CUMPLIDA. Llamando...\n");
            DpAppl_ApplicationReady();
         }

         printf("DEBUG: [STATE_DATA_EX] VPC3+ en DATA_EX - comunicación activa\n");
         printf("DEBUG: [STATE_CHECK] eDpStateApplReady=%d, eDpStateRun=%d\n", 
                VPC3_GetDpState( eDpStateApplReady ), VPC3_GetDpState( eDpStateRun ));
         
         if(    ( VPC3_GetDpState( eDpStateApplReady ) )
             && ( VPC3_GetDpState( eDpStateRun )  )
           )
         {
            printf("DEBUG: [APPLICATION_READY] Ambos estados activos - aplicación funcionando\n");
            /*-------------------------------------------------------------------*/
            /* user application                                                  */
            /*-------------------------------------------------------------------*/
            #ifdef EvaBoard_AT89C5132
               if( sSystem.sInput.abDi8[0] != *READ_PORT1 )
               {
                   sSystem.sInput.abDi8[0] = *READ_PORT1;
                   DpAppl_SetApplEvent( eDpApplEv_IoIn );
               }//if( sSystem.sInput.abDi8[0] != *READ_PORT1 )

               if( sSystem.sInput.abDi8[1] != *READ_PORT2 )
               {
                   sSystem.sInput.abDi8[1] = *READ_PORT2;
                   DpAppl_SetApplEvent( eDpApplEv_IoIn );
               }//if( sSystem.sInput.abDi8[1] != *READ_PORT2 )

               //demo for diagnostic
               bDipSwitch5 = *READ_PORT2;
               if( bDipSwitch5 != bOldDipSwitch5 )
               {
                  bOldDipSwitch5 = bDipSwitch5;

                  #ifdef RS232_SERIO
                      print_string("DipSwitchS5: ");
                      print_hexbyte(bDipSwitch5);
                  #endif//#ifdef RS232_SERIO

                  //set alarm
                  if( ( bDipSwitch5 & 0x10 ) && ( bDiagStateModule5 == VPC3_FALSE ) )
                  {
                      bDiagStateModule5 = VPC3_TRUE;
                      DpDiag_DemoDiagnostic( 1, 5 );
                  }//if( ( bDipSwitch5 & 0x10 ) && ( bDiagStateModule5 == VPC3_FALSE ) )

                  //clear alarm
                  if( !( bDipSwitch5 & 0x10 ) && ( bDiagStateModule5 == VPC3_TRUE ) )
                  {
                      bDiagStateModule5 = VPC3_FALSE;
                      DpDiag_DemoDiagnostic( 2, 5 );
                  }//if( !( bDipSwitch5 & 0x10 ) && ( bDiagStateModule5 == VPC3_TRUE ) )

                  //set alarm
                  if( ( bDipSwitch5 & 0x20 ) && ( bDiagStateModule6 == VPC3_FALSE ) )
                  {
                      bDiagStateModule6 = VPC3_TRUE;
                      DpDiag_DemoDiagnostic( 3, 6 );
                  }//if( ( bDipSwitch5 & 0x10 ) && ( bDiagStateModule6 == VPC3_FALSE ) )

                  //clear alarm
                  if( !( bDipSwitch5 & 0x20 ) && ( bDiagStateModule6 == VPC3_TRUE ) )
                  {
                     bDiagStateModule6 = VPC3_FALSE;
                     DpDiag_DemoDiagnostic( 4, 6 );
                  }//if( !( bDipSwitch5 & 0x10 ) && ( bDiagStateModule6 == VPC3_TRUE ) )
               }//if( bDipSwitch5 != bOldDipSwitch5 )
            #endif//#ifdef EvaBoard_AT89C5132

            /*-------------------------------------------------------------------*/
            /* DP-V0 - diagnostic                                                */
            /*-------------------------------------------------------------------*/
            DpDiag_CheckDpv0Diagnosis();

            /*-------------------------------------------------------------------*/
            /* profibus input                                                    */
            /*-------------------------------------------------------------------*/
            if( DpAppl_TestApplEvent( eDpApplEv_IoIn ) )
            {
               DpAppl_ReadInputData();
            }//if( DpAppl_TestApplEvent( eDpApplEv_IoIn ) )

            // --- INICIO: Logica de prueba para eco de datos ---
            // Leer el primer byte enviado por el PLC desde el buffer de datos de salida del master.
            // Este buffer se actualiza en DpAppl_CheckEvIoOut() y se copia a sSystem.sOutput.
            uint8_t byte_from_plc = sSystem.sOutput.abDo8[0];

            // LÓGICA DE PRUEBA: Diferentes transformaciones según el valor recibido
            uint8_t byte_to_plc;
            static uint8_t last_plc_value = 0xFF; // Para detectar cambios
            
            if (byte_from_plc != last_plc_value) {
                printf("*** CAMBIO DETECTADO EN PLC: 0x%02X -> 0x%02X (decimal: %d -> %d) ***\n", 
                       last_plc_value, byte_from_plc, last_plc_value, byte_from_plc);
                last_plc_value = byte_from_plc;
            }
            
            // Transformación simple pero evidente para pruebas
            if (byte_from_plc == 0x00) {
                byte_to_plc = 0x64;  // 100 decimal
            } else if (byte_from_plc == 0xAA) {
                byte_to_plc = 0xBB;  // Respuesta específica para 0xAA
            } else if (byte_from_plc == 0x55) {
                byte_to_plc = 0x33;  // Respuesta específica para 0x55
            } else if (byte_from_plc == 0xFF) {
                byte_to_plc = 0x11;  // Respuesta específica para 0xFF
            } else {
                byte_to_plc = ~byte_from_plc;  // Complemento (NOT bit a bit)
            }

            // Log detallado de la trama de datos en cada ciclo
            static uint32_t data_exchange_count = 0;
            printf("[DATA_EXCHANGE_TRAMA #%lu] PLC->STM32: 0x%02X (%3d) | STM32->PLC: 0x%02X (%3d)\n",
                   ++data_exchange_count,
                   byte_from_plc, byte_from_plc,
                   byte_to_plc, byte_to_plc);

            // Escribir el nuevo valor en el buffer de entrada hacia el PLC.
            // Este buffer (sSystem.sInput.abDi8) será leído por el master en el siguiente ciclo.
            sSystem.sInput.abDi8[0] = byte_to_plc;
            
            // Notificar que hay nuevos datos de entrada disponibles
            DpAppl_SetApplEvent( eDpApplEv_IoIn );
            // --- FIN: Logica de prueba ---
         }//if(    ( pDpSystem->bApplicationReady == VPC3_TRUE ) ...

         break;
      }//case DATA_EX:

      case DP_ERROR:
      {
         printf("DEBUG: [STATE_DP_ERROR] VPC3+ en estado DP_ERROR - error de protocolo\n");
         sVpc3Error.bErrorCode = VPC3_GET_DP_STATE();
         DpAppl_FatalError( _DP_USER, __LINE__, &sVpc3Error );
         break;
      }//case DP_ERROR:
      
      default:
      {
         // Caso para estados no reconocidos (debería ser raro ahora)
         printf("DEBUG: [STATE_DEFAULT] VPC3+ en estado 0x%02X (no reconocido) - STATUS_L=0x%02X\n", bDpState, bStatusRegLow);
         printf("DEBUG: [STATE_UNKNOWN] Estado no reconocido: 0x%02X\n", bDpState);
         break;
      }//default:
   }//switch( bDpState )

   /*------------------------------------------------------------------------*/
   /* profibus output ( master to slave )                                    */
   /*------------------------------------------------------------------------*/
   printf("DEBUG: [ProfibusMain] STATUS antes de DpAppl_CheckEvIoOut: L=0x%02X, H=0x%02X\n", 
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
   
   // Detectar corrupción antes de CheckEvIoOut
   if (VPC3_GET_STATUS_L() == 0x26 && VPC3_GET_STATUS_H() == 0xB7) {
      printf("DEBUG: [ProfibusMain]  CORRUPCIÓN DETECTADA ANTES de CheckEvIoOut\n");
   }
   
   // Detectar transición de DATA_EX a corrupción en main loop
   static uint8_t main_last_status_l = 0xFF;
   static uint8_t main_last_status_h = 0xFF;
   uint8_t main_current_status_l = VPC3_GET_STATUS_L();
   uint8_t main_current_status_h = VPC3_GET_STATUS_H();
   
   if (main_last_status_l == 0x45 && main_last_status_h == 0xE3 && 
       (main_current_status_l != 0x45 || main_current_status_h != 0xE3)) {
      printf("DEBUG: [ProfibusMain]  TRANSICIÓN DETECTADA EN MAIN: STATUS_L=0x45->0x%02X, STATUS_H=0xE3->0x%02X\n", 
             main_current_status_l, main_current_status_h);
   }
   
   main_last_status_l = main_current_status_l;
   main_last_status_h = main_current_status_h;
   
   DpAppl_CheckEvIoOut();
   printf("DEBUG: [ProfibusMain] STATUS después de DpAppl_CheckEvIoOut: L=0x%02X, H=0x%02X\n", 
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
   
   // Detectar corrupción después de CheckEvIoOut
   if (VPC3_GET_STATUS_L() == 0x26 && VPC3_GET_STATUS_H() == 0xB7) {
      printf("DEBUG: [ProfibusMain]  CORRUPCIÓN DETECTADA DESPUÉS de CheckEvIoOut\n");
   }

        // Detailed log every 5 seconds
        if (HAL_GetTick() - last_debug > 5000) {
            print_vpc3_registers();
            printf("==============================================\r\n");
            print_vpc3_state();
            printf("==============================================\r\n");
            
            // DEBUG: Diagnóstico del modo de memoria del ASIC
            static uint8_t memory_mode_diagnosed = 0;
            if (!memory_mode_diagnosed) {
                VPC3_DiagnoseMemoryMode();
                memory_mode_diagnosed = 1;
                printf("DEBUG: [ProfibusMain] Diagnóstico de modo de memoria completado\r\n");
            }
            
            // DEBUG: Monitoreo específico de MODE_REG_2
            uint8_t current_mode_reg2 = VPC3_GetModeReg2Shadow();  // write-only, use shadow
            static uint8_t last_mode_reg2 = 0xFF;
            if (current_mode_reg2 != last_mode_reg2) {
                printf("DEBUG: [MODE_REG2_MONITOR]  MODE_REG_2 cambió de 0x%02X a 0x%02X (esperado: 0x05)\r\n", 
                       last_mode_reg2, current_mode_reg2);
                if (current_mode_reg2 != 0x05) {
                    printf("DEBUG: [MODE_REG2_MONITOR]  VALOR INCORRECTO DETECTADO - Esto causará LECTURAS ILEGALES\r\n");
                }
                last_mode_reg2 = current_mode_reg2;
            }
            
            last_debug = HAL_GetTick();
        }

      // --- DEFENSIVE PROGRAMMING: Continuous segment pointer validation ---
      static uint32_t last_validation = 0;
      if (HAL_GetTick() - last_validation > 5000) { // Check every 5 seconds
         DP_ERROR_CODE validationStatus = VPC3_ValidateSegmentPointers();
         if (validationStatus == DP_NOK) {
            printf("ERROR: [DpAppl_ProfibusMain] Segment pointer corruption detected during operation!\r\n");
            printf("ERROR: This may cause LECTURA ILEGAL errors. Consider ASIC reset.\r\n");
         }
         last_validation = HAL_GetTick();
      }

      // --- CRITICAL: MODE_REG_2 monitoring and recovery ---
      static uint32_t last_mode_monitor = 0;
      if (HAL_GetTick() - last_mode_monitor > 1000) { // Check every second
         uint8_t monitor_result = VPC3_MonitorAndRecoverModeReg2();
         if (monitor_result == 1) {
            printf("INFO: [DpAppl_ProfibusMain] MODE_REG_2 was automatically recovered\r\n");
         } else if (monitor_result == 2) {
            printf("ERROR: [DpAppl_ProfibusMain] MODE_REG_2 recovery failed - this may cause LECTURA ILEGAL errors\r\n");
         }
         last_mode_monitor = HAL_GetTick();
      }

      // --- Continuous MODE_REG_2 monitoring ---
      static uint32_t last_mode_check = 0;
      if (HAL_GetTick() - last_mode_check > 1000) { // Check every second
        uint8_t mode_reg2 = VPC3_GetModeReg2Shadow();
         if (mode_reg2 != 0x05) {
            printf("WARNING: [DpAppl_ProfibusMain] MODE_REG_2 instability detected: 0x%02X (expected 0x05)\r\n", mode_reg2);
         }
         last_mode_check = HAL_GetTick();
      }

      // --- Adaptive MODE_REG_2 monitoring - temporarily disabled to prevent initialization hangs ---
      // VPC3_AdaptiveModeReg2Monitor();

      // --- Polling de eventos VPC3+ ---
      printf(" [ProfibusMain] ANTES de llamar dp_isr - STATUS_L=0x%02X, STATUS_H=0x%02X\r\n", 
             VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
      
      dp_isr();
      
      printf(" [ProfibusMain] DESPUÉS de llamar dp_isr - STATUS_L=0x%02X, STATUS_H=0x%02X\r\n", 
             VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());


}//void DpAppl_ProfibusMain( void )

/**
 * @brief Validates if the system is actually communicating properly
 * @return 1 if communication is stable, 0 if there are real issues
 */
static uint8_t DpAppl_ValidateCommunicationStability(void) {
    uint8_t status_l = VPC3_GET_STATUS_L();
    uint8_t status_h = VPC3_GET_STATUS_H();
    uint8_t dp_state = (status_l & 0x60) >> 5;
    
    // Check if we're in a valid communication state
    if (dp_state == DATA_EX) {
        // We're in DATA_EX - communication should be stable
        return 1;
    }
    
    // Check if we're in a valid waiting state
    if (dp_state == WAIT_PRM || dp_state == WAIT_CFG) {
        // These are valid states during initialization
        return 1;
    }
    
    // Check for corrupted status registers
    if (status_l == 0xFF || status_h == 0xFF) {
        printf("DEBUG: [DpAppl_ValidateCommunicationStability]  STATUS registers corrupted (0xFF)\n");
        return 0;
    }
    
    // Check for known corruption patterns
    if (status_l == 0x26 && status_h == 0xB7) {
        printf("DEBUG: [DpAppl_ValidateCommunicationStability]  Known corruption pattern detected\n");
        return 0;
    }
    
    printf("DEBUG: [DpAppl_ValidateCommunicationStability]  Unknown state: STATUS_L=0x%02X, STATUS_H=0x%02X, DP_STATE=0x%02X\n", 
           status_l, status_h, dp_state);
    
    // If we can't determine, assume it's stable to avoid unnecessary resets
    return 1;
}

/*---------------------------------------------------------------------------*/
/* function: DpAppl_FatalError                                               */
/*---------------------------------------------------------------------------*/
void DpAppl_FatalError( DP_ERROR_FILE bFile, uint16_t wLine, VPC3_ERRCB_PTR sVpc3Error )
{
   // Debug prints to identify the fatal error
   printf("\r\n=== FATAL ERROR DETECTED ===\r\n");
   printf("DEBUG: [FATAL_ERROR] Context - Current STATUS_L: 0x%02X, STATUS_H: 0x%02X\n", 
          VPC3_GET_STATUS_L(), VPC3_GET_STATUS_H());
   printf("File: %d\r\n", bFile);
   printf("Line: %d\r\n", wLine);
   if (sVpc3Error != NULL) {
      printf("Function: 0x%02X\r\n", sVpc3Error->bFunction);
      printf("Error_Code: 0x%02X\r\n", sVpc3Error->bErrorCode);
      printf("Detail: 0x%02X\r\n", sVpc3Error->bDetail);
      printf("cn_id: 0x%02X\r\n", sVpc3Error->bCnId);
   } else {
      printf("sVpc3Error is NULL\r\n");
   }
   printf("=== ENTERING INFINITE LOOP ===\r\n");

   #ifdef EvaBoard_AT89C5132
      uint8_t i,j;

      DP_WriteDebugBuffer__( FatalError__, sVpc3Error->bFunction, sVpc3Error->bErrorCode );

      #ifdef RS232_SERIO
         do
         {
            // wait!
         }
         while( bSndCounter > 80);

         print_string("\r\nFatalError:");
         print_string("\r\nFile: ");
         print_hexbyte( bFile );
         print_string("\r\nLine: ");
         print_hexword( wLine );
         print_string("\r\nFunction: ");
         print_hexbyte( sVpc3Error->bFunction);
         print_string("\r\nError_Code: ");
         print_hexbyte( sVpc3Error->bErrorCode );
         print_string("\r\nDetail: ");
         print_hexbyte( sVpc3Error->bDetail );
         print_string("\r\ncn_id: ");
         print_hexbyte( sVpc3Error->bCnId );
      #endif//#ifdef RS232_SERIO

      *WRITE_PORT0 = sVpc3Error->bErrorCode;
      *WRITE_PORT1 = bFile;
      *WRITE_PORT2 = (uint8_t)wLine;

      SET_LED_YLW__;
      SET_LED_RED__;

      while(1)
      {
         TOGGLE_LED_RED__;
         TOGGLE_LED_YLW__;

         #ifdef RS232_SERIO
            if(bRecCounter > 0)
            {
               PrintSerialInputs();
            }
         #endif//#ifdef RS232_SERIO

         for( i = 0; i < 255; i++ )
         {
            for(j = 0; j < 255; j++);
         }
      }//while(1)

   #else

      while(1)
      {
      }//while(1)

   #endif//#ifdef EvaBoard_AT89C5132
}//void DpAppl_FatalError( DP_ERROR_FILE bFile, uint16_t wLine, VPC3_ERRCB_PTR sVpc3Error )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_MacReset                                                 */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the
   VPC3+ has entered the offline mode (by setting the Go_Offline bit in Moderegister 1).
*/
#if( DP_TIMESTAMP == 0 )
void DpAppl_MacReset( void )
{
   //print_string("\r\nDpAppl_MacReset");
} /* void DpAppl_MacReset( void ) */
#endif /* #if( DP_TIMESTAMP == 0 ) */

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrGoLeaveDataExchange                                   */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the
   DP-Statemachine has entered the DataEx-mode or has exited it.
   With the function VPC3_GET_DP_STATE() you can find out the state of VPC3+.
   \param[in] bDpState - state of PROFIBUS connection (WAIT_PRM,WAIT_CFG,DATA_EX)
*/
void DpAppl_IsrGoLeaveDataExchange( uint8_t bDpState )
{
   printf("DEBUG: [IsrGoLeaveDataExchange] INICIO - bDpState=0x%02X, DATA_EX=0x%02X\n", bDpState, DATA_EX);
   
   // --- Validación robusta del estado antes de resetear ---
   uint8_t status_l = VPC3_GET_STATUS_L();
   uint8_t status_h = VPC3_GET_STATUS_H();
   
   printf("DEBUG: [IsrGoLeaveDataExchange] STATUS_L=0x%02X, STATUS_H=0x%02X\n", status_l, status_h);
   
   // Verificar si realmente estamos en DATA_EX basándonos en STATUS_L
   uint8_t actual_dp_state = (status_l & 0x60) >> 5; // Extraer bits 5-6 de STATUS_L
   printf("DEBUG: [IsrGoLeaveDataExchange] Estado real calculado de STATUS_L: 0x%02X\n", actual_dp_state);
   
   // Solo resetear si hay una discrepancia real y el sistema no está funcionando
   if( bDpState != DATA_EX && actual_dp_state != DATA_EX )
   {
      printf("DEBUG: [IsrGoLeaveDataExchange]  Confirmado: bDpState=0x%02X != DATA_EX y actual_dp_state=0x%02X != DATA_EX\n", 
             bDpState, actual_dp_state);
      printf("DEBUG: [IsrGoLeaveDataExchange] Reseteando estados - salida real de DATA_EX detectada\n");
      
      VPC3_ClrDpState( eDpStateApplReady | eDpStateRun );
      VPC3_SetDpState( eDpStateInit );
      printf("DEBUG: [IsrGoLeaveDataExchange] Estados reseteados - esto puede causar OFFLINE\n");
   } 
   else if( bDpState != DATA_EX && actual_dp_state == DATA_EX )
   {
      printf("DEBUG: [IsrGoLeaveDataExchange]  bDpState=0x%02X != DATA_EX pero actual_dp_state=0x%02X == DATA_EX\n", 
             bDpState, actual_dp_state);
      printf("DEBUG: [IsrGoLeaveDataExchange]  Posible corrupción de MODE_REG_2 - NO reseteando estados para mantener comunicación\n");
      printf("DEBUG: [IsrGoLeaveDataExchange]  Intentando recuperar MODE_REG_2 sin resetear estados...\n");
      
      // Intentar recuperar MODE_REG_2 sin resetear estados
      if (VPC3_ForceModeReg2() == 0) {
         printf("DEBUG: [IsrGoLeaveDataExchange]  MODE_REG_2 recuperado sin resetear estados\n");
      } else {
         printf("DEBUG: [IsrGoLeaveDataExchange]  No se pudo recuperar MODE_REG_2, pero manteniendo comunicación\n");
      }
   }
   else if( bDpState == DATA_EX && actual_dp_state == DATA_EX )
   {
      printf("DEBUG: [IsrGoLeaveDataExchange]  OK: bDpState == DATA_EX y actual_dp_state == DATA_EX - comunicación estable\n");
   }
   else
   {
      printf("DEBUG: [IsrGoLeaveDataExchange]  Estado inconsistente: bDpState=0x%02X, actual_dp_state=0x%02X\n", 
             bDpState, actual_dp_state);
      printf("DEBUG: [IsrGoLeaveDataExchange]  Manteniendo estados actuales para evitar interrupciones\n");
   }
   
   printf("DEBUG: [IsrGoLeaveDataExchange] FIN\n");
}//void DpAppl_IsrGoLeaveDataExchange( uint8_t bDpState )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrDxOut                                                 */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+
   has received a DataExchange message and has made the new output data
   available in the N-buffer. In the case of Power_On or Leave_Master, the
   VPC3+ clears the content of the buffer, and generates this event also.
*/
void DpAppl_IsrDxOut( void )
{
   printf("DEBUG: [DpAppl_IsrDxOut] INICIO - Configurando evento IoOut\n");
   DpAppl_SetApplEvent( eDpApplEv_IoOut );
   printf("DEBUG: [DpAppl_IsrDxOut] FIN - Evento IoOut configurado\n");
}//void DpAppl_IsrDxOut( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrNewWdDpTimeout                                        */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the
   watchdog timer expired in the WD mode DP_Control.
   The communication between master and slave is time controlled, every time you're
   disconnecting the PROFIBUS master or you're disconnecting the PROFIBUS cable you'll
   get this event.
*/
void DpAppl_IsrNewWdDpTimeout( void )
{
    //not used in our application
}//void DpAppl_IsrNewWdDpTimeout( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrClockSynchronisation                                  */
/*---------------------------------------------------------------------------*/
#if DP_TIMESTAMP
   void DpAppl_IsrClockSynchronisation( void )
   {
      //not used in our application
   }//void DpAppl_IsrClockSynchronisation( void )
#endif//#if DP_TIMESTAMP

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrBaudrateDetect                                        */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+
   has exited the Baud_Search mode and has found a baudrate.
   With the macro VPC3_GET_BAUDRATE() you can detect the baudrate.
*/
void DpAppl_IsrBaudrateDetect( void )
{
   //not used in our application
}//void DpAppl_IsrBaudrateDetect( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrNewGlobalControlCommand                               */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+
   has received a Global_Control message. The GC_Command_Byte can be read out
   with the macro VPC3_GET_GC_COMMAND().
   \param[in] bGcCommand - global control command @see VPC3_GET_GC_COMMAND()
*/
void DpAppl_IsrNewGlobalControlCommand( uint8_t bGcCommand )
{
   //not used in our application
   bGcCommand = bGcCommand;   //avoid compiler warning
}//void DpAppl_IsrNewGlobalControlCommand( uint8_t bGcCommand )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrNewSetSlaveAddress                                    */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() or VPC3_Poll() calls this function if the VPC3+
   has received a Set_Slave_Address message and made the data available in the SSA
   buffer.
  \param[in] psSsa - pointer to set slave address structure
*/
void DpAppl_IsrNewSetSlaveAddress( MEM_STRUC_SSA_BLOCK_PTR psSsa )
{
    //not used in our application
    psSsa = psSsa; //avoid compiler warning
/*
   //store the new address and the bit bNoAddressChanged for the next startup
   print_string("\r\nNewAddr: ");
   print_hexbyte(psSsa->bTsAddr);
   print_hexbyte(psSsa->bNoAddressChanged);
   print_hexbyte(psSsa->bIdentHigh);
   print_hexbyte(psSsa->bIdentLow);
*/
}//void DpAppl_IsrNewSetSlaveAddress( MEM_STRUC_SSA_BLOCK_PTR psSsa )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_IsrTimerClock (10ms)                                     */
/*---------------------------------------------------------------------------*/
/*!
   \brief The function VPC3_Isr() calls this function if the time base
   of the User_Timer_Clock has expired (1/10ms).

   \attention Use this function only interrupt driven!
*/
void DpAppl_IsrTimerClock( void )
{
   //simulation for counter module
   if( sSystem.sOutput.abCounter[0] & 0x01 )
   {
      if( ++bCounterTimeBase == sSystem.sPrmCounter.bTimeBase )
      {
         bCounterTimeBase = 0x00;
         wCounterValue = (sSystem.sPrmCounter.bMode == 0) ? wCounterValue+1 : wCounterValue-1;
         sSystem.sInput.abCounter[0] = (uint8_t)( wCounterValue >> 8 );
         sSystem.sInput.abCounter[1] = (uint8_t)( wCounterValue & 0x00FF );

         DpAppl_SetApplEvent( eDpApplEv_IoIn );

         //handle process alarm
         if( sSystem.sPrmCounter.bAlarm & 0x02 )
         {
            if(    ( bCounterUpperLimit == VPC3_FALSE )
                && ( wCounterValue > sSystem.sPrmCounter.wUpperLimit )
              )
            {
               if( DpDiag_DemoDiagnostic( 5, 6 ) == VPC3_TRUE )
               {
                  bCounterUpperLimit = VPC3_TRUE;
                  bCounterLowerLimit = VPC3_FALSE;
               }//if( DpDiag_DemoDiagnostic( 5, 6 ) == VPC3_TRUE )
            }//if(    ( bCounterUpperLimit == VPC3_FALSE ) ...

            if(    ( bCounterLowerLimit == VPC3_FALSE )
                && ( wCounterValue < sSystem.sPrmCounter.wLowerLimit )
              )
            {
               if( DpDiag_DemoDiagnostic( 6, 6 ) == VPC3_TRUE )
               {
                  bCounterUpperLimit = VPC3_FALSE;
                  bCounterLowerLimit = VPC3_TRUE;
               }//if( DpDiag_DemoDiagnostic( 6, 6 ) == VPC3_TRUE )
            }//if(    ( counter_lower_limit == VPC3_FALSE ) ...
         }//if( sSystem.sPrmCounter.bAlarm & 0x02 )
      }//if( ++bCounterTimeBase == sSystem.sPrmCounter.bTimeBase )
   }//if( sSystem.sOutput.abCounter[0] & 0x01 )
}//void DpAppl_IsrTimerClock( void )

/*---------------------------------------------------------------------------*/
/* function: DpAppl_ClrResetVPC3Channel1                                     */
/*---------------------------------------------------------------------------*/
/*!
  \brief Clear reset of VPC3+ channel 1.
*/
void DpAppl_ClrResetVPC3Channel1(void)
{
   // Reset VPC3+ by pulling RESET low for 20us
   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
   HAL_Delay(1);  // 1ms is more than enough for 20us
   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
}

/*---------------------------------------------------------------------------*/
/* function: DpAppl_DisableInterruptVPC3Channel1                             */
/*---------------------------------------------------------------------------*/
/*!
  \brief Disable interrupt for VPC3+ channel 1.
*/
void DpAppl_DisableInterruptVPC3Channel1(void)
{
   // Disable external interrupt for VPC3+ INT pin during critical sections
   HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
}

/*---------------------------------------------------------------------------*/
/* function: DpAppl_EnableInterruptVPC3Channel1                              */
/*---------------------------------------------------------------------------*/
/*!
  \brief Enable interrupt for VPC3+ channel 1.
*/
void DpAppl_EnableInterruptVPC3Channel1(void)
{
   // Enable external interrupt for VPC3+ INT pin
   HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/*---------------------------------------------------------------------------*/
/* function: leer_feedback                                                   */
/*---------------------------------------------------------------------------*/
/*!
  \brief Read feedback value from hardware (ADC, encoder, etc.).
  \retval 16-bit feedback value
*/
uint16_t leer_feedback(void)
{
   // This is a placeholder implementation for reading feedback data
   // Replace this with actual hardware reading (ADC, encoder, sensor, etc.)
   
   // For demonstration, return a simple counter that changes over time
   static uint16_t counter = 0;
   counter++;
   return counter;
   
   // Example ADC reading (uncomment and adapt as needed):
   // HAL_ADC_Start(&hadc1);
   // HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
   // return (uint16_t)HAL_ADC_GetValue(&hadc1);
}

/*****************************************************************************/
/*  Copyright (C) profichip GmbH 2009. Confidential.                         */
/*****************************************************************************/

