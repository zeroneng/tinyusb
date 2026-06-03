#include "generic_usb_msc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "daisy_seed.h"
#include "generic_usb_cdc.h"
#include "global.h"
#include "per/sdmmc.h"
#include "tusb.h"
#include "util/bsp_sd_diskio.h"

#if CFG_TUD_MSC

using namespace daisy;

enum {
    kBlockSize = 512,
    kSdTransferTimeoutMs = 1000,
};

static SdmmcHandler sdmmc;
static bool         sd_ready;
static bool         sd_init_attempted;
static uint32_t     sd_block_count;
static uint16_t     sd_block_size = kBlockSize;
/* Scratch sector for partial-sector MSC transfers. Keep it in libDaisy's
 * DMA-safe memory section because the BSP SD block APIs may use DMA under the
 * hood. Direct stack or normal static buffers caused unreliable SD reads in
 * earlier STM HAL experiments.
 */
static uint8_t DMA_BUFFER_MEM_SECTION sd_sector[kBlockSize] __attribute__((aligned(32)));
static uint8_t last_sd_result = MSD_OK;
static uint8_t last_sd_state  = SD_TRANSFER_OK;

static bool ensure_sd_ready(void);

static bool sd_read_blocks(uint32_t lba, uint8_t *buffer, uint32_t count)
{
    if(!ensure_sd_ready() || count == 0)
        return false;

    last_sd_result = BSP_SD_ReadBlocks(reinterpret_cast<uint32_t *>(buffer),
                                       lba,
                                       count,
                                       kSdTransferTimeoutMs);
    if(last_sd_result != MSD_OK)
        return false;

    uint32_t const start = System::GetNow();
    do
    {
        last_sd_state = BSP_SD_GetCardState();
        if(last_sd_state == SD_TRANSFER_OK)
            return true;
    } while((System::GetNow() - start) < kSdTransferTimeoutMs);

    return false;
}

static bool sd_write_blocks(uint32_t lba, uint8_t const *buffer, uint32_t count)
{
    if(!ensure_sd_ready() || count == 0)
        return false;

    last_sd_result = BSP_SD_WriteBlocks(
        reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(buffer)),
        lba,
        count,
        kSdTransferTimeoutMs);
    if(last_sd_result != MSD_OK)
        return false;

    uint32_t const start = System::GetNow();
    do
    {
        last_sd_state = BSP_SD_GetCardState();
        if(last_sd_state == SD_TRANSFER_OK)
            return true;
    } while((System::GetNow() - start) < kSdTransferTimeoutMs);

    return false;
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
             "MSC SD %s FAIL lba=%lu sd=%u state=%u\r\n",
             op,
             static_cast<unsigned long>(lba),
             static_cast<unsigned int>(last_sd_result),
             static_cast<unsigned int>(last_sd_state));
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

    sdmmc.Init(sd_cfg);

    last_sd_result = BSP_SD_Init();
    if(last_sd_result != MSD_OK)
        return false;

    BSP_SD_CardInfo info;
    BSP_SD_GetCardInfo(&info);
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
    last_sd_result    = MSD_OK;
    last_sd_state     = SD_TRANSFER_OK;
}

static bool ensure_sd_ready(void)
{
    if(sd_ready)
        return true;

    if(sd_init_attempted)
        return false;

    sd_init_attempted = true;

    SdmmcHandler::Config sd_cfg;
    /* Conservative SD settings are intentional. Faster/4-bit modes enumerated
     * but timed out under host sector reads on the tested cards.
     */
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
            /* TinyUSB can ask for byte ranges that do not cover a whole
             * sector. SD writes are sector-granular, so preserve untouched
             * bytes with a read/modify/write cycle.
             */
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
