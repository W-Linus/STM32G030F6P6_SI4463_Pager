/*-----------------------------------------------------------------------
*@file     POCSAG_Generate.h
*@brief    POCSAG寻呼码生成程序
*@author   谢英男(xieyingnan1994@163.com）
*@version  1.0
*@date     2020/07/22
-----------------------------------------------------------------------*/
#ifndef POCSAG_GENERATE_H
#define POCSAG_GENERATE_H

#include <stdbool.h>
#include <usart.h>

typedef struct Pager_Struct
{
	uint32_t Pager_Address; //Pager Address
	int8_t FuncCode; //Pager Function code
	uint8_t Text[41]; //Message to send
	int8_t Batch20pt; 
	bool InvertOpt;
}POCSAG_MSG;

extern uint32_t POCSAG_Batch1[];	//数组：储存码组1的数据
extern uint32_t POCSAG_Batch2[];	//数组：储存码组2的数据
extern POCSAG_MSG Pager;

#define NILL_DATA            0x7A89C197  //闲置码
#define SYNC_DATA            0x7CD215D8  //同步码
#define DATA_START           12     //串口中数据开始位置
#define TEXT_OR_NUM          11     //串口中文字或数字标志位置
#define SPEED                10     //串口中速率标志位置
#define UARTBUFF_SIZE        400    //串口接收缓冲区
#define TXBUFF_SIZE          200    //TX发送缓冲区

//定义4个功能码
#define FUNC_A_NUMERIC		0	//功能码0:纯数字，提示音1声
#define FUNC_B_CHINESE		1	//功能码1:纯中文，提示音2声
#define FUNC_C_RESERVED		2	//功能码2:保留，仅发出提示音3声
#define FUNC_D_ALPHANUMRIC	3	//功能码3:ASCII字母和数字混合，发出提示音4声
//当码字个数<=16时（仅使用1个码组时），码组2的处置方法
#define BATCH2_TRUNCATE		0	//截断，只保留码组1
#define BATCH2_COPY_BATCH1 	1	//将码组1复制到码组2
#define BATCH2_LEAVE_IDLE 	2	//保持码组2为空闲码不变
//定义若干错误码
#define POCSAG_ERR_NONE					0
#define POCSAG_ERR_INVALID_ADDRESS		-1
#define POCSAG_ERR_INVALID_FUNCCODE		-2
#define POCSAG_ERR_INVALID_BATCH2OPT	-3


POCSAG_MSG prepare_POCSAG_Data(uint8_t* OriginalPack, POCSAG_MSG MSG);

int8_t POCSAG_MakeCodeWords(uint32_t Address, int8_t FuncCode, uint8_t* Text,
							int8_t Batch2Opt, bool InvertOpt);

#ifdef POCSAG_DEBUG_MSG_ON
	    #define POCSAG_DEBUG_MSG(...) printf(__VA_ARGS__)
#else
		#define POCSAG_DEBUG_MSG(...)
#endif
#define POCSAG_ASSERT(STATEVAR) \
	    {if((STATEVAR) != POCSAG_ERR_NONE) { return(STATEVAR);}}

#endif
