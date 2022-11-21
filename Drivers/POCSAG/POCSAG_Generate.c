/*-----------------------------------------------------------------------
*@file     POCSAG_Generate.c
*@brief    POCSAG寻呼码生成程序
*@author   谢英男(xieyingnan1994@163.com）
*@version  1.0
*@date     2020/07/22
-----------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include "POCSAG_Generate.h"

uint32_t POCSAG_Batch1[16] = {0};	//码组1，包含16个码字
uint32_t POCSAG_Batch2[16] = {0};	//码组2，包含16个码字
//定义各个位数对应的掩码表
const uint8_t SizeToMask[7] = {0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f};
//用于大小端判断的共用体和宏
static union{uint8_t c[4];uint32_t l;}endian_test = {{ 'L', '?', '?', 'B' }};
#define ENDIAN_TEST() ((char)endian_test.l)//大端时为B,小端时为L

/*-----------------------------------------------------------------------
*@brief		检查输入参数的合法性
*@param		与上层函数POCSAG_MakeCodeWords()相同
*@retval	错误码，在头文件中定义，无错误时返回POCSAG_ERR_NONE(0)
-----------------------------------------------------------------------*/
static int8_t CheckParamSanity(uint32_t Address, int8_t FuncCode, int8_t Batch2Opt)
{
	if((Address > 0x1FFFFF)||(Address == 0))
		return(POCSAG_ERR_INVALID_ADDRESS);	//地址为21位，且不能为0
	if((FuncCode < FUNC_A_NUMERIC)||(FuncCode > FUNC_D_ALPHANUMRIC))
		return(POCSAG_ERR_INVALID_FUNCCODE); //功能码为0-3
	if((Batch2Opt < BATCH2_TRUNCATE)||(Batch2Opt > BATCH2_LEAVE_IDLE))
		return(POCSAG_ERR_INVALID_BATCH2OPT);	//码组2选项为0-2
	return(POCSAG_ERR_NONE);//参数无错误时返回POCSAG_ERR_NONE(0)
}
/*-----------------------------------------------------------------------
*@brief		将已生成的码字填入码字数组POCSAG_Batch1/2
*@param		codeword_index - 码字在码组中的编号，0-15表示位于第一码组
*           POCSAG_Batch1[]，16-31表示位于第二码组POCSAG_Batch2[]
*           in_codeword - 要填入的码字
*@retval	无
-----------------------------------------------------------------------*/
void StuffCodeWordItem(uint8_t codeword_index, uint32_t in_codeword)
{
	if(codeword_index > 31)
		return;	//范围检查：0-31
	POCSAG_DEBUG_MSG("Stuffed POCSAG_Batch");
	if(codeword_index < 16)
		POCSAG_Batch1[codeword_index] = in_codeword;//0-15表示位于第一码组
	else
		POCSAG_Batch2[codeword_index - 16] = in_codeword;
													//16-31表示位于第二码组
	POCSAG_DEBUG_MSG("%hhu[%hhu] ",(codeword_index<16)?1:2,
					  codeword_index);
	POCSAG_DEBUG_MSG("with %08Xh (",in_codeword);
#ifdef POCSAG_DEBUG_MSG_ON
	for(uint8_t bit=1;bit<=32;bit++,in_codeword<<=1)
	{
		if(in_codeword & 0x80000000)
			POCSAG_DEBUG_MSG("1");
		else
			POCSAG_DEBUG_MSG("0");
		if((bit%8==0) && (bit!=32))
			POCSAG_DEBUG_MSG(" ");
		if(bit == 32)
			POCSAG_DEBUG_MSG("b)\r\n");
	}
#endif
}
/*-----------------------------------------------------------------------
*@brief		计算BCH(31,21),计算偶校验位
*@param		带待计算的码字（低11位已清空为0）
*@retval	计算结果，作为最终填入码字数组中的值
-----------------------------------------------------------------------*/
static uint32_t CreateBCHandParity(uint32_t in)
{
    uint32_t work_cw = in;	//计算用
    uint32_t local_cw = in;	//储存结果用
    uint8_t parity = 0;	//奇偶校验用计数变量

    //1.计算BCH(31,21)
    for (uint8_t bit = 1; bit <= 21; bit++, work_cw <<= 1)
    {
        if (work_cw & 0x80000000)
            work_cw ^= 0xED200000;
    }
    local_cw |= (work_cw >> 21);	//保存计算结果

    //2.计算奇偶校验
    work_cw = local_cw;
    for (int bit = 1; bit <= 32; bit++, work_cw <<= 1)
    {
        if (work_cw & 0x80000000)
            parity++;
    }
    if (parity % 2)	//偶校验：如果1-31位有奇数个1
        local_cw++;//则将末尾32位设为1，以保证整个码字中有偶数个1

    return (local_cw);	//返回结果
}
/*-----------------------------------------------------------------------
*@brief		纯数字类型消息，将ASCII数字字符和部分字母符号转换为非压缩BCD码
*@param		ch - 待转换的字符
*@retval	转换结果，非压缩的BCD码
-----------------------------------------------------------------------*/
static uint8_t CharToBCD(uint8_t ch)
{
	uint8_t retval;

	if(ch >= '0' && ch <= '9')
		retval = ch - '0';
	else
	{
		switch(ch)
		{
			case '*': retval = 0x0A; break;
			case 'U': retval = 0x0B; break;
			case '\x20':retval = 0x0C; break;
			case '-': retval = 0x0D; break;
			case '(': retval = 0x0E; break;
			case ')': retval = 0x0F; break;
			default: retval = 0x0A; break;
		}
	}
	return retval;
}
/*-----------------------------------------------------------------------
*@brief		翻转7位字符位序，对于中文半字符，先减去0xA0，变为区位码后再翻转位序
*@param		ch - 待转换的字符 FuncCode - 类型，中文半字符还是ASCII
*@retval	翻转位序后的结果
-----------------------------------------------------------------------*/
static uint8_t FlipCharBitOrder(uint8_t ch, int8_t FuncCode)
{
	uint8_t result = 0;

	if(FuncCode == FUNC_B_CHINESE && ch != '\x04')
		ch -= 0xA0;//如果字符是中文半字符，需要转换为汉字区位码
	for(uint8_t i = 7;i > 0;i--)//i=7->1,因此i-1=6->0
	{
		if(ch & (1<<(i-1)))	//bit6->bit0遍历
			result |= 1<<6-(i-1);//bit0-bit6判断是否写1
	}
	return result;
}
/*-----------------------------------------------------------------------
*@brief		生成POCSAG码字
*@detail 	1.该子程序只生成码字（包含地址码字和数据码字）。不生成前导码和同步码。
*			2.生成的1组或2组码字保存在数组POCSAG_Batch1/2中。
*			3.未使用的码字按照POCSAG标准，用空闲码字填充。
*			4.输入字符串的长度：ASCII/纯数字ASCII为40个，纯汉字20个(标点和数字
*			应使用全角字符)。
*@param		Address - 地址码，其中低3位表示地址码字开始位置在码组中所在的帧
*           FuncCode - 功能码，表明发送纯数字字符、纯汉字、还是ASCII字符
*           text* - 指向文本的指针，最长40个字符
*           Batch2Opt - 当码字个数<=16时（仅使用1个码组时）码组2的处置方法
*           （截断、重复码组1内容、还是保留并填为空闲码字）          
*           InvertOpt - 数据位是否翻转
*@retval	<=0时为错误码。>0时为生成的码字数据占用的码组个数（1或2）
-----------------------------------------------------------------------*/
int8_t POCSAG_MakeCodeWords(uint32_t Address, int8_t FuncCode, uint8_t* text,
							int8_t Batch2Opt, bool InvertOpt)
{
	uint8_t txt_len;	//文本长度
	uint8_t current_cw_index;	//当前码字序号，从0开始，最大31
	uint8_t coded_len;	//已编码的数据的长度，最大45
	uint8_t coded_buf[45];//储存已编码的数据
	//注：40字符*每字符7位=280位，280位/每码字内储存20位=14码字，
	//每个码字内编码部分占用3个字节，因此编码数据占用空间为14*3=42字节
	//EOT字符还要另占7位，因此向上圆整，coded_buf数组的容量取45字节
	//==========Part0:准备工作==========
	//=====Part0.0:检查参数合法性
	POCSAG_ASSERT(CheckParamSanity(Address,FuncCode,Batch2Opt));
	//=====Part0.1:计算本文长度并在结尾填入EOT控制符
	if((txt_len = strlen((char*)text)) > 40)
		txt_len = 40;	//文本限制在最长40个字符
	uint8_t lastchar = text[txt_len];	//保存字符串最后一个字符的下一个字符
	text[txt_len] = '\x04';	//替换该字符为End-Of-Tranfer控制字符0x04
	txt_len++;	//将EOT字符计入字符串总长度
	//=====Part0.2:将两个码组内的所有码字都填为空闲码字0x7a89c197
	for(uint8_t i = 0;i < 16;i++)
	{
		POCSAG_Batch1[i] = 0x7A89C197;	//将码组1的16个码字填为空闲码字
		POCSAG_Batch2[i] = 0x7A89C197;	//将码组2的16个码字填为空闲码字
	}
	//==========Part1:生成地址码字==========
	//关于地址码字，其格式如下所示：
	//0aaaaaaa aaaaaaaa aaaffccc cccccccp
	//"0" = 固定，表示当前码字包含地址信息
	//a[18] = 地址的高18位
	//f[2] = 两位功能码，表示功能0-3，也表示4个不同的“地址空间”
	//c[10] = 10位BCH(31,21)校验码
	//p = 偶校验位，前面31位中的1位偶数时该位为0，为奇数时该位为1
	//注意：地址的低三位并未在码字中编码。这三位用来表示本地址码字在码组中的
	//的起始位置（帧编号，范围0-7）
	uint32_t address_cw = 0;	//地址码字
	current_cw_index = (Address&0x7)<<1;//由原始地址低3位算出起始码字位置
	address_cw = Address >> 3;	//地址码字为原始地址的高18位
	address_cw <<= 2;	//为功能码空出位置
	address_cw |= FuncCode&0x03;	//在地址码字的后面附加2位功能码
	StuffCodeWordItem(current_cw_index,CreateBCHandParity(address_cw<<11));
							//计算BCH和校验位，填入码字数组
	//==========Part2:转换POCSAG格式编码==========
	//关于消息码字，其格式如下所示：
	//1ttttttt tttttttt tttttccc cccccccp
	//"1" = 固定值，表示当前码字包含已编码的信息
	//t[20] = 20位ASCII文本/BCD格式的数字/汉字区位码
	//c[10] = 10位BCH(31,21)校验码
	//p = 偶校验位，前面31位中的1位偶数时该位为0，为奇数时该位为1
	//=====Part2.1:纯数字类型，将数字字符转换为BCD码，每个码字存储5个BCD码
	if(FuncCode == FUNC_A_NUMERIC)
	{
		uint8_t group_cnt = (txt_len-1)/5;//5个数字一组，求出分组数
		uint8_t residual_cnt = (txt_len-1)%5;//剩下落单的数字数
		//注意：txten-1为字符串从开头到EOT字符（不含）前的所有字符长度
		uint32_t message_bcd_cw;	//保存纯数字BCD码的消息码字
		for(uint8_t group = 0;group < group_cnt;group++)//先解决成组的数字
		{
			message_bcd_cw = 0;//每5个数字为一组，生成5个BCD占用一个码字
			for(uint8_t i = 0;i < 5;i++)
			{
				message_bcd_cw <<= 4;//左移4位让出位置
				message_bcd_cw |= CharToBCD(text[group*5+i])&0x0F;
					//数字字符转换为BCD码，按位追加到码字后面，以便左对齐
			}
			message_bcd_cw <<= 11;//填入5个BCD码后，给BCH和偶校验位让出位置
			message_bcd_cw |= 0x80000000;//最高位置1表示此码字为消息码字
			StuffCodeWordItem(++current_cw_index,
							  CreateBCHandParity(message_bcd_cw));	
		}	//生成BCH和偶校验位后将码字填入码组数组中，总码字计数+1
		if(residual_cnt !=0)//如果有落单的数字，再解决落单的数字
		{
			message_bcd_cw = 0;//这些数字的BCD靠左对齐填入码字，后部空位用0补齐
			for(uint8_t i = 0;i < residual_cnt;i++)
			{
				message_bcd_cw <<= 4;
				message_bcd_cw |= CharToBCD(text[group_cnt*5+i])&0x0F;
			}
			message_bcd_cw <<= (5 - residual_cnt)*4;//将后部空位用0补齐
			message_bcd_cw <<= 11;//填入5个BCD码后，给BCH和偶校验位让出位置
			message_bcd_cw |= 0x80000000;//最高位置1表示此码字为消息码字
			StuffCodeWordItem(++current_cw_index,
							  CreateBCHandParity(message_bcd_cw));	
		}	//生成BCH和偶校验位后将码字填入码组数组中，总码字计数+1
	}
	//=====Part2.2:纯汉字或ASCII字符
	//1.ASCII文本消息使用7位ASCII码(注意：码字中的ASCII码，位序是反的).
	//2.纯中文消息则使用汉字区位码(GB2312-1980国标信息交换用汉字编码)表示。
	//将汉字机内码减去0xA0A0即可得到区位码，例如汉字“谢”的机内码为0xD0,0xBB,
	//将其减去0xA0,0xA0后为0x30,0x1B,换算为10进制后为区位码48,27.
	//3.由于每个码字中最多智能存储20位编码文本数据，相当于3个字符-1位。
	//因此各个7位ASCII字符或区位码头尾相连压缩存储在各个码字中。
	else if((FuncCode == FUNC_B_CHINESE)||
			(FuncCode == FUNC_D_ALPHANUMRIC))
	{
		uint8_t bitcount_out = 7;//能复制的位个数，初始化为7
		uint8_t bytecount_out = 0;	//已复制的字节计数
		uint8_t bitcount_in = 7;	//能提供的位个数，初始化为7
		uint8_t bytecount_in = 0;	//已读入的字节个数
		uint8_t ch = FlipCharBitOrder(text[0],FuncCode);
		uint8_t t;	//临时保存掩码运算结果，包含已提取的几个位
		uint8_t bits2copy;	//要复制多少位
		//保存字符各位翻转后的结果，从输入字串的第一个字符开始
		//对于中文字符，在该函数中将机内码转为区位码后再翻转位序列
		memset(coded_buf,0,sizeof(coded_buf));//清空已编码的数据缓存
		coded_buf[0] = 0x80;	//消息码字的最高位为1
		coded_len = 0;	//已编码的数据长度清零

		while(true)
		{
			//要复制多少个位?地方够么?
			if(bitcount_in > bitcount_out)	//如果能复制的位数比提供的位数少
				bits2copy = bitcount_out;	//将要复制的位数限制在能复制的位数
			else							//如果能复制的位数比提供的位数多
				bits2copy = bitcount_in;	//把提供的位数全部复制
			//复制bits2copy个位，如果需要就左移。掩码运算
			t = ch & (SizeToMask[bits2copy-1]<<(bitcount_in-bits2copy));
			//放哪?如果需要就左移
			if(bitcount_in > bitcount_out)
				t >>= (bitcount_in - bitcount_out);	//右对齐
			else if(bitcount_in < bitcount_out)
				t <<= (bitcount_out - bitcount_in);	//左对齐
			coded_buf[coded_len] |= t;	//存储已提取的位

			bitcount_in -= bits2copy;
			bitcount_out -= bits2copy;	//位计数变量减去已复制的位数

			if(bitcount_out == 0)	//如果装满了一个字节
			{
				bytecount_out++;	//已复制的字节计数+1
				coded_len++;	//已编码的数据长度+1
				switch(bytecount_out)	//根据已复制的字节数散转，决定下一步工作
				{
				case 1:	//已经复制完第1字节，准备复制第2字节
					coded_buf[coded_len] = 0;//将第2字节清零
					bitcount_out = 8;//第2字节要复制8个位
					break;
				case 2: //已经复制完第2字节，准备复制第3字节
					coded_buf[coded_len] = 0;//将第3字节清零
					bitcount_out = 5;//第3字节要复制5个位
					break;
				case 3: default://已经复制完第3字节，回滚复制第1字节
					coded_buf[coded_len] = 0x80;//第一字节最高位填1
					bitcount_out = 7;//第1字节复制7个位
					bytecount_out = 0;//清空字节计数变量，准备回滚
					break;
				}
			}
			if(bitcount_in == 0)	//如果读完了一个字节
			{
				bytecount_in++;	//已读取字节计数+1
				if(bytecount_in < txt_len)//如果还有字符要读入
				{
					ch = FlipCharBitOrder(text[bytecount_in],FuncCode);
										//读入一个字符并翻转位序
					bitcount_in = 7;	//设置可提供的位数为7个
				}
				else	//如果没有更多字符要读入了
				{
					break;	//跳出while循环
				}
			}
		}
		coded_len++;	//将表示下标的变量+1，变为表示长度
		//将已编码数据缓存中每三个字节组成为一组，合成消息码字
		for(uint8_t i = 0;i < coded_len;i += 3)
		{
			uint32_t message_ascii_cw;
			message_ascii_cw = (uint32_t)coded_buf[i]<<24;
			message_ascii_cw |= (uint32_t)coded_buf[i+1]<<16;
			message_ascii_cw |= (uint32_t)coded_buf[i+2]<<11;
			StuffCodeWordItem(++current_cw_index,
							  CreateBCHandParity(message_ascii_cw));
		}
	}
	//==========Part3:后处理==========
	//=====Part3.1:根据选项，决定是否对码组数据进行相位反向
	if(InvertOpt)//如果需要反向，则将每个码字与0xFFFFFFFF异或进行反向
	{
		for(uint8_t i = 0;i < 16;i++)
		{
			POCSAG_Batch1[i] ^= 0xFFFFFFFF;
			POCSAG_Batch2[i] ^= 0xFFFFFFFF;
		}
		POCSAG_DEBUG_MSG("Bit phase is inverted!\r\n");
	}
	//=====Part3.2:POCSAG_Batch1/2的数组元素仅为容器，需转为大端格式
	if(ENDIAN_TEST() == 'L')//如果本机是小端存储结构
	{
		uint32_t temp1;
		union{uint32_t i; uint8_t bytes[4];}temp2;
		for(uint8_t i = 0;i < 16;i++)
		{
			temp1 = POCSAG_Batch1[i];//从数组抽取
			temp2.bytes[0] = (temp1>>24)&0xFF;//颠倒大小端
			temp2.bytes[1] = (temp1>>16)&0xFF;
			temp2.bytes[2] = (temp1>>8)&0xFF;
			temp2.bytes[3] = temp1&0xFF;
			POCSAG_Batch1[i] = temp2.i;//存回数组

			temp1 = POCSAG_Batch2[i];//从数组抽取
			temp2.bytes[0] = (temp1>>24)&0xFF;//颠倒大小端
			temp2.bytes[1] = (temp1>>16)&0xFF;
			temp2.bytes[2] = (temp1>>8)&0xFF;
			temp2.bytes[3] = temp1&0xFF;
			POCSAG_Batch2[i] = temp2.i;//存回数组
		}
		POCSAG_DEBUG_MSG("Little endian is changed to "
						 "big endian!\r\n");
	}
	//=====Part3.3:恢复结尾字符，返回码组个数，程序执行结束
	text[txt_len-1] = lastchar;	//将EOT符号替换回原来字符
	int8_t retval;
	if(current_cw_index < 16)	//如果码字占用小于一个码组
	{
		if(Batch2Opt == BATCH2_TRUNCATE)
			retval = 1;	//返回“占用一个码组”，程序结束
		else if(Batch2Opt == BATCH2_COPY_BATCH1)
		{
			memcpy(POCSAG_Batch2,POCSAG_Batch1,
					16);//将码组1拷贝到码组2
			retval = 2;	//返回“占用两个码组”，程序结束
		}
	}
	else
	{
		retval = 2;	//返回“占用两个码组”，程序结束
	}
	return retval;
}


/*-----------------------------------------------------------------------
*@brief		将串口接收到的原始数据包进行处理
*@param		*OriginalPack 原始数据包
*			Pager POCSAG信息结构体
*@retval	POCSAG_MAG结构体
-----------------------------------------------------------------------*/
POCSAG_MSG prepare_POCSAG_Data(uint8_t* OriginalPack, POCSAG_MSG MSG){
	MSG.FuncCode=(OriginalPack[TEXT_OR_NUM]=='N')?FUNC_A_NUMERIC:FUNC_B_CHINESE;
	MSG.InvertOpt=(OriginalPack[1]=='P')?false:true;
	
	switch(OriginalPack[TEXT_OR_NUM-1]){
		case 'T':
			MSG.Batch20pt=BATCH2_TRUNCATE;
		break;

		case 'C':
			MSG.Batch20pt=BATCH2_COPY_BATCH1;
		break;

		case 'I':
			MSG.Batch20pt=BATCH2_LEAVE_IDLE;
		break;

		default:
			MSG.Batch20pt=BATCH2_TRUNCATE;
		break;
	}

	MSG.Pager_Address = (OriginalPack[2] - '0')*1000000;//提取地址码
    MSG.Pager_Address += (OriginalPack[3] - '0')*100000;
    MSG.Pager_Address += (OriginalPack[4] - '0')*10000;
    MSG.Pager_Address += (OriginalPack[5] - '0')*1000;
    MSG.Pager_Address += (OriginalPack[6] - '0')*100;
    MSG.Pager_Address += (OriginalPack[7] - '0')*10;
    MSG.Pager_Address += (OriginalPack[8] - '0');

	for(int i=DATA_START;i<41;i++){
		if(OriginalPack[i]==0x00&&OriginalPack[i+1]==0x00){
			break;	
		}
		MSG.Text[i-DATA_START]=OriginalPack[i];	
	}

	return MSG;
}
