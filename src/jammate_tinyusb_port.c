#include "jammate_tinyusb_port.h"

#include "stm32h7xx_hal.h"
#include "tusb.h"
#include "jammate_tinyusb_audio.h"
#include "jammate_tinyusb_cdc.h"

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

static void jammate_usb_force_disconnect(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin   = GPIO_PIN_15;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_Delay(25);
}

static void jammate_usb_gpio_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin       = GPIO_PIN_14 | GPIO_PIN_15;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF12_OTG2_FS;
  HAL_GPIO_Init(GPIOB, &gpio);
}

void JamMate_TinyUSB_Init(void)
{
  jammate_usb_force_disconnect();
  jammate_usb_gpio_init();

  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

  HAL_NVIC_SetPriority(OTG_HS_EP1_OUT_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_EP1_OUT_IRQn);
  HAL_NVIC_SetPriority(OTG_HS_EP1_IN_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_EP1_IN_IRQn);
  HAL_NVIC_SetPriority(OTG_HS_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);

  JamMate_TinyUSB_AudioInit();
  JamMate_TinyUSB_CDCInit();

  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing = false;
  tud_configure(BOARD_TUD_RHPORT, TUD_CFGID_DWC2, &cfg);

  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
}

void JamMate_TinyUSB_Task(void)
{
  tud_task();
  JamMate_TinyUSB_AudioTask();
  JamMate_TinyUSB_CDCTask();
}

void JamMate_TinyUSB_OTG_HS_IRQHandler(void)
{
  tud_int_handler(BOARD_TUD_RHPORT);
}

void OTG_HS_IRQHandler(void)
{
  JamMate_TinyUSB_OTG_HS_IRQHandler();
}

void OTG_HS_EP1_IN_IRQHandler(void)
{
  JamMate_TinyUSB_OTG_HS_IRQHandler();
}

void OTG_HS_EP1_OUT_IRQHandler(void)
{
  JamMate_TinyUSB_OTG_HS_IRQHandler();
}

void OTG_HS_WKUP_IRQHandler(void)
{
  JamMate_TinyUSB_OTG_HS_IRQHandler();
}
