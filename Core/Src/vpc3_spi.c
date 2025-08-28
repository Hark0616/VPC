#include "platform.h"
#include "dp_inc.h"
#include "main.h"

// --- Declaraciones de funciones de logging ---
static void VPC3_LogMemoryAccess(VPC3_ADR address, const char* operation, const char* function_name);
static void VPC3_LogDiagnosticAccess(VPC3_ADR address, const char* operation);
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"

/* External SPI handle declaration */
extern SPI_HandleTypeDef hspi1;

/* Shadow copy for write-only MODE_REG_2 (0x0C) */
static volatile uint8_t g_vpc3_mode_reg2_shadow = INIT_VPC3_MODE_REG_2;
uint8_t VPC3_GetModeReg2Shadow(void) { return g_vpc3_mode_reg2_shadow; }

/* VPC3+S SPI Instruction Set */
#define OPC_WR_BYTE   0x12  // Write Byte
#define OPC_RD_BYTE   0x13  // Read Byte
#define OPC_WR_ARRAY  0x02  // Write Array
#define OPC_RD_ARRAY  0x03  // Read Array

/* Configuration */
#define VPC3_TIMEOUT_MS     100    // 100ms timeout for SPI
#define VPC3_MAX_ARRAY_LEN  256   // Maximum array transfer length

/* VPC3+S timing requirements (cycles @100MHz) */
#define VPC3_CSS_CYCLES     400   // tCSS: CS setup (~4μs) - extra margin
#define VPC3_CSH_CYCLES     400   // tCSH: CS hold  (~4μs) - extra margin

/* Status codes */
typedef enum {
    VPC3_OK = 0,
    VPC3_ERROR_SPI_HDR,
    VPC3_ERROR_SPI_DATA,
    VPC3_ERROR_PARAM,
    VPC3_ERROR_LENGTH
} VPC3_Status;

// Prototipos de las funciones de bajo nivel que necesita la librería
void Vpc3Write(VPC3_ADR wAddress, uint8_t bData);
uint8_t Vpc3Read(VPC3_ADR wAddress);
void Vpc3MemSet(VPC3_ADR wAddress, uint8_t bValue, uint16_t wLength);
uint8_t Vpc3MemCmp(VPC3_UNSIGNED8_PTR pToVpc3Memory1, VPC3_UNSIGNED8_PTR pToVpc3Memory2, uint16_t wLength);
void CopyToVpc3(VPC3_UNSIGNED8_PTR pToVpc3Memory, MEM_UNSIGNED8_PTR pLocalMemory, uint16_t wLength);
void CopyFromVpc3(MEM_UNSIGNED8_PTR pLocalMemory, VPC3_UNSIGNED8_PTR pToVpc3Memory, uint16_t wLength);
VPC3_Status Vpc3ReadArray(uint8_t *dst_local, VPC3_UNSIGNED8_PTR src_vpc, uint16_t len);
VPC3_Status Vpc3WriteArray(VPC3_UNSIGNED8_PTR dst_vpc, const uint8_t *src_local, uint16_t len);

/* External Profibus interrupt control */
extern void DpAppl_DisableInterruptVPC3Channel1(void);
extern void DpAppl_EnableInterruptVPC3Channel1(void);

/*-------------------------------------------------------------------------*//**
 * @brief Delay for CS setup/hold timing
 *//*-------------------------------------------------------------------------*/
static inline void vpc3_cs_delay(void) {
    for (volatile int i = 0; i < VPC3_CSS_CYCLES; i++);
}

/*-------------------------------------------------------------------------*//**
 * @brief SPI transfer of a single byte with proper timing and error handling
 * @param byte Byte to send
 * @return Received byte
 *//*-------------------------------------------------------------------------*/
static uint8_t vpc3_spi_transfer(uint8_t byte) {
    uint8_t rx;
    HAL_StatusTypeDef status;
    
    // Use HAL_SPI_TransmitReceive for better timing control
    status = HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, VPC3_TIMEOUT_MS);
    
    if (status != HAL_OK) {
        printf("ERROR: SPI transfer failed with status %d\r\n", status);
        // Don't return 0xFF as it might be a valid data value
        // Instead, try to recover the SPI bus
        HAL_SPI_Abort(&hspi1);
        HAL_Delay(1);
        return 0x00; // Return 0x00 as a safer default
    }
    
    return rx;
}

/**
 * @brief Robust write function with multiple retry attempts
 * @param wAddress Address to write to
 * @param bData Data to write
 * @param maxRetries Maximum number of retry attempts
 * @return 0 on success, 1 on failure
 */
static uint8_t vpc3_write_with_retry(VPC3_ADR wAddress, uint8_t bData, uint8_t maxRetries) {
    uint8_t attempts = 0;
    uint8_t verify_value;
    
    while (attempts < maxRetries) {
        attempts++;
        
        // 1. Iniciar transacción - CS bajo
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
        vpc3_cs_delay(); // Delay para tCSS
        
        DpAppl_DisableInterruptVPC3Channel1();
        
        // 2. Enviar byte de instrucción (0x12 para WRITE BYTE)
        vpc3_spi_transfer(OPC_WR_BYTE);
        
        // 3. Enviar dirección de 16 bits (MSB primero)
        vpc3_spi_transfer((uint8_t)(wAddress >> 8)); // Byte alto de la dirección
        vpc3_spi_transfer((uint8_t)wAddress);        // Byte bajo de la dirección
        
        // 4. Enviar el byte de datos
        vpc3_spi_transfer(bData);
        
        // 5. Finalizar transacción - CS alto
        vpc3_cs_delay(); // Delay para tCSH
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
        
        DpAppl_EnableInterruptVPC3Channel1();
        
        // 6. Verificación con delay
        HAL_Delay(2); // Longer delay for stabilization

        // 7. Read back and verify
        if (wAddress == bVpc3WoModeReg2) {
            // MODE_REG_2 is write-only – accept write without readback
            return 0;
        } else {
            verify_value = Vpc3Read(wAddress);
            if (verify_value == bData) {
                return 0; // Success
            }
        }
        
        // If verification failed, try again
        HAL_Delay(1);
    }
    
    return 1; // Failure after all retries
}

/**
 * @brief Escribe un byte en el VPC3+ siguiendo el protocolo exacto del manual.
 * @note La firma (void, dos parámetros) coincide con la declaración 'extern' en dp_inc.h.
 */
void Vpc3Write(VPC3_ADR wAddress, uint8_t bData) {
    // --- VERIFICACIÓN DE LÍMITES ---
    if (!VPC3_IsAddressValid(wAddress)) {
        printf("\r\n--- ESCRITURA ILEGAL DETECTADA EN Vpc3Write ---\r\n");
        printf("ERROR: Intento de escribir un byte (0x%02X) a una dirección (0x%04X) fuera de límites.\r\n", 
               bData, (unsigned int)wAddress);
        printf("--- FIN ESCRITURA ILEGAL ---\r\n");
        return;
    }

    // --- PROTOCOLO ULTRA-BÁSICO: OPC_WR_BYTE (0x12) ---
    
    // 1. Deshabilitar IRQ durante toda la operación
    DpAppl_DisableInterruptVPC3Channel1();
    
    // 2. CS bajo con delay extra
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
    for (volatile int d = 0; d < 2000; d++); // 20μs tCSS
    
    // 3. Enviar comando WRITE BYTE (0x12)
    uint8_t cmd = OPC_WR_BYTE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, VPC3_TIMEOUT_MS);
    
    // 4. Enviar dirección MSB primero
    uint8_t addr_high = (uint8_t)(wAddress >> 8);
    HAL_SPI_Transmit(&hspi1, &addr_high, 1, VPC3_TIMEOUT_MS);
    
    // 5. Enviar dirección LSB
    uint8_t addr_low = (uint8_t)(wAddress & 0xFF);
    HAL_SPI_Transmit(&hspi1, &addr_low, 1, VPC3_TIMEOUT_MS);
    
    // 6. Enviar dato
    HAL_SPI_Transmit(&hspi1, &bData, 1, VPC3_TIMEOUT_MS);
    
    // 7. CS alto con delay
    for (volatile int d = 0; d < 1000; d++); // 10μs tCSH
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
    
    // 8. Rehabilitar IRQ
    DpAppl_EnableInterruptVPC3Channel1();
}

/**
 * @brief Lee un byte del VPC3+.
 * @note La firma (retorna uint8_t, un parámetro) coincide con la declaración 'extern' en dp_inc.h.
 */
// Helper: lectura con dirección normal o con bytes de dirección invertidos
static uint8_t vpc3_read_impl(uint16_t addr, uint8_t swapped, uint8_t *out_data) {
    uint8_t a_hi = (uint8_t)(addr >> 8);
    uint8_t a_lo = (uint8_t)addr;
    uint8_t tx_hdr[3];
    if (swapped) {
        tx_hdr[0] = OPC_RD_BYTE;
        tx_hdr[1] = a_lo;
        tx_hdr[2] = a_hi;
    } else {
        tx_hdr[0] = OPC_RD_BYTE;
        tx_hdr[1] = a_hi;
        tx_hdr[2] = a_lo;
    }

    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
    vpc3_cs_delay();
    DpAppl_DisableInterruptVPC3Channel1();
    
    // Enviar instrucción y dirección
    (void)vpc3_spi_transfer(tx_hdr[0]);
    (void)vpc3_spi_transfer(tx_hdr[1]);
    (void)vpc3_spi_transfer(tx_hdr[2]);
    // Leer dos dummies para estabilizar; usar el segundo
    (void)vpc3_spi_transfer(0x00);
    uint8_t data = vpc3_spi_transfer(0x00);

    vpc3_cs_delay();
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
    DpAppl_EnableInterruptVPC3Channel1();
    // Pequeño espaciamiento entre transacciones
    for (volatile int i = 0; i < 600; i++);

    if (out_data) *out_data = data;
    return data;
}

uint8_t Vpc3Read(VPC3_ADR wAddress) {
    // --- VERIFICACIÓN DE LÍMITES ---
    if (!VPC3_IsAddressValid(wAddress)) {
        printf("\r\n--- LECTURA ILEGAL DETECTADA EN Vpc3Read ---\r\n");
        printf("ERROR: Intento de leer un byte desde una dirección (0x%04X) fuera de límites.\r\n", (unsigned int)wAddress);
        printf("--- FIN LECTURA ILEGAL ---\r\n");
        return 0x00;
    }

    // --- PROTOCOLO ULTRA-BÁSICO: OPC_RD_BYTE (0x13) ---
    uint8_t value = 0;
    
    // 1. Deshabilitar IRQ durante toda la operación
    DpAppl_DisableInterruptVPC3Channel1();
    
    // 2. CS bajo con delay extra
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
    for (volatile int d = 0; d < 2000; d++); // 20μs tCSS
    
    // 3. Enviar comando READ BYTE (0x13)
    uint8_t cmd = OPC_RD_BYTE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, VPC3_TIMEOUT_MS);
    
    // 4. Enviar dirección MSB primero (como especifica el manual)
    uint8_t addr_high = (uint8_t)(wAddress >> 8);
    HAL_SPI_Transmit(&hspi1, &addr_high, 1, VPC3_TIMEOUT_MS);
    
    // 5. Enviar dirección LSB
    uint8_t addr_low = (uint8_t)(wAddress & 0xFF);
    HAL_SPI_Transmit(&hspi1, &addr_low, 1, VPC3_TIMEOUT_MS);
    
    // 6. Leer byte de datos (el VPC3+S envía el byte después de la dirección)
    HAL_SPI_Receive(&hspi1, &value, 1, VPC3_TIMEOUT_MS);
    
    // 7. CS alto con delay
    for (volatile int d = 0; d < 1000; d++); // 10μs tCSH
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
    
    // 8. Rehabilitar IRQ
    DpAppl_EnableInterruptVPC3Channel1();
    
    return value;
}

// Dump de registros 0x00..0x1F con prueba normal vs swapped
void VPC3_DebugDumpRegs_00_1F(void) {
    printf("\r\n[DUMP] === VPC3 Regs 0x00..0x1F (normal vs swapped) ===\r\n");
    printf("[DUMP] SPI Mode: CPOL=%d CPHA=%d Prescaler=%lu\r\n",
           hspi1.Init.CLKPolarity, hspi1.Init.CLKPhase, (unsigned long)hspi1.Init.BaudRatePrescaler);
    for (uint16_t a=0x0000; a<=0x001F; a++) {
        uint8_t n = vpc3_read_impl(a, 0, NULL);
        uint8_t s = vpc3_read_impl(a, 1, NULL);
        printf("[DUMP] 0x%02X: normal=0x%02X swapped=0x%02X\r\n", (unsigned int)a, n, s);
    }
    printf("[DUMP] === END ===\r\n\r\n");
}

// Probe SPI CPOL/CPHA and alternative 1-byte read path using array opcode
#ifndef VPC3_SPI_PROBE
void VPC3_SpiModeProbe(void) {
    // Probe desactivado por defecto en producción
}
#else
void VPC3_SpiModeProbe(void) {
    SPI_HandleTypeDef backup = hspi1; // backup current config
    struct { uint32_t pol; uint32_t pha; const char* name; } modes[] = {
        {SPI_POLARITY_LOW,  SPI_PHASE_1EDGE, "Mode0"},
        {SPI_POLARITY_LOW,  SPI_PHASE_2EDGE, "Mode1"},
        {SPI_POLARITY_HIGH, SPI_PHASE_1EDGE, "Mode2"},
        {SPI_POLARITY_HIGH, SPI_PHASE_2EDGE, "Mode3"},
    };

    printf("\r\n[SPI_PROBE] === Probing CPOL/CPHA and opcode variants ===\r\n");
    for (unsigned i=0;i<4;i++) {
        HAL_SPI_DeInit(&hspi1);
        hspi1.Init.CLKPolarity = modes[i].pol;
        hspi1.Init.CLKPhase    = modes[i].pha;
        HAL_SPI_Init(&hspi1);

        uint8_t sl = vpc3_read_impl(0x0004, 0, NULL);
        uint8_t sh = vpc3_read_impl(0x0005, 0, NULL);
        uint8_t cr = vpc3_read_impl(0x0008, 0, NULL);

        // Alternative: use array read (OPC_RD_ARRAY) len=1 for each address
        uint8_t alt_sl=0, alt_sh=0, alt_cr=0;
        (void)Vpc3ReadArray(&alt_sl, (VPC3_UNSIGNED8_PTR)(uintptr_t)0x0004, 1);
        (void)Vpc3ReadArray(&alt_sh, (VPC3_UNSIGNED8_PTR)(uintptr_t)0x0005, 1);
        (void)Vpc3ReadArray(&alt_cr, (VPC3_UNSIGNED8_PTR)(uintptr_t)0x0008, 1);

        printf("[SPI_PROBE] %s: STATUS_L=0x%02X STATUS_H=0x%02X CTRL=0x%02X | ALT: SL=0x%02X SH=0x%02X CTRL=0x%02X\r\n",
               modes[i].name, sl, sh, cr, alt_sl, alt_sh, alt_cr);
    }
    // restore
    HAL_SPI_DeInit(&hspi1);
    hspi1 = backup;
    HAL_SPI_Init(&hspi1);
    printf("[SPI_PROBE] === End probe, SPI restored ===\r\n\r\n");
}
#endif

/**
 * @brief Detects the actual memory mode from MODE_REG_2 and adjusts calculations accordingly
 * @return 0 for 2KB mode, 1 for 4KB mode, 2 for unknown/corrupted mode
 */
uint8_t VPC3_DetectActualMemoryMode(void) {
    // Add timeout protection to prevent hanging
    uint32_t start_time = HAL_GetTick();
    uint8_t mode_reg_2;
    uint8_t attempts = 0;
    const uint8_t max_attempts = 3;
    
    while (attempts < max_attempts) {
        attempts++;
        
        // Check for timeout (100ms max)
        if (HAL_GetTick() - start_time > 100) {
            printf("DEBUG: [VPC3_DetectActualMemoryMode]  Timeout reading MODE_REG_2\r\n");
            return 2; // Unknown/corrupted mode
        }
        
        mode_reg_2 = VPC3_GetModeReg2Shadow();
        
        // Check if we got a valid response (not 0xFF which indicates no response)
        if (mode_reg_2 != 0xFF) {
            break;
        }
        
        // Small delay before retry
        HAL_Delay(1);
    }
    
    if (attempts >= max_attempts) {
        printf("DEBUG: [VPC3_DetectActualMemoryMode]  Failed to read MODE_REG_2 after %d attempts\r\n", max_attempts);
        return 2; // Unknown/corrupted mode
    }
    
    uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
    
    printf("DEBUG: [VPC3_DetectActualMemoryMode] MODE_REG_2=0x%02X, bit7=%d\r\n", mode_reg_2, bit_7);
    
    if (bit_7 == 0) {
        printf("DEBUG: [VPC3_DetectActualMemoryMode]  Detected 2KB mode\r\n");
        return 0; // 2KB mode
    } else if (bit_7 == 1) {
        printf("DEBUG: [VPC3_DetectActualMemoryMode]  Detected 4KB mode (unexpected)\r\n");
        return 1; // 4KB mode
    } else {
        printf("DEBUG: [VPC3_DetectActualMemoryMode]  Unknown/corrupted mode\r\n");
        return 2; // Unknown
    }
}

/**
 * @brief Gets the actual RAM length based on detected memory mode
 * @return Actual RAM length in bytes
 */
uint16_t VPC3_GetActualRamLength(void) {
    uint8_t actual_mode = VPC3_DetectActualMemoryMode();
    
    switch (actual_mode) {
        case 0: // 2KB mode
            return 0x0800; // 2KB
        case 1: // 4KB mode
            return 0x1000; // 4KB
        default: // Unknown/corrupted
            printf("DEBUG: [VPC3_GetActualRamLength]  Using conservative 2KB limit\r\n");
            return 0x0800; // Conservative fallback
    }
}

/**
 * @brief Enhanced boundary check that adapts to actual memory mode
 * @param address Address to check
 * @return 1 if address is valid, 0 if invalid
 */
uint8_t VPC3_IsAddressValid(VPC3_ADR address) {
    // Use a conservative approach during initialization to prevent hangs
    // For now, use the configured ASIC_RAM_LENGTH instead of trying to detect it
    uint16_t ram_limit = ASIC_RAM_LENGTH;
    
    if (address >= ram_limit) {
        printf("DEBUG: [VPC3_IsAddressValid]  Address 0x%04X exceeds RAM limit 0x%04X\r\n", 
               address, ram_limit);
        return 0;
    }
    
    return 1;
}

/**
 * @brief Logs detailed memory address analysis for debugging
 * @param address The address being accessed
 * @param operation "READ" or "WRITE"
 * @param function_name The calling function name
 */
static void VPC3_LogMemoryAccess(VPC3_ADR address, const char* operation, const char* function_name) {
    // Solo mostrar logs para direcciones fuera de rango o críticas
    if (address >= ASIC_RAM_LENGTH || address == 2046) {
        printf(" [VPC3_LogMemoryAccess] === ANÁLISIS DE DIRECCIÓN DE MEMORIA ===\r\n");
        printf(" [VPC3_LogMemoryAccess] Operación: %s\r\n", operation);
        printf(" [VPC3_LogMemoryAccess] Función llamadora: %s\r\n", function_name);
        printf(" [VPC3_LogMemoryAccess] Dirección: 0x%04X (%u decimal)\r\n", address, address);
        
        // Análisis de rangos de memoria
        printf(" [VPC3_LogMemoryAccess] Rangos de memoria:\r\n");
        printf(" [VPC3_LogMemoryAccess] - 2KB: 0x0000-0x07FF (0-2047)\r\n");
        printf(" [VPC3_LogMemoryAccess] - 4KB: 0x0000-0x0FFF (0-4095)\r\n");
        printf(" [VPC3_LogMemoryAccess] - ASIC_RAM_LENGTH: 0x%04X (%u)\r\n", ASIC_RAM_LENGTH, ASIC_RAM_LENGTH);
        
        // Verificar si la dirección está en rango
        if (address < ASIC_RAM_LENGTH) {
            printf(" [VPC3_LogMemoryAccess]  Dirección VÁLIDA dentro del rango\r\n");
        } else {
            printf(" [VPC3_LogMemoryAccess]  Dirección FUERA DE RANGO!\r\n");
            printf(" [VPC3_LogMemoryAccess]  Excede ASIC_RAM_LENGTH en %u bytes\r\n", address - ASIC_RAM_LENGTH);
        }
        
        // Análisis especial para dirección 2046
        if (address == 2046) {
            printf(" [VPC3_LogMemoryAccess]  DIRECCIÓN CRÍTICA 2046 DETECTADA!\r\n");
            printf(" [VPC3_LogMemoryAccess]  Esta es la dirección del registro de diagnóstico\r\n");
            printf(" [VPC3_LogMemoryAccess]  En modo 2KB: 2046 está en el ÚLTIMO byte (0x7FE)\r\n");
            printf(" [VPC3_LogMemoryAccess]  En modo 4KB: 2046 está en el rango medio (0x7FE)\r\n");
            
            // Verificar MODE_REG_2 para determinar el modo actual
            uint8_t mode_reg_2 = VPC3_GetModeReg2Shadow();
            uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
            printf(" [VPC3_LogMemoryAccess]  MODE_REG_2 actual: 0x%02X, bit7=%d\r\n", mode_reg_2, bit_7);
            printf(" [VPC3_LogMemoryAccess]  Modo detectado: %s\r\n", bit_7 ? "4KB" : "2KB");
            
            if (bit_7 == 0 && address >= 2048) {
                printf(" [VPC3_LogMemoryAccess]  CONFLICTO: Modo 2KB pero dirección >= 2048\r\n");
            }
        }
        
        printf(" [VPC3_LogMemoryAccess] === FIN ANÁLISIS ===\r\n");
    }
}

/**
 * @brief Logs specific analysis for diagnostic register access
 * @param address The diagnostic register address
 * @param operation "READ" or "WRITE"
 */
static void VPC3_LogDiagnosticAccess(VPC3_ADR address, const char* operation) {
    if (address == 2046 || address == 0x7FE) {
        printf(" [VPC3_LogDiagnosticAccess] === ACCESO AL REGISTRO DE DIAGNÓSTICO ===\r\n");
        printf(" [VPC3_LogDiagnosticAccess] Operación: %s\r\n", operation);
        printf(" [VPC3_LogDiagnosticAccess] Dirección: 0x%04X (%u decimal)\r\n", address, address);
        
        // Análisis del modo de memoria actual
        uint8_t mode_reg_2 = VPC3_GetModeReg2Shadow();
        uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
        
        printf(" [VPC3_LogDiagnosticAccess] MODE_REG_2: 0x%02X (bit7=%d)\r\n", mode_reg_2, bit_7);
        printf(" [VPC3_LogDiagnosticAccess] Modo actual: %s\r\n", bit_7 ? "4KB" : "2KB");
        printf(" [VPC3_LogDiagnosticAccess] ASIC_RAM_LENGTH: 0x%04X (%u)\r\n", ASIC_RAM_LENGTH, ASIC_RAM_LENGTH);
        
        // Verificar si hay conflicto
        if (bit_7 == 0 && address >= 2048) {
            printf(" [VPC3_LogDiagnosticAccess]  CONFLICTO DETECTADO!\r\n");
            printf(" [VPC3_LogDiagnosticAccess]  Modo 2KB pero dirección >= 2048\r\n");
            printf(" [VPC3_LogDiagnosticAccess]  Esto puede causar desbordamiento de memoria\r\n");
        } else if (bit_7 == 1 && address < 2048) {
            printf(" [VPC3_LogDiagnosticAccess]  Posible subutilización de memoria 4KB\r\n");
        } else {
            printf(" [VPC3_LogDiagnosticAccess]  Configuración coherente\r\n");
        }
        
        printf(" [VPC3_LogDiagnosticAccess] === FIN ANÁLISIS DIAGNÓSTICO ===\r\n");
    }
}

// Tus funciones de array ya son eficientes, las mantenemos.
VPC3_Status Vpc3WriteArray(VPC3_UNSIGNED8_PTR dst_vpc, const uint8_t *src_local, uint16_t len) {
    VPC3_ADR addr = (VPC3_ADR)(uintptr_t)dst_vpc;
    uint8_t cmd[3] = {OPC_WR_ARRAY, (uint8_t)(addr >> 8), (uint8_t)addr};

    DpAppl_DisableInterruptVPC3Channel1();
    VPC3_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 3, VPC3_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, (uint8_t*)src_local, len, VPC3_TIMEOUT_MS);
    VPC3_CS_HIGH();
    DpAppl_EnableInterruptVPC3Channel1();

    return (status == HAL_OK) ? VPC3_OK : VPC3_ERROR_SPI_DATA;
}

VPC3_Status Vpc3ReadArray(uint8_t *dst_local, VPC3_UNSIGNED8_PTR src_vpc, uint16_t len) {
    VPC3_ADR addr = (VPC3_ADR)(uintptr_t)src_vpc;
    uint8_t cmd[3] = {OPC_RD_ARRAY, (uint8_t)(addr >> 8), (uint8_t)addr};

    DpAppl_DisableInterruptVPC3Channel1();
    VPC3_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 3, VPC3_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi1, dst_local, len, VPC3_TIMEOUT_MS);
    VPC3_CS_HIGH();
    DpAppl_EnableInterruptVPC3Channel1();

    return (status == HAL_OK) ? VPC3_OK : VPC3_ERROR_SPI_DATA;
}

/**
 * @brief Escribe un array de bytes en el VPC3+. Esta función es llamada por el macro CopyToVpc3.
 */
void CopyToVpc3(VPC3_UNSIGNED8_PTR pToVpc3Memory, MEM_UNSIGNED8_PTR pLocalMemory, uint16_t wLength) {
    // --- PROTOCOLO ULTRA-BÁSICO: Escritura byte a byte ---
    VPC3_ADR base = (VPC3_ADR)(uintptr_t)pToVpc3Memory;
    
    // Deshabilitar IRQ durante toda la operación
    DpAppl_DisableInterruptVPC3Channel1();
    
    for (uint16_t i = 0; i < wLength; i++) {
        VPC3_ADR addr = base + i;
        uint8_t data = pLocalMemory[i];
        
        // 1. CS bajo con delay extra
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 2000; d++); // 20μs tCSS
        
        // 2. Enviar comando WRITE BYTE (0x12)
        uint8_t cmd = OPC_WR_BYTE;
        HAL_SPI_Transmit(&hspi1, &cmd, 1, VPC3_TIMEOUT_MS);
        
        // 3. Enviar dirección MSB primero
        uint8_t addr_high = (uint8_t)(addr >> 8);
        HAL_SPI_Transmit(&hspi1, &addr_high, 1, VPC3_TIMEOUT_MS);
        
        // 4. Enviar dirección LSB
        uint8_t addr_low = (uint8_t)(addr & 0xFF);
        HAL_SPI_Transmit(&hspi1, &addr_low, 1, VPC3_TIMEOUT_MS);
        
        // 5. Enviar dato
        HAL_SPI_Transmit(&hspi1, &data, 1, VPC3_TIMEOUT_MS);
        
        // 6. CS alto con delay
        for (volatile int d = 0; d < 1000; d++); // 10μs tCSH
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
        
        // 7. Pausa entre bytes para estabilidad
        for (volatile int d = 0; d < 500; d++); // 5μs entre bytes
        
        // 8. Pausa mayor cada 16 bytes
        if (((i + 1) % 16) == 0) {
            HAL_Delay(1);
        }
    }
    
    // Rehabilitar IRQ
    DpAppl_EnableInterruptVPC3Channel1();
}

/**
 * @brief Lee un array de bytes del VPC3+. Esta es llamada por el macro CopyFromVpc3.
 */
void CopyFromVpc3(MEM_UNSIGNED8_PTR pLocalMemory, VPC3_UNSIGNED8_PTR pToVpc3Memory, uint16_t wLength) {
    VPC3_ADR addr = (VPC3_ADR)(uintptr_t)pToVpc3Memory;

    // --- Programación Defensiva: Verificación de Límites Conservativa ---
    uint16_t ram_limit = ASIC_RAM_LENGTH;
    if ( (addr + wLength) > ram_limit ) {
        printf("\r\n--- LECTURA ILEGAL DETECTADA EN CopyFromVpc3 ---\r\n");
        printf("ERROR: Intento de leer un bloque de %u bytes desde una dirección (0x%04X) que excede los límites de la RAM (0x%04X).\r\n",
               wLength, (unsigned int)addr, ram_limit);
        printf("Esta es una condición FATAL que probablemente cause un crash. La operación de copia será OMITIDA.\r\n");
        printf("--- FIN LECTURA ILEGAL ---\r\n");
        // Opcional: llenar el buffer local con un patrón de error para que sea obvio en el debug
        memset(pLocalMemory, 0xDE, wLength); // 0xDE for 'dead' or 'deleted'
        return; // Detener la ejecución para prevenir el crash del SPI
    }
    // --- Fin de la Verificación ---

    // --- PROTOCOLO ULTRA-BÁSICO: Lectura byte a byte ---
    
    // Deshabilitar IRQ durante toda la operación
    DpAppl_DisableInterruptVPC3Channel1();
    
    for (uint16_t i = 0; i < wLength; i++) {
        VPC3_ADR current_addr = addr + i;
        
        // 1. CS bajo con delay extra
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 2000; d++); // 20μs tCSS
        
        // 2. Enviar comando READ BYTE (0x13)
        uint8_t cmd = OPC_RD_BYTE;
        HAL_SPI_Transmit(&hspi1, &cmd, 1, VPC3_TIMEOUT_MS);
        
        // 3. Enviar dirección MSB primero
        uint8_t addr_high = (uint8_t)(current_addr >> 8);
        HAL_SPI_Transmit(&hspi1, &addr_high, 1, VPC3_TIMEOUT_MS);
        
        // 4. Enviar dirección LSB
        uint8_t addr_low = (uint8_t)(current_addr & 0xFF);
        HAL_SPI_Transmit(&hspi1, &addr_low, 1, VPC3_TIMEOUT_MS);
        
        // 5. Leer byte de datos
        uint8_t data;
        HAL_SPI_Receive(&hspi1, &data, 1, VPC3_TIMEOUT_MS);
        
        // 6. Guardar en memoria local
        pLocalMemory[i] = data;
        
        // 7. CS alto con delay
        for (volatile int d = 0; d < 1000; d++); // 10μs tCSH
        HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
        
        // 8. Pausa entre bytes para estabilidad
        for (volatile int d = 0; d < 500; d++); // 5μs entre bytes
        
        // 9. Pausa mayor cada 16 bytes
        if (((i + 1) % 16) == 0) {
            HAL_Delay(1);
        }
    }
    
    // Rehabilitar IRQ
    DpAppl_EnableInterruptVPC3Channel1();
}

/**
 * @brief Llena una sección de memoria del VPC3+ con un valor.
 */
void Vpc3MemSet(VPC3_ADR wAddress, uint8_t bValue, uint16_t wLength) {
    const uint16_t BLOCK_SIZE = 256; // Tamaño de bloque para transferencias grandes
    uint8_t buffer[BLOCK_SIZE];
    
    // Llenar el buffer con el valor deseado
    memset(buffer, bValue, BLOCK_SIZE);
    
    // Procesar en bloques
    uint16_t remaining = wLength;
    VPC3_ADR currentAddr = wAddress;
    
    while (remaining > 0) {
        uint16_t blockSize = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        CopyToVpc3((VPC3_UNSIGNED8_PTR)(uintptr_t)currentAddr, buffer, blockSize);
        
        currentAddr += blockSize;
        remaining -= blockSize;
    }
}

/**
 * @brief Compara dos bloques de memoria del VPC3+. La librería espera que devuelva 0 si son iguales.
 */
uint8_t Vpc3MemCmp(VPC3_UNSIGNED8_PTR pToVpc3Memory1, VPC3_UNSIGNED8_PTR pToVpc3Memory2, uint16_t wLength) {
    uint8_t buffer1[256];
    uint8_t buffer2[256];

    if (wLength > sizeof(buffer1)) {
        wLength = sizeof(buffer1); // Limitar para evitar desbordamiento del stack
    }

    // Lee ambos bloques a la memoria local y luego compara
    CopyFromVpc3(buffer1, pToVpc3Memory1, wLength);
    CopyFromVpc3(buffer2, pToVpc3Memory2, wLength);

    return (uint8_t)memcmp(buffer1, buffer2, wLength);
}

