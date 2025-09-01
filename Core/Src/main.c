/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"
#include "platform.h"
#include "DpAppl.h"
#include "DpCfg.h"
#include "dp_if.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define INT_REG_H 0x03
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
void print_vpc3_registers(void);
void print_vpc3_state(void);
void debug_vpc3_reset_status(void);
void dp_isr(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Custom printf redirection to UART
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	return ch;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
	/* USER CODE BEGIN 1 */
	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
  MX_SPI1_Init();

  MX_USART2_UART_Init();
	MX_SPI1_Init();

	/* USER CODE BEGIN 2 */
    
    printf("\r\n");
    printf("==============================================================================\r\n");
    printf("    VPC3+S PROFIBUS Slave - Standard Initialization Approach\r\n");
    printf("==============================================================================\r\n");
    printf("\r\n");
    printf("Using standard DpAppl_ProfibusInit() for complete initialization\r\n");
    printf("\r\n");

    // Perform hardware reset of VPC3+S (Active-High)
    printf("Performing VPC3+S hardware reset (Active-High)...\r\n");
    VPC3_RESET_ASSERT();     // drive reset HIGH (active)
    HAL_Delay(100);          // hold reset for 100ms
    VPC3_RESET_RELEASE();    // release reset (LOW)
    HAL_Delay(500);          // allow ASIC to stabilize
    
    // 2. Verificar respuesta después del reset
    printf("Verificando respuesta del VPC3+ después del reset...\r\n");
    uint8_t reset_attempts = 0;
    uint8_t vpc3_responding = 0;
    
    // Intentar múltiples veces hasta que el VPC3+ responda
    while (reset_attempts < 10 && !vpc3_responding) {
        reset_attempts++;
        printf("Intento %d/10 de verificación post-reset...\r\n", reset_attempts);
        
        // Leer registros de estado
        uint8_t status_lo_check = Vpc3Read(0x04);
        uint8_t status_hi_check = Vpc3Read(0x05);
        
        printf("  STATUS_L: 0x%02X, STATUS_H: 0x%02X\r\n", status_lo_check, status_hi_check);
        
        // Verificar si el VPC3+ está respondiendo (no todos los registros en 0xFF)
        if (status_lo_check != 0xFF && status_hi_check != 0xFF) {
            printf("  ✓ VPC3+ responde correctamente en intento %d\r\n", reset_attempts);
            vpc3_responding = 1;
        } else {
            printf("  ✗ VPC3+ no responde (0xFF), esperando 200ms...\r\n");
            HAL_Delay(200); // Esperar 200ms entre intentos
        }
    }
    
    if (!vpc3_responding) {
        printf("ERROR: VPC3+ no responde después de 10 intentos de reset\r\n");
        printf("Verificar:\r\n");
        printf("  1. Conexiones de alimentación (VCC, GND)\r\n");
        printf("  2. Conexiones SPI (MOSI, MISO, SCK, CS)\r\n");
        printf("  3. Configuración del pin A1 en IOC\r\n");
        printf("  4. Voltaje de alimentación estable en 3.3V\r\n");
        
        // Continuar de todas formas para debug
        printf("Continuando con inicialización para diagnóstico...\r\n");
    } else {
        printf("✓ VPC3+ responde correctamente después del reset\r\n");
    }
    printf("Hardware reset completed.\r\n");
    
    // Check initial VPC3+ status after hardware reset
    printf("Reading initial VPC3+ status...\r\n");
    extern uint8_t Vpc3Read(uint16_t address);
    extern void Vpc3Write(uint16_t address, uint8_t data);
    
    uint8_t status_lo_initial = Vpc3Read(0x04);
    uint8_t status_hi_initial = Vpc3Read(0x05);
    printf("Status Register (hi,low): 0x%02X%02X\r\n", status_hi_initial, status_lo_initial);
    printf("\r\n");
    
    // Verificación de estado después del reset por software
    if (status_lo_initial == 0xFF || status_hi_initial == 0xFF) {
        printf("ADVERTENCIA: VPC3+ aún no responde correctamente después del reset por software\r\n");
        printf("Esto puede indicar un problema de configuración del pin A1 o conexiones SPI\r\n");
    }
    
    // Force VPC3+ to OFFLINE state before initialization
    printf("Forcing VPC3+ to OFFLINE state...\r\n");
    uint8_t status_before = Vpc3Read(0x04);
    printf("Status before GO_OFFLINE: 0x%02X\r\n", status_before);
    
    // SOLUCIÓN 2: Verificar que el VPC3+ responda antes de enviar comandos
    if (status_before == 0xFF) {
        printf("ERROR: VPC3+ no responde antes de GO_OFFLINE\r\n");
        printf("No se puede continuar con la inicialización\r\n");
        
        // Entrar en bucle infinito para debug
        printf("=== ENTERING DEBUG LOOP ===\r\n");
        while(1) {
            uint8_t debug_status = Vpc3Read(0x04);
            printf("DEBUG LOOP - STATUS_L: 0x%02X\r\n", debug_status);
            HAL_Delay(1000);
        }
    }
    
    // Send GO_OFFLINE command
    Vpc3Write(0x08, 0x04);  // VPC3_GO_OFFLINE command
    HAL_Delay(50);          // Wait for command to take effect (aumentado de 20ms)
    
    uint8_t status_after = Vpc3Read(0x04);
    printf("Status after GO_OFFLINE: 0x%02X\r\n", status_after);
    
    if (status_after & 0x01) {
        printf("WARNING: VPC3+ still not in OFFLINE state, trying again...\r\n");
        // Try a different approach - send RESET command first
        Vpc3Write(0x08, 0x02);  // VPC3_RESET command
        HAL_Delay(100);         // Aumentado de 50ms
        Vpc3Write(0x08, 0x04);  // VPC3_GO_OFFLINE command
        HAL_Delay(50);          // Aumentado de 20ms
        
        status_after = Vpc3Read(0x04);
        printf("Status after RESET+GO_OFFLINE: 0x%02X\r\n", status_after);
    }
    
    if (!(status_after & 0x01)) {
        printf("SUCCESS: VPC3+ is now in OFFLINE state\r\n");
    } else {
        printf("WARNING: VPC3+ still not in OFFLINE state\r\n");
        printf("Continuando de todas formas para diagnóstico...\r\n");
    }
    printf("\r\n");

    // Initialize PROFIBUS using the standard approach
    // This function handles all the required initialization steps:
    // - DpPrm_Init()    - Parameter handling initialization  
    // - DpCfg_Init()    - Configuration handling initialization
    // - DpDiag_Init()   - Diagnostics initialization
    // - VPC3_MemoryTest() - Memory verification
    // - VPC3_Initialization() - VPC3 chip initialization
    // - VPC3_Start()    - Start the VPC3 chip
    
    printf("Initializing PROFIBUS with configuration:\r\n");
	print_vpc3_registers();
    printf("  - Slave Address: %d (0x%02X)\r\n", DP_ADDR, DP_ADDR);
    printf("  - Ident Number:  0x%04X\r\n", IDENT_NR);
    printf("  - GSD File:      cafe.gsd\r\n");
    printf("\r\n");
    
    DpAppl_ProfibusInit();
    printf("PROFIBUS initialization completed successfully!\r\n");
    printf("\r\n");

    // Fuerza la activación del buffer de diagnóstico
    // printf("DEBUG: Llamando a DpDiag_ResetDiagnosticBuffer() para forzar diagnóstico inicial...\r\n");
    // extern void DpDiag_ResetDiagnosticBuffer(void);
    // DpDiag_ResetDiagnosticBuffer();
    // printf("DEBUG: DpDiag_ResetDiagnosticBuffer() llamada.\r\n");
    
    HAL_Delay(2000);          // Wait for command to take effect and stabilize VPC3+
    // Check final VPC3+ status after complete initialization
    printf("Reading final VPC3+ status...\r\n");
	print_vpc3_registers();
    uint8_t status_lo_final = Vpc3Read(0x04);
    uint8_t status_hi_final = Vpc3Read(0x05);
    printf("Status Register (hi,low): 0x%02X%02X\r\n", status_hi_final, status_lo_final);
    printf("\r\n");
    
    printf("Entering main operation loop...\r\n");
    printf("The PROFIBUS slave is now ready for master communication.\r\n");
    printf("\r\n");
    printf("================= PLC CONFIGURATION REQUIRED =================\r\n");
    printf("1. Load GSD file:     cafe.gsd\r\n");
    printf("2. Set slave address: %d (0x%02X)\r\n", DP_ADDR, DP_ADDR);
    printf("3. Connect PROFIBUS cable and activate communication\r\n");
    printf("================================================================\r\n");
    printf("\r\n");

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    	uint32_t last_heartbeat = 0;
    uint32_t last_debug = 0;

    // State machine for application-level status tracking
    enum AppState { APP_WAITING_CFG, APP_IN_DATA_EX };
    static enum AppState app_state = APP_WAITING_CFG;

        while (1)
        {
                /* USER CODE END WHILE */

                /* USER CODE BEGIN 3 */

        uint8_t int_reg_h = Vpc3Read(INT_REG_H);
        if (int_reg_h != 0)
        {
            printf("[INT_POLL] INT_REG_H=0x%02X -> dp_isr()\r\n", int_reg_h);
            dp_isr();
        }

        // Process PROFIBUS communication
        DpAppl_ProfibusMain();
        HAL_Delay(1);

        // Check PROFIBUS state from VPC3+
        uint8_t current_status_l = Vpc3Read(0x04);
        // Note: MASK_DP_STATE and DATA_EX come from dp_if.h
        // The state is in bits 6 and 5 of STATUS_L
        uint8_t dp_state = (current_status_l & MASK_DP_STATE) >> 5;

        // --- Application State Machine ---
        if (app_state == APP_WAITING_CFG && dp_state == DATA_EX) {
            printf("\r\n[APP_STATE_CHANGE] Entered DATA_EXCHANGE mode.\r\n\r\n");
            app_state = APP_IN_DATA_EX;
        } else if (app_state == APP_IN_DATA_EX && dp_state != DATA_EX) {
            printf("\r\n[APP_STATE_CHANGE] WARNING: Left DATA_EXCHANGE unexpectedly!\r\n");
            printf("STATUS_L is now 0x%02X, DP_STATE is %d.\r\n", current_status_l, dp_state);
            printf("Returning to wait for new configuration.\r\n\r\n");
            app_state = APP_WAITING_CFG;
        }

        // Detailed log every 5 seconds
        if (HAL_GetTick() - last_debug > 5000) {
            print_vpc3_registers();
            printf("==============================================\r\n");
            print_vpc3_state();
            printf("==============================================\r\n");
            
            // Añadir debug de reset si hay problemas
            uint8_t current_status_l = Vpc3Read(0x04);
            if (current_status_l == 0xFF) {
                printf("⚠️  PROBLEMA DETECTADO - Llamando debug de reset...\r\n");
                debug_vpc3_reset_status();
            }
            
            last_debug = HAL_GetTick();
        }

        // Heartbeat every 2 seconds
        if (HAL_GetTick() - last_heartbeat > 2000) {
            printf("[HEARTBEAT] El sistema está vivo. STATUS_L: 0x%02X\r\n", current_status_l);
            if (app_state == APP_WAITING_CFG) {
                printf("[INFO] State: Waiting for Set_Prm from master...\r\n");
            } else {
                printf("[INFO] State: DATA_EXCHANGE active.\r\n");
            }
            
            // *** MONITOREO DEL BUFFER DE LOG DEL ISR ***
            uint8_t available_entries = isr_log_get_available_entries();
            uint8_t overflow_flag = isr_log_has_overflow();
            
            if (available_entries > 0) {
                printf("[ISR_LOG] %d entradas disponibles en buffer de log\r\n", available_entries);
                
                // Leer y mostrar las últimas entradas del ISR
                char log_buffer[64];
                uint8_t entries_to_show = (available_entries > 5) ? 5 : available_entries;
                
                printf("[ISR_LOG] Últimas %d entradas del ISR:\r\n", entries_to_show);
                for (int i = 0; i < entries_to_show; i++) {
                    if (isr_log_read_entry(log_buffer, sizeof(log_buffer))) {
                        printf("[ISR_LOG] %s\r\n", log_buffer);
                    }
                }
                
                // Limpiar overflow si ocurrió
                if (overflow_flag) {
                    printf("[ISR_LOG] ⚠️ OVERFLOW detectado - limpiando flag\r\n");
                    isr_log_clear_overflow();
                }
            } else {
                printf("[ISR_LOG] No hay entradas en buffer de log\r\n");
            }
            
            last_heartbeat = HAL_GetTick();
        }
	}
	/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  /* USER CODE BEGIN SPI1_Init 0 */
  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;  // Slower for better reliability
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* USER CODE END SPI1_Init 2 */
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */
  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */
  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  /* USER CODE END USART2_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level - VPC3 CS and RESET pins */
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);     // CS inactive (high)
    VPC3_RESET_RELEASE();                                           // RESET inactive (LOW, active-high)

    /*Configure GPIO pin : VPC3_CS_PIN */
    GPIO_InitStruct.Pin = VPC3_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(VPC3_CS_PORT, &GPIO_InitStruct);

    /*Configure GPIO pin : VPC3_RESET_PIN */
    GPIO_InitStruct.Pin = VPC3_RESET_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(VPC3_RESET_PORT, &GPIO_InitStruct);

    /*Configure GPIO pin : VPC3_INT_PIN */
    GPIO_InitStruct.Pin = VPC3_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(VPC3_INT_PORT, &GPIO_InitStruct);

  /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

void print_vpc3_registers(void) {
    uint8_t status_lo = Vpc3Read(0x04);
    uint8_t status_hi = Vpc3Read(0x05);
    uint8_t mode_reg0_l = Vpc3Read(0x06);
    uint8_t mode_reg0_h = Vpc3Read(0x07);
    uint8_t mode_reg1   = Vpc3Read(0x15);
    uint8_t mode_reg2   = VPC3_GetModeReg2Shadow();
    uint8_t int_reg_l   = Vpc3Read(0x02);
    uint8_t int_reg_h   = Vpc3Read(0x03);
    uint8_t cfg_ptr = Vpc3Read(0x34);

    printf("VPC3+S REGISTERS:\r\n");
    printf("  STATUS_L   (0x04): 0x%02X\r\n", status_lo);
    printf("  STATUS_H   (0x05): 0x%02X\r\n", status_hi);
    printf("  MODE_REG_0_L (0x06): 0x%02X\r\n", mode_reg0_l);
    printf("  MODE_REG_0_H (0x07): 0x%02X\r\n", mode_reg0_h);
    printf("  MODE_REG_1   (0x15): 0x%02X (se actualiza automáticamente)\r\n", mode_reg1);
    printf("  MODE_REG_2   (shadow 0x0C): 0x%02X (puede cambiar según estado)\r\n", mode_reg2);
    printf("  INT_REG_L    (0x02): 0x%02X\r\n", int_reg_l);
    printf("  INT_REG_H    (0x03): 0x%02X\r\n", int_reg_h);
    printf("  CFG_PTR    (0x34): 0x%02X\r\n", cfg_ptr);
    printf("\r\n");
}

void print_vpc3_state(void) {
    uint8_t status_l = Vpc3Read(0x04);

    printf("PROFIBUS STATE: STATUS_L = 0x%02X -> ", status_l);

    switch (status_l & 0x0C) {
        case 0x00:
            if (status_l & VPC3_PASS_IDLE) {
                printf("PASSIVE_IDLE");
            } else {
                printf("OFFLINE");
            }
            break;
        case 0x04:
            printf("WAIT_PRM (waiting for parameterization)");
            break;
        case 0x08:
            printf("WAIT_CFG (waiting for configuration)");
            break;
        case 0x0C:
            printf("DATA_EX (data exchange active)");
            break;
        default:
            printf("UNKNOWN");
            break;
    }

    printf("\r\n");
}

// Nueva función de debug para diagnóstico de reset
void debug_vpc3_reset_status(void) {
    printf("=== DEBUG VPC3+ RESET STATUS ===\r\n");

    // Verificar respuesta básica del chip
    uint8_t status_l = Vpc3Read(0x04);
    uint8_t status_h = Vpc3Read(0x05);

    printf("STATUS_L (0x04): 0x%02X\r\n", status_l);
    printf("STATUS_H (0x05): 0x%02X\r\n", status_h);

    if (status_l == 0xFF && status_h == 0xFF) {
        printf("❌ PROBLEMA DETECTADO: VPC3+ no responde (0xFF en todos los registros)\r\n");
        printf("Posibles causas:\r\n");
        printf("  1. Chip no alimentado correctamente\r\n");
        printf("  2. Conexiones SPI incorrectas\r\n");
        printf("  3. Reset no funcionando\r\n");
        printf("  4. Chip defectuoso\r\n");
    } else if (status_l == 0x00 && status_h == 0xE3) {
        printf("✅ ESTADO CORRECTO: VPC3+ en OFFLINE (0x00, 0xE3)\r\n");
    } else {
        printf("⚠️  ESTADO INTERMEDIO: VPC3+ responde pero no en estado esperado\r\n");
        printf("   STATUS_L=0x%02X, STATUS_H=0x%02X\r\n", status_l, status_h);
    }

    // Verificar otros registros críticos
    uint8_t mode_reg0_l = Vpc3Read(0x06);
    uint8_t mode_reg0_h = Vpc3Read(0x07);
    uint8_t mode_reg2 = Vpc3Read(0x0C);

    printf("MODE_REG_0_L (0x06): 0x%02X\r\n", mode_reg0_l);
    printf("MODE_REG_0_H (0x07): 0x%02X\r\n", mode_reg0_h);
    printf("MODE_REG_2 (0x0C): 0x%02X\r\n", mode_reg2);

    printf("================================\r\n");
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
