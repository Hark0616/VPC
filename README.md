## Overview

This project implements a PROFIBUS DP slave using an STM32F401 microcontroller and the VPC3+S ASIC. The firmware is designed to initialize the VPC3+S, handle PROFIBUS communication, and provide detailed debug output for troubleshooting configuration and state issues, especially during the Chk_Cfg (configuration check) phase.

---

## 1. Main Program Flow (`main.c`)

### 1.1. Startup and Initialization

- **HAL_Init, Clock, and Peripherals:**  
  The STM32 HAL is initialized, and the system clock, GPIO, UART2 (for debug), and SPI1 (for VPC3+S) are configured.

- **UART Debug Output:**  
  `printf` is redirected to UART2, so all debug logs appear on the serial console.
S
- **VPC3+S Hardware Reset:**  
  The VPC3+S is reset via its RESET pin (GPIO), held low for 10ms, then released and allowed to stabilize.

- **Initial Register Dump:**  
  The program reads and prints the initial VPC3+S status registers (0x04, 0x05, etc.) to confirm the chip is alive.

- **Force OFFLINE State:**  
  The VPC3+S is commanded to enter the OFFLINE state by writing to its control register. Status is checked before and after.

- **PROFIBUS Stack Initialization:**  
  The function `DpAppl_ProfibusInit()` is called. This:
  - Initializes application and system data structures.
  - Initializes configuration, diagnostics, and parameter handling.
  - Performs a VPC3+S memory test.
  - Calls `VPC3_Initialization()` to set up the chip with the slave address, ident number, and configuration.
  - Starts the VPC3+S with `VPC3_Start()`.

- **Final Register Dump:**  
  After initialization, the VPC3+S registers are printed again.

- **User Instructions:**  
  The console prints instructions for configuring the PLC/master (GSD file, slave address, modules).

### 1.2. Main Loop

- **PROFIBUS Processing:**  
  The main loop repeatedly calls `DpAppl_ProfibusMain()`, which:
  - Triggers watchdogs.
  - Processes DPV1 state machines.
  - Handles input/output data and state transitions.

- **Periodic Debug Output:**  
  Every 5 seconds, the program prints the VPC3+S registers and the current PROFIBUS state (OFFLINE, WAIT_CFG, DATA_EXCH, etc.).

---

## 2. PROFIBUS Configuration and State Handling

### 2.1. Configuration Data

- **Configuration Buffer:**  
  The expected configuration is defined in `DpCfg.c` and `DpAppl.h`.  
  Example:  
  ```c
  const uint8_t DpApplDefCfg[DpApplCfgDataLength] = { 0x10, 0x20 };
  ```
  This must match the configuration sent by the PLC/master (as defined in the GSD file).

- **Buffer Sizes:**  
  Buffer sizes (input, output, config, etc.) are defined in `DpCfg.h`:
  - `DIN_BUFSIZE`, `DOUT_BUFSIZE`, `CFG_BUFSIZE`, etc.

### 2.2. Chk_Cfg (Configuration Check) Callback

- **When Called:**  
  When the master sends a Chk_Cfg telegram, the VPC3+S makes the received config data available and calls `DpCfg_ChkNewCfgData()`.

- **What Happens:**  
  - The function prints the received config bytes and their length.
  - Compares the received config with the expected config (`sDpAppl.sCfgData.abData`).
  - If lengths or contents mismatch, it logs the differences and sets the error state.
  - If OK, it logs acceptance and updates the PROFIBUS state.

- **Debug Output Example:**
  ```
  DEBUG: Chk_Cfg recibido. Longitud: 2 bytes. Contenido: 0x10 0x20 
  DEBUG: Configuración esperada local (2 bytes): 0x10 0x20 
  EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)
  ```
  Or, if there is a mismatch:
  ```
  EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: 2, recibida: 2
  MISMATCH en byte 1: recibido=0x20, esperado=0xC0
  ```

---

## 3. Low-Level Communication (`vpc3_spi.c`)

- **SPI Functions:**  
  - `Vpc3Write`/`Vpc3Read`: Write/read a single byte to/from a VPC3+S register.
  - `CopyToVpc3`/`CopyFromVpc3`: Write/read arrays of bytes (used for config, data, etc.).
  - All SPI operations are protected by disabling/enabling PROFIBUS interrupts.

- **Debugging SPI Reads:**  
  `CopyFromVpc3` now prints the address, length, and all bytes read from the chip, so you can see exactly what the VPC3+S is returning.

---

## 4. Diagnostics and Error Handling

- **Diagnostics:**  
  - Diagnostic structures and functions are in `DpDiag.c`.
  - When configuration is rejected, diagnostic entries are updated and reported to the master.

- **Error Handling:**  
  - If initialization or configuration fails, the error is logged and the system halts or signals the error state.

---

## 5. Typical Console Output

**On startup:**
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

**When Chk_Cfg is received:**
```
DEBUG: Chk_Cfg recibido. Longitud: 2 bytes. Contenido: 0x10 0x20 
DEBUG: Configuración esperada local (2 bytes): 0x10 0x20 
EVENT: Chk_Cfg recibido y aceptado (DP_CFG_OK)
```
Or, if there is a mismatch:
```
EVENT: Chk_Cfg recibido y RECHAZADO (DP_CFG_FAULT) - Longitud esperada: 2, recibida: 2
MISMATCH en byte 1: recibido=0x20, esperado=0xC0
```

**Every 5 seconds:**
```
VPC3+S REGISTERS:
  STATUS_L   (0x04): 0x80
  STATUS_H   (0x05): 0x00
  ...
PROFIBUS STATE: STATUS_L = 0x80 -> DATA_EXCH (Intercambio de datos)
```

---

## 6. How the Error Happens

- The PLC/master sends a Chk_Cfg telegram with its configuration.
- The VPC3+S makes this data available to the MCU, which calls `DpCfg_ChkNewCfgData`.
- The function compares the received config with the expected config.
- If there is a mismatch (in length or content), it logs the error, updates diagnostics, and the slave remains in WAIT_CFG or signals a configuration fault.
- The PLC will show the slave as "not reachable" or "configuration error" if the config is not accepted.

**Common causes:**
- The GSD file loaded in the PLC does not match the firmware's expected configuration.
- The slave address set in the PLC does not match `DP_ADDR`.
- The configuration buffer (`DpApplDefCfg`) does not match the modules configured in the PLC.

---

## 7. What to Check Next

- **Console logs:**  
  Look for `DEBUG: Chk_Cfg recibido...` and `MISMATCH` lines to see exactly where the config fails.
- **GSD file and PLC config:**  
  Ensure the modules and order match the expected config in the firmware.
- **Slave address:**  
  Confirm the PLC is using address 7 (or whatever is set in `DP_ADDR`).
- **Wiring:**  
  Confirm the PROFIBUS cable is connected and terminated properly.

---

## 8. Key Files and Functions

- `main.c`: Startup, hardware reset, main loop, debug output.
- `DpAppl.c`: PROFIBUS stack initialization and main processing.
- `DpCfg.c`: Configuration data, Chk_Cfg callback, config comparison.
- `vpc3_spi.c`: Low-level SPI communication with VPC3+S.
- `DpDiag.c`: Diagnostics and error reporting.
- `DpAppl.h`, `DpCfg.h`: Key constants, buffer sizes, and structures.

---

## 9. How to Continue Debugging

- Use the UART console output to see exactly what config is received and expected.
- If you see a `DP_CFG_FAULT`, check the mismatch details and adjust the GSD or firmware config as needed.
- If the slave is "not reachable", check wiring, slave address, and that the PLC is using the correct GSD file.

---
