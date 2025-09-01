#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

/*---------------------------------------------------------------------------*/
/* Debug Configuration for ISR Safety                                        */
/*---------------------------------------------------------------------------*/

// *** CRÍTICO: Configuración para evitar printf() bloqueantes en el ISR ***
// Si se define DEBUG_ISR_NO_BLOCK, se permiten printf() en funciones llamadas desde el ISR
// Si NO se define, TODOS los printf() están deshabilitados para evitar latencias críticas
// 
// RECOMENDACIÓN: NO definir DEBUG_ISR_NO_BLOCK en producción para evitar "Not reachable"

// #define DEBUG_ISR_NO_BLOCK  // DESCOMENTAR SOLO para debugging (NO en producción)

/*---------------------------------------------------------------------------*/
/* Macros de Seguridad para el ISR                                           */
/*---------------------------------------------------------------------------*/

// Por defecto, deshabilitar TODAS las funciones stdio para evitar latencias en el ISR
#ifndef DEBUG_ISR_NO_BLOCK
    // Macros no-op que eliminan funciones stdio en compilación
    #define printf(...)      ((void)0)  // No-op macro para printf()
    #define fprintf(...)     ((void)0)  // No-op macro para fprintf()
    #define puts(...)        ((void)0)  // No-op macro para puts()
    #define putchar(...)     ((void)0)  // No-op macro para putchar()
    #define fputs(...)       ((void)0)  // No-op macro para fputs()
    #define fputc(...)       ((void)0)  // No-op macro para fputc()
    #define vprintf(...)     ((void)0)  // No-op macro para vprintf()
    #define vfprintf(...)    ((void)0)  // No-op macro para vfprintf()
#endif

/*---------------------------------------------------------------------------*/
/* Justificación de la Configuración                                        */
/*---------------------------------------------------------------------------*/

/*
 * PROBLEMA CRÍTICO IDENTIFICADO:
 * 
 * 1. FUNCIONES CON PRINTF() BLOQUEANTES EN EL ISR:
 *    - DpDiag_Alarm() - Llamada desde el ISR para diagnóstico
 *    - DpDiag_SetCfgOk() - Llamada desde el ISR para confirmar configuración
 *    - vpc3_spi.c - Funciones de bajo nivel del VPC3+
 * 
 * 2. CONSECUENCIAS DE PRINTF() EN EL ISR:
 *    - Latencias que rompen los temporizados DP
 *    - Estado "Not reachable" del esclavo
 *    - Timeouts del maestro PROFIBUS
 *    - Corrupción de buffers por demoras
 * 
 * 3. SOLUCIÓN IMPLEMENTADA:
 *    - Por defecto, todas las funciones stdio se convierten en no-op
 *    - Solo se habilitan si se define DEBUG_ISR_NO_BLOCK
 *    - Protección automática sin modificar cada función individual
 * 
 * 4. FUNCIONES CRÍTICAS AFECTADAS:
 *    - DpDiag_Alarm() - Diagnóstico de alarmas
 *    - DpDiag_SetCfgOk() - Confirmación de configuración
 *    - VPC3_Write() - Escritura al ASIC
 *    - VPC3_Read() - Lectura del ASIC
 * 
 * 5. RECOMENDACIÓN DE USO:
 *    - PRODUCCIÓN: NO definir DEBUG_ISR_NO_BLOCK (stdio automáticamente deshabilitado)
 *    - DEBUGGING: Definir DEBUG_ISR_NO_BLOCK temporalmente
 *    - TESTING: Usar solo para análisis específico de problemas
 * 
 * 6. VENTAJAS DE ESTA SOLUCIÓN:
 *    - Protección automática sin modificar código existente
 *    - Fácil habilitación/deshabilitación para debugging
 *    - No hay riesgo de olvidar proteger alguna función stdio
 *    - Compilación limpia sin warnings
 *    - Cobertura completa de todas las funciones stdio problemáticas
 */

#endif /* DEBUG_CONFIG_H */
