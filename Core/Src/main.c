/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ADS1115.h"
#include "Algorithm.h"
/* USER CODE END Includes */
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */
struct
{
  uint32_t Head;
  float Value[3];
  uint32_t Tail;
} DebugData = {0}; // uart data package
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUFFER_SIZE 128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */
uint16_t adc_buffer[BUFFER_SIZE];
uint16_t raw_adc;
volatile uint8_t timer_10ms_flag = 0;
volatile uint16_t buffer_head = 0;
volatile uint16_t buffer_tail = 0;
int32_t current_bpm = 0;
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  ADS1115_Init();
  // ADS1115_UserConfig_SingleConver(&ADS1115_ADDR_GND, 0x90);
  ADS1115_UserConfig_ContinuConver(&ADS1115_ADDR_GND, ADS1115_ADDRESS_GND);
  DebugData.Head = 0x11AA22BB;
  DebugData.Tail = 0x33CC44DD;
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);//pin1 940 pin0 660
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (timer_10ms_flag == 1)
    {
      timer_10ms_flag = 0;
      uint32_t sum = 0;
      uint16_t count = 0;

      // 1. 从底层 FIFO 提取数据并求均值 (降频到 100Hz)
      while (buffer_tail != buffer_head)
      {
        sum += adc_buffer[buffer_tail];
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
        count++;
      }

      if (count > 0)
      {
        float average_val = (float)sum / count;

        // 2. 丝滑滤波
        float final_val = Smooth_Filter(average_val);

        // 3. 实时寻峰状态机
        uint8_t is_peak = 0, is_valley = 0;
        Track_Pulse_Wave(final_val, &is_peak, &is_valley, &current_bpm);

        if (HAL_UART_GetState(&huart2) == HAL_UART_STATE_READY)
        {
          // 通道 0: 原始平滑波形
          DebugData.Value[0] = final_val;

          // 通道 1: 【标记通道】 - 极其直观的视觉反馈
          if (is_peak == 1)
          {
            DebugData.Value[1] = final_val + 200.0f; // 确认为波峰时，向上打一根 200 高度的针
          }
          else if (is_valley == 1)
          {
            DebugData.Value[1] = final_val - 200.0f; // 确认为波谷时，向下打一根 200 深度的针
          }
          else
          {
            //DebugData.Value[1] = final_val; // 平时隐藏在原波形中
            DebugData.Value[1] = 0.0f; // 平时保持为0，只有在波峰/波谷时才出现明显的标记
          }

          // 通道 2: 实时心率
          DebugData.Value[2] = (float)current_bpm;

          HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&DebugData, sizeof(DebugData));
        }
      }
    }
    /* USER CODE END 3 */
  }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == htim2.Instance)
  {
    timer_10ms_flag = 1;
  }
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

  if (GPIO_Pin == GPIO_PIN_0)
  {
    ADS1115_ReadRawData(&ADS1115_ADDR_GND);
    raw_adc = ADS1115_ADDR_GND.ADS1115_RawData[0];
    adc_buffer[buffer_head] = raw_adc;
    buffer_head = (buffer_head + 1) % 128;
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

#ifdef USE_FULL_ASSERT
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
