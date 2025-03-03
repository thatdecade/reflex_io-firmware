/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdbool.h"
#include "uart.h"
#include "commands.h"
#include "msgbus.h"
#include "debug_leds.h"
#include "error_handler.h"
#include "commtests.h"
#include "ledtests.h"
#include "color.h"
#include "tusb_config.h"
#include "tusb.h"
#include "tusb_hid.h"
#include "config_mode.h"
#include "profile_config.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USB_HID_PACKET_SIZE_BYTES (64U)
#define BYTES_PER_SEGMENT (64U)
#define SEGMENTS_PER_PANEL (4U)
#define PANELS_PER_PLATFORM (4U)

#define SENSOR_RESPONSE_LEN (8U)

#define COMPLETE_FRAME (0xFFFF)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define BYTES_PER_PANEL (BYTES_PER_SEGMENT * SEGMENTS_PER_PANEL)
#define LED_ARRAY_SIZE (BYTES_PER_PANEL * PANELS_PER_PLATFORM)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
USART_HandleTypeDef husart1;
USART_HandleTypeDef husart2;
USART_HandleTypeDef husart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

volatile ErrorCode Panic_Error = 0;
volatile uint32_t Panic_Data = 0;

uint8_t sensor_buffer[USB_HID_PACKET_SIZE_BYTES];
uint8_t usb_sensor_buffer[USB_HID_PACKET_SIZE_BYTES];

volatile uint8_t last_usb_header;
volatile uint32_t packets_fetched = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_Init(void);
static void MX_USART2_Init(void);
static void MX_USART3_Init(void);
static void MX_USB_PCD_Init(void);
/* USER CODE BEGIN PFP */

static void process_hid_packet(void);
static inline void process_led_data(uint8_t *packet);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


static inline void send_request_sensors() {
    Request req = request_create(Command_Request_Sensors);
    req.response_len = SENSOR_RESPONSE_LEN;

    req.comport_id = Comport_Left;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Left) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Down;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Down) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Up;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Up) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Right;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Right) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);
}

static inline void send_sensor_update_usb() {
    tud_hid_report(
        USB_SEND_REPORT_ID,
        usb_sensor_buffer,
        USB_HID_PACKET_SIZE_BYTES
    );
}

// Breaks when this is being done after a bunch of times
static inline void process_sensor_data(Response * resp) {
    uint8_t offset = (uint8_t)resp->comport_id * SENSOR_RESPONSE_LEN;

    // Copy data over into usb sensor array
    for (uint8_t i = 0; i < resp->data_length; i++) {
        usb_sensor_buffer[offset + i] = resp->data[i];
    }   
}

static inline void send_commit_LEDs() {
    Request req = request_create(Command_Commit_LEDs);
    req.comport_id = Comport_Left;
    msgbus_send_request(req);
    req.comport_id = Comport_Down;
    msgbus_send_request(req);
    req.comport_id = Comport_Up;
    msgbus_send_request(req);
    req.comport_id = Comport_Right;
    msgbus_send_request(req);
}

static inline void send_process_led_segment(uint8_t panel, uint8_t * data_ptr) {
    Request req = request_create(Command_Process_LED_Segment);
    req.comport_id = (ComportId)panel;
    req.send_data = data_ptr;
    req.send_data_len = BYTES_PER_SEGMENT;
    msgbus_send_request(req);
}

static void process_hid_packet() {
    uint8_t *packet = usb_get_packet();
    if (packet == NULL) {
        return;
    }
    
    // First, use the config-mode filter to check for enter/exit packets.
    config_modes_t mode = packet_filter_for_config_mode(packet);
    if (mode != CONFIG_MODE_NORMAL) {
        // Either the "enter" or "exit" magic packet was received.
        set_config_mode(mode);
        return;
    }
    
    // Process LED Data or Config Packets
    if (!is_config_mode()) {
         // not config mode
        process_led_data(packet);
    }
    else 
    {
        // is config mode
        DBG_LED2_TOGGLE(); // Rapid blink comm LEDs
        
        uint8_t header = packet[0];
        if (header == PROFILE_PUSH_PACKET) {
            // Bytes 1�32 contain sensor thresholds/hysteresis data and bytes 33�36 the panel keys.
            // Save this configuration using the profile_config module.
            /*HAL_StatusTypeDef status = */ (void)profile_config_save(packet + 1);
        } else if (header == PROFILE_READ_PACKET) {
            // Prepare a reply packet with header 0xF1 and profile data read from EEPROM.
            uint8_t reply[64] = {0};
            reply[0] = 0xF1;
            profile_config_read(reply + 1);
            tud_hid_report(USB_SEND_REPORT_ID, reply, 64);
        }
    }
}

static inline void process_led_data(uint8_t *packet) {
    static uint16_t segments_received = 0x0000;
    static uint8_t led_buffer[LED_ARRAY_SIZE];
    static uint8_t previous_frame = 0xFF;
    
    // If the previous full frame has been received, commit the LED data.
    if (segments_received == COMPLETE_FRAME) {
        DBG_LED3_ON();
        segments_received = 0x0000;
        send_commit_LEDs();
    }
    
    uint8_t header  = packet[0];
    last_usb_header = header;
    uint8_t panel   = (header >> 6) & 0x03;
    uint8_t segment = (header >> 4) & 0x03;
    uint8_t frame   = header & 0x0F;
    
    uint16_t buffer_offset = panel * BYTES_PER_PANEL + segment * BYTES_PER_SEGMENT;
    
    for (uint8_t i = 0; i < USB_HID_PACKET_SIZE_BYTES; i++) {
        led_buffer[i + buffer_offset] = packet[i];
    }

    if (frame != previous_frame) {
        segments_received = 0x0000;
    }

    previous_frame = frame;
    segments_received |= (1 << (panel * PANELS_PER_PLATFORM + segment));
    send_process_led_segment(panel, led_buffer + buffer_offset);
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
  MX_USART1_Init();
  MX_USART2_Init();
  MX_USART3_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */
  uart_init();
  msgbus_init();
  tusb_init();
  DBG_LED1_ON();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
        // Process any pending messages from the internal (panel-to-panel) comms.
        msgbus_process_flags();
        
        if (msgbus_have_pending_response()) {
            Response *resp = msgbus_get_pending_response();
            switch (resp->request_command) {
                case Command_Request_Sensors:
                    process_sensor_data(resp);
                    break;
                // Add other command responses if needed.
            }
        }
        
        // Instead of directly processing LED data, process any incoming USB HID packets.
        // This will filter out config packets and process profile commands if in config mode.
        process_hid_packet();
        
        // Only send sensor data over USB if we are in normal (non-config) mode.
        if (!is_config_mode()) {
            send_sensor_update_usb();
        }
        
        // Always keep requesting sensor data.
        send_request_sensors();
        
        // Let the TinyUSB stack process USB events.
        tud_task();
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_USART3;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_SYSCLK;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_SYSCLK;
  PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_SYSCLK;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  husart1.Instance = USART1;
  husart1.Init.BaudRate = 3000000;
  husart1.Init.WordLength = USART_WORDLENGTH_8B;
  husart1.Init.StopBits = USART_STOPBITS_2;
  husart1.Init.Parity = USART_PARITY_NONE;
  husart1.Init.Mode = USART_MODE_TX_RX;
  husart1.Init.CLKPolarity = USART_POLARITY_LOW;
  husart1.Init.CLKPhase = USART_PHASE_1EDGE;
  husart1.Init.CLKLastBit = USART_LASTBIT_DISABLE;
  if (HAL_USART_Init(&husart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  husart2.Instance = USART2;
  husart2.Init.BaudRate = 3000000;
  husart2.Init.WordLength = USART_WORDLENGTH_8B;
  husart2.Init.StopBits = USART_STOPBITS_2;
  husart2.Init.Parity = USART_PARITY_NONE;
  husart2.Init.Mode = USART_MODE_TX_RX;
  husart2.Init.CLKPolarity = USART_POLARITY_LOW;
  husart2.Init.CLKPhase = USART_PHASE_1EDGE;
  husart2.Init.CLKLastBit = USART_LASTBIT_DISABLE;
  if (HAL_USART_Init(&husart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  husart3.Instance = USART3;
  husart3.Init.BaudRate = 3000000;
  husart3.Init.WordLength = USART_WORDLENGTH_8B;
  husart3.Init.StopBits = USART_STOPBITS_2;
  husart3.Init.Parity = USART_PARITY_NONE;
  husart3.Init.Mode = USART_MODE_TX_RX;
  husart3.Init.CLKPolarity = USART_POLARITY_LOW;
  husart3.Init.CLKPhase = USART_PHASE_1EDGE;
  husart3.Init.CLKLastBit = USART_LASTBIT_DISABLE;
  if (HAL_USART_Init(&husart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USART2_TX0_DR_GPIO_Port, USART2_TX0_DR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, USART3_TX_DR_Pin|USART3_RX_DR_Pin|USART1_TX_DR_Pin|USART1_RX_DR_Pin
                          |USART2_RX1_DR_Pin|USART2_TX1_DR_Pin|USART2_RX0_DR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA6 PA7
                           PA13 PA14 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : USART2_TX0_DR_Pin */
  GPIO_InitStruct.Pin = USART2_TX0_DR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USART2_TX0_DR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART3_TX_DR_Pin USART3_RX_DR_Pin USART1_TX_DR_Pin USART1_RX_DR_Pin
                           USART2_RX1_DR_Pin USART2_TX1_DR_Pin USART2_RX0_DR_Pin */
  GPIO_InitStruct.Pin = USART3_TX_DR_Pin|USART3_RX_DR_Pin|USART1_TX_DR_Pin|USART1_RX_DR_Pin
                          |USART2_RX1_DR_Pin|USART2_TX1_DR_Pin|USART2_RX0_DR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB13 PB7 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_13|GPIO_PIN_7|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF15_EVENTOUT;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF15_EVENTOUT;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
        DBG_LED3_TOGGLE();
        HAL_Delay(250);
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
