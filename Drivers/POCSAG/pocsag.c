#include <pocsag.h>

uint32_t TxBuff[TXBUFF_SIZE] = {0};//TX发送缓冲区
uint8_t Tx_Num;//地址码发射次序
uint8_t beep;//功能位，1，2，3，4
uint8_t UartBuff[UARTBUFF_SIZE] = "#P12345674HT0001$";//串口缓冲区
uint16_t UartCount = 13;//串口接收计数
uint8_t UartTmp;
uint8_t NewData;
uint8_t TM0_FLAG;
uint8_t SBUF;
uint8_t TI;
uint16_t Timer_Counter;

extern uint8_t uartBuffer[255];

uint16_t txSpeed;

uint32_t calc_bch_and_parity(uint32_t cw_e) //BCH校验和奇偶校验函数
{ 
    uint8_t i;
    uint8_t  parity = 0; //奇偶校验计数    
    uint32_t local_cw; //临时存放数      
    local_cw=cw_e;//保存cw_e参数值
    for(i=1;i<=21; i++,cw_e<<=1)       
        if (cw_e & 0x80000000) cw_e ^= 0xED200000;  
    cw_e=cw_e&0xFFC00000;//保留前10位，BCH校验值共11位，只保留前10位有效数据      
    local_cw |= (cw_e >> 21); //BCH校验数移至第22位到31位，BCH共10位，并和原始数据相加
    cw_e=local_cw;     
    for(i=0; i<31; i++, cw_e<<=1) if(cw_e&0x80000000) parity++;        
    if(parity%2) local_cw+=1;//从1至31位判断为奇数则后面加1补充为偶数
    return local_cw;     
}  


uint32_t calc_addr(uint32_t add,uint8_t fun) //地址转换，第1参数为地址，第2参数为功能
{
    uint32_t adr;
    uint32_t tem;    
    Tx_Num=(uint8_t)(add&0x00000007);//获取地址发射的帧位次，111位第7帧，后3位地址数据隐藏不发送，接收按帧位还原
    adr=0x00;
    adr=add&0xFFFFFFF8;    //去掉地址码后3位
    adr=adr<<10;  //地址左移10位
    tem=0x00;
    tem=fun;  //功能位
    tem=tem<<11;//功能位左移11位，功能位为00 01 10 11四种状态，代表4个地址码
    adr=adr|tem; //地址码和功能位合成地址码；
    return adr;
}


void Send_Num(uint32_t s)//发送数据
{
   uint8_t n; 
   for (n=0;n<32;n++)
    {  
      if(s&0x80000000)
        {
            /*if(UartBuff[1] == 'N')//判断正负相位，下同
                //HAL_GPIO_WritePin(TX_PORT,TX_PIN,GPIO_PIN_RESET);
                //TX = LOW;
            else
                //HAL_GPIO_WritePin(TX_PORT,TX_PIN,GPIO_PIN_SET);
            // TX= HIGH;
        }
        else
        {
            if(UartBuff[1] == 'N')
                //HAL_GPIO_WritePin(TX_PORT,TX_PIN,GPIO_PIN_SET);
                //TX = HIGH;
            else
             HAL_GPIO_WritePin(TX_PORT,TX_PIN,GPIO_PIN_RESET);
                //TX=LOW;*/
        }
        //HAL_Delay_us(txSpeed);
      //WaitTF0();//等待延时结束 0.833ms                    
      s<<=1;
    }   
}


void Empty_Buff()//清空缓冲区
{
    uint16_t i;
    for(i = TEXT_OR_NUM; i < UARTBUFF_SIZE; i++)
    {
        UartBuff[i] = 0x00000000;
    }
    for(i = TEXT_OR_NUM; i < TXBUFF_SIZE; i++)
    {
        TxBuff[i] = 0x00000000;
    }
}


void GetAddrNumber()
{
  uint8_t i;
    uint32_t tem;
    uint32_t addr_tmp;
    
    addr_tmp = (UartBuff[2] - '0')*1000000;//提取地址码
    addr_tmp += (UartBuff[3] - '0')*100000;
    addr_tmp += (UartBuff[4] - '0')*10000;
    addr_tmp += (UartBuff[5] - '0')*1000;
    addr_tmp += (UartBuff[6] - '0')*100;
    addr_tmp += (UartBuff[7] - '0')*10;
    addr_tmp += (UartBuff[8] - '0');
    
    beep = UartBuff[9] - '0';//提取功能位

    tem=calc_addr(addr_tmp,beep);//前面是地址码，后面是BB机内00 01 10 11  代表0，1，2，3种不同的声音
    
    for(i = 0; i < 8; i++)
    {
        if(i == Tx_Num)//地址码放入对应的帧中
            TxBuff[i*2] = calc_bch_and_parity(tem);//取得BCH校验后的地址码序列 
        else
        {
            TxBuff[i*2] = NILL_DATA;//其他帧填充闲置码
            TxBuff[i*2+1] = NILL_DATA;
        }
    }
}


void calc_NumberData() //计算数值数据（数字机）
{
        uint32_t Num_Negate[UARTBUFF_SIZE] = {0};
    uint16_t i, j, k, n;
    uint8_t byte_tmp[10], byte_tmp_negate[10];
    float TxCount = 0.0;
    
    for(i = DATA_START; i < UARTBUFF_SIZE; i++)  //表意字符替换
    {
        if(UartBuff[i] == 'A')
            UartBuff[i] = 0x0A;
        if(UartBuff[i] == 'B')
            UartBuff[i] = 0x0B;
        if(UartBuff[i] == 'C')
            UartBuff[i] = 0x0C;
        if(UartBuff[i] == 'D')
            UartBuff[i] = 0x0D;
        if(UartBuff[i] == 'E')
            UartBuff[i] = 0x0E;
        if(UartBuff[i] == 'F')
            UartBuff[i] = 0x0F;
    }
    
    n = DATA_START;
    for(i = DATA_START; i < UARTBUFF_SIZE; i++)      //数字的ASIIC码只用到第四位，所以将所有数字的低四位合并，高四位丢弃
    {
        if(i % 2 == 0)
            UartBuff[n] = UartBuff[i] << 4;
        else
        {
            UartBuff[n] |= UartBuff[i] & 0x0f;
            n++;
        }
    }
    
    k = Tx_Num * 2 + 1;                               //定位到地址码后数据所在的位置
    for(i = DATA_START; i < UARTBUFF_SIZE; i += 5)                      //按四位取出并放在高四位准备倒序
    {
        byte_tmp[0] = (UartBuff[i] & 0xf0);
        byte_tmp[1] = (UartBuff[i] & 0x0f) << 4;
        byte_tmp[2] = (UartBuff[i + 1] & 0xf0);
        byte_tmp[3] = (UartBuff[i + 1] & 0x0f) << 4;
        byte_tmp[4] = (UartBuff[i + 2] & 0xf0);
        byte_tmp[5] = (UartBuff[i + 2] & 0x0f) << 4;
        byte_tmp[6] = (UartBuff[i + 3] & 0xf0);
        byte_tmp[7] = (UartBuff[i + 3] & 0x0f) << 4;
        byte_tmp[8] = (UartBuff[i + 4] & 0xf0);
        byte_tmp[9] = (UartBuff[i + 4] & 0x0f) << 4;
        
        for(j = 0; j < 10; j++)                           //倒序
        {
            for(n = 0; n < 8; n++)
            {
                byte_tmp_negate[j] <<= 1;
                byte_tmp_negate[j] |= (byte_tmp[j] >> n) & 0x01;
            }
        }
        
        Num_Negate[k] = 0x80000000;                                        //重新组合
        Num_Negate[k] |= (uint32_t)byte_tmp_negate[0] << 27;
        Num_Negate[k] |= (uint32_t)byte_tmp_negate[1] << 23;
        Num_Negate[k] |= (uint32_t)byte_tmp_negate[2] << 19;
        Num_Negate[k] |= (uint32_t)byte_tmp_negate[3] << 15;
        Num_Negate[k] |= (uint32_t)byte_tmp_negate[4] << 11;
        Num_Negate[k + 1] = 0x80000000;
        Num_Negate[k + 1] |= (uint32_t)byte_tmp_negate[5] << 27;
        Num_Negate[k + 1] |= (uint32_t)byte_tmp_negate[6] << 23;
        Num_Negate[k + 1] |= (uint32_t)byte_tmp_negate[7] << 19;
        Num_Negate[k + 1] |= (uint32_t)byte_tmp_negate[8] << 15;
        Num_Negate[k + 1] |= (uint32_t)byte_tmp_negate[9] << 11;
        k = k + 2;
        
    }
    
    //UartCount = 25;
    n = Tx_Num * 2 + 1;                                                   //将数据合并到Tx发送缓冲区并计算校验码
    TxCount = 0.0;
    for(i = Tx_Num * 2 + 1; i < UARTBUFF_SIZE; i++)
    {
        if(TxCount >= (UartCount - DATA_START - 1))
            break;
        if(i % 16 == 0)                                                     //每8帧（16个码字）插入一个同步码
            TxBuff[n++] = SYNC_DATA;
        TxBuff[n++] = calc_bch_and_parity( Num_Negate[i]);
        TxCount += 5;                                                     //串口接收5个数字相当于POCSAG码中一个码字
        if(n >= TXBUFF_SIZE || i >= UARTBUFF_SIZE)                        //超出缓冲区退出
            break;
    }
    

}

void calc_TextData()  //计算汉字数据
{
    uint32_t Text_Negate[UARTBUFF_SIZE] = {0};
    uint16_t n, i, k, byte_tmp;
    float TxCount = 0.0;
    for(k = DATA_START; k < UARTBUFF_SIZE; k++)         //字节按位倒序
    {
        byte_tmp = UartBuff[k];
        UartBuff[k] = 0;
        for(n = 0; n < 8; n++)   //倒序
        {
            UartBuff[k] <<= 1;
            UartBuff[k] |= (byte_tmp >> n) & 0x01;
        }
        UartBuff[k] &= 0xFE;    //倒序后丢弃最低位
    }

    k = Tx_Num * 2 + 1;
    for(n = DATA_START; n < UARTBUFF_SIZE; n += 20)                 //移位，20个字节一循环
    {
        Text_Negate[k] = 0x80000000;
        Text_Negate[k] |= (uint32_t)UartBuff[n] << 23;
        Text_Negate[k] |= (uint32_t)UartBuff[n + 1] << 16;
        Text_Negate[k] |= (uint32_t)UartBuff[n + 2] << 9;
        Text_Negate[k] &= 0xfffff800;
        
        Text_Negate[k + 1] = 0x80000000;
        Text_Negate[k + 1] |= (uint32_t)UartBuff[n + 2] << 29;
        Text_Negate[k + 1] &= 0xC0000000;
        Text_Negate[k + 1] |= (uint32_t)UartBuff[n + 3] << 22;
        Text_Negate[k + 1] |= (uint32_t)UartBuff[n + 4] << 15;
        Text_Negate[k + 1] |= (uint32_t)UartBuff[n + 5] << 8;
        Text_Negate[k + 1] &= 0xfffff800;
        
        Text_Negate[k + 2] = 0x80000000;
        Text_Negate[k + 2] |= (uint32_t)UartBuff[n + 5] << 28;
        Text_Negate[k + 2] &= 0xE0000000;
        Text_Negate[k + 2] |= (uint32_t)UartBuff[n + 6] << 21;
        Text_Negate[k + 2] |= (uint32_t)UartBuff[n + 7] << 14;
        Text_Negate[k + 2] |= (uint32_t)UartBuff[n + 8] << 7;
        Text_Negate[k + 2] &= 0xfffff800;
        
        Text_Negate[k + 3] = 0x80000000;
        Text_Negate[k + 3] |= (uint32_t)UartBuff[n + 8] << 27;
        Text_Negate[k + 3] &= 0xF0000000;
        Text_Negate[k + 3] |= (uint32_t)UartBuff[n + 9] << 20;
        Text_Negate[k + 3] |= (uint32_t)UartBuff[n + 10] << 13;
        Text_Negate[k + 3] |= (uint32_t)UartBuff[n + 11] << 6;
        Text_Negate[k + 3] &= 0xfffff800;
        
        Text_Negate[k + 4] = 0x80000000;
        Text_Negate[k + 4] |= (uint32_t)UartBuff[n + 11] << 26;
        Text_Negate[k + 4] &= 0xF8000000;
        Text_Negate[k + 4] |= (uint32_t)UartBuff[n + 12] << 19;
        Text_Negate[k + 4] |= (uint32_t)UartBuff[n + 13] << 12;
        Text_Negate[k + 4] |= (uint32_t)UartBuff[n + 14] << 5;
        Text_Negate[k + 4] &= 0xfffff800;
        
        Text_Negate[k + 5] = 0x80000000;
        Text_Negate[k + 5] |= (uint32_t)UartBuff[n + 14] << 25;
        Text_Negate[k + 5] &= 0xFC000000;
        Text_Negate[k + 5] |= (uint32_t)UartBuff[n + 15] << 18;
        Text_Negate[k + 5] |= (uint32_t)UartBuff[n + 16] << 11;
        Text_Negate[k + 5] |= (uint32_t)UartBuff[n + 17] << 4;
        Text_Negate[k + 5] &= 0xfffff800;
        
        Text_Negate[k + 6] = 0x80000000;
        Text_Negate[k + 6] |= (uint32_t)UartBuff[n + 17] << 24;
        Text_Negate[k + 6] &= 0xFE000000;
        Text_Negate[k + 6] |= (uint32_t)UartBuff[n + 18] << 17;
        Text_Negate[k + 6] |= (uint32_t)UartBuff[n + 19] << 10;
        Text_Negate[k + 6] &= 0xfffff800;
        
        k += 7;
    }
    
    n = Tx_Num * 2 + 1;
    TxCount = 0.0;
    for(i = Tx_Num * 2 + 1; i < UARTBUFF_SIZE; i++)                       //将数据合并到Tx发送缓冲区并计算校验码
    {
        if(TxCount >= (UartCount - DATA_START - 1))
            break;
        if(i % 16 == 0)                                                     //每8帧（16个码字）插入一个同步码
            TxBuff[n++] = SYNC_DATA;
        TxBuff[n++] = calc_bch_and_parity( Text_Negate[i]);
        TxCount += 2.8571428571;                                            //串口接收2.8571428571个字节相当于POCSAG码中一个码字
        if(n >= TXBUFF_SIZE || i >= UARTBUFF_SIZE)
            break;
    }
}

void SendTxBuff()
{
    //HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);

    UartSendString("UartCount=",10);
    SBUF = UartCount / 100 + '0';
    printf("%d",SBUF);
    while(TI == 0);
    TI = 0;
    SBUF = (UartCount / 10) % 10 + '0';
    printf("%d",SBUF);
    while(TI == 0);
    TI = 0;
    SBUF = UartCount % 10 + '0';
    printf("%d",SBUF);
    while(TI == 0);
    TI = 0;
    UartSendString("\r\n",2);

    uint16_t i;
    
    //HAL_GPIO_WritePin(PTT_PORT,PTT_PIN,GPIO_PIN_RESET);
    //PTT = 0;                    //触发PTT
    Delay200ms();                  //PTT延时，根据对讲机不同进行调整
    if(UartBuff[SPEED] == 'L')
    {
        /*TL0 = LOWSPEED_TIMER_L;        //设置定时初值
        TH0 = LOWSPEED_TIMER_H;*/
        Timer_Counter=LOWSPEED_TIMER;
        txSpeed=BPS_512;
    }
    if(UartBuff[SPEED] == 'H')
    {
        /*TL0 = HIGHTSPEED_TIMER_L;        //设置定时初值
        TH0 = HIGHTSPEED_TIMER_H;*/
        Timer_Counter=HIGHSPEED_TIMER;
        txSpeed=BPS_1200;
    }
    if(UartBuff[SPEED]=='S'){
        txSpeed=BPS_2400;
    }
    //TR0 = 1; //启动定时器
    //TIM16->CNT=Timer_Counter;
    //HAL_TIM_Base_Start_IT(&htim16);
    
    for(i = 0; i < 18; i++)//至少发送576个前导码，只能多，不能少
    {
        Send_Num(0xAAAAAAAA);
    }
    
    Send_Num(SYNC_DATA);//发送同步码

    for(i = 0; i < TXBUFF_SIZE; i++)//将TX发送缓冲区中的数据发出
    {
        if(TxBuff[i] == 0x00000000 || TxBuff[i] == 0xffffffff)//如果缓冲区中数据为全0或全1则表示发送完毕
            break;
        Send_Num(TxBuff[i]);
    }
    
    //TX=LOW; 
    //HAL_GPIO_WritePin(TX_PORT,TX_PIN,GPIO_PIN_RESET);

    //HAL_TIM_Base_Start_IT(&htim16);
    Delay200ms();                       //发送完毕PTT延时，根据对讲机不同进行调整
    Delay200ms();
    Delay200ms();
    //HAL_GPIO_WritePin(PTT_PORT,PTT_PIN,GPIO_PIN_SET);
    //PTT = 1;

    UartSendString("OK\r\n",4);          //串口输出调试信息
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);

}


void Delay200ms(){
    HAL_Delay(200);
}




void UartSendString(unsigned char* ch, unsigned char n)  //串口发送字符串函数
{
    printf("%s",ch);
}


void UART_RxData()  //串口接收中断函数
{
        if(UartTmp == '#')              //串口开头
            UartCount = 0;
        if(UartTmp == '$')              //串口结尾
        {
            UartTmp = 0x00;
            NewData = 1;
        }
        UartBuff[UartCount] = UartTmp;
        UartCount++;                     //串口计数
        if(UartCount >= UARTBUFF_SIZE)   //超出缓冲区归零
            UartCount = 0;
}

