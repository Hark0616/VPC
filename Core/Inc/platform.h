/**************************************************************************//**
 * @file    platform.h
 * @brief   Platform Configuration for VPC3+ Profibus Interface on STM32F411
 * @details
 *   This header defines compiler attributes, bus mode settings, strap-pin mappings,
 *   and HAL integrations required to operate the VPC3+S ASIC in SPI mode.
 *
 *   (C) profichip GmbH 2009. Confidential. All rights reserved.
 *   Technical support: support@profichip.com
 *   Phone: +49-9132-744-2150    Fax: +49-9132-744-29150
 *****************************************************************************/

#ifndef PLATFORM_H
#define PLATFORM_H

/**************************************************************************//**
 * @section include_hierarchy  Header Include Hierarchy
 ******************************************************************************/

/*****************************************************************************/
/* VPC3+C/S interface mode                                                   */
/*****************************************************************************/
#define VPC3_SERIAL_MODE        1    /*!< 1: serial (SPI/I²C), 0: parallel */
#define VPC3_SPI                1    /*!< SPI mode */
#define VPC3_I2C                0    /*!< I²C mode (unused) */
#define VPC3_PORT_PINS          0    /*!< Parallel pin-adapt mode disabled */

/**************************************************************************//**
 * @section hal_config  STM32 HAL Specific Configurations
 ******************************************************************************/
#include "stm32f4xx_hal.h"
extern SPI_HandleTypeDef hspi1;

/*****************************************************************************/
/* Data-bus width in serial/parallel modes                                  */
/*****************************************************************************/
#define BUSINTERFACE_8_BIT      1    /*!< 8-bit bus (fixed in serial) */
#define BUSINTERFACE_16_BIT     0    /*!< 16-bit bus (unused)       */
#define BUSINTERFACE_32_BIT     0    /*!< 32-bit bus (unused)       */

/*****************************************************************************/
/* Profibus-driver extensions & interrupt handling                          */
/*****************************************************************************/
#define VPC3_EXTENSION          /*!< Fixed, do not edit */
#if VPC3_SERIAL_MODE
  #define DP_INTERRUPT_MASK_8BIT 0  /*!< Special ACK/NACK in serial */
#else
  #define DP_INTERRUPT_MASK_8BIT 1  /*!< Standard ACK/NACK in parallel */
#endif

/*****************************************************************************/
/* Motorola vs. Intel byte ordering                                          */
/*****************************************************************************/
#define MOTOROLA_MODE           0    /*!< 0=Intel, 1=Motorola */


/*****************************************************************************/
/* Packing macro                                                             */
/*****************************************************************************/
#define _PACKED_               __attribute__((packed))

/*****************************************************************************/
/* Byte ordering                                                             */
/*****************************************************************************/
#define LITTLE_ENDIAN          1    /*!< Low address = low byte */
#define BIG_ENDIAN             0    /*!< Low address = high byte */

/*****************************************************************************/
/* Compiler-specific pointer & const attributes                              */
/*****************************************************************************/
#ifdef __GNUC__
  #undef PTR_ATTR
  #define PTR_ATTR             /* empty */

  #undef VPC3_PTR
  #define VPC3_PTR             *         /*!< Normal pointer */

  #undef VPC3_UNSIGNED8_PTR
  #define VPC3_UNSIGNED8_PTR   uint8_t * /*!< Byte pointer */

  #undef ROMCONST__
  #define ROMCONST__           const     /*!< Standard const */
  #include <stdint.h>
#else
  #define PTR_ATTR             xdata     /*!< ASIC pointer attribute */
  #define VPC3_PTR             PTR_ATTR *
  #define VPC3_UNSIGNED8_PTR   uint8_t PTR_ATTR * /*!< ASIC byte pointer */
  #define ROMCONST__           code      /*!< ROM const attribute */
  #include <stdint.h>
#endif

/*****************************************************************************/
/* Basic type definitions                                                    */
/*****************************************************************************/
#define TRUE                   1
#define FALSE                  0
typedef unsigned char       BOOL;
typedef uint16_t            VPC3_ADR;
#define VPC3_NULL_PTR          ((void VPC3_PTR)0)
#define VPC3_TRUE              TRUE
#define VPC3_FALSE             FALSE

/*****************************************************************************/
/* Memory pointer attributes                                                 */
/*****************************************************************************/
#define MEM_PTR_ATTR           /* empty */
#define MEM_PTR                MEM_PTR_ATTR *
#define MEM_UNSIGNED8_PTR      uint8_t MEM_PTR_ATTR *

/*****************************************************************************/
/* ASIC base-address definitions                                              */
/*****************************************************************************/
#if VPC3_I2C
  #define VPC3_I2C_ADDRESS      ((uint8_t)0x50)  /*!< I2C slave address */
#endif
#if VPC3_SERIAL_MODE
#define VPC3_ASIC_ADDRESS     ((unsigned char *)0x00000000)  /*!< VPC3+S in SPI mode - use zero base */
#else
#define VPC3_ASIC_ADDRESS     ((unsigned char *)0x00028000)  /*!< VPC3+C/parallel */
#endif

/**************************************************************************//**
 * @section strap_pins  Configuration-Strap Pins on VPC3+S ASIC
 ******************************************************************************/
#define VPC3_RESET_PIN         GPIO_PIN_4  /*!< Reset del VPC3+S */
#define VPC3_RESET_PORT        GPIOB

#define VPC3_INT_PIN           GPIO_PIN_5  /*!< Interrupción INT_EV# */
#define VPC3_INT_PORT          GPIOB

#define VPC3_HLDTOK_PIN        GPIO_PIN_6  /*!< Hold Token HLDTOK# */
#define VPC3_HLDTOK_PORT       GPIOB

#define VPC3_SYNC_PIN          GPIO_PIN_7  /*!< Sincronización SYNC */
#define VPC3_SYNC_PORT         GPIOB

/**************************************************************************//**
 * @section spi_pins SPI1 Pin Mapping & Chip-Select Macros
 ******************************************************************************/
#define VPC3_CS_PIN            GPIO_PIN_4
#define VPC3_CS_PORT           GPIOA
#define VPC3_SCK_PIN           GPIO_PIN_5
#define VPC3_SCK_PORT          GPIOA
#define VPC3_MISO_PIN          GPIO_PIN_6
#define VPC3_MISO_PORT         GPIOA
#define VPC3_MOSI_PIN          GPIO_PIN_7
#define VPC3_MOSI_PORT         GPIOA

#define VPC3_CS_LOW()          HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_RESET)
#define VPC3_CS_HIGH()         HAL_GPIO_WritePin(VPC3_CS_PORT, VPC3_CS_PIN, GPIO_PIN_SET)

/*****************************************************************************/
/* Include VPC3 driver headers                                               */
/*****************************************************************************/
#include "DpCfg.h"     /*!< Default Profibus config */
#include "dpl_list.h"  /*!< DP list macros */
#include "dp_if.h"     /*!< VPC3 API & structures */
#include "dp_inc.h"    /*!< Additional macros */

#endif /* PLATFORM_H */
