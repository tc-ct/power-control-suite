/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32h5xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */
extern DMA_HandleTypeDef handle_GPDMA1_Channel1;

extern DMA_HandleTypeDef handle_GPDMA1_Channel0;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

	/* USER CODE BEGIN MspInit 0 */

	/* USER CODE END MspInit 0 */

	/* System interrupt init*/

	/* USER CODE BEGIN MspInit 1 */

	/* USER CODE END MspInit 1 */
}

/**
  * @brief SMBUS MSP Initialization
  * This function configures the hardware resources used in this example
  * @param hsmbus: SMBUS handle pointer
  * @retval None
  */
void HAL_SMBUS_MspInit(SMBUS_HandleTypeDef* hsmbus)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if (hsmbus->Instance == I2C1) {
		/* USER CODE BEGIN I2C1_MspInit 0 */

		/* USER CODE END I2C1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
		PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C1 GPIO Configuration
		PB5     ------> I2C1_SMBA
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C1_CLK_ENABLE();
		/* I2C1 interrupt Init */
		HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
		HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
		/* USER CODE BEGIN I2C1_MspInit 1 */

		/* USER CODE END I2C1_MspInit 1 */
	} else if (hsmbus->Instance == I2C2) {
		/* USER CODE BEGIN I2C2_MspInit 0 */

		/* USER CODE END I2C2_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
		PeriphClkInitStruct.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C2 GPIO Configuration
		PB10     ------> I2C2_SCL
		PB12     ------> I2C2_SMBA
		PB13     ------> I2C2_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C2_CLK_ENABLE();
		/* I2C2 interrupt Init */
		HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
		HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
		/* USER CODE BEGIN I2C2_MspInit 1 */

		/* USER CODE END I2C2_MspInit 1 */
	}

}

/**
  * @brief SMBUS MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param hsmbus: SMBUS handle pointer
  * @retval None
  */
void HAL_SMBUS_MspDeInit(SMBUS_HandleTypeDef* hsmbus)
{
	if (hsmbus->Instance == I2C1) {
		/* USER CODE BEGIN I2C1_MspDeInit 0 */

		/* USER CODE END I2C1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C1_CLK_DISABLE();

		/**I2C1 GPIO Configuration
		PB5     ------> I2C1_SMBA
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);

		/* I2C1 interrupt DeInit */
		HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
		HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
		/* USER CODE BEGIN I2C1_MspDeInit 1 */

		/* USER CODE END I2C1_MspDeInit 1 */
	} else if (hsmbus->Instance == I2C2) {
		/* USER CODE BEGIN I2C2_MspDeInit 0 */

		/* USER CODE END I2C2_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C2_CLK_DISABLE();

		/**I2C2 GPIO Configuration
		PB10     ------> I2C2_SCL
		PB12     ------> I2C2_SMBA
		PB13     ------> I2C2_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);

		/* I2C2 interrupt DeInit */
		HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
		HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
		/* USER CODE BEGIN I2C2_MspDeInit 1 */

		/* USER CODE END I2C2_MspDeInit 1 */
	}

}

/**
  * @brief I3C MSP Initialization
  * This function configures the hardware resources used in this example
  * @param hi3c: I3C handle pointer
  * @retval None
  */
void HAL_I3C_MspInit(I3C_HandleTypeDef* hi3c)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if (hi3c->Instance == I3C2) {
		/* USER CODE BEGIN I3C2_MspInit 0 */

		/* USER CODE END I3C2_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I3C2;
		PeriphClkInitStruct.I3c2ClockSelection = RCC_I3C2CLKSOURCE_PCLK3;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		/* Peripheral clock enable */
		__HAL_RCC_I3C2_CLK_ENABLE();

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I3C2 GPIO Configuration
		PB3(JTDO/TRACESWO)     ------> I3C2_SCL
		PB4(NJTRST)     ------> I3C2_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF3_I3C2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* USER CODE BEGIN I3C2_MspInit 1 */

		/* USER CODE END I3C2_MspInit 1 */

	}

}

/**
  * @brief I3C MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param hi3c: I3C handle pointer
  * @retval None
  */
void HAL_I3C_MspDeInit(I3C_HandleTypeDef* hi3c)
{
	if (hi3c->Instance == I3C2) {
		/* USER CODE BEGIN I3C2_MspDeInit 0 */

		/* USER CODE END I3C2_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I3C2_CLK_DISABLE();

		/**I3C2 GPIO Configuration
		PB3(JTDO/TRACESWO)     ------> I3C2_SCL
		PB4(NJTRST)     ------> I3C2_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_3 | GPIO_PIN_4);

		/* USER CODE BEGIN I3C2_MspDeInit 1 */

		/* USER CODE END I3C2_MspDeInit 1 */
	}

}

/**
  * @brief SPI MSP Initialization
  * This function configures the hardware resources used in this example
  * @param hspi: SPI handle pointer
  * @retval None
  */
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if (hspi->Instance == SPI1) {
		/* USER CODE BEGIN SPI1_MspInit 0 */

		/* USER CODE END SPI1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
		PeriphClkInitStruct.Spi1ClockSelection = RCC_SPI1CLKSOURCE_PLL1Q;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		/* Peripheral clock enable */
		__HAL_RCC_SPI1_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**SPI1 GPIO Configuration
		PA5     ------> SPI1_SCK
		PA6     ------> SPI1_MISO
		PA7     ------> SPI1_MOSI
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* SPI1 DMA Init */
		/* GPDMA1_REQUEST_SPI1_RX Init */
		handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
		handle_GPDMA1_Channel1.Init.Request = GPDMA1_REQUEST_SPI1_RX;
		handle_GPDMA1_Channel1.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
		handle_GPDMA1_Channel1.Init.Direction = DMA_PERIPH_TO_MEMORY;
		handle_GPDMA1_Channel1.Init.SrcInc = DMA_SINC_FIXED;
		handle_GPDMA1_Channel1.Init.DestInc = DMA_DINC_FIXED;
		handle_GPDMA1_Channel1.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
		handle_GPDMA1_Channel1.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
		handle_GPDMA1_Channel1.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
		handle_GPDMA1_Channel1.Init.SrcBurstLength = 1;
		handle_GPDMA1_Channel1.Init.DestBurstLength = 1;
		handle_GPDMA1_Channel1.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
		handle_GPDMA1_Channel1.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
		handle_GPDMA1_Channel1.Init.Mode = DMA_NORMAL;

		if (HAL_DMA_Init(&handle_GPDMA1_Channel1) != HAL_OK)
			Error_Handler();

		__HAL_LINKDMA(hspi, hdmarx, handle_GPDMA1_Channel1);

		if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1, DMA_CHANNEL_NPRIV) != HAL_OK)
			Error_Handler();

		/* GPDMA1_REQUEST_SPI1_TX Init */
		handle_GPDMA1_Channel0.Instance = GPDMA1_Channel0;
		handle_GPDMA1_Channel0.Init.Request = GPDMA1_REQUEST_SPI1_TX;
		handle_GPDMA1_Channel0.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
		handle_GPDMA1_Channel0.Init.Direction = DMA_MEMORY_TO_PERIPH;
		handle_GPDMA1_Channel0.Init.SrcInc = DMA_SINC_FIXED;
		handle_GPDMA1_Channel0.Init.DestInc = DMA_DINC_FIXED;
		handle_GPDMA1_Channel0.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
		handle_GPDMA1_Channel0.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
		handle_GPDMA1_Channel0.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
		handle_GPDMA1_Channel0.Init.SrcBurstLength = 1;
		handle_GPDMA1_Channel0.Init.DestBurstLength = 1;
		handle_GPDMA1_Channel0.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
		handle_GPDMA1_Channel0.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
		handle_GPDMA1_Channel0.Init.Mode = DMA_NORMAL;

		if (HAL_DMA_Init(&handle_GPDMA1_Channel0) != HAL_OK)
			Error_Handler();

		__HAL_LINKDMA(hspi, hdmatx, handle_GPDMA1_Channel0);

		if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0, DMA_CHANNEL_NPRIV) != HAL_OK)
			Error_Handler();

		/* SPI1 interrupt Init */
		HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(SPI1_IRQn);
		/* USER CODE BEGIN SPI1_MspInit 1 */

		/* USER CODE END SPI1_MspInit 1 */

	}

}

/**
  * @brief SPI MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param hspi: SPI handle pointer
  * @retval None
  */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{
	if (hspi->Instance == SPI1) {
		/* USER CODE BEGIN SPI1_MspDeInit 0 */

		/* USER CODE END SPI1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_SPI1_CLK_DISABLE();

		/**SPI1 GPIO Configuration
		PA5     ------> SPI1_SCK
		PA6     ------> SPI1_MISO
		PA7     ------> SPI1_MOSI
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);

		/* SPI1 DMA DeInit */
		HAL_DMA_DeInit(hspi->hdmarx);
		HAL_DMA_DeInit(hspi->hdmatx);

		/* SPI1 interrupt DeInit */
		HAL_NVIC_DisableIRQ(SPI1_IRQn);
		/* USER CODE BEGIN SPI1_MspDeInit 1 */

		/* USER CODE END SPI1_MspDeInit 1 */
	}

}

/**
  * @brief TIM_Base MSP Initialization
  * This function configures the hardware resources used in this example
  * @param htim_base: TIM_Base handle pointer
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
	if (htim_base->Instance == TIM1) {
		/* USER CODE BEGIN TIM1_MspInit 0 */

		/* USER CODE END TIM1_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_TIM1_CLK_ENABLE();
		/* TIM1 interrupt Init */
		HAL_NVIC_SetPriority(TIM1_BRK_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_BRK_IRQn);
		HAL_NVIC_SetPriority(TIM1_UP_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
		HAL_NVIC_SetPriority(TIM1_TRG_COM_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_TRG_COM_IRQn);
		HAL_NVIC_SetPriority(TIM1_CC_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
		/* USER CODE BEGIN TIM1_MspInit 1 */

		/* USER CODE END TIM1_MspInit 1 */

	}

}

/**
  * @brief TIM_Base MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param htim_base: TIM_Base handle pointer
  * @retval None
  */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
	if (htim_base->Instance == TIM1) {
		/* USER CODE BEGIN TIM1_MspDeInit 0 */

		/* USER CODE END TIM1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_TIM1_CLK_DISABLE();

		/* TIM1 interrupt DeInit */
		HAL_NVIC_DisableIRQ(TIM1_BRK_IRQn);
		HAL_NVIC_DisableIRQ(TIM1_UP_IRQn);
		HAL_NVIC_DisableIRQ(TIM1_TRG_COM_IRQn);
		HAL_NVIC_DisableIRQ(TIM1_CC_IRQn);
		/* USER CODE BEGIN TIM1_MspDeInit 1 */

		/* USER CODE END TIM1_MspDeInit 1 */
	}

}

/**
  * @brief UART MSP Initialization
  * This function configures the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if (huart->Instance == USART1) {
		/* USER CODE BEGIN USART1_MspInit 0 */

		/* USER CODE END USART1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
		PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		/* Peripheral clock enable */
		__HAL_RCC_USART1_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**USART1 GPIO Configuration
		PA9     ------> USART1_TX
		PA10     ------> USART1_RX
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* USER CODE BEGIN USART1_MspInit 1 */

		/* USER CODE END USART1_MspInit 1 */

	}

}

/**
  * @brief UART MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
	if (huart->Instance == USART1) {
		/* USER CODE BEGIN USART1_MspDeInit 0 */

		/* USER CODE END USART1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_USART1_CLK_DISABLE();

		/**USART1 GPIO Configuration
		PA9     ------> USART1_TX
		PA10     ------> USART1_RX
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

		/* USER CODE BEGIN USART1_MspDeInit 1 */

		/* USER CODE END USART1_MspDeInit 1 */
	}

}

/**
  * @brief PCD MSP Initialization
  * This function configures the hardware resources used in this example
  * @param hpcd: PCD handle pointer
  * @retval None
  */
void HAL_PCD_MspInit(PCD_HandleTypeDef* hpcd)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if (hpcd->Instance == USB_DRD_FS) {
		/* USER CODE BEGIN USB_DRD_FS_MspInit 0 */

		/* USER CODE END USB_DRD_FS_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
		PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
			Error_Handler();

		/* Peripheral clock enable */
		__HAL_RCC_USB_CLK_ENABLE();
		/* USB_DRD_FS interrupt Init */
		HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(USB_DRD_FS_IRQn);
		/* USER CODE BEGIN USB_DRD_FS_MspInit 1 */

		/* USER CODE END USB_DRD_FS_MspInit 1 */

	}

}

/**
  * @brief PCD MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param hpcd: PCD handle pointer
  * @retval None
  */
void HAL_PCD_MspDeInit(PCD_HandleTypeDef* hpcd)
{
	if (hpcd->Instance == USB_DRD_FS) {
		/* USER CODE BEGIN USB_DRD_FS_MspDeInit 0 */

		/* USER CODE END USB_DRD_FS_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_USB_CLK_DISABLE();

		/* USB_DRD_FS interrupt DeInit */
		HAL_NVIC_DisableIRQ(USB_DRD_FS_IRQn);
		/* USER CODE BEGIN USB_DRD_FS_MspDeInit 1 */

		/* USER CODE END USB_DRD_FS_MspDeInit 1 */
	}

}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
