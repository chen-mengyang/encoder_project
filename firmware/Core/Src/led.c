/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    led.c
  * @brief   LED PWM 驱动（TIM1 CH3 / PA10）
  ******************************************************************************
  * @note    TIM1 CH3：Prescaler=63, Period=1000 -> 1kHz，CCR 0~1000
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "led.h"
#include "tim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_PWM_PERIOD   1000u   /* TIM1 ARR */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
#define LED_BREATH_STEP_MS   8u   /* 呼吸步进周期 */
#define LED_BLINK_PERIOD_MS    500u /* 闪烁周期(亮+灭) */
#define LED_BREATH_AMP         50u  /* 呼吸最大幅度(%) */
#define LED_BREATH_BASE        25u  /* 呼吸基线(%) */

static uint8_t  s_ledPct = 0u;          /* 当前亮度 0~100 */
static uint8_t  s_ledMode = LED_MODE_OFF; /* 当前模式 */
static uint32_t s_ledTick = 0u;         /* 呼吸/闪烁节拍计数器(ms) */
static uint8_t  s_breathDir = 1u;       /* 呼吸方向(1=变亮,-1->255 变暗) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  初始化 LED：启动 TIM1 CH3 PWM，初始熄灭
  */
void LED_Init(void)
{
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0u);
  s_ledPct = 0u;
}

/**
  * @brief  设置 LED 亮度
  * @param  pct 0~100（超范围自动钳位）
  */
void LED_SetBrightness(uint8_t pct)
{
  uint32_t ccr;

  if (pct > 100u)
  {
    pct = 100u;
  }

  s_ledPct = pct;
  ccr = ((uint32_t)pct * LED_PWM_PERIOD) / 100u;
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr);
}

/**
  * @brief  点亮 LED（100% 亮度）
  */
void LED_On(void)
{
  LED_SetBrightness(100u);
}

/**
  * @brief  熄灭 LED（0% 亮度）
  */
void LED_Off(void)
{
  LED_SetBrightness(0u);
}

/**
  * @brief  切换 LED 工作模式
  * @param  mode BREATH/BLINK/OFF
  */
void LED_SetMode(LED_ModeTypeDef mode)
{
  if (mode != s_ledMode)
  {
    s_ledMode = mode;
    s_ledTick = 0u;
    s_breathDir = 1u;
    if (mode == LED_MODE_OFF)
    {
      LED_SetBrightness(0u);
    }
  }
}

/**
  * @brief  驱动呼吸灯 / 闪烁（主循环周期调用）
  * @note   使用 HAL_GetTick 毫秒级节拍；呼吸灯为正弦近似波动，闪烁为固定占空。
  */
void LED_Task(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - s_ledTick) < LED_BREATH_STEP_MS)
  {
    return;
  }
  s_ledTick = now;

  switch (s_ledMode)
  {
    case LED_MODE_BREATH:
    {
      /* 简单三角波呼吸：在 BASE ~ BASE+AMP 之间上升/下降 */
      if (s_ledPct >= (LED_BREATH_BASE + LED_BREATH_AMP))
      {
        s_breathDir = 0u;
      }
      else if (s_ledPct <= LED_BREATH_BASE)
      {
        s_breathDir = 1u;
      }
      if (s_breathDir)
      {
        s_ledPct += 1u;
      }
      else
      {
        s_ledPct -= 1u;
      }
      LED_SetBrightness(s_ledPct);
    }
    break;

    case LED_MODE_BLINK:
    {
      /* 500ms 周期闪烁 */
      static uint8_t blinkOn = 0u;
      if (now % (LED_BLINK_PERIOD_MS * 2u) < LED_BLINK_PERIOD_MS)
      {
        blinkOn = 1u;
      }
      else
      {
        blinkOn = 0u;
      }
      if (blinkOn)
      {
        LED_SetBrightness(50u);
      }
      else
      {
        LED_SetBrightness(0u);
      }
    }
    break;

    case LED_MODE_OFF:
    default:
      LED_SetBrightness(0u);
      break;
  }
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN 3 */

/* USER CODE END 3 */