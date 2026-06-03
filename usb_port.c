#include "usb_port.h"

#include "stm32h7xx_hal.h"
#include "tusb.h"
#include "global.h"
#include "usb_audio.h"
#include "usb_cdc.h"
#include "usb_hid.h"
#include "usb_midi.h"
#include "usb_msc.h"

#ifndef BOARD_TUD_RHPORT
/* Daisy's external USB connector is wired to OTG HS running at full speed.
 * TinyUSB calls this root hub port 1. Port 0 is the internal OTG FS block.
 */
#define BOARD_TUD_RHPORT 1
#endif

static uint32_t generic_usb_now_ms;

static void generic_usb_force_disconnect(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Pull D+ low briefly before TinyUSB starts. This gives the host a clean
   * detach/attach edge after flashing or soft reset.
   */
  gpio.Pin   = GPIO_PIN_15;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_Delay(25);
}

static void generic_usb_gpio_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* OTG HS in full-speed mode uses the external Daisy USB pins:
   *   PB14 = DM
   *   PB15 = DP
   */
  gpio.Pin       = GPIO_PIN_14 | GPIO_PIN_15;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF12_OTG2_FS;
  HAL_GPIO_Init(GPIOB, &gpio);
}

void GenericUSB_Init(void)
{
  generic_usb_now_ms = 0;

  generic_usb_force_disconnect();
  generic_usb_gpio_init();

  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

  HAL_NVIC_SetPriority(OTG_HS_EP1_OUT_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_EP1_OUT_IRQn);
  HAL_NVIC_SetPriority(OTG_HS_EP1_IN_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_EP1_IN_IRQn);
  HAL_NVIC_SetPriority(OTG_HS_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);

#if USB_ENABLE_AUDIO
  GenericUSB_AudioInit();
#endif
#if USB_ENABLE_CDC
  GenericUSB_CDCInit();
#endif
#if USB_ENABLE_MIDI
  GenericUSB_MIDIInit();
#endif
#if USB_ENABLE_HID
  GenericUSB_HIDInit();
#endif
#if USB_ENABLE_MSC
  GenericUSB_MSCInit();
#endif

  /* The Daisy external port is self-powered from the board side for this
   * bring-up, so do not wait for a hardware VBUS sense pin before attaching.
   */
  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing = false;
  tud_configure(BOARD_TUD_RHPORT, TUD_CFGID_DWC2, &cfg);

  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
}

void GenericUSB_SetNowMs(uint32_t now_ms)
{
  generic_usb_now_ms = now_ms;
}

uint32_t GenericUSB_NowMs(void)
{
  return generic_usb_now_ms;
}

void GenericUSB_Task(void)
{
  tud_task();
#if USB_ENABLE_AUDIO
  GenericUSB_AudioTask();
#endif
#if USB_ENABLE_CDC
  GenericUSB_CDCTask();
#endif
#if USB_ENABLE_MIDI
  GenericUSB_MIDITask();
#endif
#if USB_ENABLE_HID
  GenericUSB_HIDTask();
#endif
}

void GenericUSB_OTG_HS_IRQHandler(void)
{
  tud_int_handler(BOARD_TUD_RHPORT);
}

/* These handlers intentionally replace the libDaisy/ST USB device handlers
 * for OTG HS. Projects that also use libDaisy's USB CDC stack must keep that
 * stack off this peripheral while TinyUSB owns it.
 */
void OTG_HS_IRQHandler(void)
{
  GenericUSB_OTG_HS_IRQHandler();
}

void OTG_HS_EP1_IN_IRQHandler(void)
{
  GenericUSB_OTG_HS_IRQHandler();
}

void OTG_HS_EP1_OUT_IRQHandler(void)
{
  GenericUSB_OTG_HS_IRQHandler();
}

void OTG_HS_WKUP_IRQHandler(void)
{
  GenericUSB_OTG_HS_IRQHandler();
}
