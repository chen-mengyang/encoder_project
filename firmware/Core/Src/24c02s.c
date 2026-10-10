/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    24c02s.c
  * @brief   24C02S (XBLW) I2C EEPROM 驱动
  ******************************************************************************
  * @note    SOT23-5 封装无 A0/A1/A2，器件地址固定 0xA0。
  *          WP 写保护引脚 = PC15（EEPROM_WP），初始化时拉低使能写入。
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "24c02s.h"
#include "i2c.h"

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void EEPROM_WaitWriteCycle(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  初始化 24C02S：WP 写保护引脚拉低
  * @note   I2C1 已在 i2c.c 中由 CubeMX 初始化
  */
void EEPROM_Init(void)
{
  HAL_GPIO_WritePin(EEPROM_WP_GPIO_Port, EEPROM_WP_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  检查 24C02S 是否在线（发送器件地址并等待 ACK）
  * @retval HAL_OK 器件在线；HAL_TIMEOUT/HAL_ERROR 器件无响应
  */
HAL_StatusTypeDef EEPROM_IsReady(void)
{
  return HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 3u, EEPROM_TRANS_TIMEOUT);
}

/**
  * @brief  写数据到 24C02S，自动按 8 字节页分片
  * @param  addr  起始地址（0x00 ~ 0xFF）
  * @param  data  数据指针
  * @param  len   字节数
  * @retval HAL_OK 成功；HAL_ERROR/HAL_TIMEOUT 失败
  */
HAL_StatusTypeDef EEPROM_Write(uint16_t addr, const uint8_t *data, uint16_t len)
{
  HAL_StatusTypeDef st = HAL_OK;
  uint8_t retries;

  if (data == NULL)
  {
    return HAL_ERROR;
  }

  while (len > 0u)
  {
    uint16_t page_remain = EEPROM_PAGE_SIZE - (addr % EEPROM_PAGE_SIZE);
    uint16_t chunk = (len < page_remain) ? len : page_remain;

    /* 每页最多重试 3 次，提高抗干扰能力 */
    retries = 3u;
    do
    {
      st = HAL_I2C_Mem_Write(&hi2c1, EEPROM_I2C_ADDR, (uint16_t)addr,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, chunk,
                             EEPROM_TRANS_TIMEOUT);
      if (st == HAL_OK)
      {
        break;
      }
      HAL_Delay(1u);  /* 短暂等待后重试 */
    } while (--retries > 0u);

    if (st != HAL_OK)
    {
      break;
    }

    /* 等待内部写周期完成（轮询器件应答） */
    EEPROM_WaitWriteCycle();

    addr += chunk;
    data += chunk;
    len  -= chunk;
  }

  return st;
}

/**
  * @brief  从 24C02S 读取数据
  * @param  addr  起始地址（0x00 ~ 0xFF）
  * @param  data  接收缓冲区
  * @param  len   字节数
  * @retval HAL_OK 成功；HAL_ERROR/HAL_TIMEOUT 失败
  */
HAL_StatusTypeDef EEPROM_Read(uint16_t addr, uint8_t *data, uint16_t len)
{
  if (data == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, (uint16_t)addr,
                          I2C_MEMADD_SIZE_8BIT, data, len,
                          EEPROM_TRANS_TIMEOUT);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  轮询等待 24C02S 内部写周期结束
  * @note   EEPROM 在内部写周期期间不应答，轮询直到收到应答或超时
  */
static void EEPROM_WaitWriteCycle(void)
{
  uint32_t timeout = EEPROM_TIMEOUT_MS;

  while (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 1u, 1u) != HAL_OK)
  {
    if (--timeout == 0u)
    {
      break;
    }
  }
}