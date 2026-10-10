/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mt6816.c
  * @brief   MT6816 磁编码器 SPI 驱动
  ******************************************************************************
  * @note    读单寄存器：CS 拉低 -> 发命令字节 + 再给 8 个时钟 -> 数据在第二字节返回 -> CS 拉高
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "mt6816.h"
#include "spi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MT6816_SPI_TIMEOUT  10u    /* SPI 传输超时 ms */
#define MT6816_SAMPLE_MS    2u     /* sampling period ms */
#define MT6816_ALPHA        64u    /* low-pass factor /256 = 0.25 */
#define MT6816_CPR          16384u /* counts per rev */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint16_t MT6816_FilteredAngle = 0u;   /* filtered angle 0~16383 */
static uint8_t  MT6816_LastStatus   = 0u;    /* last sampled status */
static uint8_t  MT6816_InitDone     = 0u;    /* filter initialized? */
static uint32_t MT6816_LastTick     = 0u;    /* last sample tick */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static uint8_t MT6816_ReadReg(uint8_t cmd);
/* USER CODE END PFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  初始化 MT6816：CS 拉高（片选释放）
  * @note   SPI1 与 PA1 引脚已在 spi.c / gpio.c 中由 CubeMX 初始化
  */
void MT6816_Init(void)
{
  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
}

/**
  * @brief  读取 MT6816 14 位角度
  * @param  ang 输出角度数据（angle=14 位角度，status=高 2 位状态）
  */
void MT6816_ReadAngle(MT6816_Angle_t *ang)
{
  uint8_t hi;
  uint8_t lo;

  if (ang == NULL)
  {
    return;
  }

  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
  hi = MT6816_ReadReg(MT6816_CMD_ANGLE_H);
  lo = MT6816_ReadReg(MT6816_CMD_ANGLE_L);
  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);

  /* 合并 16 位原始数据：[ANGLE_H<<8 | ANGLE_L]，丢弃低 2 位(bit1=No_Mag / bit0=奇校验)得到 14 位角度 */
  uint16_t raw = ((uint16_t)hi << 8u) | lo;
  ang->angle  = raw >> 2u;
  /* 状态位：bit0=奇校验位，bit1=No_Mag 警告（在 16 位原始数据的低 2 位） */
  ang->status = (uint8_t)(raw & 0x03u);
}

/**
  * @brief  Periodic sample + low-pass filter (call from main loop)
  * @note   1st-order low-pass with wrap-around (0/16383) handling
  */
void MT6816_Update(void)
{
  MT6816_Angle_t a;
  int32_t diff;
  int32_t step;
  int32_t filt;

  if ((HAL_GetTick() - MT6816_LastTick) < MT6816_SAMPLE_MS)
  {
    return;
  }
  MT6816_LastTick = HAL_GetTick();

  MT6816_ReadAngle(&a);
  MT6816_LastStatus = a.status;

  if (!MT6816_InitDone)
  {
    MT6816_FilteredAngle = a.angle;
    MT6816_InitDone = 1u;
    return;
  }

  diff = (int32_t)a.angle - (int32_t)MT6816_FilteredAngle;
  if (diff >  (int32_t)(MT6816_CPR/2)) diff -= (int32_t)MT6816_CPR;
  if (diff <= -(int32_t)(MT6816_CPR/2)) diff += (int32_t)MT6816_CPR;

  step = (diff * (int32_t)MT6816_ALPHA) / 256;
  filt = (int32_t)MT6816_FilteredAngle + step;
  filt %= (int32_t)MT6816_CPR;
  if (filt < 0) filt += (int32_t)MT6816_CPR;
  MT6816_FilteredAngle = (uint16_t)filt;
}

/**
  * @brief  Get filtered 14-bit angle
  */
uint16_t MT6816_GetFilteredAngle(void)
{
  return MT6816_FilteredAngle;
}

/**
  * @brief  Get latest sample status bits (bit1=No_Mag, bit0=parity)
  */
uint8_t MT6816_GetFilteredStatus(void)
{
  return MT6816_LastStatus;
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  通过 SPI 读取 MT6816 一个寄存器
  * @param  cmd 寄存器读命令
  * @retval 寄存器数据（失败返回 0xFF）
  */
static uint8_t MT6816_ReadReg(uint8_t cmd)
{
  uint8_t tx[2] = { cmd, 0x00u };
  uint8_t rx[2] = { 0x00u, 0x00u };

  if (HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2u, MT6816_SPI_TIMEOUT) != HAL_OK)
  {
    return 0xFFu;
  }
  return rx[1];
}