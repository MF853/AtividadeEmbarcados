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
#include <string.h>
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
typedef enum {
    IDLE,
    WAITING_SERVER,
    BLINKING_LED,
  SENDING_PC,
  WAITING_PC_DMA
} State_t;

#define SERVER_TABLE_SIZE 120U
#define SERVER_TOTAL_RX   121U

uint8_t rx_buffer[SERVER_TOTAL_RX];
State_t currentState = IDLE;
uint8_t cmd_request = 0x5A;       // Comando para o servidor
uint8_t server_counter = 0;       // Valor X recebido do servidor
char team_table[SERVER_TABLE_SIZE + 1]; // Buffer para a tabela via DMA
char pc_msg[100];                 // Mensagem formatada para o PC
volatile uint8_t data_ready = 0;           // Flag de controle
uint8_t table_dma_done = 0;       // Flag para recepção completa da tabela
uint8_t pc_dma_busy = 0;          // Flag para transmissão da tabela ao PC
uint8_t pc_msg_it_busy = 0;       // Flag para transmissão da mensagem por IT

volatile uint8_t log_cmd_sent    = 0;  // Comando 0x5A enviado com sucesso
volatile uint8_t log_rx_done     = 0;  // Recepção do servidor concluída
volatile uint8_t log_went_idle   = 0;  // Sistema voltou ao IDLE

// Variáveis para substituir o Delay bloqueante
uint32_t tick_anterior = 0;
uint8_t piscadas_atuais = 0;      // Contador de piscadas executadas
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Log(const char* msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}
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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (log_cmd_sent) {
	            log_cmd_sent = 0;
	            Log("[UART1 TX] Comando 0x5A enviado ao servidor.\r\n");
	            Log("[STATE] -> WAITING_SERVER\r\n");
	        }

	        if (log_rx_done) {
	            log_rx_done = 0;
	            char buf[80];
	            snprintf(buf, sizeof(buf),
	                "[UART1 RX] Contador X = %d\r\n", server_counter);
	            Log(buf);
	            Log("[DMA] Inicio da tabela: ");
	            char preview[42] = {0};
	            strncpy(preview, team_table, 40);
	            Log(preview);
	            Log("\r\n");
	        }

	        if (log_went_idle) {
	            log_went_idle = 0;
	            Log("[STATE] -> IDLE\r\n");
	        }


	        switch (currentState) {
	            case WAITING_SERVER:
	            if (data_ready) {
	                    data_ready = 0;
	                    piscadas_atuais = 0;           // Zera as piscadas
	                    tick_anterior = HAL_GetTick(); // Pega o tempo atual
	                    currentState = BLINKING_LED;
	                }
	                break;

	            case BLINKING_LED:
	                // Se o contador for zero, nem entra na lógica de piscar e vai direto pro PC
	                if (server_counter == 0) {
	                    currentState = SENDING_PC;
	                    break;
	                }

	                // Pisca o LED em 1Hz (500ms ON / 500ms OFF) SEM usar HAL_Delay
	                if (HAL_GetTick() - tick_anterior >= 500) {
	                    tick_anterior = HAL_GetTick();
	                    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); // Inverte o estado do LED

	                    // Se o LED apagou, significa que completou um ciclo inteiro (1 piscada)
	                    if (HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin) == GPIO_PIN_RESET) {
	                        piscadas_atuais++;
	                    }

	                    // Se já piscou a quantidade necessária, desliga o LED por segurança e avança
	                    if (piscadas_atuais >= server_counter) {
	                        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
	                        currentState = SENDING_PC;
	                    }
	                }
	                break;

	            case SENDING_PC:

	                if (pc_msg_it_busy != 0U || pc_dma_busy != 0U) {
	                  break;
	                }

	                // 1. Formata a mensagem contendo o número de eventos
	                sprintf(pc_msg, "\r\nNúmero de eventos = %d\r\n", server_counter);

	                // 2. Envia a mensagem pro PC por interrupção (IT)
	                pc_msg_it_busy = 1;
	                HAL_UART_Transmit_IT(&huart2, (uint8_t*)pc_msg, strlen(pc_msg));

	                currentState = WAITING_PC_DMA;
	                break;

	              case WAITING_PC_DMA:
	                if (pc_msg_it_busy == 0U && pc_dma_busy == 0U) {
	                  currentState = IDLE;
	                }
	                break;

	            default:
	                break;
	        }
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == B1_Pin && currentState == IDLE) {

        // Limpa buffers de sessão anterior
        memset(rx_buffer,  0, sizeof(rx_buffer));
        memset(team_table, 0, sizeof(team_table));
        data_ready     = 0;
        pc_msg_it_busy = 0;
        pc_dma_busy    = 0;

        // 1. Arma DMA para receber tudo de uma vez (1 byte contador + 120 bytes tabela)
        HAL_UART_Receive_DMA(&huart1, rx_buffer, SERVER_TOTAL_RX);

        // 2. Envia comando bloqueante (1 byte ≈ 87 µs a 115200 bps — seguro em ISR)
        HAL_UART_Transmit(&huart1, &cmd_request, 1, 100);

        // Seta flag de log (Log() não pode ser chamada dentro de ISR)
        log_cmd_sent = 1;

        currentState = WAITING_SERVER;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Separa o contador (byte 0) da tabela (bytes 1..120)
        server_counter = rx_buffer[0];
        memcpy(team_table, &rx_buffer[1], SERVER_TABLE_SIZE);
        team_table[SERVER_TABLE_SIZE] = '\0'; // Garante terminador

        data_ready   = 1;
        log_rx_done  = 1;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (pc_msg_it_busy != 0U) {
            // Mensagem de eventos enviada — agora envia a tabela por DMA
            pc_msg_it_busy = 0;
            pc_dma_busy    = 1;
            HAL_UART_Transmit_DMA(&huart2,
                (uint8_t*)team_table, strlen(team_table));
        } else if (pc_dma_busy != 0U) {
            // Tabela enviada — transmissão concluída
            pc_dma_busy = 0;
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
