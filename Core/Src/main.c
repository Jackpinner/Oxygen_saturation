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
  float Value[6];
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
PPG_Filter_State ir_state = {0};  // 红外光滤波状态
PPG_Filter_State red_state = {0}; // 红光滤波状态
uint16_t red_buffer[BUFFER_SIZE];
uint16_t ir_buffer[BUFFER_SIZE];
uint16_t raw_adc;
volatile uint8_t timer_10ms_flag = 0;
volatile uint16_t red_head = 0;
volatile uint16_t red_tail = 0;
volatile uint16_t ir_head = 0;
volatile uint16_t ir_tail = 0;
volatile uint8_t light_state = 0;
uint16_t latest_ir_index = 0;
uint16_t latest_red_index = 0;
int32_t current_bpm = 0;
float current_spo2 = 0.f;
float current_r_value = 0.f;
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  ADS1115_Init();
  // ADS1115_UserConfig_SingleConver(&ADS1115_ADDR_GND, 0x90);//单次转换
  // ADS1115_UserConfig_ContinuConver(&ADS1115_ADDR_GND, ADS1115_ADDRESS_GND);//连续转换
  DebugData.Head = 0x11AA22BB;
  DebugData.Tail = 0x33CC44DD;
  HAL_TIM_Base_Start_IT(&htim2);
  light_state = 0;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);   // 亮 940nm 红外 (PB1)
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // 灭 660nm 红光 (PB0)

  // 启动第一次单次转换！从此进入永动机循环
  ADS1115_UserConfig_SingleConver_Interrupt(&ADS1115_ADDR_GND, ADS1115_ADDRESS_GND);
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

      // ==========================================
      // 1. 获取并处理原始数据 (从 FIFO 中提取并平均)
      // ==========================================
      uint32_t ir_sum = 0, red_sum = 0;
      uint16_t ir_count = 0, red_count = 0;

      while (ir_tail != ir_head)
      {
        ir_sum += ir_buffer[ir_tail];
        ir_tail = (ir_tail + 1) % BUFFER_SIZE;
        ir_count++;
      }
      while (red_tail != red_head)
      {
        red_sum += red_buffer[red_tail];
        red_tail = (red_tail + 1) % BUFFER_SIZE;
        red_count++;
      }

      // ==========================================
      // 2. 喂入带通滤波器 (AC/DC 分离)
      // ==========================================
      if (ir_count > 0 || red_count > 0)
      {
        float ir_raw = (ir_count > 0) ? ((float)ir_sum / ir_count) : ir_state.dc_baseline;
        float red_raw = (red_count > 0) ? ((float)red_sum / red_count) : red_state.dc_baseline;

        // 核心：调用刚才写的新滤波器
        IIR_Bandpass_Filter(ir_raw, &ir_state);
        IIR_Bandpass_Filter(red_raw, &red_state);

        //==========================================
        // 3. 寻峰、计算血氧及标记针逻辑
        //==========================================
        uint8_t is_peak = 0, is_valley = 0;

        // 调用升级后的函数：传入红外和红光的 AC 和 DC 4个分量
        Track_Pulse_Wave_Dual(ir_state.ac_filtered, red_state.ac_filtered, ir_state.dc_baseline, red_state.dc_baseline, &is_peak, &is_valley, &current_bpm, &current_spo2, &current_r_value);

        //==========================================
        // 4. 串口数据打包发送 (VOFA+ 等上位机看图)
        //==========================================
        if (HAL_UART_GetState(&huart2) == HAL_UART_STATE_READY)
        {
          // DebugData.Value[0] = ir_state.ac_filtered;  // 通道1：纯净的红外交流波形
          // DebugData.Value[1] = red_state.ac_filtered; // 通道2：纯净的红光交流波形
          DebugData.Value[0] = ir_raw;; // 通道1：660LED原始数据
          DebugData.Value[1] = red_raw;// 通道2：660VCSEL原始数据

          latest_ir_index = (ir_head == 0) ? (BUFFER_SIZE - 1) : (ir_head - 1);
          latest_red_index = (red_head == 0) ? (BUFFER_SIZE - 1) : (red_head - 1);
          // --- 双向标记针逻辑 ---
          if (is_peak == 1)
          {
            DebugData.Value[2] = 2500.0f; // 波峰向上打针
          }
          else if (is_valley == 1)
          {
            DebugData.Value[2] = -2500.0f; // 波谷向下打针
          }
          else
          {
            DebugData.Value[2] = 0.0f; // 平时跟随基准线
          }

          DebugData.Value[3] = (float)current_bpm; // 通道4：心率
          DebugData.Value[4] = current_spo2;       // 通道5：血氧
          // DebugData.Value[3] = ir_raw; // 通道4：红外原始数据
          // DebugData.Value[4] = red_raw;// 通道5：红光原始数据
          // DebugData.Value[3] = (float)ir_buffer[latest_ir_index];; // 通道4：红外adc原始数据
          // DebugData.Value[4] = (float)red_buffer[latest_red_index];// 通道5：红光adc原始数据
          DebugData.Value[5] = current_r_value;
          // DebugData.Value[5] = ir_state.dc_baseline;// 通道6：红外直流基线 (DC)
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
    // 1. 马上读取刚刚转换好的数据
    ADS1115_ReadRawData(&ADS1115_ADDR_GND);
    int16_t raw_adc = (int16_t)ADS1115_ADDR_GND.ADS1115_RawData[0];

    // 2. 乒乓状态机：根据当前亮的是什么灯，将数据塞入不同的仓库
    if (light_state == 0) // === 当前亮的是 940nm 红外光 ===
    {
      // 存入红外 FIFO
      ir_buffer[ir_head] = raw_adc;
      ir_head = (ir_head + 1) % BUFFER_SIZE;

      // 状态切换动作：灭红外灯(PB1)，亮红灯(PB0)
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      light_state = 1;
    }
    else // === 当前亮的是 660nm 红光 ===
    {
      // 存入红光 FIFO
      red_buffer[red_head] = raw_adc;
      red_head = (red_head + 1) % BUFFER_SIZE;

      // 状态切换动作：灭红灯(PB0)，亮红外灯(PB1)
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
      light_state = 0;
    }

    // 3. 再次触发！立刻命令 ADS1115 开始下一次转换 (测量刚刚切换的灯光)
    ADS1115_Trigger_Next_Conversion(&ADS1115_ADDR_GND);
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
