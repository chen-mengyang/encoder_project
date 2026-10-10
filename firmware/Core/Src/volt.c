/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    volt.c
  * @brief   电压检测 ADC 驱动（ADC1 IN0 / PA0）
  ******************************************************************************
  * @note    100 点滑动平均：每次 VOLT_Read_mV() 采样 1 次入窗，
  *          窗口未满时按已采点数平均，满 100 后输出稳定平均值。
  *          累加器用 uint32_t（100 x 4095 = 409500 不溢出）。
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "volt.h"
#include "adc.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint16_t s_vBuf[VOLT_AVG_WINDOW];  /* 滑动窗口缓冲 */
static uint8_t  s_vCnt = 0u;              /* 已采样点数 */
static uint8_t  s_vIdx = 0u;              /* 环形写入指针 */
static uint32_t s_vSum = 0u;              /* 窗口累加和 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  初始化电压检测：启动 ADC，清空滑动窗口
  */
void VOLT_Init(void)
{
  uint8_t i;

  HAL_ADC_Start(&hadc1);

  for (i = 0u; i < VOLT_AVG_WINDOW; i++)
  {
    s_vBuf[i] = 0u;
  }
  s_vCnt = 0u;
  s_vIdx = 0u;
  s_vSum = 0u;
}

/**
  * @brief  采样一次电压并返回滑动平均结果
  * @retval 平均电压 mV
  */
uint16_t VOLT_Read_mV(void)
{
  uint16_t raw;
  uint16_t avg_mv;
  uint32_t sum;
  uint8_t  div;

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10u) != HAL_OK)
  {
    raw = s_vBuf[(s_vIdx + VOLT_AVG_WINDOW - 1u) % VOLT_AVG_WINDOW];
  }
  else
  {
    raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  /* 环形窗口滑动：减旧值、加新值 */
  if (s_vCnt >= VOLT_AVG_WINDOW)
  {
    s_vSum -= s_vBuf[s_vIdx];
  }
  s_vBuf[s_vIdx] = raw;
  s_vSum += raw;
  s_vIdx++;
  if (s_vIdx >= VOLT_AVG_WINDOW)
  {
    s_vIdx = 0u;
  }
  if (s_vCnt < VOLT_AVG_WINDOW)
  {
    s_vCnt++;
  }

  sum = s_vSum;
  div = (s_vCnt > 0u) ? s_vCnt : 1u;
  avg_mv = (uint16_t)((sum * VOLT_REF_MV) / ((uint32_t)div * VOLT_ADC_MAX));

  return avg_mv;
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN 3 */

/* USER CODE END 3 */