/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart_protocol.c
  * @brief   USART1 半双工单总线从机协议
  ******************************************************************************
  * @note    帧格式：AA BB ADDR FUNC LEN DATA... SUM（累加和从 ADDR 到 DATA 末字节）
  *          地址匹配规则：
  *            - 单播(ADDR==slaveAddr)：CMD_READ_ANGLE/CMD_READ_VOLT/CMD_READ_STATUS 应答
  *            - 广播(ADDR==0xFF) + CMD_SET_ADDR：写入 EEPROM 并立即生效
  *          从机地址存 24C02 EEPROM 首字节，默认(未配置)为 0x00。
  *          半双工切换：EnableTransmitter -> 发送 -> EnableReceiver。
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "uart_protocol.h"
#include "usart.h"
#include "mt6816.h"
#include "volt.h"
#include "24c02s.h"
#include "led.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 协议配置 */
#define FRAME_HEAD0         0xAAu  /* 帧头第 1 字节 */
#define FRAME_HEAD1         0xBBu  /* 帧头第 2 字节 */
#define MAX_RX_DATA_LEN     60u    /* 接收数据区上限 */
#define MAX_TX_DATA_LEN     4u     /* 应答数据区上限(角度应答最大) */

/* 应答帧缓冲：帧头2 + 地址1 + 功能1 + LEN1 + 数据N + SUM1 */
#define UART1_TX_BUFFER_LEN (2u + 1u + 1u + 1u + MAX_TX_DATA_LEN + 1u)

/* 地址有效性范围（0x00 为广播地址，不可作为从机地址） */
#define SLAVE_ADDR_MIN      0x01u
#define SLAVE_ADDR_MAX      0xFFu
/* USER CODE END PD */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 帧解析状态机 */
typedef enum
{
  FRAME_STATE_WAIT_HEAD0 = 0,
  FRAME_STATE_WAIT_HEAD1,
  FRAME_STATE_WAIT_ADDR,
  FRAME_STATE_WAIT_FUNC,
  FRAME_STATE_WAIT_LEN,
  FRAME_STATE_WAIT_DATA,
  FRAME_STATE_WAIT_SUM
} FrameState_TypeDef;

/* 接收帧结构 */
typedef struct
{
  uint8_t addr;                   /* 地址 */
  uint8_t func;                   /* 功能码 */
  uint8_t len;                    /* 数据长度 */
  uint8_t data[MAX_RX_DATA_LEN];  /* 数据区 */
  uint8_t sum;                    /* 累加和校验 */
} RxFrame_TypeDef;
/* USER CODE END PTD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern uint8_t uartRxByte;                            /* 单字节接收缓冲（main.c 定义） */
static uint8_t UART1_TxBuffer[UART1_TX_BUFFER_LEN];  /* 应答发送缓冲 */

static volatile FrameState_TypeDef frameState = FRAME_STATE_WAIT_HEAD0;  /* 当前解析状态 */
static volatile RxFrame_TypeDef    rxFrame;                              /* 当前帧 */
static volatile uint8_t  dataIdx = 0;       /* 数据区写入指针 */

static volatile uint8_t pendingRsp = RSP_NONE;  /* 待主循环处理的应答类型 */
static volatile uint8_t newAddrValue = 0u;      /* 广播设置的新地址 */

static uint8_t slaveAddr = DEFAULT_SLAVE_ADDR;  /* 本从机地址（EEPROM 首字节） */

/* 调试统计（调试器观察用） */
static volatile uint32_t reqCount = 0;     /* 收到并应答的请求次数 */
static volatile uint32_t txErrCount = 0;   /* 应答发送超时次数 */
static volatile uint32_t eepErrCount = 0;  /* EEPROM 读写失败次数 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static uint8_t UART1_CalcSum(void);
static void    UART1_OnFrameComplete(void);
static void    UART1_ParseByte(uint8_t b);
static void    UART1_SendResponse(uint8_t rsp, const uint8_t *data, uint8_t len);
static uint8_t UART1_BuildStatus(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  半双工从机协议初始化
  * @note   从 24C02 首字节读取本从机地址（无效则使用默认 0x00），
  *         默认接收模式，挂起 USART1 单字节中断接收。
  */
void UART_Protocol_Init(void)
{
  uint8_t eepAddr = 0xFFu;

  /* 读 EEPROM 首字节地址 */
  if (EEPROM_Read(EEPROM_SLAVE_ADDR_OFFSET, &eepAddr, 1u) == HAL_OK)
  {
    if ((eepAddr >= SLAVE_ADDR_MIN) && (eepAddr <= SLAVE_ADDR_MAX))
    {
      slaveAddr = eepAddr;   /* 有效地址 */
    }
    else
    {
      slaveAddr = DEFAULT_SLAVE_ADDR;  /* 0xFF / 0x00 / 其它无效 -> 默认 0x00（未配置） */
    }
  }
  else
  {
    slaveAddr = DEFAULT_SLAVE_ADDR;
    eepErrCount++;
  }

  /* USART1 优先级降为 1：避免高负载持续中断饿死 SysTick（HAL_Delay 依赖） */
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_HalfDuplex_EnableReceiver(&huart1);
  HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
}

/**
  * @brief  获取当前从机地址
  * @retval 从机地址
  */
uint8_t UART_Protocol_GetSlaveAddr(void)
{
  return slaveAddr;
}

/**
  * @brief  主循环任务：处理挂起的应答请求
  * @note   须在 while(1) 中周期调用。
  *         广播设置地址 -> 写 EEPROM + 更新地址 + 应答；读请求 -> 采样后应答。
  */
void UART_Protocol_Task(void)
{
  uint8_t  rsp;
  uint8_t  data[MAX_TX_DATA_LEN];
  uint8_t  len = 0u;
  MT6816_Angle_t angle;
  uint16_t voltMv;

  if (pendingRsp == RSP_NONE)
  {
    return;
  }

  /* 临界区内取走响应类型，避免与中断回调竞争 */
  __disable_irq();
  rsp = pendingRsp;
  pendingRsp = RSP_NONE;
  __enable_irq();

  switch (rsp)
  {
    case RSP_READ_ANGLE:
      /* reply with latest filtered angle (periodically sampled) */
      angle.angle  = MT6816_GetFilteredAngle();
      angle.status = MT6816_GetFilteredStatus();
      data[0] = (uint8_t)((angle.angle >> 8u) & 0x3Fu);  /* 角度高 6 位 */
      data[1] = (uint8_t)(angle.angle & 0xFFu);          /* 角度低 8 位 */
      data[2] = angle.status;                            /* 校验位+No_Mag */
      data[3] = 0x00u;                                   /* 保留 */
      len = 4u;
      UART1_SendResponse(RSP_READ_ANGLE, data, len);
      break;

    case RSP_READ_VOLT:
      /* 电压(mV)，大端 2 字节 */
      voltMv = VOLT_Read_mV();
      data[0] = (uint8_t)((voltMv >> 8u) & 0xFFu);
      data[1] = (uint8_t)(voltMv & 0xFFu);
      len = 2u;
      UART1_SendResponse(RSP_READ_VOLT, data, len);
      break;

    case RSP_READ_STATUS:
      data[0] = UART1_BuildStatus();
      len = 1u;
      UART1_SendResponse(RSP_READ_STATUS, data, len);
      break;

    case RSP_SET_ADDR:
      /* 广播设置地址：写 EEPROM 首字节，成功才更新 RAM 并应答 */
      if (EEPROM_Write(EEPROM_SLAVE_ADDR_OFFSET, (const uint8_t *)&newAddrValue, 1u) == HAL_OK)
      {
        slaveAddr = newAddrValue;
        data[0] = newAddrValue;
        len = 1u;
        UART1_SendResponse(RSP_SET_ADDR, data, len);
        reqCount++;
        /* 配置完成：LED 闪烁提示后自动恢复呼吸（由 LED_Task 处理） */
        LED_SetMode(LED_MODE_BLINK);
      }
      else
      {
        eepErrCount++;
        /* 写失败不应答，主机可重试 */
      }
      break;

    case RSP_GET_ADDR:
      /* 广播获取地址：回应当前从机地址 */
      data[0] = slaveAddr;
      len = 1u;
      UART1_SendResponse(RSP_GET_ADDR, data, len);
      break;

    default:
      break;
  }
}

/**
  * @brief  中断分发：逐字节喂入帧解析状态机
  * @param  b 接收到的字节
  */
void UART_Protocol_FeedByte(uint8_t b)
{
  UART1_ParseByte(b);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  计算帧累加和（地址 ~ 数据末字节，1 字节截断）
  * @retval 校验值
  */
static uint8_t UART1_CalcSum(void)
{
  uint8_t i;
  uint8_t sum = 0;

  sum += rxFrame.addr;
  sum += rxFrame.func;
  sum += rxFrame.len;
  for (i = 0; i < rxFrame.len; i++)
  {
    sum += rxFrame.data[i];
  }
  return sum;
}

/**
  * @brief  一帧接收完成：校验通过后按地址/功能码分发
  * @note   单播(ADDR==slaveAddr)：读角度/电压/状态字 -> 挂起应答
  *         广播(ADDR==0xFF)+CMD_SET_ADDR：保存新地址 -> 挂起写 EEPROM+应答
  *         其余一律静默忽略
  */
static void UART1_OnFrameComplete(void)
{
  uint8_t newAddr;

  if (UART1_CalcSum() != rxFrame.sum)
  {
    return;  /* 校验失败，静默忽略 */
  }

  /* 广播帧：所有从机都会收到 */
  if (rxFrame.addr == BROADCAST_ADDR)
  {
    if (rxFrame.func == CMD_SET_ADDR)
    {
      if ((rxFrame.len == 1u) &&
          (rxFrame.data[0] >= SLAVE_ADDR_MIN) &&
          (rxFrame.data[0] <= SLAVE_ADDR_MAX))
      {
        newAddr = rxFrame.data[0];
        newAddrValue = newAddr;
        pendingRsp = RSP_SET_ADDR;
      }
    }
    else if (rxFrame.func == CMD_GET_ADDR)
    {
      /* 广播获取地址：所有从机都回应自己的地址 */
      pendingRsp = RSP_GET_ADDR;
    }
    return;  /* 广播帧只用于设置/获取地址，其它广播命令不响应 */
  }

  /* 单播：地址必须匹配 */
  if (rxFrame.addr != slaveAddr)
  {
    return;  /* 地址不匹配，静默忽略 */
  }

  switch (rxFrame.func)
  {
    case CMD_READ_ANGLE:
      pendingRsp = RSP_READ_ANGLE;
      break;

    case CMD_READ_VOLT:
      pendingRsp = RSP_READ_VOLT;
      break;

    case CMD_READ_STATUS:
      pendingRsp = RSP_READ_STATUS;
      break;

    default:
      break;  /* 未知命令，静默忽略 */
  }
}

/**
  * @brief  逐字节喂入帧解析状态机（支持粘包/拆包重组）
  * @param  b 接收到的字节
  */
static void UART1_ParseByte(uint8_t b)
{
  switch (frameState)
  {
    case FRAME_STATE_WAIT_HEAD0:
      if (b == FRAME_HEAD0)
      {
        frameState = FRAME_STATE_WAIT_HEAD1;
      }
      break;

    case FRAME_STATE_WAIT_HEAD1:
      if (b == FRAME_HEAD1)
      {
        frameState = FRAME_STATE_WAIT_ADDR;
      }
      else if (b != FRAME_HEAD0)
      {
        /* AA AA BB 情形：当前字节是 0xAA 时保持等待第二个帧头 */
        frameState = FRAME_STATE_WAIT_HEAD0;
      }
      break;

    case FRAME_STATE_WAIT_ADDR:
      rxFrame.addr = b;
      frameState = FRAME_STATE_WAIT_FUNC;
      break;

    case FRAME_STATE_WAIT_FUNC:
      rxFrame.func = b;
      frameState = FRAME_STATE_WAIT_LEN;
      break;

    case FRAME_STATE_WAIT_LEN:
      rxFrame.len = b;
      dataIdx = 0;
      if (rxFrame.len > MAX_RX_DATA_LEN)
      {
        /* 长度非法，丢弃并重新同步 */
        frameState = FRAME_STATE_WAIT_HEAD0;
      }
      else if (rxFrame.len == 0u)
      {
        frameState = FRAME_STATE_WAIT_SUM;
      }
      else
      {
        frameState = FRAME_STATE_WAIT_DATA;
      }
      break;

    case FRAME_STATE_WAIT_DATA:
      rxFrame.data[dataIdx++] = b;
      if (dataIdx >= rxFrame.len)
      {
        frameState = FRAME_STATE_WAIT_SUM;
      }
      break;

    case FRAME_STATE_WAIT_SUM:
      rxFrame.sum = b;
      UART1_OnFrameComplete();
      frameState = FRAME_STATE_WAIT_HEAD0;
      break;

    default:
      frameState = FRAME_STATE_WAIT_HEAD0;
      break;
  }
}

/**
  * @brief  组装应答帧并经半双工串口发送
  * @param  rsp  应答功能码
  * @param  data 应答数据区
  * @param  len  应答数据长度（<= MAX_TX_DATA_LEN）
  */
static void UART1_SendResponse(uint8_t rsp, const uint8_t *data, uint8_t len)
{
  uint8_t i;
  uint8_t sum = 0;

  UART1_TxBuffer[0] = FRAME_HEAD0;
  UART1_TxBuffer[1] = FRAME_HEAD1;
  UART1_TxBuffer[2] = slaveAddr;
  UART1_TxBuffer[3] = rsp;
  UART1_TxBuffer[4] = len;
  for (i = 0; i < len; i++)
  {
    UART1_TxBuffer[5u + i] = data[i];
    sum += data[i];
  }
  sum += UART1_TxBuffer[2] + UART1_TxBuffer[3] + UART1_TxBuffer[4];
  UART1_TxBuffer[5u + len] = sum;

  /* 半双工切换：发射 -> 发送 -> 接收 */
  HAL_HalfDuplex_EnableTransmitter(&huart1);
  if (HAL_UART_Transmit(&huart1, UART1_TxBuffer, (5u + len + 1u), 100u) != HAL_OK)
  {
    txErrCount++;
  }
  HAL_HalfDuplex_EnableReceiver(&huart1);
  reqCount++;
}

/**
  * @brief  构建从机状态字
  * @retval 状态字（STATUS_xxx 位或）
  */
static uint8_t UART1_BuildStatus(void)
{
  uint8_t status = STATUS_OK;
  MT6816_Angle_t angle;

  /* 地址未配置(默认 0x00) */
  if (slaveAddr == DEFAULT_SLAVE_ADDR)
  {
    status |= STATUS_ADDR_UNSET;
  }

  /* MT6816 磁场/校验异常 */
  MT6816_ReadAngle(&angle);
  if (angle.status != 0x00u)
  {
    status |= STATUS_MAG_ERR;
  }

  /* 24C02 EEPROM 通信异常(器件不在线) */
  if (EEPROM_IsReady() != HAL_OK)
  {
    status |= STATUS_EEPROM_ERR;
  }

  return status;
}

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
