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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_tim.h"
#include "usbd_core.h"
#include "usbd_customhid.h"
#include "usbd_customhid_if.h"
#include "usbd_desc.h"
#include "usbd_composite_builder.h"
#include "INA238.h"
#include "INA260.h"
#include "stm32h5xx_hal_gpio.h"
#include <stdint.h>
#include <stdio.h>
#include "dac_driver.h"
#include "proto_pkg.h"
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

SMBUS_HandleTypeDef hsmbus1;
SMBUS_HandleTypeDef hsmbus2;

I3C_HandleTypeDef hi3c2;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef handle_GPDMA1_Channel1;
DMA_HandleTypeDef handle_GPDMA1_Channel0;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_I2C2_SMBUS_Init(void);
static void MX_I2C1_SMBUS_Init(void);
static void MX_I3C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

DAC_HandleTypeDef hdac_dac[3]; // SS0 (PA4)

/* I2C1上INA238设备数量 */
#define I2C1_INA238_NUM 13
/* I2C2上INA238设备数量 */
#define I2C2_INA238_NUM 13

/* 数据缓冲区 */
float adc_values[27];

/* 设备地址列表 */
const uint8_t i2c1_dev_addrs[I2C1_INA238_NUM] = {0x40, 0x41, 0x42, 0x43, 0x44,
                                                 0x45, 0x46, 0x47, 0x48, 0x49,
                                                 0x4A, 0x4B, 0x4C};
const uint8_t i2c2_dev_addrs[I2C2_INA238_NUM] = {0x40, 0x41, 0x42, 0x43, 0x45,
                                                 0x46, 0x47, 0x48, 0x49, 0x4A,
                                                 0x4B, 0x4C, 0x4D};
//                   USB
uint8_t CDC_EpAdd_Inst[3] = {CDC_IN_EP, CDC_OUT_EP, CDC_CMD_EP}; 	/* CDC Endpoint Addresses array */
// uint8_t HID_EpAdd_Inst = HID_EPIN_ADDR;								/* HID Endpoint Address array */
uint8_t CustomHID_EpAdress[2] = {CUSTOM_HID_EPIN_ADDR, CUSTOM_HID_EPOUT_ADDR};								/* HID Endpoint Address array */
USBD_HandleTypeDef hUsbDeviceFS;
uint8_t hid_report_buffer[SAMPLE_REPORT_SIZE];
uint8_t HID_InstID = 0, CDC_InstID = 0, CUSTOMHID_InstID = 0;
uint8_t current_sampling_enabled;
uint8_t voltage_sampling_enabled;
uint8_t pending_cmd;
extern VoltageConfigPacket_t pending_config;
extern SequenceConfigPacket_t pending_sequence;
extern uint8_t pending_power_on;
extern uint8_t pending_power_off;

static const uint8_t power_enable_ports[POWER_SUPPLY_COUNT] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
static const uint8_t power_enable_pins[POWER_SUPPLY_COUNT] = {
    6, 7, 8, 9, 10, 11, 1, 3, 0, 4, 0, 2, 5, 13, 0, 1, 15, 14};

static GPIO_TypeDef *GetPowerEnablePort(uint8_t port_index)
{
  switch (port_index) {
  case 0:
    return GPIOB;
  case 1:
    return GPIOC;
  default:
    return NULL;
  }
}

static uint16_t GetPowerEnablePinMask(uint8_t pin_index)
{
  if (pin_index >= 16) {
    return 0;
  }
  uint16_t pin_mask = 1 << (pin_index & 0x0F);
  return pin_mask;
}

static void ExecutePowerOnSequence(const SequenceConfigPacket_t *sequence)
{
  // 创建一个数组来存储电源ID和顺序
  typedef struct {
    uint8_t power_id;
    uint8_t order;
    uint16_t delay;
  } PowerStep;
  PowerStep steps[POWER_SUPPLY_COUNT];
  
  for (uint8_t i = 0; i < POWER_SUPPLY_COUNT; ++i) {
    steps[i].power_id = i;  // i 是电源ID
    steps[i].order = sequence->sequence[i];  // sequence[i] 是电源 i 的上电顺序值
    steps[i].delay = sequence->interval_ms[i];
  }
  
  // 冒泡排序按 order 升序（从小到大）
  for (uint8_t i = 0; i < POWER_SUPPLY_COUNT - 1; ++i) {
    for (uint8_t j = 0; j < POWER_SUPPLY_COUNT - i - 1; ++j) {
      if (steps[j].order > steps[j + 1].order) {
        PowerStep temp = steps[j];
        steps[j] = steps[j + 1];
        steps[j + 1] = temp;
      }
    }
  }
  
  // 按排序后的顺序执行上电
  for (uint8_t i = 0; i < POWER_SUPPLY_COUNT; ++i) {
    uint8_t power_id = steps[i].power_id;
    GPIO_TypeDef *port = GetPowerEnablePort(power_enable_ports[power_id]);
    uint16_t pin_mask = GetPowerEnablePinMask(power_enable_pins[power_id]);
    if (port != NULL && pin_mask != 0) {
      HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_SET);
    }
    
    if (steps[i].delay != 0) {
      HAL_Delay(steps[i].delay);
    }
  }
}

static void ExecutePowerOff(void)
{
  for (uint8_t power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
    GPIO_TypeDef *port = GetPowerEnablePort(power_enable_ports[power_id]);
    uint16_t pin_mask = GetPowerEnablePinMask(power_enable_pins[power_id]);
    if (port != NULL && pin_mask != 0) {
      HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_RESET);
    }
  }

  for (uint8_t ch = 0; ch < 8; ++ch) {
    DAC_SetVoltage(&hdac_dac[0], ch, 0);
    DAC_SetVoltage(&hdac_dac[1], ch, 0);
  }
  DAC_SetVoltage(&hdac_dac[2], 0, 0);
  DAC_SetVoltage(&hdac_dac[2], 1, 0);
}

void SendSampleReport(uint8_t type, float *values) {
  SampleDataPacket_t *pkt = (SampleDataPacket_t *)hid_report_buffer;
  memset(pkt, 0, sizeof(SAMPLE_REPORT_SIZE));
  uint32_t timestamp = HAL_GetTick();

  pkt->report_id = ADC_REPORT_ID;
  pkt->type = type;
  memcpy(&pkt->timestamp, &timestamp, sizeof(timestamp));
  memcpy(&pkt->values, values, SAMPLE_DATA_COUNT * sizeof(float));

  USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, (uint8_t *)pkt, SAMPLE_REPORT_SIZE);
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
  MX_GPDMA1_Init();
  MX_I2C2_SMBUS_Init();
  MX_I2C1_SMBUS_Init();
  MX_I3C2_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

    // 初始化三个DAC设备
  DAC_Init(&hdac_dac[0], DAC_TYPE_7568, GPIOA, GPIO_PIN_4, &hspi1);
  DAC_Init(&hdac_dac[1], DAC_TYPE_7568, GPIOA, GPIO_PIN_3, &hspi1);
  DAC_Init(&hdac_dac[2], DAC_TYPE_7563, GPIOA, GPIO_PIN_2, &hspi1);

  /* 显式禁用内部参考，确保使用外部参考 */
  DAC_DisableInternalRef(&hdac_dac[0]);
  DAC_DisableInternalRef(&hdac_dac[1]);
  DAC_DisableInternalRef(&hdac_dac[2]);

  // INA260初始化
  INA260_Init(&hsmbus2, INA260_DEV_ADDR);
  // INA238初始化
  for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
    //每个设备独立初始化
    INA238_Init(&hsmbus1, i2c1_dev_addrs[i], R_SHUNT1, MAX_CURRENT1);
  }
  for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
    //每个设备独立初始化 
    INA238_Init(&hsmbus2, i2c2_dev_addrs[i], R_SHUNT2, MAX_CURRENT2);
  }
  //等待配置生效 
  HAL_Delay(2);


  for (uint8_t i = 0; i < 8; i++) {
    //每个设备独立初始化 
    DAC_SetVoltage(&hdac_dac[0], i, 0);
    DAC_SetVoltage(&hdac_dac[1], i, 0);
  }
  DAC_SetVoltage(&hdac_dac[2], 0, 0);
  DAC_SetVoltage(&hdac_dac[2], 1, 0);
  //开启中断定时器
  //HAL_TIM_Base_Start_IT(&htim1);

//                                           USB
  USBD_UsrLog("\r\n=== Welcome to stm32 custom-hid driver! ===");
  /* Initialize the USB Device Library */
  if(USBD_Init(&hUsbDeviceFS, &Class_Desc, 0) != USBD_OK)
    Error_Handler();
  /* Store HID Instance Class ID */
  CUSTOMHID_InstID = hUsbDeviceFS.classId;
  /* Register the HID Class */
#ifdef USE_USBD_COMPOSITE
  if(USBD_RegisterClassComposite(&hUsbDeviceFS, USBD_CUSTOM_HID_CLASS, CLASS_TYPE_CHID, CustomHID_EpAdress) != USBD_OK)
#else
    /* Add Supported Class */
  if(USBD_RegisterClass(&hUsbDeviceFS, &USBD_CUSTOM_HID) != USBD_OK)
#endif /* USE_USBD_COMPOSITE */
    Error_Handler();


#ifdef USE_USBD_COMPOSITE
  /* Add Custom HID Interface Class */
  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_CHID, 0) != 0xFF)
  {
    USBD_CUSTOM_HID_RegisterInterface(&hUsbDeviceFS, &USBD_CustomHID_fops);
  }

  // /* Add CDC Interface Class */
  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_CDC, 0) != 0xFF)
  {
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_CDC_Template_fops);
  }
#else
  /* Add Custom HID callbacks */
  USBD_CUSTOM_HID_RegisterInterface(&hUsbDeviceFS, &USBD_CustomHID_fops);

#endif /* USE_USBD_COMPOSITE */

  USBD_Start(&hUsbDeviceFS);
//HAL_GPIO_WritePin(GPIOC,GPIO_PIN_12,GPIO_PIN_SET);
//DAC_SetVoltage(&hdac_dac[1], DAC7568_CH_C, 200);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {

    if (pending_cmd) {
      pending_cmd = 0;
      DAC_SetVoltage(&hdac_dac[pending_config.device_id],
                     pending_config.channel, pending_config.dac_value);
    }

    if (pending_power_off) {
      pending_power_off = 0;
      ExecutePowerOff();
    }

    if (pending_power_on) {
      pending_power_on = 0;
      ExecutePowerOnSequence(&pending_sequence);
    }

    if (current_sampling_enabled) {
      // 读取 I2C1
      memset(adc_values, 0, sizeof(adc_values));
      for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
        INA238_ReadData(&hsmbus1, i2c1_dev_addrs[i], &adc_values[i],
                        INA238_DATA_CURRENT,MAX_CURRENT1);
      }
      // 读取 I2C2
      for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
        INA238_ReadData(&hsmbus2, i2c2_dev_addrs[i], &adc_values[I2C1_INA238_NUM+i],
                        INA238_DATA_CURRENT,MAX_CURRENT2);
      }
      INA260_ReadData(&hsmbus2, INA260_DEV_ADDR, &adc_values[I2C1_INA238_NUM+I2C2_INA238_NUM],
                      INA260_DATA_CURRENT);
      // HAL_Delay(2);
      SendSampleReport(I2C_DATA_CURRENT, adc_values);
    }

    if (voltage_sampling_enabled) {
      // 读取 I2C1
      memset(adc_values, 0, sizeof(adc_values));
      for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
        INA238_ReadData(&hsmbus1, i2c1_dev_addrs[i], &adc_values[i],
                        INA238_DATA_VBUS,MAX_CURRENT1);
      }
      // 读取 I2C2
      for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
        INA238_ReadData(&hsmbus2, i2c2_dev_addrs[i], &adc_values[I2C1_INA238_NUM+i],
                        INA238_DATA_VBUS,MAX_CURRENT2);
      }
      INA260_ReadData(&hsmbus2, INA260_DEV_ADDR, &adc_values[I2C1_INA238_NUM+I2C2_INA238_NUM],
                      INA260_DATA_BUS_VOLTAGE);
      // HAL_Delay(2);
      SendSampleReport(I2C_DATA_VBUS, adc_values);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_1);
}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPDMA1_Init(void)
{

  /* USER CODE BEGIN GPDMA1_Init 0 */

  /* USER CODE END GPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* GPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);

  /* USER CODE BEGIN GPDMA1_Init 1 */

  /* USER CODE END GPDMA1_Init 1 */
  /* USER CODE BEGIN GPDMA1_Init 2 */

  /* USER CODE END GPDMA1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_SMBUS_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hsmbus1.Instance = I2C1;
  hsmbus1.Init.Timing = 0x00300B29;
  hsmbus1.Init.AnalogFilter = SMBUS_ANALOGFILTER_ENABLE;
  hsmbus1.Init.OwnAddress1 = 2;
  hsmbus1.Init.AddressingMode = SMBUS_ADDRESSINGMODE_7BIT;
  hsmbus1.Init.DualAddressMode = SMBUS_DUALADDRESS_DISABLE;
  hsmbus1.Init.OwnAddress2 = 0;
  hsmbus1.Init.OwnAddress2Masks = SMBUS_OA2_NOMASK;
  hsmbus1.Init.GeneralCallMode = SMBUS_GENERALCALL_DISABLE;
  hsmbus1.Init.NoStretchMode = SMBUS_NOSTRETCH_ENABLE;
  hsmbus1.Init.PacketErrorCheckMode = SMBUS_PEC_DISABLE;
  hsmbus1.Init.PeripheralMode = SMBUS_PERIPHERAL_MODE_SMBUS_SLAVE;
  hsmbus1.Init.SMBusTimeout = 0x0000830D;
  if (HAL_SMBUS_Init(&hsmbus1) != HAL_OK)
  {
    Error_Handler();
  }

  /** configuration Alert Mode
  */
  if (HAL_SMBUS_EnableAlert_IT(&hsmbus1) != HAL_OK)
  {
    Error_Handler();
  }

  /** SMBus Fast mode Plus enable
  */
  if (HAL_SMBUSEx_ConfigFastModePlus(&hsmbus1, SMBUS_FASTMODEPLUS_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_SMBUS_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hsmbus2.Instance = I2C2;
  hsmbus2.Init.Timing = 0x00300B29;
  hsmbus2.Init.AnalogFilter = SMBUS_ANALOGFILTER_ENABLE;
  hsmbus2.Init.OwnAddress1 = 2;
  hsmbus2.Init.AddressingMode = SMBUS_ADDRESSINGMODE_7BIT;
  hsmbus2.Init.DualAddressMode = SMBUS_DUALADDRESS_DISABLE;
  hsmbus2.Init.OwnAddress2 = 0;
  hsmbus2.Init.OwnAddress2Masks = SMBUS_OA2_NOMASK;
  hsmbus2.Init.GeneralCallMode = SMBUS_GENERALCALL_DISABLE;
  hsmbus2.Init.NoStretchMode = SMBUS_NOSTRETCH_ENABLE;
  hsmbus2.Init.PacketErrorCheckMode = SMBUS_PEC_DISABLE;
  hsmbus2.Init.PeripheralMode = SMBUS_PERIPHERAL_MODE_SMBUS_SLAVE;
  hsmbus2.Init.SMBusTimeout = 0x0000830D;
  if (HAL_SMBUS_Init(&hsmbus2) != HAL_OK)
  {
    Error_Handler();
  }

  /** configuration Alert Mode
  */
  if (HAL_SMBUS_EnableAlert_IT(&hsmbus2) != HAL_OK)
  {
    Error_Handler();
  }

  /** SMBus Fast mode Plus enable
  */
  if (HAL_SMBUSEx_ConfigFastModePlus(&hsmbus2, SMBUS_FASTMODEPLUS_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief I3C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I3C2_Init(void)
{

  /* USER CODE BEGIN I3C2_Init 0 */

  /* USER CODE END I3C2_Init 0 */

  I3C_FifoConfTypeDef sFifoConfig = {0};
  I3C_CtrlConfTypeDef sCtrlConfig = {0};

  /* USER CODE BEGIN I3C2_Init 1 */

  /* USER CODE END I3C2_Init 1 */
  hi3c2.Instance = I3C2;
  hi3c2.Mode = HAL_I3C_MODE_CONTROLLER;
  hi3c2.Init.CtrlBusCharacteristic.SDAHoldTime = HAL_I3C_SDA_HOLD_TIME_0_5;
  hi3c2.Init.CtrlBusCharacteristic.WaitTime = HAL_I3C_OWN_ACTIVITY_STATE_0;
  hi3c2.Init.CtrlBusCharacteristic.SCLPPLowDuration = 0x1e;
  hi3c2.Init.CtrlBusCharacteristic.SCLI3CHighDuration = 0x13;
  hi3c2.Init.CtrlBusCharacteristic.SCLODLowDuration = 0x1e;
  hi3c2.Init.CtrlBusCharacteristic.SCLI2CHighDuration = 0x00;
  hi3c2.Init.CtrlBusCharacteristic.BusFreeDuration = 0x0d;
  hi3c2.Init.CtrlBusCharacteristic.BusIdleDuration = 0x3e;
  if (HAL_I3C_Init(&hi3c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure FIFO
  */
  sFifoConfig.RxFifoThreshold = HAL_I3C_RXFIFO_THRESHOLD_1_4;
  sFifoConfig.TxFifoThreshold = HAL_I3C_TXFIFO_THRESHOLD_1_4;
  sFifoConfig.ControlFifo = HAL_I3C_CONTROLFIFO_DISABLE;
  sFifoConfig.StatusFifo = HAL_I3C_STATUSFIFO_DISABLE;
  if (HAL_I3C_SetConfigFifo(&hi3c2, &sFifoConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure controller
  */
  sCtrlConfig.DynamicAddr = 0;
  sCtrlConfig.StallTime = 0x00;
  sCtrlConfig.HotJoinAllowed = DISABLE;
  sCtrlConfig.ACKStallState = DISABLE;
  sCtrlConfig.CCCStallState = DISABLE;
  sCtrlConfig.TxStallState = DISABLE;
  sCtrlConfig.RxStallState = DISABLE;
  sCtrlConfig.HighKeeperSDA = DISABLE;
  if (HAL_I3C_Ctrl_Config(&hi3c2, &sCtrlConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I3C2_Init 2 */

  /* USER CODE END I3C2_Init 2 */

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
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x7;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 63999;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 99;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = USBD_FS_SPEED;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2
                          |GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC0 PC1 PC2
                           PC3 PC4 PC5 PC6
                           PC7 PC8 PC9 PC10
                           PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2
                          |GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
 void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
 
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
