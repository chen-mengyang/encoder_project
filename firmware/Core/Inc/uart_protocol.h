/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart_protocol.h
  * @brief   USART1 半双工单总线从机协议
  ******************************************************************************
  * @note    帧格式：AA BB ADDR FUNC LEN DATA... SUM
  *          支持主机读角度/读电压/读状态字，以及 0x00 广播设置从机地址
  *          （默认地址 0x00，地址存 24C02 EEPROM 首字节）
  */
/* USER CODE END Header */
#ifndef __UART_PROTOCOL_H__
#define __UART_PROTOCOL_H__

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
/* 功能码枚举：主机<->从机半双工命令 */
typedef enum
{
  CMD_NONE        = 0x00u,  /* 无效命令 */
  CMD_READ_ANGLE  = 0x01u,  /* 主机->从机：读 MT6816 角度 */
  CMD_READ_VOLT   = 0x02u,  /* 主机->从机：读 ADC 电压(mV) */
  CMD_READ_STATUS = 0x03u,  /* 主机->从机：读状态字 */
  CMD_SET_ADDR    = 0x04u,  /* 主机->从机(广播 0x00)：设置本从机地址 */
  CMD_GET_ADDR    = 0x05u,  /* 主机->从机(广播 0x00)：获取本从机地址 */
} CmdType_TypeDef;

/* 应答枚举：从机->主机 */
typedef enum
{
  RSP_NONE        = 0x80u,  /* 无效应答 */
  RSP_READ_ANGLE  = 0x81u,  /* 从机->主机：角度应答 */
  RSP_READ_VOLT   = 0x82u,  /* 从机->主机：电压应答 */
  RSP_READ_STATUS = 0x83u,  /* 从机->主机：状态字应答 */
  RSP_SET_ADDR    = 0x84u,  /* 从机->主机：设置地址完成应答 */
  RSP_GET_ADDR    = 0x85u,  /* 从机->主机：地址应答(广播) */
} RspType_TypeDef;

/* 从机状态字 bit 定义（低字节，供状态字应答） */
#define STATUS_OK            0x00u  /* 正常 */
#define STATUS_ADDR_UNSET    0x01u  /* 地址未配置(EEPROM 无效，当前为默认 0x00) */
#define STATUS_MAG_ERR       0x02u  /* MT6816 无磁场/校验错误 */
#define STATUS_EEPROM_ERR    0x04u  /* 24C02 EEPROM 通信失败(器件不在线) */

/* EEPROM 保存从机地址的偏移（24C02 首字节） */
#define EEPROM_SLAVE_ADDR_OFFSET  0x00u

/* 广播地址：所有从机响应地址设置（0x00 同时是出厂默认地址） */
#define BROADCAST_ADDR        0x00u
/* 从机默认地址 */
#define DEFAULT_SLAVE_ADDR    0x00u
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void UART_Protocol_Init(void);              /* 半双工从机初始化：从 EEPROM 读地址+接收模式 */
void UART_Protocol_Task(void);              /* 主循环调用：处理读请求/广播并应答 */
void UART_Protocol_FeedByte(uint8_t b);     /* 中断分发：逐字节喂入帧解析状态机 */
uint8_t UART_Protocol_GetSlaveAddr(void);   /* 获取当前从机地址 */

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __UART_PROTOCOL_H__ */