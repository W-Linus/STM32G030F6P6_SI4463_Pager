/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <si446x.h>
#include <POCSAG_Generate.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

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
uint8_t g_UartRxBuffer[ 400 ] = { 0 };
uint16_t i=0;

uint8_t strLength=0;
uint32_t TxBuffer[TXBUFF_SIZE];

uint8_t txSize=0;

uint8_t rxChar;
uint8_t rxOKFlag[4]={0,0,0,0}; // 00-rx not start yet. 10-rx started but not finish. 11-rx finished. rxOKFlag[2]=Index. rxOKFlag[3]=rx finished.
uint8_t UartBuffer[256]={0};

POCSAG_MSG Pager; //Pager Data Struct
int8_t POCSAG_Message_Generated_Status;

extern uint32_t POCSAG_Batch1[16];	//码组1，包含16个码字
extern uint32_t POCSAG_Batch2[16];	//码组2，包含16个码字
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	SI446x_Init();
  HAL_UART_Receive_DMA(&huart1,&rxChar,1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if(/*rxOKFlag[3]==1*/rxOKFlag[3]==1&&(UartBuffer[TEXT_OR_NUM]=='N'||UartBuffer[TEXT_OR_NUM]=='T')){
      prepare_POCSAG_Data(UartBuffer,Pager);

      if(UartBuffer[TEXT_OR_NUM]=='N'){
        printf("Numberic Data Detected!\r\n");
        
      }

      if(UartBuffer[TEXT_OR_NUM]=='T'){
        printf("Text Data Detected!\r\n");
      }

      Pager=prepare_POCSAG_Data(UartBuffer,Pager);

      POCSAG_Message_Generated_Status=POCSAG_MakeCodeWords(Pager.Pager_Address,Pager.FuncCode,Pager.Text,\
      Pager.Batch20pt,Pager.InvertOpt);

      if(POCSAG_Message_Generated_Status>POCSAG_ERR_NONE){
        SI446x_Send_Packet((uint8_t*)POCSAG_Batch1,64,0,0);
      }
      memset(POCSAG_Batch1,0,sizeof(POCSAG_Batch1));
     // SI446x_Send_Packet((uint8_t*)TxBuff,sizeof(TxBuff),0,0);

     // printf("Data:%x,%x,%x,%x\r\n",test4,test3,test2,test1);
      txSize=0;
      rxOKFlag[3]=0;
    }
	
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
    g_UartRxBuffer[rxOKFlag[2]]=rxChar;
    rxOKFlag[2]+=1;
    
    if(rxChar=='$'){
      memset(UartBuffer,0,sizeof(UartBuffer));
      for(int i=0;i<256;i++){
        if(g_UartRxBuffer[i]=='$'){
            strLength=i+1;
            break;
          }
      }
      memcpy(UartBuffer,g_UartRxBuffer,strLength);
      memset(g_UartRxBuffer,0,sizeof(g_UartRxBuffer));
      rxOKFlag[2]=0;
      rxOKFlag[3]=1;
      printf("Received:%s\r\n",UartBuffer);
    }
  }
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
