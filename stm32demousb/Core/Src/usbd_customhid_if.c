/**
  ******************************************************************************
  * @file    usbd_customhid_if.c
  * @author  MCD Application Team
  * @brief   USB Device Custom HID interface file.
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


/* Includes ------------------------------------------------------------------ */
#include "usbd_customhid_if.h"
#include "main.h"
#include "dac_driver.h"

/* Private typedef ----------------------------------------------------------- */
/* Private define ------------------------------------------------------------ */
/* Private macro ------------------------------------------------------------- */
/* Private function prototypes ----------------------------------------------- */
static int8_t CustomHID_Init(void);
static int8_t CustomHID_DeInit(void);
static int8_t CustomHID_OutEvent(uint8_t event_idx, uint8_t cmd_id, uint8_t *data);

/* Private variables --------------------------------------------------------- */
ALIGN_32BYTES (uint32_t ADCConvertedValue[8]) = {0};
uint32_t ADC_Prev_ConvertedValue = 0;
uint8_t SendBuffer[2];
extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t CUSTOMHID_InstID;
extern uint8_t Sample_State;

__ALIGN_BEGIN static uint8_t
CustomHID_ReportDesc[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END = {
	0x06, 0xFF, 0x00,             /* USAGE_PAGE (Vendor Page: 0xFF00) */
	0x09, 0x01,                   /* USAGE (Demo Kit) */
	0xa1, 0x01,                   /* COLLECTION (Application) */
	/* 6 */

	/* LED1 */
	0x85, SAMPLE_REPORT_ID,         /* REPORT_ID (1) */
	0x09, 0x01,                   /* USAGE (LED 1) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x08,                   /* REPORT_SIZE (8) */
	0x95, SAMPLE_REPORT_COUNT,      /* REPORT_COUNT (1) */
	0xB1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x85, SAMPLE_REPORT_ID,         /* REPORT_ID (1) */
	0x09, 0x01,                   /* USAGE (LED 1) */
	0x91, 0x82,                   /* OUTPUT (Data,Var,Abs,Vol) */
	/* 26 */

	/* LED2 */
	0x85, LED2_REPORT_ID,         /* REPORT_ID 2 */
	0x09, 0x02,                   /* USAGE (LED 2) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x08,                   /* REPORT_SIZE (8) */
	0x95, LED2_REPORT_COUNT,      /* REPORT_COUNT (1) */
	0xB1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x85, LED2_REPORT_ID,         /* REPORT_ID (2) */
	0x09, 0x02,                   /* USAGE (LED 2) */
	0x91, 0x82,                   /* OUTPUT (Data,Var,Abs,Vol) */
	/* 46 */

	/* LED3 */
	0x85, LED3_REPORT_ID,         /* REPORT_ID (3) */
	0x09, 0x03,                   /* USAGE (LED 3) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x08,                   /* REPORT_SIZE (8) */
	0x95, LED3_REPORT_COUNT,      /* REPORT_COUNT (1) */
	0xB1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x85, LED3_REPORT_ID,         /* REPORT_ID (3) */
	0x09, 0x03,                   /* USAGE (LED 3) */
	0x91, 0x82,                   /* OUTPUT (Data,Var,Abs,Vol) */
	/* 66 */

	/* LED4 */
	0x85, LED4_REPORT_ID,         /* REPORT_ID 4) */
	0x09, 0x04,                   /* USAGE (LED 4) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x08,                   /* REPORT_SIZE (8) */
	0x95, LED4_REPORT_COUNT,      /* REPORT_COUNT (1) */
	0xB1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x85, LED4_REPORT_ID,         /* REPORT_ID (4) */
	0x09, 0x04,                   /* USAGE (LED 4) */
	0x91, 0x82,                   /* OUTPUT (Data,Var,Abs,Vol) */
	/* 86 */

	/* key Push Button */
	0x85, KEY_REPORT_ID,          /* REPORT_ID (5) */
	0x09, 0x05,                   /* USAGE (Push Button) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x01,                   /* REPORT_SIZE (1) */
	0x81, 0x82,                   /* INPUT (Data,Var,Abs,Vol) */

	0x09, 0x05,                   /* USAGE (Push Button) */
	0x75, 0x01,                   /* REPORT_SIZE (1) */
	0xb1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x75, 0x07,                   /* REPORT_SIZE (7) */
	0x81, 0x83,                   /* INPUT (Cnst,Var,Abs,Vol) */
	0x85, KEY_REPORT_ID,          /* REPORT_ID (2) */

	0x75, 0x07,                   /* REPORT_SIZE (7) */
	0xb1, 0x83,                   /* FEATURE (Cnst,Var,Abs,Vol) */
	/* 114 */

	/* Tamper Push Button */
	0x85, TAMPER_REPORT_ID,       /* REPORT_ID (6) */
	0x09, 0x06,                   /* USAGE (Tamper Push Button) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x25, 0x01,                   /* LOGICAL_MAXIMUM (1) */
	0x75, 0x01,                   /* REPORT_SIZE (1) */
	0x81, 0x82,                   /* INPUT (Data,Var,Abs,Vol) */

	0x09, 0x06,                   /* USAGE (Tamper Push Button) */
	0x75, 0x01,                   /* REPORT_SIZE (1) */
	0xb1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */

	0x75, 0x07,                   /* REPORT_SIZE (7) */
	0x81, 0x83,                   /* INPUT (Cnst,Var,Abs,Vol) */
	0x85, TAMPER_REPORT_ID,       /* REPORT_ID (6) */

	0x75, 0x07,                   /* REPORT_SIZE (7) */
	0xb1, 0x83,                   /* FEATURE (Cnst,Var,Abs,Vol) */
	/* 142 */

	/* ADC IN */
	0x85, ADC_REPORT_ID,          /* REPORT_ID */
	0x09, 0x07,                   /* USAGE (ADC IN) */
	0x15, 0x00,                   /* LOGICAL_MINIMUM (0) */
	0x26, 0xff, 0x00,             /* LOGICAL_MAXIMUM (255) */
	0x75, 0x08,                   /* REPORT_SIZE (8) */
	0x95, SAMPLE_REPORT_COUNT,      /* REPORT_COUNT (1) */
	0x81, 0x82,                   /* INPUT (Data,Var,Abs,Vol) */
	0x85, ADC_REPORT_ID,          /* REPORT_ID (7) */
	0x09, 0x07,                   /* USAGE (ADC in) */
	0xb1, 0x82,                   /* FEATURE (Data,Var,Abs,Vol) */
	/* 161 */

	0xc0                          /* END_COLLECTION */
};

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops = {
	CustomHID_ReportDesc,
	CustomHID_Init,
	CustomHID_DeInit,
	CustomHID_OutEvent,
};

/* Private functions --------------------------------------------------------- */

/**
  * @brief  CustomHID_Init
  *         Initializes the CUSTOM HID media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CustomHID_Init(void)
{
	// USBD_UsrLog("CustomHID_Init");

	// SendBuffer[0] = KEY_REPORT_ID;
	// SendBuffer[1] = 0x01;
	// USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, SendBuffer, 2);

	return (0);
}

/**
  * @brief  CustomHID_DeInit
  *         DeInitializes the CUSTOM HID media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CustomHID_DeInit(void)
{
	/*
	 * Add your de-initialization code here */
	return (0);
}


/**
  * @brief  CustomHID_OutEvent
  *         Manage the CUSTOM HID class Out Event
  * @param  event_idx: LED Report Number
  * @param  state: LED states (ON/OFF)
  */
static int8_t CustomHID_OutEvent(uint8_t event_idx, uint8_t cmd_id, uint8_t *data)
{
	VoltageConfigPacket_t *cfg = (VoltageConfigPacket_t*)data;
	USBD_UsrLog("VoltageConfigPacket_t event_idx= %d cmd_id=%d %d %d %d", event_idx, cfg->cmd_id, cfg->channel, cfg->voltage_mv, cfg->current_ma);

	switch (cmd_id) {
		case 1:                      /* Config voltage */
			DAC_VoltageConfig(cfg);
			break;

		case 2:                      /* Powen on/off */
			DAC_PowerOnOff(*data);
			break;

		case 3:                      /* Start sampling */
			Sample_State = *(data + 1);
			// ADC_SampleControl(*data);
			break;

		case 4:                      /* End sampling */

			break;

		default:
			break;
	}

	/* Start next USB packet transfer once data processing is completed */
	USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS);

	return (0);
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
