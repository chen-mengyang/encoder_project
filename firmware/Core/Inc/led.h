/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    led.h
  * @brief   LED PWM 驱动（TIM1 CH3 / PA10）
  ******************************************************************************
  * @note    TIM1 已由 CubeMX 配置：Prescaler=63, Period=1000 -> 1kHz PWM
  */
/* USER CODE END Header */
#ifndef __LED_H__
#define __LED_H__

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
/* LED 模式枚举 */
typedef enum
{
  LED_MODE_OFF    = 0u,  /* 常暗 */
  LED_MODE_BREATH = 1u,  /* 呼吸灯（正常待机） */
  LED_MODE_BLINK  = 2u,  /* 闪烁（广播设置地址完成 / 错误） */
} LED_ModeTypeDef;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void LED_Init(void);
void LED_SetBrightness(uint8_t pct);   /* 0~100 */
void LED_On(void);
void LED_Off(void);
void LED_SetMode(LED_ModeTypeDef mode); /* 切换模式(BREATH/BLINK/OFF) */
void LED_Task(void);                    /* 主循环调用：驱动呼吸/闪烁 */

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */