/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "usb_host.h"
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
#define BL_DEBUG_MSG_EN


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s3;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

uint8_t supported_commands[] = {
								BL_GET_VER,
								BL_GET_HELP,
								BL_GET_CID,
								BL_GET_RDP_STATUS,
								BL_GO_TO_ADDR,
								BL_FLASH_ERASE,
								BL_MEM_WRITE,
								BL_EN_RW_PROTECT,
								BL_MEM_READ,
								BL_READ_SECTOR_P_STATUS,
								BL_OTP_READ,
								BL_DIS_R_W_PROTECT};




#define D_UART &huart3
#define C_UART &huart2
#define BL_RX_LEN 200
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_CRC_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */
static void printmsg(char *format, ...);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
char somedata[] = "Hello from Bootloader\r\n";

uint8_t bl_rx_buffer[BL_RX_LEN];

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
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_SPI1_Init();
  MX_USB_HOST_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_CRC_Init();

  /* Let's check whether button is pressed or not, if not pressed jump to user application */
  if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET)
  {
	  printmsg("BL_DEBUG_MSG:Button is pressed....going to BL mode\r\n");
	  // we should continue in bootloader mode
	  bootloader_uart_read_data();
  }
  else
  {
	  printmsg("BL_DEBUG_MSG:Button is not pressed....going to app mode\r\n");

	  // we should continue in app mode
	  bootloader_jump_to_app();
  }
}

void bootloader_uart_read_data(void)
{
	uint8_t rcv_len = 0;
	while(1)
	{
		memset(bl_rx_buffer, 0, 200);
		// Here we will read and decode the commands coming from host
		// first read only one byte from the host, which is the length field
		HAL_UART_Receive(C_UART, bl_rx_buffer, 1, HAL_MAX_DELAY);
		rcv_len = (uint8_t) bl_rx_buffer[0];
		HAL_UART_Receive(C_UART, &bl_rx_buffer[1], rcv_len, HAL_MAX_DELAY);
		switch(bl_rx_buffer[1])
		{
			case BL_GET_VER:
				bootloader_handle_getver_cmd(bl_rx_buffer);
				break;
			case BL_GET_HELP:
				bootloader_handle_gethelp_cmd(bl_rx_buffer);
				break;
			case BL_GET_CID:
				bootloader_handle_getcid_cmd(bl_rx_buffer);
				break;
			case BL_GET_RDP_STATUS:
				bootloader_handle_getrdp_cmd(bl_rx_buffer);
				break;
			case BL_GO_TO_ADDR:
				bootloader_handle_go_cmd(bl_rx_buffer);
				break;
			case BL_FLASH_ERASE:
				bootloader_handle_flash_erase_cmd(bl_rx_buffer);
				break;
			case BL_MEM_WRITE:
				bootloader_handle_mem_write_cmd(bl_rx_buffer);
				break;
			case BL_EN_RW_PROTECT:
				bootloader_handle_en_rw_protect(bl_rx_buffer);
				break;
			case BL_DIS_R_W_PROTECT:
                bootloader_handle_dis_rw_protect(bl_rx_buffer);
			case BL_MEM_READ:
				bootloader_handle_mem_read(bl_rx_buffer);
				break;
			case BL_READ_SECTOR_P_STATUS:
				bootloader_handle_read_sector_protection_status(bl_rx_buffer);
				break;
			default:
				printmsg("BL_DEBUG_MSG:Invalid command received from host \n");
				break;
		}
	}
}

void bootloader_jump_to_app(void){
	// just a function pointer to hold the address of the reset handler of the user app
	void (*app_reset_handler)(void);

	printmsg("BL_DEBUG_MSG:bootloader_jump_to_app\r\n");

//	__disable_irq();
	// 1. configure the MSP by reading the value from the base address of the sector 2
	uint32_t msp_value = *(__IO uint32_t *) FLASH_SECTOR2_BASE_ADDRESS;

	// This function comes from CMSIS
	__set_MSP(msp_value);

	// SCB->VTOR = FLASH_SECTOR1_BASE_ADDRESS

	// 2. Now fetch the reset handler address of the user application from the location FLASH_SECTOR2_BASE_ADDRESS
	uint32_t resethandler_address = *(__IO uint32_t *) (FLASH_SECTOR2_BASE_ADDRESS + 4);

	app_reset_handler = (void (*) (void)) resethandler_address;

	printmsg("BL_DEBUG_MSG:app reset handler address: %#x\r\n", app_reset_handler);

//	__enable_irq();
	// 3. Jump to reset handler of the application
	app_reset_handler();
}


/* prints formatted string to console over UART */
void printmsg(char *format, ...)
{
#ifdef BL_DEBUG_MSG_EN
	char str[80];
	/*Extract the argument list using VA pins */
	va_list args;
	va_start(args, format);
	vsprintf(str, format, args);
	HAL_UART_Transmit(D_UART, (uint8_t *) str, strlen(str), HAL_MAX_DELAY);
	va_end(args);
#endif
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
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
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

}

/********* Implementation of Bootloader command handle functions *********************/
void bootloader_handle_gethelp_cmd(uint8_t *pBuffer)
{
	// 1. verify the checksum
	printmsg("BL_DEBUG_MSG: bootloader_handle_gethelp_cmd \r\n");

	// Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t *) (pBuffer + command_packet_len - 4));

	if(!bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
		printmsg("BL_DEBUG_MSG: checksum success !! \r\n");
		// checksum is correct
		bootloader_send_ack(pBuffer[0], sizeof(supported_commands));
		bootloader_uart_write_data(supported_commands, sizeof(supported_commands));
	}
	else
	{
		printmsg("BL_DEBUG_MSG: checksum fail !! \r\n");
		// checksum is wrong send neck
		bootloader_send_nack();
	}
}


void bootloader_handle_getver_cmd(uint8_t *bl_rx_buffer)
{
	uint8_t bl_version;
	printmsg("BL_DEBUG_MSG: bootloader_handle_getver_cmd \r\n");

	// Total length of the command packet
	uint32_t command_packet_len = bl_rx_buffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t *) (bl_rx_buffer + command_packet_len - 4));

	if(!bootloader_verify_crc(&bl_rx_buffer[0], command_packet_len - 4, host_crc))
	{
		printmsg("BL_DEBUG_MSG:checksum success !! \r\n");
		// checksum is correct
		bootloader_send_ack(bl_rx_buffer[0], 1);
		bl_version = get_bootloader_version();
		printmsg("BL_DEBUG_MSG:BL_VER : %d %#x", bl_version, bl_version);
		bootloader_uart_write_data(&bl_version, 1);
	}
	else
	{
		printmsg("BL_DEBUG_MSG: checksum fail !! \r\n");
		// checksum is wrong send neck
		bootloader_send_nack();
	}
}


void bootloader_handle_getcid_cmd(uint8_t *pBuffer)
{
	uint16_t bl_cid_num;
	// 1. verify the checksum
	printmsg("BL_DEBUG_MSG: bootloader_handle_getcid_cmd \r\n");

	// Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t *) (pBuffer + command_packet_len - 4));

	if(!bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
		printmsg("BL_DEBUG_MSG: checksum success !! \r\n");
		// checksum is correct
		bootloader_send_ack(pBuffer[0], 2);
		bl_cid_num = get_mcu_chip_id();
		printmsg("BL_DEBUG_MSG: MCU id : %d %#x !! \r\n", bl_cid_num, bl_cid_num);
		bootloader_uart_write_data((uint8_t *) &bl_cid_num, 2);
	}
	else
	{
		printmsg("BL_DEBUG_MSG: checksum fail !! \r\n");
		// checksum is wrong send neck
		bootloader_send_nack();
	}
}

void bootloader_handle_getrdp_cmd(uint8_t *pBuffer)
{
	uint8_t rdp_level = 0x00;
	// 1. verify the checksum
	printmsg("BL_DEBUG_MSG: bootloader_handle_getrdp_cmd \r\n");

	// Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t *) (pBuffer + command_packet_len - 4));

	if(!bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
		printmsg("BL_DEBUG_MSG: checksum success !! \r\n");
		// checksum is correct
		bootloader_send_ack(pBuffer[0], 1);
		rdp_level = get_flash_rdp_level();
		printmsg("BL_DEBUG_MSG: RDP level : %d %#x !! \r\n", rdp_level, rdp_level);
		bootloader_uart_write_data(&rdp_level, 1);
	}
	else
	{
		printmsg("BL_DEBUG_MSG: checksum fail !! \r\n");
		// checksum is wrong send neck
		bootloader_send_nack();
	}

}

void bootloader_handle_go_cmd(uint8_t *pBuffer)
{
    uint32_t go_address=0;
    uint8_t addr_valid = ADDR_VALID;
    uint8_t addr_invalid = ADDR_INVALID;

    printmsg("BL_DEBUG_MSG:bootloader_handle_go_cmd\n");

    //Total length of the command packet
	uint32_t command_packet_len = pBuffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer+command_packet_len - 4) ) ;

	if (!bootloader_verify_crc(&pBuffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");

        bootloader_send_ack(pBuffer[0],1);

        //extract the go address
        go_address = *((uint32_t *)&pBuffer[2] );
        printmsg("BL_DEBUG_MSG:GO addr: %#x\n",go_address);

        if( verify_address(go_address) == ADDR_VALID )
        {
            //tell host that address is fine
            bootloader_uart_write_data(&addr_valid,1);

            /*jump to "go" address.
            we dont care what is being done there.
            host must ensure that valid code is present over there
            Its not the duty of bootloader. so just trust and jump */

            /* Not doing the below line will result in hardfault exception for ARM cortex M */
            //watch : https://www.youtube.com/watch?v=VX_12SjnNhY

//            go_address+=1; //make T bit =1

            void (*lets_jump)(void) = (void *)go_address;

            printmsg("BL_DEBUG_MSG: jumping to go address! \n");

            lets_jump();

		}else
		{
            printmsg("BL_DEBUG_MSG:GO addr invalid ! \n");
            //tell host that address is invalid
            bootloader_uart_write_data(&addr_invalid,1);
		}

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}
}


void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer)
{
    uint8_t erase_status = 0x00;
    printmsg("BL_DEBUG_MSG:bootloader_handle_flash_erase_cmd\n");

    //Total length of the command packet
	uint32_t command_packet_len = pBuffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);
        printmsg("BL_DEBUG_MSG:initial_sector : %d  no_ofsectors: %d\n",pBuffer[2],pBuffer[3]);

        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin,GPIO_PIN_SET);
        erase_status = execute_flash_erase(pBuffer[2] , pBuffer[3]);
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin,GPIO_PIN_RESET);

        printmsg("BL_DEBUG_MSG: flash erase status: %#x\n",erase_status);

        bootloader_uart_write_data(&erase_status,1);

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}


}


/*Helper function to handle BL_MEM_WRITE command */
void bootloader_handle_mem_write_cmd(uint8_t *pBuffer)
{
	uint8_t write_status = 0x00;
	uint8_t payload_len = pBuffer[6];

	uint32_t mem_address = *((uint32_t *) ( &pBuffer[2]) );

    printmsg("BL_DEBUG_MSG:bootloader_handle_mem_write_cmd\n");

    //Total length of the command packet
	uint32_t command_packet_len = pBuffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer+command_packet_len - 4) ) ;


	if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");

        bootloader_send_ack(pBuffer[0],1);

        printmsg("BL_DEBUG_MSG: mem write address : %#x\n",mem_address);

		if( verify_address(mem_address) == ADDR_VALID )
		{

            printmsg("BL_DEBUG_MSG: valid mem write address\n");

            //glow the led to indicate bootloader is currently writing to memory
            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);

            //execute mem write
            write_status = execute_mem_write(&pBuffer[7], mem_address, payload_len);

            //turn off the led to indicate memory write is over
            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

            //inform host about the status
            bootloader_uart_write_data(&write_status,1);

		}else
		{
            printmsg("BL_DEBUG_MSG: invalid mem write address\n");
            write_status = ADDR_INVALID;
            //inform host that address is invalid
            bootloader_uart_write_data(&write_status,1);
		}


	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}

}


/*Helper function to handle BL_EN_RW_PROTECT  command */
void bootloader_handle_en_rw_protect(uint8_t *pBuffer)
{
    uint8_t status = 0x00;
    printmsg("BL_DEBUG_MSG:bootloader_handle_endis_rw_protect\n");

    //Total length of the command packet
	uint32_t command_packet_len = pBuffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);

        status = configure_flash_sector_rw_protection(pBuffer[2] , pBuffer[3], 0);

        printmsg("BL_DEBUG_MSG: flash erase status: %#x\n",status);

        bootloader_uart_write_data(&status,1);

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}

}

/*Helper function to handle BL_EN_RW_PROTECT  command */
void bootloader_handle_dis_rw_protect(uint8_t *pBuffer)
{
    uint8_t status = 0x00;
    printmsg("BL_DEBUG_MSG:bootloader_handle_dis_rw_protect\n");

    //Total length of the command packet
	uint32_t command_packet_len = pBuffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);

        status = configure_flash_sector_rw_protection(0,0,1);

        printmsg("BL_DEBUG_MSG: flash erase status: %#x\n",status);

        bootloader_uart_write_data(&status,1);

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}


}


void bootloader_handle_mem_read(uint8_t *bl_rx_buffer)
{

}


/*Helper function to handle _BL_READ_SECTOR_P_STATUS command */
void bootloader_handle_read_sector_protection_status(uint8_t *pBuffer)
{
	uint16_t status;
	printmsg("BL_DEBUG_MSG:bootloader_handle_read_sector_protection_status\n");

    //Total length of the command packet
	uint32_t command_packet_len = bl_rx_buffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (bl_rx_buffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],2);
        status=read_OB_rw_protection_status();
        printmsg("BL_DEBUG_MSG: nWRP status: %#x\n",status);
        bootloader_uart_write_data((uint8_t*)&status,2);

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}

}


void bootloader_handle_read_otp(uint8_t *bl_rx_buffer)
{

}

uint8_t get_bootloader_version(void)
{
	return (uint8_t) BL_VERSION;
}

void bootloader_uart_write_data(uint8_t *pBuffer, uint32_t len)
{
	HAL_UART_Transmit(C_UART, pBuffer, len, HAL_MAX_DELAY);
}

//This verifies the CRC of the given buffer in pData .
uint8_t bootloader_verify_crc (uint8_t *pData, uint32_t len, uint32_t crc_host)
{
    uint32_t uwCRCValue=0xff;

    for (uint32_t i=0 ; i < len ; i++)
	{
        uint32_t i_data = pData[i];
        uwCRCValue = HAL_CRC_Accumulate(&hcrc, &i_data, 1);
	}

	 /* Reset CRC Calculation Unit */
    __HAL_CRC_DR_RESET(&hcrc);

	if( uwCRCValue == crc_host)
	{
		return VERIFY_CRC_SUCCESS;
	}

	return VERIFY_CRC_FAIL;
}

/* This function sends ACK if CRC matches along with "len to follow" */
void bootloader_send_ack(uint8_t command_code, uint8_t follow_len)
{
	// here we send 2 bytes...first byte is ack and second byte is len value
	uint8_t ack_buff[2];
	ack_buff[0] = BL_ACK;
	ack_buff[1] = follow_len;
	HAL_UART_Transmit(C_UART, ack_buff, 2, HAL_MAX_DELAY);
}

void bootloader_send_nack(void)
{
	uint8_t nack = BL_NACK;
	HAL_UART_Transmit(C_UART, &nack, 1, HAL_MAX_DELAY);
}

uint16_t get_mcu_chip_id(void)
{
	uint16_t cid;
	cid = (uint16_t) (DBGMCU -> IDCODE) & 0x0FFF;
	return cid;
}

uint8_t get_flash_rdp_level(void)
{
	uint8_t rdp_status = 0;
#if 0
	// use the HAL Flash API to get the rdp level
	FLASH_OBProgramInitTypeDef ob_handle;
	HAL_FLASHEx_OBGetConfig(&ob_handle);
	rdp_status = (uint8_t) ob_handle.RDPLevel;
#else
	volatile uint32_t *pOB_addr = (uint32_t*) 0x1FFFC000;
	rdp_status = (uint8_t)(*pOB_addr >> 8);
#endif
	return rdp_status;
}

//verify the address sent by the host .
uint8_t verify_address(uint32_t go_address)
{
	//so, what are the valid addresses to which we can jump ?
	//can we jump to system memory ? yes
	//can we jump to sram1 memory ?  yes
	//can we jump to sram2 memory ? yes
	//can we jump to backup sram memory ? yes
	//can we jump to peripheral memory ? its possible , but dont allow. so no
	//can we jump to external memory ? yes.

//incomplete -poorly written .. optimize it
	if ( go_address >= SRAM1_BASE && go_address <= SRAM1_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= SRAM2_BASE && go_address <= SRAM2_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= FLASH_BASE && go_address <= FLASH_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= BKPSRAM_BASE && go_address <= BKPSRAM_END)
	{
		return ADDR_VALID;
	}
	else
		return ADDR_INVALID;
}

uint8_t execute_flash_erase(uint8_t sector_number , uint8_t number_of_sector)
{
    //we have totally 12 sectors in STM32F407 mcu .. sector[0 to 11]
	//number_of_sector has to be in the range of 0 to 11
	//if sector_number = 0xff , that means mass erase !
	//Code needs to modified if your MCU supports more flash sectors
	FLASH_EraseInitTypeDef flashErase_handle;
	uint32_t sectorError;
	HAL_StatusTypeDef status;


	if( number_of_sector > 12 )
		return INVALID_SECTOR;

	if( (sector_number == 0xff ) || (sector_number <= 11) )
	{
		if(sector_number == (uint8_t) 0xff)
		{
			flashErase_handle.TypeErase = FLASH_TYPEERASE_MASSERASE;
		}else
		{
		    /*Here we are just calculating how many sectors needs to erased */
			uint8_t remanining_sector = 12 - sector_number;
           if( number_of_sector > remanining_sector)
           {
           	number_of_sector = remanining_sector;
           }
			flashErase_handle.TypeErase = FLASH_TYPEERASE_SECTORS;
			flashErase_handle.Sector = sector_number; // this is the initial sector
			flashErase_handle.NbSectors = number_of_sector;
		}
		flashErase_handle.Banks = FLASH_BANK_1;

		/*Get access to touch the flash registers */
		HAL_FLASH_Unlock();
		flashErase_handle.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // our mcu will work on this voltage range
		status = (uint8_t) HAL_FLASHEx_Erase(&flashErase_handle, &sectorError);
		HAL_FLASH_Lock();

		return status;
	}


	return INVALID_SECTOR;
}

/*This function writes the contents of pBuffer to  "mem_address" byte by byte */
//Note1 : Currently this function supports writing to Flash only .
//Note2 : This functions does not check whether "mem_address" is a valid address of the flash range.
uint8_t execute_mem_write(uint8_t *pBuffer, uint32_t mem_address, uint32_t len)
{
    uint8_t status=HAL_OK;

    //We have to unlock flash module to get control of registers
    HAL_FLASH_Unlock();

    for(uint32_t i = 0 ; i <len ; i++)
    {
        //Here we program the flash byte by byte
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,mem_address+i,pBuffer[i] );
    }

    HAL_FLASH_Lock();

    return status;
}


/*
Modifying user option bytes
To modify the user option value, follow the sequence below:
1. Check that no Flash memory operation is ongoing by checking the BSY bit in the
FLASH_SR register
2. Write the desired option value in the FLASH_OPTCR register.
3. Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
4. Wait for the BSY bit to be cleared.
*/
uint8_t configure_flash_sector_rw_protection(uint8_t sector_details, uint8_t protection_mode, uint8_t disable)
{
    //First configure the protection mode
    //protection_mode =1 , means write protect of the user flash sectors
    //protection_mode =2, means read/write protect of the user flash sectors
    //According to RM of stm32f446xx TABLE 9, We have to modify the address 0x1FFF C008 bit 15(SPRMOD)

	 //Flash option control register (OPTCR)
    volatile uint32_t *pOPTCR = (uint32_t*) 0x40023C14;

	  if(disable)
		{
			//disable all r/w protection on sectors

			//Option byte configuration unlock
			HAL_FLASH_OB_Unlock();

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			//clear the 31st bit (default state)
			//please refer : Flash option control register (FLASH_OPTCR) in RM
			*pOPTCR &= ~(1 << 31);

			//clear the protection : make all bits belonging to sectors as 1
			*pOPTCR |= (0xFF << 16);

			//Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
			*pOPTCR |= ( 1 << 1);

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			HAL_FLASH_OB_Lock();

			return 0;

		}

	   if(protection_mode == (uint8_t) 1)
    {
           //we are putting write protection on the sectors encoded in sector_details argument

			//Option byte configuration unlock
			HAL_FLASH_OB_Unlock();

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			//here we are setting just write protection for the sectors
			//clear the 31st bit
			//please refer : Flash option control register (FLASH_OPTCR) in RM
			*pOPTCR &= ~(1 << 31);

			//put write protection on sectors
			*pOPTCR &= ~ (sector_details << 16);

			//Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
			*pOPTCR |= ( 1 << 1);

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			HAL_FLASH_OB_Lock();
		}

		else if (protection_mode == (uint8_t) 2)
    {
	  	//Option byte configuration unlock
			HAL_FLASH_OB_Unlock();

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			//here wer are setting read and write protection for the sectors
			//set the 31st bit
			//please refer : Flash option control register (FLASH_OPTCR) in RM
			*pOPTCR |= (1 << 31);

			//put read and write protection on sectors
            *pOPTCR &= ~(0xff << 16);
			*pOPTCR |= (sector_details << 16);

			//Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
			*pOPTCR |= ( 1 << 1);

			//wait till no active operation on flash
			while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

			HAL_FLASH_OB_Lock();
    }

		return 0;
}

uint16_t read_OB_rw_protection_status(void)
{
    //This structure is given by ST Flash driver to hold the OB(Option Byte) contents .
	FLASH_OBProgramInitTypeDef OBInit;

	//First unlock the OB(Option Byte) memory access
	HAL_FLASH_OB_Unlock();
	//get the OB configuration details
	HAL_FLASHEx_OBGetConfig(&OBInit);
	//Lock back .
	HAL_FLASH_Lock();

	//We are just interested in r/w protection status of the sectors.
	return (uint16_t)OBInit.WRPSector;

}

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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
