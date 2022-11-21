#ifndef __POCSAG_H_
#define __POCSAG_H_

#include <stdio.h>
#include <stm32g0xx_hal.h>

/*
#define TX_PORT              GPIOA    //数据输出端
#define TX_PIN               GPIO_PIN_1
#define PTT_PORT             GPIOA    //PTT控制端
#define PTT_PIN              GPIO_PIN_2
*/
#define HIGH                 1      //高电平
#define LOW                  0      //低电平
#define NILL_DATA            0x7A89C197  //闲置码
#define SYNC_DATA            0x7CD215D8  //同步码
#define DATA_START           12     //串口中数据开始位置
#define TEXT_OR_NUM          11     //串口中文字或数字标志位置
#define SPEED                10     //串口中速率标志位置
#define UARTBUFF_SIZE        400    //串口接收缓冲区
#define TXBUFF_SIZE          200    //TX发送缓冲区
#define LOWSPEED_TIMER_L     0xE7   //512bit/s时定时器初值，需要微调
#define LOWSPEED_TIMER_H     0x31
#define HIGHTSPEED_TIMER_L   0x1A   //1200bit/s时定时器初值，需要微调
#define HIGHTSPEED_TIMER_H   0xA8

#define LOWSPEED_TIMER       45994/*19541us*/
#define HIGHSPEED_TIMER      57201/*8334us*/

#define BPS_2400             416
#define BPS_1200             833
#define BPS_512              1952

void UartInit(void);//函数声明
void Delay200ms();
uint32_t calc_bch_and_parity(uint32_t cw_e);
uint32_t calc_addr(uint32_t add,uint8_t fun);
void Timer16Init();
void WaitTF0(void);
void Send_Num(uint32_t s);
void GetAddrNumber();
void calc_NumberData();
void SendTxBuff();
void Empty_Buff();
void calc_TextData();
void UartSendString(unsigned char* ch, unsigned char n);
void UART_RxData();
void IntTimer16();




#endif
