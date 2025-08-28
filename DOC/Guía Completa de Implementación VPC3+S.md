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

**Verificación para el Siguiente Paso:** El siguiente paso (Configuración de Registros de Modo) requiere que el chip esté en estado `OFFLINE`. Esta condición se cumple si `status_l` es `0x00`.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.6 "Initializing of VPC3+" (página 30, Figura 3-24)
- `vpc3pluss_user-manual.txt` - Sección 3.5 "Memorytest of VPC3+" (describe el valor de reset de STATUS_H como 0xE3 para VPC3+S)

**Documentación en el Manual (`VPC_Software_description.txt`):**
- Sección 3.6 "Initializing of VPC3+": Menciona que el chip debe estar en estado `OFFLINE`.
- Sección 3.5 "Memorytest of VPC3+": Describe el valor de reset de `STATUS_H` (0xE3 para VPC3+S).

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

**Verificación para el Siguiente Paso:** El siguiente paso (Secuencia de START) requiere que el firmware esté completamente inicializado. Esta condición se cumple si `VPC3_Initialization()` devuelve `DP_OK`.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.6 "Initializing of VPC3+" (página 30, Figura 3-24)
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

**Verificación para el Siguiente Paso:** El siguiente paso (Configuración de MODE_REG_0_H) requiere que este registro esté configurado correctamente para establecer la base del modo de operación.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 "Set Hardware Mode" (página 597-604)
- `vpc3pluss_user-manual.txt` - Sección de Mode Registers

### 2.2 Configuración de MODE_REG_0_H (0x07)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`), y MODE_REG_0_L ha sido configurado correctamente.

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

**Verificación para el Siguiente Paso:** El siguiente paso (Configuración de MODE_REG_2) requiere que ambos registros de modo base estén configurados para establecer la configuración completa del hardware.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 "Set Hardware Mode" (página 641-664)

### 2.3 Configuración de MODE_REG_2 (0x0C)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`), y los registros MODE_REG_0_L y MODE_REG_0_H han sido configurados correctamente.

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
// Al leer debe devolver 0x05
uint8_t mode_reg_2 = Vpc3Read(0x0C);  // Debe ser 0x05
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
if (mode_reg_2 == 0x05) {
    printf("MODE_REG_2 configurado correctamente\n");
} else {
    printf("Error en MODE_REG_2: 0x%02X\n", mode_reg_2);
}
```

**¿Por qué debe cumplirse?**
- 0x05 configura modo 2KB RAM
- Habilita interrupción GC después de cada telegrama
- Configura polaridad SYNC negativa
- Habilita verificación de bits reservados

**Verificación para el Siguiente Paso:** El siguiente paso (Configuración de MODE_REG_3) requiere que este registro esté configurado para completar la configuración del modo de operación.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 "Set Hardware Mode" (página 673-698)

### 2.4 Configuración de MODE_REG_3 (0x12)

**Prerrequisito:** El chip ha sido reseteado y se encuentra en estado `OFFLINE` (`STATUS_L == 0x00`), y todos los registros de modo anteriores han sido configurados correctamente.

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
// Al leer debe devolver 0x00
uint8_t mode_reg_3 = Vpc3Read(0x12);  // Debe ser 0x00
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir
if (mode_reg_3 == 0x00) {
    printf("MODE_REG_3 configurado correctamente\n");
} else {
    printf("Error en MODE_REG_3: 0x%02X\n", mode_reg_3);
}
```

**¿Por qué debe cumplirse?**
- 0x00 deshabilita PLL
- Habilita verificación de S_SAP
- Configura interrupción DX_Out después de cada telegrama
- Habilita interrupción GC solo si cambió

**Verificación para el Siguiente Paso:** La función de inicialización del firmware (`VPC3_Initialization`) espera que estos registros de modo ya estén configurados para poder establecer correctamente el entorno de operación del protocolo.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 "Set Hardware Mode" (página 704-723)
- Sección 3.2.10 "Set Hardware Mode": Detalla cada bit de estos registros. El manual enfatiza que solo deben modificarse mientras el chip está `offline`

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

**Verificación para el Siguiente Paso:** El siguiente paso (Procesamiento de Configuración) requiere que la parametrización haya sido aceptada exitosamente por el maestro.

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
```

**¿Por qué debe cumplirse?**
- El maestro espera confirmación de aceptación
- Sin respuesta, el maestro no puede continuar
- Es parte del handshake obligatorio

**Verificación para el Siguiente Paso:** El siguiente paso (Procesamiento de Configuración) requiere que la respuesta de parametrización haya sido enviada exitosamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)

---

## 6. Procesamiento de Configuración

### 6.1 Detección de Evento NEW_CFG_DATA

**Prerrequisito:** La parametrización ha sido aceptada exitosamente por el maestro y se ha enviado la respuesta correspondiente.

**Acción General:** El maestro envía la configuración específica del esclavo. El esclavo debe recibirla, validarla y responder.

**Encargado:** Maestro (inicia), Microcontrolador (procesa y responde).

**Significado:** Este es el segundo paso del handshake. El esclavo le dice al maestro si acepta la configuración específica de entrada/salida.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar evento
uint16_t events = Vpc3Read(INT_REG_L) | (Vpc3Read(INT_REG_H) << 8);

if (events & 0x0400) {  // NEW_CFG_DATA
    printf("Evento NEW_CFG_DATA detectado\n");
    // Procesar configuración
}
```

**¿Dónde lees?**
- INT_REG_L (0x02), bit 6
- INT_REG_H (0x03), bit 10

**¿Qué respuesta esperas?**
```c
// El bit 0x0400 debe estar activo
// Indica que llegó nueva trama de configuración
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// No es necesario esperar, solo detectar cuando esté activo
```

**¿Por qué debe cumplirse?**
- 0x0400 indica que el maestro envió trama Set_Cfg
- Es el segundo paso para establecer comunicación
- Sin configuración válida, no puede continuar

**Verificación para el Siguiente Paso:** El siguiente paso (Obtención del Puntero del Buffer de Configuración) requiere que se haya detectado el evento NEW_CFG_DATA.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1075-1083)

### 6.2 Obtención del Puntero del Buffer de Configuración

**Prerrequisito:** Se ha detectado el evento NEW_CFG_DATA y se debe procesar la configuración.

**Acción General:** Obtener la dirección del buffer de configuración donde están almacenados los datos de configuración enviados por el maestro.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para acceder a los datos de configuración que definen la estructura de entrada/salida del esclavo.

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
// Típicamente 0x00BA para el primer buffer
if (cfg_ptr >= 0x0000 && cfg_ptr <= 0x07FF) {
    printf("Puntero CFG válido: 0x%04X\n", cfg_ptr);
} else {
    printf("Puntero CFG inválido: 0x%04X\n", cfg_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de leer
// Debe ser una dirección válida dentro del rango de memoria del VPC3+
```

**¿Por qué debe cumplirse?**
- El puntero indica dónde están los datos de configuración
- Sin puntero válido, no puedes leer la configuración
- La dirección debe estar dentro del rango de memoria configurado

**Verificación para el Siguiente Paso:** El siguiente paso (Lectura de Datos de Configuración) requiere que se haya obtenido el puntero del buffer de configuración.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1110-1127)

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
// Estructura esperada según GSD:
// [0]: 0x20 (Station_Status_2)
// [1]: 0x10 (Watchdog_Time)

printf("Configuración recibida: 0x%02X 0x%02X\n", local_buffer[0], local_buffer[1]);
```

**¿Cómo la esperas?**
```c
// Verificar que CopyFromVpc3 retorne éxito
// Verificar que los bytes tengan valores esperados
// Verificar que Station_Status_2 sea compatible
```

**¿Por qué debe cumplirse?**
- La configuración define la estructura de entrada/salida
- Station_Status_2 debe ser compatible con el esclavo
- Watchdog_Time afecta el timing de comunicación

**Verificación para el Siguiente Paso:** El siguiente paso (Validación de Configuración) requiere que se hayan leído los datos de configuración del buffer del VPC3+.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1114-1127)
- `vpc3pluss_user-manual.txt` - Sección de Configuration Data

### 6.4 Validación de Configuración

**Prerrequisito:** Se han leído los datos de configuración del buffer del VPC3+ y están disponibles en el buffer local.

**Acción General:** Validar que la configuración recibida es compatible con las capacidades del esclavo y cumple con los requisitos del protocolo.

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
// Debe validar Station_Status_2 y Watchdog_Time
```

**¿Por qué debe cumplirse?**
- Sin validación exitosa, no puede continuar
- La configuración debe ser compatible con las capacidades
- El maestro espera confirmación de aceptación

**Verificación para el Siguiente Paso:** El siguiente paso (Respuesta de Configuración) requiere que la configuración haya sido validada exitosamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)

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
```

**¿Por qué debe cumplirse?**
- El maestro espera confirmación de aceptación
- Sin respuesta, el maestro no puede continuar
- Es parte del handshake obligatorio

**Verificación para el Siguiente Paso:** El siguiente paso (Transición a DATA_EX) requiere que la respuesta de configuración haya sido enviada exitosamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)

---

## 7. Transición a DATA_EX

### 7.1 Envío del Comando GO_OFFLINE

**Prerrequisito:** La configuración ha sido aceptada exitosamente por el maestro y se ha enviado la respuesta correspondiente.

**Acción General:** Poner el VPC3+ en estado OFFLINE temporalmente para cambiar la configuración de los registros de modo antes de la transición final a DATA_EX.

**Encargado:** Microcontrolador (STM32).

**Significado:** Es necesario para poder modificar los registros de modo que solo pueden cambiarse mientras el chip está offline.

**¿Qué haces?**
```c
// Enviar comando GO_OFFLINE
Vpc3Write(0x08, 0x04);  // GO_OFFLINE command
```

**¿Dónde escribes?**
- Registro 0x08 (Control Register)

**¿Qué respuesta esperas?**
```c
// STATUS_L debe cambiar a 0x00 (OFFLINE)
// STATUS_H debe mantenerse en 0xE3
uint8_t status_l = Vpc3Read(0x04);  // Debe ser 0x00
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
        printf("Timeout esperando OFFLINE\n");
        return 0;
    }
} while (status_l != 0x00);

printf("VPC3+ en OFFLINE\n");
```

**¿Por qué debe cumplirse?**
- 0x00 indica que el VPC3+ está en estado OFFLINE
- Solo en este estado se pueden modificar los registros de modo
- Es necesario para la transición a DATA_EX

**Verificación para el Siguiente Paso:** El siguiente paso (Configuración de Registros de Modo para DATA_EX) requiere que el chip esté en estado OFFLINE.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.7 (página 873-878)
- `vpc3pluss_user-manual.txt` - Sección de Go Offline Command

### 7.2 Configuración de Registros de Modo para DATA_EX

**Prerrequisito:** El VPC3+ está en estado OFFLINE (`STATUS_L == 0x00`) después de enviar el comando GO_OFFLINE.

**Acción General:** Configurar los registros de modo para habilitar la operación en modo DATA_EX, incluyendo la habilitación de interrupciones y la configuración de buffers.

**Encargado:** Microcontrolador (STM32).

**Significado:** Es necesario para preparar el chip para la operación normal de intercambio de datos con el maestro.

**¿Qué haces?**
```c
// Configurar registros de modo para DATA_EX
Vpc3Write(0x06, 0xC0);  // MODE_REG_0_L: modo DP, watchdog 10ms
Vpc3Write(0x07, 0x25);  // MODE_REG_0_H: timer 1ms, EOI timing
Vpc3Write(0x0C, 0x05);  // MODE_REG_2: 2KB RAM, interrupciones GC
Vpc3Write(0x12, 0x00);  // MODE_REG_3: sin PLL, verificación S_SAP
```

**¿Dónde escribes?**
- Registro 0x06 (MODE_REG_0_L)
- Registro 0x07 (MODE_REG_0_H)
- Registro 0x0C (MODE_REG_2)
- Registro 0x12 (MODE_REG_3)

**¿Qué respuesta esperas?**
```c
// Al leer cada registro debe devolver el valor escrito
uint8_t mode_reg_0_l = Vpc3Read(0x06);  // Debe ser 0xC0
uint8_t mode_reg_0_h = Vpc3Read(0x07);  // Debe ser 0x25
uint8_t mode_reg_2 = Vpc3Read(0x0C);    // Debe ser 0x05
uint8_t mode_reg_3 = Vpc3Read(0x12);    // Debe ser 0x00

if (mode_reg_0_l == 0xC0 && mode_reg_0_h == 0x25 && 
    mode_reg_2 == 0x05 && mode_reg_3 == 0x00) {
    printf("Registros de modo configurados para DATA_EX\n");
} else {
    printf("Error en configuración de registros de modo\n");
}
```

**¿Cómo la esperas?**
```c
// Verificar inmediatamente después de escribir cada registro
// Todos los registros deben mantener los valores escritos
```

**¿Por qué debe cumplirse?**
- Los registros de modo definen el comportamiento del chip
- Solo se pueden modificar mientras está offline
- Son necesarios para la operación en modo DATA_EX

**Verificación para el Siguiente Paso:** El siguiente paso (Envío del Comando START para DATA_EX) requiere que todos los registros de modo estén configurados correctamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.2.10 "Set Hardware Mode" (página 597-723)
- `vpc3pluss_user-manual.txt` - Sección de Mode Registers

### 7.3 Envío del Comando START para DATA_EX

**Prerrequisito:** Los registros de modo han sido configurados correctamente para la operación en modo DATA_EX.

**Acción General:** Poner el VPC3+ "en línea" nuevamente para que comience a operar en modo DATA_EX y pueda intercambiar datos con el maestro.

**Encargado:** Microcontrolador (STM32).

**Significado:** El chip transiciona del estado OFFLINE al estado DATA_EX, donde está listo para el intercambio normal de datos.

**¿Qué haces?**
```c
// Enviar comando START para DATA_EX
Vpc3Write(0x08, 0x01);  // START command
```

**¿Dónde escribes?**
- Registro 0x08 (Control Register)

**¿Qué respuesta esperas?**
```c
// STATUS_L debe cambiar a 0x91 (PASSIVE_IDLE)
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
        printf("Timeout esperando PASSIVE_IDLE para DATA_EX\n");
        return 0;
    }
} while (status_l != 0x91);

printf("VPC3+ en PASSIVE_IDLE para DATA_EX\n");
```

**¿Por qué debe cumplirse?**
- 0x91 indica que el VPC3+ está en estado PASSIVE_IDLE
- El chip está listo para recibir tramas del maestro
- La comunicación física está establecida para DATA_EX

**Verificación para el Siguiente Paso:** El siguiente paso (Intercambio de Datos) requiere que el chip esté en estado PASSIVE_IDLE y listo para DATA_EX.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 3.7 (página 873-878)
- `vpc3pluss_user-manual.txt` - Sección de Start Command

---

## 8. Intercambio de Datos

### 8.1 Detección de Eventos de Datos

**Prerrequisito:** El VPC3+ está en estado PASSIVE_IDLE (`STATUS_L == 0x91`) y configurado para operación en modo DATA_EX.

**Acción General:** Monitorear continuamente los eventos de interrupción para detectar cuando hay datos de entrada o salida disponibles para procesar.

**Encargado:** Microcontrolador (STM32).

**Significado:** Es necesario para detectar y procesar las tramas de datos que llegan del maestro o que deben enviarse al maestro.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar eventos de datos
uint16_t events = Vpc3Read(INT_REG_L) | (Vpc3Read(INT_REG_H) << 8);

if (events & 0x0200) {  // NEW_DX_IN_DATA
    printf("Evento NEW_DX_IN_DATA detectado\n");
    // Procesar datos de entrada
}

if (events & 0x0100) {  // NEW_DX_OUT_DATA
    printf("Evento NEW_DX_OUT_DATA detectado\n");
    // Procesar datos de salida
}
```

**¿Dónde lees?**
- INT_REG_L (0x02), bit 5 (NEW_DX_IN_DATA)
- INT_REG_L (0x02), bit 4 (NEW_DX_OUT_DATA)

**¿Qué respuesta esperas?**
```c
// Los bits 0x0200 y 0x0100 deben estar activos cuando corresponda
// Indican que hay datos de entrada o salida disponibles
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar los bits
// No es necesario esperar, solo detectar cuando estén activos
```

**¿Por qué debe cumplirse?**
- 0x0200 indica que llegaron datos de entrada del maestro
- 0x0100 indica que hay datos de salida para enviar al maestro
- Son necesarios para el intercambio normal de datos

**Verificación para el Siguiente Paso:** El siguiente paso (Procesamiento de Datos de Entrada) requiere que se haya detectado el evento NEW_DX_IN_DATA.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1075-1083)
- `vpc3pluss_user-manual.txt` - Sección de Data Exchange Events

### 8.2 Procesamiento de Datos de Entrada

**Prerrequisito:** Se ha detectado el evento NEW_DX_IN_DATA y se deben procesar los datos de entrada recibidos del maestro.

**Acción General:** Obtener la dirección del buffer de datos de entrada, leer los datos y procesarlos según la aplicación específica.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para recibir y procesar los datos de control enviados por el maestro al esclavo.

**¿Qué haces?**
```c
// Obtener puntero del buffer de datos de entrada
uint16_t dx_in_ptr = Vpc3Read(0x38);  // DX_IN_PTR register

// Leer datos del buffer
uint8_t input_data[32];  // Buffer local para datos de entrada
CopyFromVpc3(input_data, dx_in_ptr, 32);  // Copiar 32 bytes

// Procesar datos según la aplicación
ProcessInputData(input_data, 32);
```

**¿Dónde lees?**
- Registro 0x38 (DX_IN_PTR)
- Desde la dirección dx_in_ptr en memoria del VPC3+

**¿Qué respuesta esperas?**
```c
// dx_in_ptr debe ser una dirección válida en el rango 0x0000-0x07FF
// input_data debe contener los datos válidos del maestro

if (dx_in_ptr >= 0x0000 && dx_in_ptr <= 0x07FF) {
    printf("Puntero DX_IN válido: 0x%04X\n", dx_in_ptr);
    printf("Datos de entrada recibidos: ");
    for (int i = 0; i < 32; i++) {
        printf("0x%02X ", input_data[i]);
    }
    printf("\n");
} else {
    printf("Puntero DX_IN inválido: 0x%04X\n", dx_in_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar que dx_in_ptr sea una dirección válida
// Verificar que CopyFromVpc3 retorne éxito
// Verificar que los datos tengan sentido para la aplicación
```

**¿Por qué debe cumplirse?**
- Los datos de entrada contienen comandos del maestro
- Son necesarios para controlar el comportamiento del esclavo
- Sin procesamiento, el esclavo no responde a comandos

**Verificación para el Siguiente Paso:** El siguiente paso (Procesamiento de Datos de Salida) puede ejecutarse independientemente, pero requiere que el procesamiento de datos de entrada haya sido exitoso.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1110-1127)
- `vpc3pluss_user-manual.txt` - Sección de Input Data Processing

### 8.3 Procesamiento de Datos de Salida

**Prerrequisito:** Se ha detectado el evento NEW_DX_OUT_DATA y se deben preparar los datos de salida para enviar al maestro.

**Acción General:** Obtener la dirección del buffer de datos de salida, preparar los datos según la aplicación específica y escribirlos en el buffer.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para enviar datos de estado o respuesta al maestro, completando el ciclo de intercambio de datos.

**¿Qué haces?**
```c
// Obtener puntero del buffer de datos de salida
uint16_t dx_out_ptr = Vpc3Read(0x3A);  // DX_OUT_PTR register

// Preparar datos de salida según la aplicación
uint8_t output_data[16];  // Buffer local para datos de salida
PrepareOutputData(output_data, 16);  // Función específica de la aplicación

// Escribir datos en el buffer del VPC3+
CopyToVpc3(dx_out_ptr, output_data, 16);  // Copiar 16 bytes
```

**¿Dónde escribes?**
- Registro 0x3A (DX_OUT_PTR)
- Hacia la dirección dx_out_ptr en memoria del VPC3+

**¿Qué respuesta esperas?**
```c
// dx_out_ptr debe ser una dirección válida en el rango 0x0000-0x07FF
// CopyToVpc3 debe retornar éxito

if (dx_out_ptr >= 0x0000 && dx_out_ptr <= 0x07FF) {
    printf("Puntero DX_OUT válido: 0x%04X\n", dx_out_ptr);
    printf("Datos de salida preparados: ");
    for (int i = 0; i < 16; i++) {
        printf("0x%02X ", output_data[i]);
    }
    printf("\n");
} else {
    printf("Puntero DX_OUT inválido: 0x%04X\n", dx_out_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar que dx_out_ptr sea una dirección válida
// Verificar que CopyToVpc3 retorne éxito
// Verificar que los datos tengan sentido para la aplicación
```

**¿Por qué debe cumplirse?**
- Los datos de salida contienen respuestas del esclavo
- Son necesarios para informar el estado al maestro
- Sin datos de salida, el maestro no conoce el estado del esclavo

**Verificación para el Siguiente Paso:** El siguiente paso (Manejo de Diagnósticos) puede ejecutarse independientemente, pero requiere que el procesamiento de datos de salida haya sido exitoso.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1110-1127)
- `vpc3pluss_user-manual.txt` - Sección de Output Data Processing

---

## 9. Manejo de Diagnósticos

### 9.1 Detección de Eventos de Diagnóstico

**Prerrequisito:** El VPC3+ está operando en modo DATA_EX y puede recibir solicitudes de diagnóstico del maestro.

**Acción General:** Monitorear continuamente los eventos de interrupción para detectar cuando el maestro solicita información de diagnóstico del esclavo.

**Encargado:** Microcontrolador (STM32).

**Significado:** Es necesario para responder a las solicitudes de diagnóstico del maestro y mantener la comunicación estable.

**¿Qué haces?**
```c
// En función VPC3_Poll(), detectar eventos de diagnóstico
uint16_t events = Vpc3Read(INT_REG_L) | (Vpc3Read(INT_REG_H) << 8);

if (events & 0x0080) {  // NEW_DIAG_DATA
    printf("Evento NEW_DIAG_DATA detectado\n");
    // Procesar solicitud de diagnóstico
}
```

**¿Dónde lees?**
- INT_REG_L (0x02), bit 3 (NEW_DIAG_DATA)

**¿Qué respuesta esperas?**
```c
// El bit 0x0080 debe estar activo
// Indica que el maestro solicitó información de diagnóstico
```

**¿Cómo la esperas?**
```c
// En polling continuo, verificar el bit
// No es necesario esperar, solo detectar cuando esté activo
```

**¿Por qué debe cumplirse?**
- 0x0080 indica que el maestro solicitó diagnóstico
- Es necesario para mantener la comunicación estable
- Permite al maestro conocer el estado del esclavo

**Verificación para el Siguiente Paso:** El siguiente paso (Preparación de Datos de Diagnóstico) requiere que se haya detectado el evento NEW_DIAG_DATA.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1075-1083)
- `vpc3pluss_user-manual.txt` - Sección de Diagnostic Events

### 9.2 Preparación de Datos de Diagnóstico

**Prerrequisito:** Se ha detectado el evento NEW_DIAG_DATA y se deben preparar los datos de diagnóstico para enviar al maestro.

**Acción General:** Obtener la dirección del buffer de diagnóstico, preparar los datos de diagnóstico según la aplicación específica y escribirlos en el buffer.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para informar al maestro sobre el estado actual del esclavo y cualquier condición de error o advertencia.

**¿Qué haces?**
```c
// Obtener puntero del buffer de diagnóstico
uint16_t diag_ptr = Vpc3Read(0x3C);  // DIAG_PTR register

// Preparar datos de diagnóstico según la aplicación
uint8_t diag_data[20];  // Buffer local para datos de diagnóstico
PrepareDiagnosticData(diag_data, 20);  // Función específica de la aplicación

// Escribir datos en el buffer del VPC3+
CopyToVpc3(diag_ptr, diag_data, 20);  // Copiar 20 bytes
```

**¿Dónde escribes?**
- Registro 0x3C (DIAG_PTR)
- Hacia la dirección diag_ptr en memoria del VPC3+

**¿Qué respuesta esperas?**
```c
// diag_ptr debe ser una dirección válida en el rango 0x0000-0x07FF
// CopyToVpc3 debe retornar éxito

if (diag_ptr >= 0x0000 && diag_ptr <= 0x07FF) {
    printf("Puntero DIAG válido: 0x%04X\n", diag_ptr);
    printf("Datos de diagnóstico preparados: ");
    for (int i = 0; i < 20; i++) {
        printf("0x%02X ", diag_data[i]);
    }
    printf("\n");
} else {
    printf("Puntero DIAG inválido: 0x%04X\n", diag_ptr);
}
```

**¿Cómo la esperas?**
```c
// Verificar que diag_ptr sea una dirección válida
// Verificar que CopyToVpc3 retorne éxito
// Verificar que los datos tengan sentido para la aplicación
```

**¿Por qué debe cumplirse?**
- Los datos de diagnóstico informan el estado del esclavo
- Son necesarios para el mantenimiento y monitoreo
- Sin diagnóstico, el maestro no puede detectar problemas

**Verificación para el Siguiente Paso:** El siguiente paso (Envío de Respuesta de Diagnóstico) requiere que se hayan preparado los datos de diagnóstico correctamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.2 (página 1110-1127)
- `vpc3pluss_user-manual.txt` - Sección de Diagnostic Data Preparation

### 9.3 Envío de Respuesta de Diagnóstico

**Prerrequisito:** Se han preparado los datos de diagnóstico y están disponibles en el buffer del VPC3+.

**Acción General:** Enviar la respuesta de diagnóstico al maestro a través del VPC3+ para completar la solicitud de diagnóstico.

**Encargado:** Microcontrolador.

**Significado:** Es necesario para completar la solicitud de diagnóstico del maestro y mantener la comunicación estable.

**¿Qué haces?**
```c
// Enviar respuesta de diagnóstico
uint8_t response = VPC3_SET_DIAG_DATA_OK();
```

**¿Dónde escribes?**
- No escribes directamente, la función lo hace internamente
- El VPC3+ responde automáticamente al maestro

**¿Qué respuesta esperas?**
```c
// response debe ser 0 (éxito) o código de error
if (response == 0) {
    printf("Respuesta de diagnóstico enviada\n");
} else {
    printf("Error enviando respuesta de diagnóstico: %d\n", response);
}
```

**¿Cómo la esperas?**
```c
// La función debe retornar inmediatamente
// Debe indicar éxito o fallo en el envío
```

**¿Por qué debe cumplirse?**
- El maestro espera confirmación de la respuesta de diagnóstico
- Sin respuesta, el maestro puede considerar el esclavo como no funcional
- Es necesario para mantener la comunicación estable

**Verificación para el Siguiente Paso:** El siguiente paso (Continuación del Ciclo de Datos) puede ejecutarse independientemente, pero requiere que la respuesta de diagnóstico haya sido enviada exitosamente.

**Manual de referencia:**
- `VPC_Software_description.txt` - Sección 4.2.1 (página 1094-1097)
- `vpc3pluss_user-manual.txt` - Sección de Diagnostic Response

---

## Resumen de Verificaciones Críticas

### Puntos de Verificación por Fase

**Fase 1: Inicialización del Sistema**
- ✅ Reset hardware exitoso: `STATUS_L=0x00, STATUS_H=0xE3`
- ✅ Firmware inicializado: `VPC3_Initialization()` retorna `DP_OK`
- ✅ Estado post-inicialización: `STATUS_L=0x00, STATUS_H=0xE3`

**Fase 2: Configuración de Registros**
- ✅ MODE_REG_0_L configurado: `0x06 = 0xC0`
- ✅ MODE_REG_0_H configurado: `0x07 = 0x25`
- ✅ MODE_REG_2 configurado: `0x0C = 0x05`
- ✅ MODE_REG_3 configurado: `0x12 = 0x00`

**Fase 3: Secuencia de START**
- ✅ Comando START enviado: `0x08 = 0x01`
- ✅ Estado PASSIVE_IDLE alcanzado: `STATUS_L=0x91`
- ✅ Baudrate detectado y estable

**Fase 4: Handshake con Maestro**
- ✅ Parametrización aceptada: `NEW_PRM_DATA` procesado
- ✅ Configuración aceptada: `NEW_CFG_DATA` procesado
- ✅ Transición a DATA_EX exitosa

**Fase 5: Operación Normal**
- ✅ Datos de entrada procesados: `NEW_DX_IN_DATA`
- ✅ Datos de salida procesados: `NEW_DX_OUT_DATA`
- ✅ Diagnósticos manejados: `NEW_DIAG_DATA`

### Códigos de Error Comunes

**Errores de Inicialización:**
- `DP_NOT_OFFLINE_ERROR`: VPC3+ no está en estado OFFLINE
- `DP_ADDRESS_ERROR`: Dirección de esclavo inválida
- `DP_CALCULATE_IO_ERROR`: Error en cálculo de buffers
- `DP_LESS_MEM_ERROR`: Memoria insuficiente

**Errores de Estado:**
- `STATUS_L != 0x00/0x91`: Estado incorrecto para la operación
- `STATUS_H != 0xE3`: Chip no identificado como VPC3+S
- `CTRL(0x08) != 0x00`: Registro de control no limpio

**Errores de Comunicación:**
- `SPI transfer failed`: Fallo en comunicación SPI
- `HAL_TIMEOUT`: Timeout en operación SPI
- `CopyToVpc3/CopyFromVpc3 failed`: Fallo en transferencia de memoria

### Recomendaciones de Debugging

1. **Verificar Comunicación Básica Primero:**
   - Reset hardware y lectura de registros de estado
   - Verificar configuración SPI (CPOL/CPHA, velocidad)
   - Confirmar que `STATUS_L=0x00, STATUS_H=0xE3` post-reset

2. **Verificar Inicialización del Firmware:**
   - Confirmar que `VPC3_Initialization()` retorna `DP_OK`
   - Verificar estado post-inicialización
   - Revisar logs de error si falla

3. **Verificar Secuencia de START:**
   - Confirmar transición a `PASSIVE_IDLE` (`STATUS_L=0x91`)
   - Verificar detección de baudrate estable
   - Revisar timing y delays

4. **Verificar Handshake:**
   - Confirmar procesamiento de `NEW_PRM_DATA`
   - Confirmar procesamiento de `NEW_CFG_DATA`
   - Verificar transición exitosa a DATA_EX

5. **Verificar Operación Normal:**
   - Confirmar procesamiento de `NEW_DX_IN_DATA`
   - Confirmar procesamiento de `NEW_DX_OUT_DATA`
   - Verificar manejo de `NEW_DIAG_DATA`

### Referencias de Manuales

**Documentación Principal:**
- `VPC_Software_description.txt`: Descripción completa del software y protocolo
- `vpc3pluss_user-manual.txt`: Manual de usuario del chip VPC3+S

**Secciones Críticas:**
- Sección 3.6 "Initializing of VPC3+": Inicialización del sistema
- Sección 3.2.10 "Set Hardware Mode": Configuración de registros de modo
- Sección 3.7 "Start Command": Secuencia de START
- Sección 4.2.1 "Event Handling": Manejo de eventos
- Sección 4.2.2 "Buffer Management": Gestión de buffers

**Notas Importantes:**
- Todos los registros de modo solo pueden modificarse mientras el chip está `OFFLINE`
- La inicialización del firmware es crítica y debe retornar `DP_OK`
- El handshake con el maestro es obligatorio antes de DATA_EX
- El polling continuo es necesario para detectar eventos
- Los timeouts y delays son críticos para la estabilidad

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