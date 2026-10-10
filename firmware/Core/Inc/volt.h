/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    volt.h
  * @brief   电压检测 ADC 驱动（ADC1 IN0 / PA0）
  ******************************************************************************
  * @note    ADC1 已由 CubeMX 配置：12 位，单次采样。
  *          本模块内部维护 100 点滑动平均滤波，每次调用采样 1 次。
  */
/* USER CODE END Header */
#ifndef __VOLT_H__
#define __VOLT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define VOLT_REF_MV        3300u   /* 参考电压 mV */
#define VOLT_ADC_MAX       4095u   /* 12 位 ADC 满量程 */
#define VOLT_AVG_WINDOW    100u    /* 滑动平均窗口 */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void     VOLT_Init(void);
uint16_t VOLT_Read_mV(void);   /* 每次调用采样 1 次并返回 100 点滑动平均电压(mV) */

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __VOLT_H__ */