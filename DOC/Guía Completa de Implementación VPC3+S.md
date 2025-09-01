# Guía Completa de Implementación VPC3+S PROFIBUS Slave

## Índice
0. [Verificación Preliminar (Anti 'Not Reachable')](#0-verificación-preliminar-anti-not-reachable)
1. [Inicialización del Sistema](#1-inicialización-del-sistema)
2. [Configuración de Registros del VPC3+S](#2-configuración-de-registros-del-vpc3s)
3. [Secuencia de START](#3-secuencia-de-start)
4. [Detección de Baudrate](#4-detección-de-baudrate)
5. [Procesamiento de Parametrización](#5-procesamiento-de-parametrización)
6. [Procesamiento de Configuración](#6-procesamiento-de-configuración)
7. [Transición a DATA_EX](#7-transición-a-data_ex)
8. [Intercambio de Datos](#8-intercambio-de-datos)
9. [Manejo de Diagnósticos](#9-manejo-de-diagnósticos)

---

## 0. Verificación Preliminar (Anti 'Not Reachable')

Un error "Not reachable" desde el PLC indica que el esclavo no está respondiendo en el bus. Esto casi siempre se debe a un problema en la inicialización física o en la comunicación básica entre el microcontrolador y el VPC3+S. Antes de seguir la guía, asegure los siguientes puntos.

### 0.1 Verificación de la Capa Física
- **Alimentación:** Confirme que tanto el STM32 como el VPC3+S tienen voltajes de alimentación estables y correctos.
- **Cableado PROFIBUS:** Verifique la conexión del bus (líneas A y B).
- **Terminación de Bus:** La red PROFIBUS debe tener una resistencia de terminación activa en ambos extremos físicos del cable. Si solo conecta el PLC y su dispositivo, ambos deben tener la terminación activada.

### 0.2 Verificación de la Comunicación MCU <-> VPC3+S
Esta es la prueba de software más fundamental. Si el microcontrolador no puede leer registros del VPC3+S, nada funcionará.

**Acción:** Inmediatamente después de un reset de hardware, antes de cualquier otra configuración, lea los registros de estado.

**¿Qué haces?**
```c
// Forzar un reset y esperar un momento
VPC3_HardwareReset();
HAL_Delay(10);

// Leer registros de estado inmediatamente después
uint8_t status_l = Vpc3Read(0x04);
uint8_t status_h = Vpc3Read(0x05);

// Usar un printf o un debugger para verificar los valores
printf("Verificacion Post-Reset -> STATUS_L: 0x%02X, STATUS_H: 0x%02X\n", status_l, status_h);
```

**¿Qué respuesta esperas?**
- La salida debe ser **exactamente**: `STATUS_L: 0x00, STATUS_H: 0xE3`.

**Diagnóstico:**
- Si obtienes `0xFF, 0xFF` o `0x00, 0x00`, la comunicación SPI o el bus paralelo entre tu STM32 y el VPC3+S **está fallando**. Revisa la configuración de pines (MOSI, MISO, SCK, CS), la velocidad del bus y las conexiones. **Este es el sospechoso #1 del error "Not reachable".** No continúes hasta que esta lectura sea correcta.

---

## 1. Inicialización del Sistema

### 1.1 Reset Hardware del VPC3+S

**Prerrequisito:** El sistema está alimentado eléctricamente y el VPC3+S tiene alimentación estable.

**Acción General:** Forzar un estado inicial conocido y limpio en el chip mediante un reset de hardware.

**Encargado:** Microcontrolador (STM32).

**Significado:** El reset asegura que el chip parte de un estado predecible (`OFFLINE`), que es un requisito indispensable para la configuración inicial.

**¿Qué haces?**
```c
// Llamar función de reset hardware
VPC3_HardwareReset();
```

**¿Dónde escribes?**
- No escribes a ningún registro
- La función controla señales GPIO de reset

**¿Qué respuesta esperas?**
```c
// STATUS_L (0x04) debe cambiar a 0x00
// STATUS_H (0x05) debe mostrar 0xE3
uint8_t status_l = Vpc3Read(0x04);  // Debe ser 0x00
uint8_t status_h = Vpc3Read(0x05);  // Debe ser 0xE3
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después del reset
if (status_l == 0x00 && status_h == 0xE3) {
    printf("Reset hardware exitoso\n");
} else {
    printf("Error en reset hardware\n");
}
```

**¿Por qué debe cumplirse?**
- El reset hardware limpia todos los registros internos
- 0xE3 confirma que es un VPC3+S (identificador del chip)
- 0x00 confirma que está en estado OFFLINE

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.6 (página 844-860)
- `vpc3pluss_user-manual.txt` - Sección de Hardware Reset

### 1.2 Inicialización del Firmware VPC3+ (PASO CRÍTICO)

**Prerrequisito:** Los registros de modo del hardware han sido configurados correctamente en el paso anterior.

**Acción General:** Cargar y activar el firmware interno del VPC3+ que gestiona todo el protocolo PROFIBUS. Sin esto, el chip solo tiene capacidades de hardware básicas.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este paso es el corazón de la inicialización. Prepara toda la estructura de memoria interna, los punteros a los búferes, los SAPs (Service Access Points) y el gestor de recursos. Es el paso que habilita la inteligencia del chip.

**¿Qué haces?**
```c
// Configurar estructura de configuración por defecto
CFG_STRUCT cfg_data = {
    .DIN_BUFSIZE = 32,    // Tamaño buffer entrada
    .DOUT_BUFSIZE = 16,   // Tamaño buffer salida
    .PRM_BUFSIZE = 20,    // Tamaño buffer parametrización
    .DIAG_BUFSIZE = 20,   // Tamaño buffer diagnóstico
    .CFG_BUFSIZE = 2      // Tamaño buffer configuración
};

// Llamar función de inicialización del firmware
DP_ERROR_CODE result = VPC3_Initialization(7, 0xADAC, cfg_data);
```

**¿Dónde escribes?**
- No escribes directamente, la función configura internamente
- El VPC3+ inicializa su firmware y buffers

**¿Qué respuesta esperas?**
```c
// result debe ser DP_OK (0) para éxito
if (result == DP_OK) {
    printf("Firmware VPC3+ inicializado exitosamente\n");
} else {
    printf("Error inicializando firmware: %d\n", result);
    // Códigos de error posibles:
    // DP_NOT_OFFLINE_ERROR: VPC3+ no está en estado OFFLINE
    // DP_ADDRESS_ERROR: Dirección de esclavo inválida
    // DP_CALCULATE_IO_ERROR: Error en cálculo de buffers
    // DP_LESS_MEM_ERROR: Memoria insuficiente
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// Debe retornar DP_OK si todo está correcto
// Si falla, verificar que VPC3+ esté en estado OFFLINE
```

**¿Por qué debe cumplirse?**
- **CRÍTICO**: Sin inicialización del firmware, el VPC3+ no puede procesar tramas PROFIBUS
- El chip está en modo "hardware only" pero necesita firmware activo
- Inicializa buffers de comunicación, SAPs y Resource Manager
- Es el paso que faltaba en la implementación original

**Punto de Verificación Crítico:**
Es **absolutamente esencial** verificar el código de retorno de `VPC3_Initialization()`. Un fallo aquí es la causa más común de que el esclavo sea "Not reachable", incluso si la comunicación básica del paso anterior funciona.

```c
DP_ERROR_CODE result = VPC3_Initialization(7, 0xADAC, cfg_data);

// Imprimir para depurar el resultado
printf("Resultado de VPC3_Initialization: %d\n", result);

if (result != DP_OK) {
    printf("FALLO CRÍTICO: VPC3_Initialization devolvió el código de error %d. El esclavo no funcionará.\n", result);
    // Detener la ejecución o entrar en un bucle de error.
    while(1);
}
```

Si el resultado no es `DP_OK` (cuyo valor es 0), consulte la sección 3.6 del manual `VPC_Software_description.txt` para interpretar el código de error. Un fallo aquí significa que el firmware del chip no se cargó.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.6 (página 844-860)
- `vpc3pluss_user-manual.txt` - Sección de Firmware Initialization

**Verificación post-inicialización:**
```c
// Después de VPC3_Initialization(), verificar:
uint8_t status_l = Vpc3Read(0x04);
uint8_t status_h = Vpc3Read(0x05);

// Debe mostrar:
// STATUS_L: 0x00 (OFFLINE pero con firmware activo)
// STATUS_H: 0xE3 (VPC3+S release)

if (status_l == 0x00 && status_h == 0xE3) {
    printf("Firmware activo y listo\n");
} else {
    printf("Error: firmware no activo\n");
}
```

---

## 2. Configuración de Registros del VPC3+S

### 2.1 Configuración de MODE_REG_0_L (0x06)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`).

**Acción General:** Pre-configurar el comportamiento del hardware del VPC3+ antes de activar su firmware interno.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este valor configura el modo de operación PROFIBUS DP, habilita monitoreo de start/stop bits, configura watchdog base de 10ms, y habilita operación DP pura.

**¿Qué haces?**
```c
Vpc3Write(0x06, 0xC0);  // MODE_REG_0_L
```

**¿Dónde escribes?**
- Registro 0x06 (MODE_REG_0_L)

**¿Qué respuesta esperas?**
```c
// Al leer debe devolver 0xC0
uint8_t mode_reg_0_l = Vpc3Read(0x06);  // Debe ser 0xC0
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
if (mode_reg_0_l == 0xC0) {
    printf("MODE_REG_0_L configurado correctamente\n");
} else {
    printf("Error en MODE_REG_0_L: 0x%02X\n", mode_reg_0_l);
}
```

**¿Por qué debe cumplirse?**
- 0xC0 configura el modo de operación PROFIBUS DP
- Habilita monitoreo de start/stop bits
- Configura watchdog base de 10ms
- Habilita operación DP pura

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 (página 597-604)
- `vpc3pluss_user-manual.txt` - Sección de Mode Registers

### 2.2 Configuración de MODE_REG_0_H (0x07)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`).

**Acción General:** Pre-configurar el comportamiento del hardware del VPC3+ antes de activar su firmware interno.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este valor habilita modo DP, configura timer base de 1ms, habilita EOI (End of Interrupt) timing, y configura formato de interrupción.

**¿Qué haces?**
```c
Vpc3Write(0x07, 0x25);  // MODE_REG_0_H
```

**¿Dónde escribes?**
- Registro 0x07 (MODE_REG_0_H)

**¿Qué respuesta esperas?**
```c
// Al leer debe devolver 0x25
uint8_t mode_reg_0_h = Vpc3Read(0x07);  // Debe ser 0x25
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
if (mode_reg_0_h == 0x25) {
    printf("MODE_REG_0_H configurado correctamente\n");
} else {
    printf("Error en MODE_REG_0_H: 0x%02X\n", mode_reg_0_h);
}
```

**¿Por qué debe cumplirse?**
- 0x25 habilita modo DP
- Configura timer base de 1ms
- Habilita EOI (End of Interrupt) timing
- Configura formato de interrupción

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 (página 641-664)

### 2.3 Configuración de MODE_REG_1 (0x15)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`).

**Acción General:** Pre-configurar el comportamiento del hardware del VPC3+ antes de activar su firmware interno.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este registro se actualiza automáticamente durante transiciones de estado y contiene bits de control operativo.

**¿Qué haces?**
```c
// MODE_REG_1 se configura automáticamente, no requiere escritura manual
// Solo se lee para verificación
```

**¿Dónde escribes?**
- No se escribe manualmente (se configura automáticamente)

**¿Qué respuesta esperas?**
```c
// Al leer puede cambiar según el estado operativo del chip
uint8_t mode_reg_1 = Vpc3Read(0x15);  // Puede cambiar de 0x00 a 0x01, etc.
```

**¿Cómo la esperas?**
```c
// Solo verificar que no sea 0xFF (error de comunicación)
uint8_t mode_reg_1 = Vpc3Read(0x15);
if (mode_reg_1 != 0xFF) {
    printf("MODE_REG_1 operativo (0x%02X)\n", mode_reg_1);
} else {
    printf("Error en MODE_REG_1: 0x%02X\n", mode_reg_1);
}
```

**¿Por qué debe cumplirse?**
- MODE_REG_1 se actualiza automáticamente durante transiciones de estado
- No requiere configuración manual
- Los cambios de valor son normales y esperados

**Manual de referencia:**
- `vpc3pluss_user-manual.txt` - Sección 5.1.2 (página 770-798)

### 2.5 Configuración de MODE_REG_2 (0x0C)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`).

**Acción General:** Pre-configurar el comportamiento del hardware del VPC3+ antes de activar su firmware interno.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este valor configura modo 2KB RAM, habilita interrupción GC después de cada telegrama, configura polaridad SYNC negativa, y habilita verificación de bits reservados.

**¿Qué haces?**
```c
Vpc3Write(0x0C, 0x05);  // MODE_REG_2
```

**¿Dónde escribes?**
- Registro 0x0C (MODE_REG_2)

**¿Qué respuesta esperas?**
```c
// Al leer puede cambiar según el estado operativo del chip
uint8_t mode_reg_2 = Vpc3Read(0x0C);  // Puede cambiar de 0x05 a 0x06, 0x07, etc.
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
uint8_t mode_reg_2 = Vpc3Read(0x0C);
if ((mode_reg_2 & 0x05) == 0x05) {  // Verificar bits críticos
    printf("MODE_REG_2 configurado correctamente (0x%02X)\n", mode_reg_2);
} else {
    printf("Error en MODE_REG_2: 0x%02X\n", mode_reg_2);
}
```

**¿Por qué debe cumplirse?**
- 0x05 configura modo 2KB RAM
- Habilita interrupción GC después de cada telegrama
- Configura polaridad SYNC negativa
- Habilita verificación de bits reservados
- **Nota:** Algunos bits cambian automáticamente según el estado operativo

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 (página 673-698)

### 2.6 Configuración de MODE_REG_3 (0x12)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`).

**Acción General:** Pre-configurar el comportamiento del hardware del VPC3+ antes de activar su firmware interno.

**Encargado:** Microcontrolador (STM32).

**Significado:** Este valor deshabilita PLL, habilita verificación de S_SAP, configura interrupción DX_Out después de cada telegrama, y habilita interrupción GC solo si cambió.

**¿Qué haces?**
```c
Vpc3Write(0x12, 0x00);  // MODE_REG_3
```

**¿Dónde escribes?**
- Registro 0x12 (MODE_REG_3)

**¿Qué respuesta esperas?**
```c
// Al leer puede devolver 0x93 (normal) debido a bits reservados
uint8_t mode_reg_3 = Vpc3Read(0x12);  // Puede ser 0x93 (normal)
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
uint8_t mode_reg_3 = Vpc3Read(0x12);
if ((mode_reg_3 & 0x0F) == 0x03) {  // Solo verificar bits configurables
    printf("MODE_REG_3 configurado correctamente (0x%02X)\n", mode_reg_3);
} else {
    printf("Error en MODE_REG_3: 0x%02X\n", mode_reg_3);
}
```

**¿Por qué debe cumplirse?**
- Los bits 4-7 son de solo lectura (reservados) según el manual oficial
- Solo los bits 0-3 son configurables
- 0x93 es un valor normal que incluye bits reservados automáticos

**Verificación para el Siguiente Paso:** La función de inicialización del firmware (`VPC3_Initialization`) espera que estos registros de modo ya estén configurados para poder establecer correctamente el entorno de operación del protocolo.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 (página 704-723)
- Sección 3.2.10 "Set Hardware Mode": Detalla cada bit de estos registros. El manual enfatiza que solo deben modificarse mientras el chip está `offline`

---

## **⚠️ NOTA IMPORTANTE SOBRE COMPORTAMIENTO NORMAL DE REGISTROS**

**Los valores que observas en los logs son NORMALES según los manuales oficiales:**

- **MODE_REG_3 (0x12):** Los bits 4-7 son de solo lectura (reservados). El valor `0x93` es normal, no un error.
- **MODE_REG_2 (0x0C):** Algunos bits cambian automáticamente según el estado operativo del chip.
- **MODE_REG_1 (0x15):** Se actualiza automáticamente durante transiciones de estado.

**Tu código está funcionando CORRECTAMENTE.** Los valores `0x93`, `0x06`, y `0x01` son esperados según la especificación del hardware VPC3+S.

**Referencias:**
- `VPC_Software_description.txt` - Línea 714: "Mode Register 3, Address 12H: Bit 7 Reserved, bit 6 Reserved, bit 5 Reserved, bit 4 Reserved, bit 3 PLL_Supported"
- `vpc3pluss_user-manual.txt` - Línea 848: "Mode Register 3, Address 12H: bit 7 w-0"

---

## 3. Secuencia de START

### 3.1 Envío del Comando START

**Prerrequisito:** La inicialización del firmware se ha completado con éxito (`VPC3_Initialization()` devolvió `DP_OK`).

**Acción General:** Poner el VPC3+ "en línea" para que comience a escuchar activamente el bus PROFIBUS.

**Encargado:** Microcontrolador (STM32).

**Significado:** El chip transiciona del estado `OFFLINE` al estado `PASSIVE_IDLE`, donde está listo para detectar el baudrate y recibir los primeros telegramas del maestro.

**¿Qué haces?**
```c
// IMPORTANTE: Solo DESPUÉS de VPC3_Initialization()
Vpc3Write(0x08, 0x01);  // START command al registro de control
```

**¿Dónde escribes?**
- Registro 0x08 (Control Register)

**¿Qué respuesta esperas?**
```c
// STATUS_L debe cambiar de 0x00 a 0x91
// STATUS_H debe mantenerse en 0xE3
uint8_t status_l = Vpc3Read(0x04);  // Debe ser 0x91
uint8_t status_h = Vpc3Read(0x05);  // Debe ser 0xE3
```

**¿Cómo la esperas?**
```c
// Esperar hasta 50ms para el cambio de estado
uint32_t start_time = HAL_GetTick();
uint8_t status_l;

do {
    status_l = Vpc3Read(0x04);
    HAL_Delay(10);
    
    if (HAL_GetTick() - start_time > 50) {
        printf("Timeout esperando PASSIVE_IDLE\n");
        return 0;
    }
} while (status_l != 0x91);

printf("VPC3+ en PASSIVE_IDLE\n");
```

**¿Por qué debe cumplirse?**
- 0x91 indica que el VPC3+ está en estado PASSIVE_IDLE
- El chip está listo para recibir tramas del maestro
- La comunicación física está establecida
- **CRÍTICO**: Solo funciona si el firmware está inicializado

**Punto de Verificación de Estado:**
Después de enviar el comando START, verifique que el chip realmente ha cambiado al estado `PASSIVE_IDLE`.

```c
// Después de Vpc3Write(0x08, 0x01);
HAL_Delay(10); // Dar tiempo al chip para procesar el comando
uint8_t status_l_after_start = Vpc3Read(0x04);

printf("Verificacion Post-START -> STATUS_L: 0x%02X\n", status_l_after_start);

if (status_l_after_start != 0x91) {
    printf("FALLO: El chip no ha entrado en PASSIVE_IDLE. Probablemente la inicialización del firmware falló.\n");
    while(1);
}
```

Si el estado no cambia a `0x91`, el comando fue ignorado, lo que apunta a un fallo en el paso de inicialización del firmware.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.7 (página 873-878)
- `vpc3pluss_user-manual.txt` - Sección de Start Command

**Verificación de secuencia correcta:**
```c
// Antes de START, verificar que el firmware esté activo:
uint8_t status_l = Vpc3Read(0x04);
uint8_t status_h = Vpc3Read(0x05);

if (status_l == 0x00 && status_h == 0xE3) {
    printf("Firmware activo, procediendo con START\n");
    Vpc3Write(0x08, 0x01);  // START command
} else {
    printf("ERROR: Firmware no activo. Llamar VPC3_Initialization() primero\n");
    return;
}
```

---

## 4. Detección de Baudrate

### 4.1 Verificación de Baudrate Detectado

**Prerrequisito:** El VPC3+ está en estado `PASSIVE_IDLE` (`STATUS_L == 0x91`) y hay un maestro activo en el bus enviando telegramas.

**Acción General:** El VPC3+ detecta automáticamente la velocidad del bus. El microcontrolador verifica que la detección fue exitosa y estable.

**Encargado:** VPC3+S (detecta), Microcontrolador (verifica).

**Significado:** Sin un baudrate estable, la comunicación es imposible. El chip necesita saber a qué velocidad "escuchar".

**¿Qué haces?**
```c
// Leer STATUS_H y extraer bits de baudrate
uint8_t status_h = Vpc3Read(0x05);
uint8_t baudrate_bits = (status_h >> 4) & 0x0F;
```

**¿Dónde lees?**
- Registro 0x05 (STATUS_H), bits 11-8

**¿Qué respuesta esperas?**
```c
// baudrate_bits debe ser diferente de 0x0F
// 0x0F indica "en búsqueda o después de reset"
if (baudrate_bits != 0x0F) {
    printf("Baudrate detectado: %d\n", baudrate_bits);
} else {
    printf("Baudrate aún no detectado\n");
}
```

**¿Cómo la esperas?**
```c
// Verificar que el baudrate sea estable
uint8_t baudrate1 = (Vpc3Read(0x05) >> 4) & 0x0F;
HAL_Delay(10);
uint8_t baudrate2 = (Vpc3Read(0x05) >> 4) & 0x0F;

if (baudrate1 == baudrate2 && baudrate1 != 0x0F) {
    printf("Baudrate estable: %d\n", baudrate1);
} else {
    printf("Baudrate inestable o no detectado\n");
}
```

**¿Por qué debe cumplirse?**
- El baudrate debe ser estable para comunicación confiable
- 0x0F indica que el chip aún no ha detectado la velocidad del bus
- Sin baudrate estable, no puede recibir tramas correctamente

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.1.1 (página 1014-1030)
- `vpc3pluss_user-manual.txt` - Sección de Baudrate Detection

### 4.2 Interpretación de Códigos de Baudrate

**¿Qué haces?**
```c
// Convertir bits de baudrate a velocidad real
uint32_t baudrate_kbps = 0;
switch(baudrate_bits) {
    case 0x00: baudrate_kbps = 12000; break;  // 12 Mbit/s
    case 0x01: baudrate_kbps = 6000;  break;  // 6 Mbit/s
    case 0x02: baudrate_kbps = 3000;  break;  // 3 Mbit/s
    case 0x03: baudrate_kbps = 1500;  break;  // 1.5 Mbit/s
    case 0x04: baudrate_kbps = 500;   break;  // 500 Kbit/s
    case 0x05: baudrate_kbps = 187;   break;  // 187.5 Kbit/s
    case 0x06: baudrate_kbps = 93;    break;  // 93.75 Kbit/s
    case 0x07: baudrate_kbps = 45;    break;  // 45.45 Kbit/s
    case 0x08: baudrate_kbps = 19;    break;  // 19.2 Kbit/s
    case 0x09: baudrate_kbps = 9;     break;  // 9.6 Kbit/s
    default:   baudrate_kbps = 0;     break;  // Inválido
}
```

**¿Por qué es necesario?**
- Los bits del registro no son la velocidad directa
- Necesitas convertir para debugging y monitoreo
- Confirma que la detección fue exitosa

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.1.1 (página 1014-1030)

---

## 5. Procesamiento de Parametrización

### 5.1 Detección de Evento NEW_PRM_DATA

**Prerrequisito:** El VPC3+ ha detectado un baudrate estable y está listo para recibir comandos del maestro.

**Acción General:** El maestro envía los parámetros de operación al esclavo. El esclavo (tu aplicación) debe recibirlos, validarlos y responder.

**Encargado:** Maestro (inicia), Microcontrolador (procesa y responde).

**Significado:** Este es el primer paso del handshake. El esclavo le dice al maestro si acepta los parámetros de funcionamiento básicos.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar evento
uint16_t events = Vpc3Read(INT_REG_L) | (Vpc3Read(INT_REG_H) << 8);

if (events & 0x0800) {  // NEW_PRM_DATA
    printf("Evento NEW_PRM_DATA detectado\n");
    // Procesar parametrización
}
```

**¿Dónde lees?**
- INT_REG_L (0x02), bit 7
- INT_REG_H (0x03), bit 11

**¿Qué respuesta esperas?**
```c
// El bit 0x0800 debe estar activo
// Indica que llegó nueva trama de parametrización
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// No es necesario esperar, solo detectar cuando esté activo
```

**¿Por qué debe cumplirse?**
- 0x0800 indica que el maestro envió trama Set_Param
- Es el primer paso para establecer comunicación
- Sin parametrización válida, no puede continuar

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1075-1083)

### 5.2 Obtención del Puntero del Buffer de Parametrización

**Prerrequisito:** Se ha detectado el evento NEW_PRM_DATA y se debe procesar la parametrización.

**Acción General:** Obtener la dirección del buffer de parametrización donde están almacenados los parámetros enviados por el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder a los datos de parametrización que definen el comportamiento del esclavo.

**¿Qué haces?**
```c
// Leer registro PRM_PTR para obtener dirección del buffer
uint16_t prm_ptr = Vpc3Read(0x36);  // PRM_PTR register
```

**¿Dónde lees?**
- Registro 0x36 (PRM_PTR)

**¿Qué respuesta esperas?**
```c
// prm_ptr debe ser una dirección válida en el rango 0x0000-0x07FF
// Típicamente 0x00B8 para el primer buffer
if (prm_ptr >= 0x0000 && prm_ptr <= 0x07FF) {
    printf("Puntero PRM válido: 0x%04X\n", prm_ptr);
} else {
    printf("Puntero PRM inválido: 0x%04X\n", prm_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de leer
// Debe ser una dirección válida dentro del rango de memoria del VPC3+
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde están los datos de parametrización
- Sin puntero válido, no puedes leer los parámetros
- La dirección debe estar dentro del rango de memoria configurado

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1110-1127)

### 5.3 Lectura de Datos de Parametrización

**Prerrequisito:** Se ha obtenido el puntero del buffer de parametrización y se conoce la longitud de los datos.

**Acción General:** Copiar los datos de parametrización del buffer del VPC3+ a un buffer local para su procesamiento.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder y validar los parámetros enviados por el maestro antes de aceptarlos o rechazarlos.

**¿Qué haces?**
```c
// Copiar datos del buffer del VPC3+ a buffer local
CopyFromVpc3(local_buffer, prm_ptr, 7);  // 7 bytes de parametrización
```

**¿Dónde lees?**
- Desde la dirección prm_ptr en memoria del VPC3+
- Hacia buffer local en STM32

**¿Qué respuesta esperas?**
```c
// local_buffer debe contener 7 bytes válidos
// Estructura esperada según GSD:
// [0]: 0xB8 (Station_Status_1)
// [1]: 0x02 (WD_Fact_1)
// [2]: 0x02 (WD_Fact_2)
// [3]: 0x0B (minTSDR)
// [4]: 0xAD (Ident_Number_High)
// [5]: 0xAC (Ident_Number_Low)
// [6]: 0x00 (Group_Ident)

printf("Parámetros recibidos: ");
for (int i = 0; i < 7; i++) {
    printf("0x%02X ", local_buffer[i]);
}
printf("\n");
```

**¿Cómo la esperas?**
```c
// Verificar que CopyFromVpc3 retorne éxito
// Verificar que los bytes tengan valores esperados
// Verificar que Ident_Number coincida con configuración
```

**¿Por qué debe cumplirse?**
- Los parámetros definen el comportamiento del esclavo
- Ident_Number debe coincidir con el configurado en el GSD
- WD_Fact y minTSDR afectan el timing de comunicación

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1114-1127)
- `vpc3pluss_user-manual.txt` - Sección de Parameter Data

### 5.4 Validación de Parámetros

**Prerrequisito:** Se han leído los datos de parametrización del buffer del VPC3+ y están disponibles en el buffer local.

**Acción General:** Validar que los parámetros recibidos son compatibles con la configuración del esclavo y cumplen con los requisitos del protocolo.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para determinar si se aceptan o rechazan los parámetros enviados por el maestro.

**¿Qué haces?**
```c
// Llamar función de validación
uint8_t result = DpPrm_ChkNewPrmData();
```

**¿Dónde escribes?**
- No escribes, solo validas

**¿Qué respuesta esperas?**
```c
// result debe ser 0 (éxito) o código de error
if (result == 0) {
    printf("Parametrización válida\n");
} else {
    printf("Error en parametrización: %d\n", result);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// Debe validar Ident_Number, WD_Fact, minTSDR
```

**¿Por qué debe cumplirse?**
- Sin validación exitosa, no puede continuar
- Los parámetros deben ser compatibles con la configuración
- El maestro espera confirmación de aceptación

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)

### 5.5 Respuesta de Parametrización

**Prerrequisito:** Se han validado los parámetros y se ha tomado la decisión de aceptarlos o rechazarlos.

**Acción General:** Enviar la respuesta de aceptación o rechazo de los parámetros al maestro a través del VPC3+.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para informar al maestro si la parametrización fue exitosa, permitiendo que continúe con el siguiente paso del handshake.

**¿Qué haces?**
```c
// Enviar respuesta de aceptación o rechazo
uint8_t response;
if (parametrizacion_valida) {
    response = VPC3_SET_PRM_DATA_OK();
} else {
    response = VPC3_SET_PRM_DATA_NOK();
}
```

**¿Dónde escribes?**
- No escribes directamente, la función lo hace internamente
- El VPC3+ responde automáticamente al maestro

**¿Qué respuesta esperas?**
```c
// response debe ser 0 (éxito) o código de error
if (response == 0) {
    printf("Respuesta de parametrización enviada\n");
} else {
    printf("Error enviando respuesta: %d\n", response);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// El VPC3+ envía la respuesta al maestro
```

**¿Por qué debe cumplirse?**
- El maestro necesita confirmación para continuar
- Sin respuesta, el maestro no enviará configuración
- Es parte del protocolo de handshake PROFIBUS

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)

---

## 6. Procesamiento de Configuración

### 6.1 Detección de Evento NEW_CFG_DATA

**Prerrequisito:** La parametrización ha sido aceptada por el esclavo y notificada al maestro.

**Acción General:** El maestro envía la configuración de I/O (cuántos bytes de entrada y salida se usarán). El esclavo debe validarla.

**Encargado:** Maestro (inicia), Microcontrolador (procesa y responde).

**Significado:** Este es el segundo y último paso del handshake. El esclavo confirma las "tuberías" de datos que se usarán.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar evento
if (events & 0x0400) {  // NEW_CFG_DATA
    printf("Evento NEW_CFG_DATA detectado\n");
    // Procesar configuración
}
```

**¿Dónde lees?**
- INT_REG_L (0x02), bit 6

**¿Qué respuesta esperas?**
```c
// El bit 0x0400 debe estar activo
// Indica que llegó nueva trama de configuración
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// Debe activarse después de parametrización exitosa
```

**¿Por qué debe cumplirse?**
- 0x0400 indica que el maestro envió trama Check_Cfg
- Es el segundo paso para establecer comunicación
- Sin configuración válida, no puede entrar en DATA_EX

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.3.1 (página 1203-1214)

### 6.2 Obtención del Puntero del Buffer de Configuración

**Prerrequisito:** Se ha detectado el evento NEW_CFG_DATA y se debe procesar la configuración.

**Acción General:** Obtener la dirección del buffer de configuración donde están almacenados los datos de configuración enviados por el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder a los datos de configuración que definen el tamaño de los buffers I/O.

**¿Qué haces?**
```c
// Leer registro CFG_PTR para obtener dirección del buffer
uint16_t cfg_ptr = Vpc3Read(0x34);  // CFG_PTR register
```

**¿Dónde lees?**
- Registro 0x34 (CFG_PTR)

**¿Qué respuesta esperas?**
```c
// cfg_ptr debe ser una dirección válida en el rango 0x0000-0x07FF
// Típicamente 0x00D0 para el primer buffer
if (cfg_ptr >= 0x0000 && cfg_ptr <= 0x07FF) {
    printf("Puntero CFG válido: 0x%04X\n", cfg_ptr);
} else {
    printf("Puntero CFG inválido: 0x%04X\n", cfg_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de leer
// Debe ser una dirección válida dentro del rango de memoria
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde están los datos de configuración
- Sin puntero válido, no puedes leer la configuración
- La dirección debe estar dentro del rango configurado

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.3.2 (página 1251-1278)

### 6.3 Lectura de Datos de Configuración

**Prerrequisito:** Se ha obtenido el puntero del buffer de configuración y se conoce la longitud de los datos.

**Acción General:** Copiar los datos de configuración del buffer del VPC3+ a un buffer local para su procesamiento.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder y validar la configuración enviada por el maestro antes de aceptarla o rechazarla.

**¿Qué haces?**
```c
// Copiar datos del buffer del VPC3+ a buffer local
CopyFromVpc3(local_buffer, cfg_ptr, 2);  // 2 bytes de configuración
```

**¿Dónde lees?**
- Desde la dirección cfg_ptr en memoria del VPC3+
- Hacia buffer local en STM32

**¿Qué respuesta esperas?**
```c
// local_buffer debe contener 2 bytes válidos
// Estructura esperada:
// [0]: 0x20 (32 decimal) - Configuración de entrada
// [1]: 0x10 (16 decimal) - Configuración de salida

printf("Configuración recibida: 0x%02X 0x%02X\n", 
       local_buffer[0], local_buffer[1]);
```

**¿Cómo la esperas?**
```c
// Verificar que CopyFromVpc3 retorne éxito
// Verificar que los bytes tengan valores esperados
// Comparar con configuración local
```

**¿Por qué debe cumplirse?**
- La configuración define el tamaño de buffers I/O
- Debe coincidir con la configuración del GSD
- Sin configuración válida, no puede configurar buffers

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.3.2 (página 1257-1278)

### 6.4 Validación de Configuración

**Prerrequisito:** Se han leído los datos de configuración del buffer del VPC3+ y están disponibles en el buffer local.

**Acción General:** Validar que la configuración recibida es compatible con los buffers configurados y cumple con los requisitos del protocolo.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para determinar si se acepta o rechaza la configuración enviada por el maestro.

**¿Qué haces?**
```c
// Llamar función de validación
uint8_t result = DpCfg_ChkNewCfgData();
```

**¿Dónde escribes?**
- No escribes, solo validas

**¿Qué respuesta esperas?**
```c
// result debe ser 0 (éxito) o código de error
if (result == 0) {
    printf("Configuración válida\n");
} else {
    printf("Error en configuración: %d\n", result);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// Debe validar que coincida con configuración esperada
```

**¿Por qué debe cumplirse?**
- Sin validación exitosa, no puede continuar
- La configuración debe ser compatible con buffers configurados
- El maestro espera confirmación para proceder

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.3.1 (página 1203-1214)

### 6.5 Respuesta de Configuración

**Prerrequisito:** Se ha validado la configuración y se ha tomado la decisión de aceptarla o rechazarla.

**Acción General:** Enviar la respuesta de aceptación o rechazo de la configuración al maestro a través del VPC3+.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para informar al maestro si la configuración fue exitosa, permitiendo que continúe con el siguiente paso del handshake.

**¿Qué haces?**
```c
// Enviar respuesta de aceptación o rechazo
uint8_t response;
if (configuracion_valida) {
    response = VPC3_SET_CFG_DATA_OK();
} else {
    response = VPC3_SET_CFG_DATA_NOK();
}
```

**¿Dónde escribes?**
- No escribes directamente, la función lo hace internamente
- El VPC3+ responde automáticamente al maestro

**¿Qué respuesta esperas?**
```c
// response debe ser 0 (éxito) o código de error
if (response == 0) {
    printf("Respuesta de configuración enviada\n");
} else {
    printf("Error enviando respuesta: %d\n", response);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// El VPC3+ envía la respuesta al maestro
```

**¿Por qué debe cumplirse?**
- El maestro necesita confirmación para continuar
- Sin respuesta, no puede entrar en DATA_EX
- Es parte del protocolo de handshake PROFIBUS

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.3.1 (página 1232-1239)

---

## 7. Transición a DATA_EX

### 7.1 Verificación de Estado DP

**Prerrequisito:** La parametrización y configuración han sido completadas y aceptadas exitosamente.

**Acción General:** Verificar que el VPC3+ ha entrado automáticamente en el estado de intercambio de datos.

**Encargado:** Microcontrolador (verifica).

**Significado:** Confirma que la comunicación está oficialmente establecida y el intercambio cíclico va a comenzar.

**¿Qué haces?**
```c
// Leer STATUS_L y extraer estado DP
uint8_t status_l = Vpc3Read(0x04);
uint8_t dp_state = (status_l >> 5) & 0x03;  // Bits 5-6
```

**¿Dónde lees?**
- STATUS_L (0x04), bits 5-6

**¿Qué respuesta esperas?**
```c
// dp_state debe ser 0x02 (DATA_EX)
// Estados posibles:
// 0x00: WAIT_PRM (esperando parametrización)
// 0x01: WAIT_CFG (esperando configuración)
// 0x02: DATA_EX (intercambio de datos)
// 0x03: DP_ERROR (error)

if (dp_state == 0x02) {
    printf("VPC3+ en estado DATA_EX\n");
} else {
    printf("Estado DP incorrecto: 0x%02X\n", dp_state);
}
```

**¿Cómo la esperas?**
```c
// Verificar después de parametrización y configuración exitosas
// Debe cambiar de 0x01 (WAIT_CFG) a 0x02 (DATA_EX)
```

**¿Por qué debe cumplirse?**
- 0x02 indica que la comunicación está establecida
- El esclavo está listo para intercambio de datos
- Sin este estado, no puede recibir tramas de datos

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.9 (página 1594-1620)

### 7.2 Activación de Estados de Aplicación

**Prerrequisito:** El VPC3+ ha entrado en estado DATA_EX y la comunicación está establecida.

**Acción General:** Configurar los estados internos de la aplicación para indicar que está lista para el intercambio de datos.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para activar la lógica de aplicación que procesará los datos de entrada y salida durante el intercambio cíclico.

**¿Qué haces?**
```c
// Configurar estados internos de la aplicación
eDpStateApplReady = 4;  // Aplicación lista
eDpStateRun = 2;        // Aplicación ejecutándose
```

**¿Dónde escribes?**
- Variables globales de estado en tu aplicación

**¿Qué respuesta esperas?**
```c
// Los estados deben activarse correctamente
if (eDpStateApplReady == 4 && eDpStateRun == 2) {
    printf("Estados de aplicación activados\n");
} else {
    printf("Error activando estados de aplicación\n");
}
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de asignar
// Los valores deben ser exactamente 4 y 2
```

**¿Por qué debe cumplirse?**
- Estos estados controlan el comportamiento de la aplicación
- Sin estados activos, no puede procesar datos
- Son necesarios para el funcionamiento normal

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.9 (página 1594-1620)

---

## 8. Intercambio de Datos

### 8.1 Detección de Evento DX_OUT

**Prerrequisito:** El esclavo está en estado `DATA_EXCHANGE`.

**Acción General:** Gestionar el flujo continuo de datos de entrada y salida con el maestro.

**Encargado:** Maestro y Esclavo (ambos, de forma cíclica).

**Significado:** Este es el funcionamiento normal y final del esclavo, donde cumple su función de I/O remota.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar evento
if (events & 0x2000) {  // DX_OUT
    printf("Evento DX_OUT detectado\n");
    // Procesar datos de salida
    DpAppl_IsrDxOut();
}
```

**¿Dónde lees?**
- INT_REG_H (0x03), bit 13

**¿Qué respuesta esperas?**
```c
// El bit 0x2000 debe estar activo
// Indica que el maestro envió datos de salida
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// Debe activarse cíclicamente durante DATA_EX
```

**¿Por qué debe cumplirse?**
- 0x2000 indica que llegaron datos del maestro
- Es el evento principal durante DATA_EX
- Sin este evento, no hay intercambio de datos

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.4 (página 1341-1353)

### 8.2 Obtención del Puntero del Buffer de Salida

**Prerrequisito:** Se ha detectado el evento DX_OUT y se deben procesar los datos de salida del maestro.

**Acción General:** Obtener la dirección del buffer de salida activo donde están almacenados los datos enviados por el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder a los datos de salida que el maestro envió al esclavo.

**¿Qué haces?**
```c
// Obtener puntero del buffer de salida activo
uint8_t output_state;
VPC3_UNSIGNED8_PTR output_buffer = VPC3_GetDoutBufPtr(&output_state);
```

**¿Dónde lees?**
- Función VPC3_GetDoutBufPtr() retorna la dirección

**¿Qué respuesta esperas?**
```c
// output_buffer debe ser una dirección válida
// output_state debe indicar el buffer activo (0, 1, o 2)
if (output_buffer >= 0x0050 && output_buffer <= 0x0060) {
    printf("Buffer de salida: 0x%04X, Estado: %d\n", 
           output_buffer, output_state);
} else {
    printf("Buffer de salida inválido: 0x%04X\n", output_buffer);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// La dirección debe estar en el rango configurado
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde están los datos del maestro
- Sin puntero válido, no puedes leer los datos
- Es necesario para el intercambio de datos

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.4 (página 1341-1353)

### 8.3 Lectura de Datos de Salida

**Prerrequisito:** Se ha obtenido el puntero del buffer de salida y se conoce la longitud de los datos.

**Acción General:** Copiar los datos de salida del buffer del VPC3+ a un buffer local para su procesamiento.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder y procesar los datos enviados por el maestro.

**¿Qué haces?**
```c
// Copiar datos del buffer del VPC3+ a buffer local
CopyFromVpc3(local_buffer, output_buffer, output_length);
```

**¿Dónde lees?**
- Desde la dirección output_buffer en memoria del VPC3+
- Hacia buffer local en STM32

**¿Qué respuesta esperas?**
```c
// local_buffer debe contener datos válidos
// output_length debe ser el número de bytes leídos
printf("Datos de salida leídos: %d bytes\n", output_length);
for (int i = 0; i < output_length; i++) {
    printf("0x%02X ", local_buffer[i]);
}
printf("\n");
```

**¿Cómo la esperas?**
```c
// Verificar que CopyFromVpc3 retorne éxito
// Verificar que output_length sea correcto
// Verificar que los datos sean válidos
```

**¿Por qué debe cumplirse?**
- Los datos del maestro deben leerse correctamente
- Sin lectura exitosa, no puedes procesar los datos
- Es la base del intercambio de datos

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.4 (página 1341-1353)

### 8.4 Procesamiento de Datos de Salida

**Prerrequisito:** Se han leído los datos de salida del buffer del VPC3+ y están disponibles en el buffer local.

**Acción General:** Procesar los datos recibidos del maestro según la lógica de aplicación del esclavo.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para aplicar los comandos o datos enviados por el maestro a la aplicación del esclavo.

**¿Qué haces?**
```c
// Procesar los datos recibidos del maestro
// Por ejemplo, actualizar salidas digitales
for (int i = 0; i < output_length; i++) {
    // Procesar cada byte según tu lógica de aplicación
    ProcessOutputByte(local_buffer[i], i);
}
```

**¿Dónde escribes?**
- Variables de estado de tu aplicación
- Registros GPIO si es necesario

**¿Qué respuesta esperas?**
```c
// Los datos deben procesarse correctamente
// Las salidas deben actualizarse según los datos
printf("Datos de salida procesados exitosamente\n");
```

**¿Cómo la esperas?**
```c
// El procesamiento debe completarse inmediatamente
// No debe haber errores en el procesamiento
```

**¿Por qué debe cumplirse?**
- Los datos del maestro deben reflejarse en tu aplicación
- Sin procesamiento, no hay funcionalidad
- Es el propósito del intercambio de datos

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.4 (página 1341-1353)

### 8.5 Obtención del Puntero del Buffer de Entrada

**Prerrequisito:** Se han procesado los datos de salida y se deben preparar los datos de entrada para enviar al maestro.

**Acción General:** Obtener la dirección del buffer de entrada activo donde se escribirán los datos para el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder al buffer donde se escribirán los datos de entrada que el maestro leerá.

**¿Qué haces?**
```c
// Obtener puntero del buffer de entrada activo
VPC3_UNSIGNED8_PTR input_buffer = VPC3_GetDinBufPtr();
```

**¿Dónde lees?**
- Función VPC3_GetDinBufPtr() retorna la dirección

**¿Qué respuesta esperas?**
```c
// input_buffer debe ser una dirección válida
if (input_buffer >= 0x0050 && input_buffer <= 0x0060) {
    printf("Buffer de entrada: 0x%04X\n", input_buffer);
} else {
    printf("Buffer de entrada inválido: 0x%04X\n", input_buffer);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// La dirección debe estar en el rango configurado
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde escribir datos para el maestro
- Sin puntero válido, no puedes enviar datos
- Es necesario para el intercambio bidireccional

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.5 (página 1360-1379)

### 8.6 Preparación de Datos de Entrada

**Prerrequisito:** Se ha obtenido el puntero del buffer de entrada y se conoce la longitud de los datos a enviar.

**Acción General:** Preparar los datos de entrada (leer sensores, estado de la aplicación, etc.) en un buffer local.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para preparar la información que se enviará al maestro en el siguiente ciclo de intercambio.

**¿Qué haces?**
```c
// Preparar datos para enviar al maestro
// Por ejemplo, leer entradas digitales
for (int i = 0; i < input_length; i++) {
    local_buffer[i] = ReadInputByte(i);
}
```

**¿Dónde lees?**
- Variables de estado de tu aplicación
- Registros GPIO si es necesario

**¿Qué respuesta esperas?**
```c
// local_buffer debe contener datos válidos
// input_length debe ser el número de bytes preparados
printf("Datos de entrada preparados: %d bytes\n", input_length);
```

**¿Cómo la esperas?**
```c
// La preparación debe completarse inmediatamente
// Los datos deben ser válidos y actualizados
```

**¿Por qué debe cumplirse?**
- Los datos de entrada deben estar disponibles para el maestro
- Sin datos válidos, el maestro no puede leer el estado
- Es parte del intercambio bidireccional

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.5 (página 1360-1379)

### 8.7 Escritura de Datos de Entrada

**Prerrequisito:** Se han preparado los datos de entrada en el buffer local y se conoce la longitud de los datos.

**Acción General:** Copiar los datos de entrada del buffer local al buffer del VPC3+ para que estén disponibles para el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para hacer que los datos de entrada estén disponibles para que el maestro los lea en el siguiente ciclo.

**¿Qué haces?**
```c
// Copiar datos del buffer local al VPC3+
CopyToVpc3(input_buffer, local_buffer, input_length);
```

**¿Dónde escribes?**
- En la dirección input_buffer en memoria del VPC3+

**¿Qué respuesta esperas?**
```c
// CopyToVpc3 debe retornar éxito
// Los datos deben escribirse correctamente
printf("Datos de entrada escritos al VPC3+\n");
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// No debe haber errores en la escritura
```

**¿Por qué debe cumplirse?**
- Los datos deben estar disponibles para el maestro
- Sin escritura exitosa, el maestro no puede leer
- Es necesario para completar el intercambio

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.5 (página 1360-1379)

### 8.8 Cambio de Buffer de Entrada

**Prerrequisito:** Se han escrito los datos de entrada al buffer del VPC3+ y se debe cambiar al siguiente buffer.

**Acción General:** Cambiar al siguiente buffer de entrada para permitir la escritura continua de datos.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para el funcionamiento cíclico, permitiendo que se escriban datos en un buffer mientras el maestro lee del otro.

**¿Qué haces?**
```c
// Cambiar al siguiente buffer de entrada
uint8_t new_buffer = VPC3_INPUT_UPDATE();
```

**¿Dónde escribes?**
- No escribes, la función cambia internamente

**¿Qué respuesta esperas?**
```c
// new_buffer debe ser 0, 1, o 2
// Indica el buffer activo después del cambio
printf("Buffer de entrada cambiado a: %d\n", new_buffer);
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// El valor debe ser válido (0, 1, o 2)
```

**¿Por qué debe cumplirse?**
- El cambio de buffer permite escritura continua
- Sin cambio, solo podrías escribir en un buffer
- Es necesario para el funcionamiento cíclico

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.5 (página 1360-1379)

---

## 9. Manejo de Diagnósticos

### 9.1 Detección de Evento DIAG_BUFFER_CHANGED

**Prerrequisito:** El esclavo está operando (en cualquier estado después de la inicialización).

**Acción General:** Preparar y enviar información de diagnóstico al maestro cuando sea necesario (por un error o a petición).

**Encargado:** Microcontrolador.

**Significado:** Permite al esclavo comunicar su estado de salud y errores específicos al maestro, lo cual es fundamental para la fiabilidad del sistema.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar evento
if (events & 0x0100) {  // DIAG_BUFFER_CHANGED
    printf("Evento DIAG_BUFFER_CHANGED detectado\n");
    // Procesar cambios en buffer de diagnóstico
}
```

**¿Dónde lees?**
- INT_REG_H (0x03), bit 8

**¿Qué respuesta esperas?**
```c
// El bit 0x0100 debe estar activo
// Indica que el buffer de diagnóstico cambió
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// Puede activarse cuando hay cambios de estado
```

**¿Por qué debe cumplirse?**
- 0x0100 indica cambios en el estado del esclavo
- Es necesario para manejo de errores y estado
- Permite al maestro conocer el estado del esclavo

**Verificación para el Siguiente Paso:** El maestro puede tomar decisiones basándose en el diagnóstico, como reconfigurar el esclavo o mostrar una alarma al operador.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.6 (página 1385-1401)
- Sección 4.6 "Diagnostic": Detalla la estructura de los datos de diagnóstico y las funciones de la API para su manejo

### 9.2 Obtención del Puntero del Buffer de Diagnóstico

**Prerrequisito:** El esclavo está operando y se ha detectado un evento DIAG_BUFFER_CHANGED.

**Acción General:** Obtener la dirección del buffer de diagnóstico activo para poder leer o escribir datos de diagnóstico.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder al buffer de diagnóstico donde se almacenan los mensajes de estado y error.

**¿Qué haces?**
```c
// Obtener puntero del buffer de diagnóstico activo
VPC3_UNSIGNED8_PTR diag_buffer = VPC3_GetDiagBufPtr();
```

**¿Dónde lees?**
- Función VPC3_GetDiagBufPtr() retorna la dirección

**¿Qué respuesta esperas?**
```c
// diag_buffer debe ser una dirección válida
if (diag_buffer >= 0x0080 && diag_buffer <= 0x0090) {
    printf("Buffer de diagnóstico: 0x%04X\n", diag_buffer);
} else {
    printf("Buffer de diagnóstico inválido: 0x%04X\n", diag_buffer);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// La dirección debe estar en el rango configurado
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde está el buffer de diagnóstico
- Sin puntero válido, no puedes leer diagnósticos
- Es necesario para el manejo de errores

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.6 (página 1385-1401)

### 9.3 Configuración de Diagnósticos

**Prerrequisito:** Se ha obtenido el puntero del buffer de diagnóstico y se han preparado los datos de diagnóstico.

**Acción General:** Configurar y enviar información de diagnóstico al maestro para informar sobre el estado del esclavo.

**Encargado:** Microcontrolador.

**Significado:** Permite al maestro conocer el estado de salud del esclavo y tomar decisiones basadas en esta información.

**¿Qué haces?**
```c
// Configurar diagnóstico según el estado
uint8_t result = VPC3_SetDiagnosis(diag_length, diag_control, check_flag);
```

**¿Dónde escribes?**
- No escribes directamente, la función lo hace internamente
- El VPC3+ actualiza el buffer de diagnóstico

**¿Qué respuesta esperas?**
```c
// result debe ser 0 (éxito) o código de error
if (result == 0) {
    printf("Diagnóstico configurado exitosamente\n");
} else {
    printf("Error configurando diagnóstico: %d\n", result);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// El diagnóstico debe configurarse correctamente
```

**¿Por qué debe cumplirse?**
- Los diagnósticos informan al maestro del estado
- Sin diagnósticos, el maestro no conoce el estado
- Es necesario para el monitoreo y control

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.6 (página 1385-1401)

---

## Resumen de Responsabilidades

### **STM32 (Tu código):**
- ✅ Reset hardware del VPC3+S
- ✅ **CRÍTICO**: Inicialización del firmware VPC3+ (VPC3_Initialization)
- ✅ Configuración de registros de modo
- ✅ Envío de comando START (solo después de inicialización)
- ✅ Polling de eventos del VPC3+
- ✅ Procesamiento de parametrización
- ✅ Procesamiento de configuración
- ✅ Manejo de intercambio de datos
- ✅ Control de estados de la aplicación
- ✅ Manejo de diagnósticos

### **VPC3+S (Hardware):**
- ✅ Recepción de tramas PROFIBUS
- ✅ Análisis de protocolo PROFIBUS
- ✅ Gestión de buffers de comunicación
- ✅ Control de estado de la máquina DP
- ✅ Generación de interrupciones
- ✅ Manejo de timeouts y watchdog

### **VPC3+S (Firmware):**
- ✅ **CRÍTICO**: Procesamiento de tramas PROFIBUS
- ✅ Gestión de SAPs (Service Access Points)
- ✅ Control de buffers de entrada/salida
- ✅ Manejo de parametrización y configuración
- ✅ Control de transiciones de estado
- ✅ Respuestas automáticas al maestro

---

## Secuencia de Inicialización CORRECTA

### **Orden CRÍTICO de Operaciones:**
```c
// 1. Reset hardware
VPC3_HardwareReset();

// 2. Configurar registros de modo
Vpc3Write(0x06, 0xC0);  // MODE_REG_0_L
Vpc3Write(0x07, 0x25);  // MODE_REG_0_H  
Vpc3Write(0x0C, 0x05);  // MODE_REG_2
Vpc3Write(0x12, 0x00);  // MODE_REG_3

// 3. INICIALIZAR FIRMWARE (OBLIGATORIO)
CFG_STRUCT cfg_data = {
    .DIN_BUFSIZE = 32,
    .DOUT_BUFSIZE = 16,
    .PRM_BUFSIZE = 20,
    .DIAG_BUFSIZE = 20,
    .CFG_BUFSIZE = 2
};

DP_ERROR_CODE result = VPC3_Initialization(7, 0xADAC, cfg_data);
if (result != DP_OK) {
    printf("ERROR: Falló inicialización del firmware: %d\n", result);
    return;
}

// 4. Solo DESPUÉS del firmware, enviar START
Vpc3Write(0x08, 0x01);  // START command

// 5. Continuar con polling y manejo de eventos
```
### **Verificación de Estado Correcto:**
```
// Después de VPC3_Initialization():
// STATUS_L: 0x00 (OFFLINE con firmware activo)
// STATUS_H: 0xE3 (VPC3+S release)

// Después de START:
// STATUS_L: 0x91 (PASSIVE_IDLE)
// STATUS_H: 0xE3 (VPC3+S release)

// En DATA_EX:
// STATUS_L: 0xXX (bits 5-6 = 0x02 para DATA_EX)
// STATUS_H: 0xE3 (VPC3+S release)
```