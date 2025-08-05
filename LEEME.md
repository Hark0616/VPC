
# STM32 + VPC3+S PROFIBUS Slave: Guía de Flujo y Depuración

## Resumen

Este proyecto implementa un esclavo PROFIBUS DP usando un microcontrolador STM32F401 y el ASIC VPC3+S. El firmware inicializa el VPC3+S, gestiona la comunicación PROFIBUS y proporciona logs detallados para depurar problemas de configuración y estado, especialmente durante la fase de verificación de configuración (Chk_Cfg).

---

## 1. Flujo Principal del Programa (`main.c`)

### 1.1. Arranque e Inicialización

- **Inicialización HAL, Reloj y Periféricos:**  
  Se inicializa la HAL de STM32, el reloj del sistema, GPIO, UART2 (para logs) y SPI1 (para el VPC3+S).

- **Redirección de printf a UART:**  
  Todos los logs de depuración se envían por UART2 y pueden verse en la consola serie.

- **Reset Hardware del VPC3+S:**  
  Se realiza un reset físico al VPC3+S mediante su pin RESET (GPIO), manteniéndolo bajo 10ms y luego liberándolo.

- **Lectura Inicial de Registros:**  
  Se leen y muestran los registros de estado iniciales del VPC3+S (0x04, 0x05, etc.) para confirmar que el chip responde.

- **Forzar Estado OFFLINE:**  
  Se fuerza al VPC3+S a entrar en estado OFFLINE escribiendo en su registro de control. Se verifica el estado antes y después.

- **Inicialización de la Pila PROFIBUS:**  
  Se llama a `DpAppl_ProfibusInit()`, que:
  - Inicializa estructuras de datos de aplicación y sistema.
  - Inicializa la configuración, diagnóstico y parámetros.
  - Realiza un test de memoria del VPC3+S.
  - Llama a `VPC3_Initialization()` con la dirección, número de identificación y configuración.
  - Arranca el VPC3+S con `VPC3_Start()`.

- **Lectura Final de Registros:**  
  Tras la inicialización, se vuelven a mostrar los registros del VPC3+S.

- **Instrucciones al Usuario:**  
  La consola imprime instrucciones para configurar el PLC/master (archivo GSD, dirección de esclavo, módulos).

### 1.2. Bucle Principal

- **Procesamiento PROFIBUS:**  
  El bucle principal llama repetidamente a `DpAppl_ProfibusMain()`, que:
  - Refresca los watchdogs.
  - Procesa las máquinas de estado DPV1.
  - Gestiona datos de entrada/salida y transiciones de estado.

- **Logs Periódicos:**  
  Cada 5 segundos, se imprimen los registros del VPC3+S y el estado PROFIBUS actual (OFFLINE, WAIT_CFG, DATA_EXCH, etc.).

---

## 2. Manejo de Configuración y Estados PROFIBUS

### 2.1. Datos de Configuración

- **Buffer de Configuración:**  
  La configuración esperada está definida en `DpCfg.c` y `DpAppl.h`.  
  Ejemplo:  
  ```c
  const uint8_t DpApplDefCfg[DpApplCfgDataLength] = { 0x10, 0x20 };
  ```
  Esto debe coincidir con la configuración enviada por el PLC/master (según el archivo GSD).

- **Tamaños de Buffer:**  
  Los tamaños de buffers (entrada, salida, config, etc.) están en `DpCfg.h`:
  - `DIN_BUFSIZE`, `DOUT_BUFSIZE`, `CFG_BUFSIZE`, etc.

### 2.2. Callback de Chk_Cfg (Verificación de Configuración)

- **Cuándo se Llama:**  
  Cuando el maestro envía un telegrama Chk_Cfg, el VPC3+S pone los datos recibidos a disposición del MCU, que llama a `DpCfg_ChkNewCfgData()`.

- **Qué Hace:**  
  - Imprime los bytes de configuración recibidos y su longitud.
  - Compara la configuración recibida con la esperada (`sDpAppl.sCfgData.abData`).
  - Si hay diferencias de longitud o contenido, lo muestra y marca el error.
  - Si todo es correcto, acepta la configuración y actualiza el estado PROFIBUS.

- **Ejemplo de Log:**
  ```
  DEBUG: Chk_Cfg recibido. Longitud: 2 bytes. Contenido: 0x10 0x20 
  DEBUG: Configuración esperada local (2 bytes): 0x10 0x20 
  EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)
  ```
  O si hay error:
  ```
  EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: 2, recibida: 2
  MISMATCH en byte 1: recibido=0x20, esperado=0xC0
  ```

---

## 3. Comunicación de Bajo Nivel (`vpc3_spi.c`)

- **Funciones SPI:**  
  - `Vpc3Write`/`Vpc3Read`: Escriben/leen un byte en un registro del VPC3+S.
  - `CopyToVpc3`/`CopyFromVpc3`: Escriben/leen arrays de bytes (usados para config, datos, etc.).
  - Todas las operaciones SPI deshabilitan/habilitan las interrupciones PROFIBUS para evitar conflictos.

- **Depuración de Lecturas SPI:**  
  `CopyFromVpc3` ahora imprime la dirección, longitud y todos los bytes leídos del chip, para ver exactamente qué devuelve el VPC3+S.

---

## 4. Diagnóstico y Manejo de Errores

- **Diagnóstico:**  
  - Las estructuras y funciones de diagnóstico están en `DpDiag.c`.
  - Si la configuración es rechazada, se actualizan los diagnósticos y se reporta el error al maestro.

- **Manejo de Errores:**  
  - Si falla la inicialización o configuración, se loguea el error y el sistema se detiene o señala el estado de error.

---

## 5. Ejemplo de Salida en Consola

**Al iniciar:**
```
==============================================================================
    VPC3+S PROFIBUS Slave - Standard Initialization Approach
==============================================================================
Using standard DpAppl_ProfibusInit() for complete initialization

Performing VPC3+S hardware reset...
Hardware reset completed.
Reading initial VPC3+ status...
Status Register (hi,low): 0x0001

Forcing VPC3+ to OFFLINE state...
Status before GO_OFFLINE: 0x01
Status after GO_OFFLINE: 0x04
SUCCESS: VPC3+ is now in OFFLINE state

Initializing PROFIBUS with configuration:
VPC3+S REGISTERS:
  STATUS_L   (0x04): 0x04
  STATUS_H   (0x05): 0x00
  ...
  CFG_PTR    (0x34): 0x00

  - Slave Address: 7 (0x07)
  - Ident Number:  0xADAC
  - GSD File:      Profibus.gsd

PROFIBUS initialization completed successfully!

Reading final VPC3+ status...
VPC3+S REGISTERS:
  STATUS_L   (0x04): 0x10
  STATUS_H   (0x05): 0x00
  ...
  CFG_PTR    (0x34): 0x00

Status Register (hi,low): 0x0010

Entering main operation loop...
The PROFIBUS slave is now ready for master communication.

================= PLC CONFIGURATION REQUIRED =================
1. Load GSD file:     Profibus.gsd
2. Set slave address: 7 (0x07)
3. Connect PROFIBUS cable and activate communication
================================================================
```

**Cuando se recibe Chk_Cfg:**
```
DEBUG: Chk_Cfg recibido. Longitud: 2 bytes. Contenido: 0x10 0x20 
DEBUG: Configuración esperada local (2 bytes): 0x10 0x20 
EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)
```
O si hay error:
```
EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: 2, recibida: 2
MISMATCH en byte 1: recibido=0x20, esperado=0xC0
```

**Cada 5 segundos:**
```
VPC3+S REGISTERS:
  STATUS_L   (0x04): 0x80
  STATUS_H   (0x05): 0x00
  ...
PROFIBUS STATE: STATUS_L = 0x80 -> DATA_EXCH (Intercambio de datos)
```

---

## 6. Cómo se Llega al Error

- El PLC/master envía un telegrama Chk_Cfg con su configuración.
- El VPC3+S pone estos datos a disposición del MCU, que llama a `DpCfg_ChkNewCfgData`.
- La función compara la configuración recibida con la esperada.
- Si hay diferencias (longitud o contenido), se loguea el error, se actualizan los diagnósticos y el esclavo permanece en WAIT_CFG o señala un fallo de configuración.
- El PLC mostrará el esclavo como "no alcanzable" o "error de configuración" si la configuración no es aceptada.

**Causas comunes:**
- El archivo GSD cargado en el PLC no coincide con la configuración esperada por el firmware.
- La dirección de esclavo configurada en el PLC no coincide con `DP_ADDR`.
- El buffer de configuración (`DpApplDefCfg`) no coincide con los módulos configurados en el PLC.

---

## 7. Qué Revisar

- **Logs de consola:**  
  Busca líneas `DEBUG: Chk_Cfg recibido...` y `MISMATCH` para ver exactamente dónde falla la configuración.
- **Archivo GSD y configuración del PLC:**  
  Asegúrate de que los módulos y el orden coincidan con la configuración esperada en el firmware.
- **Dirección de esclavo:**  
  Confirma que el PLC usa la dirección 7 (o la que esté en `DP_ADDR`).
- **Cableado:**  
  Verifica que el cable PROFIBUS esté conectado y correctamente terminado.

---

## 8. Archivos y Funciones Clave

- `main.c`: Arranque, reset hardware, bucle principal, logs.
- `DpAppl.c`: Inicialización de la pila PROFIBUS y procesamiento principal.
- `DpCfg.c`: Datos de configuración, callback de Chk_Cfg, comparación de configuración.
- `vpc3_spi.c`: Comunicación SPI de bajo nivel con el VPC3+S.
- `DpDiag.c`: Diagnóstico y reporte de errores.
- `DpAppl.h`, `DpCfg.h`: Constantes clave, tamaños de buffer y estructuras.

---

## 9. Cómo Continuar la Depuración

- Usa la salida por UART para ver exactamente qué configuración se recibe y cuál se espera.
- Si ves un `DP_CFG_FAULT`, revisa los detalles del mismatch y ajusta el GSD o la configuración del firmware según sea necesario.
- Si el esclavo está "no alcanzable", revisa el cableado, la dirección de esclavo y que el PLC use el archivo GSD correcto.

¡Excelente observación! Aquí tienes una sección adicional para el README, enfocada en **dónde está el problema actual**, qué se observa en los logs, y por qué es confuso el tema de los valores de configuración y los registros. Esto ayudará a tu compañero a entender exactamente **dónde y por qué el programa se queda “atrapado”** y qué debe investigar.

---

## 10. **Situación Actual**

**const uint8_t DpApplDefCfg[DpApplCfgDataLength] **

- El esclavo STM32+VPC3+S arranca correctamente, inicializa el hardware y entra en el bucle principal.
- El PLC (maestro) detecta el esclavo en la red, pero al intentar configurarlo, **la comunicación se detiene en la fase de Chk_Cfg** (“Check Configuration”).
- En la consola UART aparecen logs como estos:

  ```
  DEBUG: Chk_Cfg recibido. Longitud: 2 bytes. Contenido: 0xA0 0x0A 
  DEBUG: Configuración esperada local (2 bytes): 0x10 0x20 
  EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: 2, recibida: 2
  MISMATCH en byte 0: recibido=0xA0, esperado=0x10
  MISMATCH en byte 1: recibido=0x0A, esperado=0x20
  ```

### **¿Por qué es raro?**

- **El firmware espera** que la configuración recibida sea `[0x10, 0x20]` (según el array `DpApplDefCfg`).
- **Pero el PLC envía** `[0xA0, 0x0A]` (o valores distintos, como `[0x10, 0x20]` en otros intentos, dependiendo de la configuración del GSD y los módulos).
- **Esto provoca que la función `DpCfg_ChkNewCfgData` rechace la configuración** y el esclavo nunca pase a estado DATA_EXCH (intercambio de datos).

### **¿Qué puede estar pasando?**

1. **Desfase entre el GSD y el firmware:**  
   El archivo GSD cargado en el PLC no coincide exactamente con la estructura de módulos o los IDs que espera el firmware.  
   - Por ejemplo, el GSD puede estar configurando módulos con IDs `[0xA0, 0x0A]` pero el firmware espera `[0x10, 0x20]`.

2. **Endianess o interpretación de bytes:**  
   En algunos casos, el PLC puede enviar los bytes en un orden diferente, o el firmware puede estar interpretando mal la estructura.

3. **Configuración incorrecta en el PLC:**  
   El usuario puede estar seleccionando módulos o parámetros diferentes a los que espera el firmware.

### **¿Qué buscar en los logs?**

- **`DEBUG: Chk_Cfg recibido...`**  
  Muestra los bytes que realmente llegan del PLC.
- **`DEBUG: Configuración esperada local...`**  
  Muestra lo que el firmware espera.
- **`MISMATCH en byte X: recibido=0xYY, esperado=0xZZ`**  
  Indica exactamente en qué byte hay diferencia.

### **¿Qué hacer para avanzar?**

- **Comparar el archivo GSD y la configuración del PLC** con el array `DpApplDefCfg` en el firmware.  
  - Asegúrate de que los módulos, el orden y los IDs coincidan.
- **Modificar el firmware o el GSD** para que ambos coincidan exactamente.
- **Si los valores recibidos son siempre extraños (ej: 0xA0, 0x0A)**, revisar si el PLC está usando un GSD diferente, o si hay un error en la interpretación de los datos.
- **Verificar si hay logs de error adicionales** en el PLC que den más pistas sobre el rechazo de la configuración.

---
