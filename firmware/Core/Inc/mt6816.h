/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mt6816.h
  * @brief   MT6816 磁编码器 SPI 驱动
  ******************************************************************************
  * @note    SPI1 已由 CubeMX 配置为 Mode3（CPOL=High / CPHA=2EDGE），
  *          软件 CS 引脚为 PA1（SPI1_NSS）。
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __MT6816_H__
#define __MT6816_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/* MT6816 角度数据 */
typedef struct
{
  uint16_t angle;   /* 14 位角度值 0~16383（360° = 16384） */
  uint8_t  status;  /* bit0=奇校验位 bit1=No_Mag 警告（在 lo 低 2 位） */
} MT6816_Angle_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* MT6816 寄存器读命令 */
#define MT6816_CMD_ANGLE_H   0x83u  /* 角度高字节寄存器 */
#define MT6816_CMD_ANGLE_L   0x84u  /* 角度低字节寄存器 */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void MT6816_Init(void);
void MT6816_ReadAngle(MT6816_Angle_t *ang);
void MT6816_Update(void);       /* periodic: sample + low-pass filter */
uint16_t MT6816_GetFilteredAngle(void); /* get filtered 14-bit angle */
uint8_t  MT6816_GetFilteredStatus(void);/* get latest sample status */

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __MT6816_H__ */