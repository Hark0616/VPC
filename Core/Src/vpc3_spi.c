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
#define VPC3_CSS_CYCLES     200   // tCSS: CS setup (2μs) - increased for reliability
#define VPC3_CSH_CYCLES     200   // tCSH: CS hold (2μs) - increased for reliability

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
    // DEBUG: Monitoreo de escrituras a registros críticos
    if (wAddress == 0x0C) {  // MODE_REG_2 (bVpc3WoModeReg2)
        printf("DEBUG: [Vpc3Write] ⚠️ ESCRITURA a MODE_REG_2 (0x0C): 0x%02X\r\n", bData);
        if (bData != 0x05) {
            printf("DEBUG: [Vpc3Write] ⚠️ ADVERTENCIA: MODE_REG_2 se está configurando a 0x%02X en lugar de 0x05\r\n", bData);
            printf("DEBUG: [Vpc3Write] ⚠️ VALOR INCORRECTO DETECTADO - Esto causará LECTURAS ILEGALES\r\n");
        }
        
        // Stack trace para identificar quién está escribiendo
        printf("DEBUG: [Vpc3Write] 📍 Stack trace - Escritura a MODE_REG_2 desde:\r\n");
        printf("DEBUG: [Vpc3Write] 📍 - Función llamadora: %s\r\n", __FUNCTION__);
        printf("DEBUG: [Vpc3Write] 📍 - Línea: %d\r\n", __LINE__);
        printf("DEBUG: [Vpc3Write] 📍 - Archivo: %s\r\n", __FILE__);
        
        // SPI Protocol Debug
        printf("DEBUG: [Vpc3Write] 🔧 SPI Protocol Debug - MODE_REG_2 write:\r\n");
        printf("DEBUG: [Vpc3Write] 🔧 - Instruction: 0x%02X (OPC_WR_BYTE)\r\n", OPC_WR_BYTE);
        printf("DEBUG: [Vpc3Write] 🔧 - Address: 0x%04X (High: 0x%02X, Low: 0x%02X)\r\n", wAddress, (uint8_t)(wAddress >> 8), (uint8_t)wAddress);
        printf("DEBUG: [Vpc3Write] 🔧 - Data: 0x%02X\r\n", bData);
    }
    
    if (wAddress == 0x09) {  // MODE_REG_1_R (bVpc3WoModeReg1_R)
        printf("DEBUG: [Vpc3Write] ESCRITURA a MODE_REG_1_R (0x09): 0x%02X\r\n", bData);
    }
    
    if (wAddress == 0x07 || wAddress == 0x08) {  // MODE_REG_0_H, MODE_REG_0_L
        printf("DEBUG: [Vpc3Write] ESCRITURA a MODE_REG_0_%c (0x%02X): 0x%02X\r\n", 
               (wAddress == 0x07) ? 'H' : 'L', wAddress, bData);
    }
    
    if (wAddress == 0x0B) {  // MODE_REG_3
        printf("DEBUG: [Vpc3Write] ESCRITURA a MODE_REG_3 (0x0B): 0x%02X\r\n", bData);
    }

    // Maintain shadow for write-only MODE_REG_2
    if (wAddress == bVpc3WoModeReg2) {
        g_vpc3_mode_reg2_shadow = bData;
    }

    // Use retry mechanism for critical registers
    uint8_t maxRetries = (wAddress == 0x0C) ? 5 : 1; // More retries for MODE_REG_2
    uint8_t result = vpc3_write_with_retry(wAddress, bData, maxRetries);
    
    if (result != 0 && wAddress == 0x0C) {
        printf("DEBUG: [Vpc3Write] ❌ ERROR: MODE_REG_2 write failed after %d retries!\r\n", maxRetries);
    }
}

/**
 * @brief Lee un byte del VPC3+.
 * @note La firma (retorna uint8_t, un parámetro) coincide con la declaración 'extern' en dp_inc.h.
 */
uint8_t Vpc3Read(VPC3_ADR wAddress) {
    // --- Logging detallado de acceso a memoria ---
    VPC3_LogMemoryAccess(wAddress, "READ", "Vpc3Read");
    VPC3_LogDiagnosticAccess(wAddress, "READ");
    
    // --- Programación Defensiva: Verificación de Límites Adaptativa ---
    if (!VPC3_IsAddressValid(wAddress)) {
        printf("\r\n--- LECTURA ILEGAL DETECTADA EN Vpc3Read ---\r\n");
        printf("ERROR: Intento de leer un byte desde una dirección (0x%04X) que está fuera de los límites de la RAM.\r\n",
               (unsigned int)wAddress);
        printf("Esta es una condición FATAL. Se retornará 0x00 para indicar el error.\r\n");
        printf("--- FIN LECTURA ILEGAL ---\r\n");
        return 0x00; // Retornar un valor de error claro
    }
    // --- Fin de la Verificación ---

    // 1. Iniciar transacción - CS bajo
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET);
    vpc3_cs_delay(); // Delay para tCSS
    
    DpAppl_DisableInterruptVPC3Channel1();
    
    // 2. Enviar byte de instrucción (0x13 para READ BYTE)
    vpc3_spi_transfer(OPC_RD_BYTE);
    
    // 3. Enviar dirección de 16 bits (MSB primero)
    vpc3_spi_transfer((uint8_t)(wAddress >> 8)); // Byte alto de la dirección
    vpc3_spi_transfer((uint8_t)wAddress);        // Byte bajo de la dirección
    
    // 4. Continuar generando 8 pulsos de reloj para leer el byte de datos
    uint8_t bData = vpc3_spi_transfer(0x00); // Enviar dummy byte, recibir datos
    
    // 5. Finalizar transacción - CS alto
    vpc3_cs_delay(); // Delay para tCSH
    HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET);
    
    DpAppl_EnableInterruptVPC3Channel1();
    
    return bData;
}

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
            printf("DEBUG: [VPC3_DetectActualMemoryMode] ⚠️ Timeout reading MODE_REG_2\r\n");
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
        printf("DEBUG: [VPC3_DetectActualMemoryMode] ❌ Failed to read MODE_REG_2 after %d attempts\r\n", max_attempts);
        return 2; // Unknown/corrupted mode
    }
    
    uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
    
    printf("DEBUG: [VPC3_DetectActualMemoryMode] MODE_REG_2=0x%02X, bit7=%d\r\n", mode_reg_2, bit_7);
    
    if (bit_7 == 0) {
        printf("DEBUG: [VPC3_DetectActualMemoryMode] ✅ Detected 2KB mode\r\n");
        return 0; // 2KB mode
    } else if (bit_7 == 1) {
        printf("DEBUG: [VPC3_DetectActualMemoryMode] ⚠️ Detected 4KB mode (unexpected)\r\n");
        return 1; // 4KB mode
    } else {
        printf("DEBUG: [VPC3_DetectActualMemoryMode] ❌ Unknown/corrupted mode\r\n");
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
            printf("DEBUG: [VPC3_GetActualRamLength] ⚠️ Using conservative 2KB limit\r\n");
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
        printf("DEBUG: [VPC3_IsAddressValid] ❌ Address 0x%04X exceeds RAM limit 0x%04X\r\n", 
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
        printf("🔍 [VPC3_LogMemoryAccess] === ANÁLISIS DE DIRECCIÓN DE MEMORIA ===\r\n");
        printf("🔍 [VPC3_LogMemoryAccess] Operación: %s\r\n", operation);
        printf("🔍 [VPC3_LogMemoryAccess] Función llamadora: %s\r\n", function_name);
        printf("🔍 [VPC3_LogMemoryAccess] Dirección: 0x%04X (%u decimal)\r\n", address, address);
        
        // Análisis de rangos de memoria
        printf("🔍 [VPC3_LogMemoryAccess] Rangos de memoria:\r\n");
        printf("🔍 [VPC3_LogMemoryAccess] - 2KB: 0x0000-0x07FF (0-2047)\r\n");
        printf("🔍 [VPC3_LogMemoryAccess] - 4KB: 0x0000-0x0FFF (0-4095)\r\n");
        printf("🔍 [VPC3_LogMemoryAccess] - ASIC_RAM_LENGTH: 0x%04X (%u)\r\n", ASIC_RAM_LENGTH, ASIC_RAM_LENGTH);
        
        // Verificar si la dirección está en rango
        if (address < ASIC_RAM_LENGTH) {
            printf("🔍 [VPC3_LogMemoryAccess] ✅ Dirección VÁLIDA dentro del rango\r\n");
        } else {
            printf("🔍 [VPC3_LogMemoryAccess] ❌ Dirección FUERA DE RANGO!\r\n");
            printf("🔍 [VPC3_LogMemoryAccess] ❌ Excede ASIC_RAM_LENGTH en %u bytes\r\n", address - ASIC_RAM_LENGTH);
        }
        
        // Análisis especial para dirección 2046
        if (address == 2046) {
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 DIRECCIÓN CRÍTICA 2046 DETECTADA!\r\n");
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 Esta es la dirección del registro de diagnóstico\r\n");
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 En modo 2KB: 2046 está en el ÚLTIMO byte (0x7FE)\r\n");
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 En modo 4KB: 2046 está en el rango medio (0x7FE)\r\n");
            
            // Verificar MODE_REG_2 para determinar el modo actual
            uint8_t mode_reg_2 = VPC3_GetModeReg2Shadow();
            uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 MODE_REG_2 actual: 0x%02X, bit7=%d\r\n", mode_reg_2, bit_7);
            printf("🔍 [VPC3_LogMemoryAccess] 🚨 Modo detectado: %s\r\n", bit_7 ? "4KB" : "2KB");
            
            if (bit_7 == 0 && address >= 2048) {
                printf("🔍 [VPC3_LogMemoryAccess] 🚨 CONFLICTO: Modo 2KB pero dirección >= 2048\r\n");
            }
        }
        
        printf("🔍 [VPC3_LogMemoryAccess] === FIN ANÁLISIS ===\r\n");
    }
}

/**
 * @brief Logs specific analysis for diagnostic register access
 * @param address The diagnostic register address
 * @param operation "READ" or "WRITE"
 */
static void VPC3_LogDiagnosticAccess(VPC3_ADR address, const char* operation) {
    if (address == 2046 || address == 0x7FE) {
        printf("🚨 [VPC3_LogDiagnosticAccess] === ACCESO AL REGISTRO DE DIAGNÓSTICO ===\r\n");
        printf("🚨 [VPC3_LogDiagnosticAccess] Operación: %s\r\n", operation);
        printf("🚨 [VPC3_LogDiagnosticAccess] Dirección: 0x%04X (%u decimal)\r\n", address, address);
        
        // Análisis del modo de memoria actual
        uint8_t mode_reg_2 = VPC3_GetModeReg2Shadow();
        uint8_t bit_7 = (mode_reg_2 >> 7) & 0x01;
        
        printf("🚨 [VPC3_LogDiagnosticAccess] MODE_REG_2: 0x%02X (bit7=%d)\r\n", mode_reg_2, bit_7);
        printf("🚨 [VPC3_LogDiagnosticAccess] Modo actual: %s\r\n", bit_7 ? "4KB" : "2KB");
        printf("🚨 [VPC3_LogDiagnosticAccess] ASIC_RAM_LENGTH: 0x%04X (%u)\r\n", ASIC_RAM_LENGTH, ASIC_RAM_LENGTH);
        
        // Verificar si hay conflicto
        if (bit_7 == 0 && address >= 2048) {
            printf("🚨 [VPC3_LogDiagnosticAccess] ❌ CONFLICTO DETECTADO!\r\n");
            printf("🚨 [VPC3_LogDiagnosticAccess] ❌ Modo 2KB pero dirección >= 2048\r\n");
            printf("🚨 [VPC3_LogDiagnosticAccess] ❌ Esto puede causar desbordamiento de memoria\r\n");
        } else if (bit_7 == 1 && address < 2048) {
            printf("🚨 [VPC3_LogDiagnosticAccess] ⚠️ Posible subutilización de memoria 4KB\r\n");
        } else {
            printf("🚨 [VPC3_LogDiagnosticAccess] ✅ Configuración coherente\r\n");
        }
        
        printf("🚨 [VPC3_LogDiagnosticAccess] === FIN ANÁLISIS DIAGNÓSTICO ===\r\n");
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
    const uint16_t MAX_TRANSFER_SIZE = 240; // Tamaño máximo seguro para transferencias SPI
    VPC3_ADR addr = (VPC3_ADR)(uintptr_t)pToVpc3Memory;
    
    uint16_t remaining = wLength;
    uint16_t offset = 0;
    
    while (remaining > 0) {
        uint16_t transferSize = (remaining > MAX_TRANSFER_SIZE) ? MAX_TRANSFER_SIZE : remaining;
        uint8_t cmd[3] = {OPC_WR_ARRAY, (uint8_t)((addr + offset) >> 8), (uint8_t)(addr + offset)};

        DpAppl_DisableInterruptVPC3Channel1();
        VPC3_CS_LOW();
        HAL_SPI_Transmit(&hspi1, cmd, 3, VPC3_TIMEOUT_MS);
        HAL_SPI_Transmit(&hspi1, (uint8_t*)pLocalMemory + offset, transferSize, VPC3_TIMEOUT_MS);
        VPC3_CS_HIGH();
        DpAppl_EnableInterruptVPC3Channel1();
        
        offset += transferSize;
        remaining -= transferSize;
    }
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

    const uint16_t MAX_TRANSFER_SIZE = 240; // Tamaño máximo seguro para transferencias SPI
    uint16_t remaining = wLength;
    uint16_t offset = 0;
    
    // --- NUEVAS LÍNEAS DE DEBUG ---
    //printf("DEBUG: [CopyFromVpc3] Lectura de 0x%04X, longitud %d\r\n", (unsigned int)addr, wLength);
    // -----------------------------

    while (remaining > 0) {
        uint16_t transferSize = (remaining > MAX_TRANSFER_SIZE) ? MAX_TRANSFER_SIZE : remaining;
        uint8_t cmd[3] = {OPC_RD_ARRAY, (uint8_t)((addr + offset) >> 8), (uint8_t)(addr + offset)};

        DpAppl_DisableInterruptVPC3Channel1();
        VPC3_CS_LOW();
        HAL_SPI_Transmit(&hspi1, cmd, 3, VPC3_TIMEOUT_MS);
        HAL_SPI_Receive(&hspi1, (uint8_t*)pLocalMemory + offset, transferSize, VPC3_TIMEOUT_MS);
        VPC3_CS_HIGH();
        DpAppl_EnableInterruptVPC3Channel1();
        
        offset += transferSize;
        remaining -= transferSize;
    }
    // --- NUEVAS LÍNEAS DE DEBUG ---
    //printf("DEBUG: [CopyFromVpc3] Datos leídos: ");
    //for(int k=0; k<wLength; k++) {
    //    printf("0x%02X ", ((uint8_t*)pLocalMemory)[k]);
    //}
    //printf("\r\n");
    // -----------------------------
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

