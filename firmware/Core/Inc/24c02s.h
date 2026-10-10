/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    24c02s.h
  * @brief   24C02S (XBLW) I2C EEPROM 驱动
  ******************************************************************************
  * @note    SOT23-5 封装：无 A0/A1/A2 地址引脚，I2C 器件地址固定 0xA0。
  *          写操作内部按 8 字节页自动分片，并轮询应答等待内部写周期完成。
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __24C02S_H__
#define __24C02S_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define EEPROM_I2C_ADDR      0xA0u      /* 24C02S 固定 I2C 器件地址（含 W bit） */
#define EEPROM_PAGE_SIZE     8u         /* 24C02 页大小 8 字节 */
#define EEPROM_CAPACITY      256u       /* 2Kbit = 256 字节 */

#define EEPROM_TIMEOUT_MS    100u       /* 内部写周期轮询总超时 */
#define EEPROM_TRANS_TIMEOUT 50u        /* 单次 I2C 传输超时 */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void             EEPROM_Init(void);
HAL_StatusTypeDef EEPROM_IsReady(void);   /* 检查 24C02S 是否在线(ACK) */
HAL_StatusTypeDef EEPROM_Write(uint16_t addr, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef EEPROM_Read(uint16_t addr, uint8_t *data, uint16_t len);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __24C02S_H__ */