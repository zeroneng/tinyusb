#include "generic_usb_msc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "daisy_seed.h"
#include "global.h"
#include "generic_usb_cdc.h"
#include "per/sdmmc.h"
#include "tusb.h"

extern "C" {
extern SD_HandleTypeDef hsd1;
}

#if CFG_TUD_MSC

using namespace daisy;

enum {
    kBlockSize = 512,
    kSdTimeoutMs = 1000,
};

static SdmmcHandler sdmmc;
static bool         sd_ready;
static bool         sd_init_attempted;
static uint32_t     sd_block_count;
static uint16_t     sd_block_size = kBlockSize;
static uint8_t DMA_BUFFER_MEM_SECTION sd_sector[kBlockSize] __attribute__((aligned(32)));
static HAL_StatusTypeDef last_hal_status;
static uint32_t          last_hal_error;
static HAL_SD_CardStateTypedef last_card_state;

static bool ensure_sd_ready(void);

static bool wait_for_transfer(void)
{
    uint32_t const start = HAL_GetTick();
    while((last_card_state = HAL_SD_GetCardState(&hsd1)) != HAL_SD_CARD_TRANSFER)
    {
        if((HAL_GetTick() - start) > 1000u)
        {
            last_hal_error = hsd1.ErrorCode;
            return false;
        }
    }

    return true;
}

static bool sd_read_blocks(uint32_t lba, uint8_t *buffer, uint32_t count)
{
    if(!ensure_sd_ready() || count == 0)
        return false;

    last_hal_status = HAL_SD_ReadBlocks(&hsd1, buffer, lba, count, kSdTimeoutMs);
    last_hal_error  = hsd1.ErrorCode;
    if(last_hal_status != HAL_OK)
    {
        return false;
    }

    if(!wait_for_transfer())
        return false;

    SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t *>(buffer),
                                 count * kBlockSize);
    return true;
}

static bool sd_write_blocks(uint32_t lba, uint8_t const *buffer, uint32_t count)
{
    if(!ensure_sd_ready() || count == 0)
        return false;

    SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(buffer)),
                            count * kBlockSize);

    last_hal_status = HAL_SD_WriteBlocks(&hsd1,
                                         const_cast<uint8_t *>(buffer),
                                         lba,
                                         count,
                                         kSdTimeoutMs);
    last_hal_error  = hsd1.ErrorCode;
    if(last_hal_status != HAL_OK)
    {
        return false;
    }

    return wait_for_transfer();
}

static bool sd_read_sector(uint32_t lba)
{
    memset(sd_sector, 0, sizeof(sd_sector));
    return sd_read_blocks(lba, sd_sector, 1);
}

static void log_sd_failure(char const *op, uint32_t lba)
{
#if DEBUG_TEST_CDC
    char line[128];
    snprintf(line,
             sizeof(line),
             "MSC SD %s FAIL lba=%lu hal=%d err=0x%08lx state=%d\r\n",
             op,
             static_cast<unsigned long>(lba),
             static_cast<int>(last_hal_status),
             static_cast<unsigned long>(last_hal_error),
             static_cast<int>(last_card_state));
    GenericUSB_CDC_WriteString(line);
    GenericUSB_CDC_Flush();
#else
    (void)op;
    (void)lba;
#endif
}

static bool sd_init_with_config(SdmmcHandler::Config const &sd_cfg)
{
    sd_ready       = false;
    sd_block_count = 0;
    sd_block_size  = kBlockSize;

    HAL_SD_DeInit(&hsd1);
    sdmmc.Init(sd_cfg);

    if(HAL_SD_Init(&hsd1) != HAL_OK)
        return false;

    if(sd_cfg.width == SdmmcHandler::BusWidth::BITS_4
       && HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
    {
        return false;
    }

    HAL_SD_CardInfoTypeDef info;
    if(HAL_SD_GetCardInfo(&hsd1, &info) != HAL_OK)
        return false;

    if(info.LogBlockNbr == 0 || info.LogBlockSize == 0)
        return false;

    sd_block_count = info.LogBlockNbr;
    sd_block_size  = static_cast<uint16_t>(info.LogBlockSize);
    sd_ready       = true;

    return sd_block_size == kBlockSize;
}

void GenericUSB_MSCInit(void)
{
    sd_ready          = false;
    sd_init_attempted = false;
    sd_block_count    = 0;
    sd_block_size     = kBlockSize;
    last_hal_status   = HAL_OK;
    last_hal_error    = 0;
    last_card_state   = HAL_SD_CARD_READY;
}

static bool ensure_sd_ready(void)
{
    if(sd_ready)
        return true;

    if(sd_init_attempted)
        return false;

    sd_init_attempted = true;

    SdmmcHandler::Config sd_cfg;
    sd_cfg.width = SdmmcHandler::BusWidth::BITS_1;
    sd_cfg.speed = SdmmcHandler::Speed::MEDIUM_SLOW;
    sd_cfg.clock_powersave = false;
    return sd_init_with_config(sd_cfg);
}

extern "C" uint8_t tud_msc_get_maxlun_cb(void)
{
    return 0;
}

extern "C" void tud_msc_inquiry_cb(uint8_t lun,
                                    uint8_t vendor_id[8],
                                    uint8_t product_id[16],
                                    uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id, "Generic ", 8);
    memcpy(product_id, "SD MSC Test     ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

extern "C" uint32_t tud_msc_inquiry2_cb(uint8_t lun,
                                         scsi_inquiry_resp_t *inquiry_resp,
                                         uint32_t bufsize)
{
    (void)lun;
    (void)bufsize;
    memcpy(inquiry_resp->vendor_id, "Generic ", 8);
    memcpy(inquiry_resp->product_id, "SD MSC Test     ", 16);
    memcpy(inquiry_resp->product_rev, "1.0 ", 4);
    return sizeof(scsi_inquiry_resp_t);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    if(!ensure_sd_ready())
    {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

extern "C" void tud_msc_capacity_cb(uint8_t lun,
                                     uint32_t *block_count,
                                     uint16_t *block_size)
{
    (void)lun;
    bool const ready = ensure_sd_ready();
    *block_count = ready ? sd_block_count : 0;
    *block_size  = ready ? sd_block_size : kBlockSize;
}

extern "C" bool tud_msc_is_writable_cb(uint8_t lun)
{
    (void)lun;
    return true;
}

extern "C" bool tud_msc_start_stop_cb(uint8_t lun,
                                       uint8_t power_condition,
                                       bool start,
                                       bool load_eject)
{
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;

    return true;
}

extern "C" int32_t tud_msc_read10_cb(uint8_t lun,
                                      uint32_t lba,
                                      uint32_t offset,
                                      void *buffer,
                                      uint32_t bufsize)
{
    (void)lun;

    if(!sd_ready || sd_block_size != kBlockSize || lba >= sd_block_count
       || offset >= kBlockSize || bufsize == 0)
    {
        return -1;
    }

    uint8_t *dst      = static_cast<uint8_t *>(buffer);
    uint32_t done     = 0;
    uint32_t cur_lba  = lba;
    uint32_t cur_off  = offset;
    uint32_t remaining = bufsize;

    while(remaining > 0)
    {
        if(cur_lba >= sd_block_count)
            return -1;

        uint32_t const chunk = (remaining < (kBlockSize - cur_off))
                                   ? remaining
                                   : (kBlockSize - cur_off);
        if(!sd_read_sector(cur_lba))
        {
            tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x00);
            log_sd_failure("READ", cur_lba);
            return -1;
        }

        memcpy(dst + done, sd_sector + cur_off, chunk);

        done += chunk;
        remaining -= chunk;
        cur_lba++;
        cur_off = 0;
    }

    return static_cast<int32_t>(bufsize);
}

extern "C" int32_t tud_msc_write10_cb(uint8_t lun,
                                       uint32_t lba,
                                       uint32_t offset,
                                       uint8_t *buffer,
                                       uint32_t bufsize)
{
    (void)lun;

    if(!sd_ready || sd_block_size != kBlockSize || lba >= sd_block_count
       || offset >= kBlockSize || bufsize == 0)
    {
        return -1;
    }

    uint32_t done      = 0;
    uint32_t cur_lba   = lba;
    uint32_t cur_off   = offset;
    uint32_t remaining = bufsize;

    while(remaining > 0)
    {
        if(cur_lba >= sd_block_count)
            return -1;

        uint32_t const chunk = (remaining < (kBlockSize - cur_off))
                                   ? remaining
                                   : (kBlockSize - cur_off);

        if(chunk != kBlockSize || cur_off != 0)
        {
            if(!sd_read_sector(cur_lba))
            {
                tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x00);
                log_sd_failure("RMWREAD", cur_lba);
                return -1;
            }
        }

        memcpy(sd_sector + cur_off, buffer + done, chunk);

        if(!sd_write_blocks(cur_lba, sd_sector, 1))
        {
            tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x00);
            log_sd_failure("WRITE", cur_lba);
            return -1;
        }

        done += chunk;
        remaining -= chunk;
        cur_lba++;
        cur_off = 0;
    }

    return static_cast<int32_t>(bufsize);
}

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun,
                                    uint8_t const scsi_cmd[16],
                                    void *buffer,
                                    uint16_t bufsize)
{
    (void)buffer;
    (void)bufsize;
    (void)scsi_cmd;

    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

#endif
