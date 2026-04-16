#include "usb_message_handler.h"

#include "main.h"
#include "proto_pkg.h"
#include "stm32h5xx_hal.h"
#include "dac_driver.h"
#include "usbd_conf.h"
#include "usbd_customhid_if.h"

#include <stdint.h>
#include <string.h>

extern volatile uint8_t current_sampling_enabled;
extern volatile uint8_t voltage_sampling_enabled;
extern DAC_HandleTypeDef hdac_dac[3];
extern USBD_HandleTypeDef hUsbDeviceFS;

#define USB_MSG_QUEUE_DEPTH 32U

typedef struct {
  uint8_t event_idx;
  uint8_t cmd_id;
  uint16_t payload_len;
  uint8_t payload[DOWNLOAD_PACKET_SIZE];
} UsbMessage_t;

static UsbMessage_t g_usb_queue[USB_MSG_QUEUE_DEPTH];
static volatile uint8_t g_usb_head = 0;
static volatile uint8_t g_usb_tail = 0;
static volatile uint8_t g_usb_count = 0;
static volatile uint32_t g_usb_dropped = 0;

static VoltageConfigPacket_t g_power_cfg[POWER_SUPPLY_COUNT];
static uint8_t g_power_index = 0;

static GPIO_TypeDef *GetPortFromIndex(uint8_t port_index)
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
  if (pin_index >= 16U) {
    return 0;
  }
  return (uint16_t)(1U << (pin_index & 0x0FU));
}

static void ExecutePowerOnSequence(const SequenceConfigPacket_t *sequence)
{
	typedef struct {
		uint8_t power_id;
		uint8_t order;
		uint8_t pin_port;
		uint8_t enable_pin;
		uint16_t delay;
	} PowerStep;

	PowerStep steps[POWER_SUPPLY_COUNT];
	uint8_t i;
	uint8_t j;

	for (i = 0; i < POWER_SUPPLY_COUNT; ++i) {
		steps[i].power_id = i;
		steps[i].order = sequence->sequence[i];
		steps[i].pin_port = g_power_cfg[i].pin_port;
		steps[i].enable_pin = g_power_cfg[i].enable_pin;
		steps[i].delay = sequence->interval_ms[i];
		USBD_DbgLog("Power ID %u: order=%u, port=%u, pin=%u, delay=%ums\r\n",
					steps[i].power_id, steps[i].order, steps[i].pin_port,
					steps[i].enable_pin, steps[i].delay);
	}

	for (i = 0; i < POWER_SUPPLY_COUNT - 1U; ++i) {
		for (j = 0; j < POWER_SUPPLY_COUNT - i - 1U; ++j) {
			if (steps[j].order > steps[j + 1U].order) {
				PowerStep temp = steps[j];
				steps[j] = steps[j + 1U];
				steps[j + 1U] = temp;
			}
		}
	}

	for (i = 0; i < POWER_SUPPLY_COUNT; ++i) {
		GPIO_TypeDef *port = GetPortFromIndex(steps[i].pin_port);
		uint16_t pin_mask = GetPowerEnablePinMask(steps[i].enable_pin);
		if (port != NULL && pin_mask != 0U) {
			HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_SET);
		}

		if (steps[i].delay != 0U) {
			HAL_Delay(steps[i].delay);
		}
	}
}

static void ExecutePowerOff(void)
{
  uint8_t power_id;
  uint8_t ch;

  for (power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
    GPIO_TypeDef *port = GetPortFromIndex(g_power_cfg[power_id].pin_port);
    uint16_t pin_mask = GetPowerEnablePinMask(g_power_cfg[power_id].enable_pin);
    if (port != NULL && pin_mask != 0U) {
      HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_RESET);
    }
  }

  for (ch = 0; ch < 8U; ++ch) {
    DAC_SetVoltage(&hdac_dac[0], ch, 0);
    DAC_SetVoltage(&hdac_dac[1], ch, 0);
  }
  DAC_SetVoltage(&hdac_dac[2], 0, 0);
  DAC_SetVoltage(&hdac_dac[2], 1, 0);
}

static void HandleSetVoltage(const uint8_t *payload)
{
  const VoltageConfigPacket_t *cfg = (const VoltageConfigPacket_t *)payload;
    if(g_power_index > POWER_SUPPLY_COUNT){
        USBD_ErrLog("Power config index out of range\r\n");
        return;
    }
    if (cfg->device_id > 3U) {
        USBD_ErrLog("Invalid device_id: %u\r\n", cfg->device_id);
        return;
    }

    DAC_SetVoltage(&hdac_dac[cfg->device_id], cfg->channel, cfg->dac_value);
    g_power_cfg[g_power_index] = *cfg;
    g_power_index++;
    USBD_DbgLog("Set voltage: device_id=%u, channel=%u, dac_value=%u, pin_port=%u, enable_pin=%u\r\n",
                cfg->device_id, cfg->channel, cfg->dac_value, cfg->pin_port, cfg->enable_pin);

}

static void HandleSetPin(const uint8_t *payload)
{
  const PinConfigPacket_t *pin = (const PinConfigPacket_t *)payload;
  GPIO_TypeDef *port = GetPortFromIndex(pin->port);
  if (port == NULL) {
    return;
  }

  {
    uint16_t pin_mask = (uint16_t)(1U << (pin->pin & 0x0F));
    GPIO_PinState state = (pin->level == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(port, pin_mask, state);
  }
}

static void HandlePowerOn(const uint8_t *payload)
{
  const SequenceConfigPacket_t *sequence = (const SequenceConfigPacket_t *)payload;
  ExecutePowerOnSequence(sequence);
}

static void HandlePowerOff(void)
{
  ExecutePowerOff();
}

static void HandleStartSampling(const uint8_t *payload)
{
  const SampleConfigPacket_t *sample = (const SampleConfigPacket_t *)payload;
  USBD_DbgLog("Start sampling: type=%u, state=%u\r\n", sample->type, sample->state);
  switch (sample->type) {
  case SAMPLE_TYPE_VOLTAGE:
    voltage_sampling_enabled = sample->state;
    break;
  case SAMPLE_TYPE_CURRENT:
    current_sampling_enabled = sample->state;
    break;
  default:
    break;
  }
}

static void DispatchMessage(const UsbMessage_t *msg)
{
  switch (msg->cmd_id) {
  case CMD_SET_VOLTAGE:
    HandleSetVoltage(msg->payload);
    break;
  case CMD_SET_PIN:
    HandleSetPin(msg->payload);
    break;
  case CMD_POWER_ON:
    HandlePowerOn(msg->payload);
    break;
  case CMD_POWER_OFF:
    HandlePowerOff();
    break;
  case CMD_START_SAMPLING:
    HandleStartSampling(msg->payload);
    break;
  default:
    break;
  }
  /* Start next USB packet transfer once data processing is completed */
  USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS);
}

void UsbMsg_Reset(void)
{
  __disable_irq();
  g_usb_head = 0;
  g_usb_tail = 0;
  g_usb_count = 0;
  g_usb_dropped = 0;
  __enable_irq();
}

uint8_t UsbMsg_Enqueue(uint8_t event_idx, uint8_t cmd_id, const uint8_t *data, uint16_t len)
{
  UsbMessage_t *slot;

  if (data == NULL) {
    return 0;
  }

  if (len > DOWNLOAD_PACKET_SIZE) {
    len = DOWNLOAD_PACKET_SIZE;
  }

  __disable_irq();
  if (g_usb_count >= USB_MSG_QUEUE_DEPTH) {
    g_usb_dropped++;
    USBD_ErrLog("USB message queue full, dropping message (total dropped: %lu)\r\n", g_usb_dropped);
    __enable_irq();
    return 0;
  }

  slot = &g_usb_queue[g_usb_tail];
  slot->event_idx = event_idx;
  slot->cmd_id = cmd_id;
  slot->payload_len = len;
  memcpy(slot->payload, data, len);
  if (len < DOWNLOAD_PACKET_SIZE) {
    memset(slot->payload + len, 0, DOWNLOAD_PACKET_SIZE - len);
  }

  g_usb_tail = (uint8_t)((g_usb_tail + 1U) % USB_MSG_QUEUE_DEPTH);
  g_usb_count++;
  __enable_irq();

  return 1;
}

uint8_t UsbMsg_ProcessNext(void)
{
  UsbMessage_t msg;

  __disable_irq();
  if (g_usb_count == 0U) {
    __enable_irq();
    return 0;
  }

  msg = g_usb_queue[g_usb_head];
  g_usb_head = (uint8_t)((g_usb_head + 1U) % USB_MSG_QUEUE_DEPTH);
  g_usb_count--;
  __enable_irq();

  DispatchMessage(&msg);
  return 1;
}

uint32_t UsbMsg_GetDroppedCount(void)
{
  uint32_t dropped;

  __disable_irq();
  dropped = g_usb_dropped;
  __enable_irq();

  return dropped;
}
