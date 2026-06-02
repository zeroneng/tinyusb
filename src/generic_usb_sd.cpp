#include "generic_usb_sd.h"

#include <stdio.h>
#include <string.h>

#include "daisy_seed.h"
#include "ff.h"
#include "generic_usb_cdc.h"
#include "generic_usb_port.h"
#include "global.h"
#include "per/sdmmc.h"
#include "sys/fatfs.h"

using namespace daisy;

#if DEBUG_TEST_SD

extern "C" {
extern SD_HandleTypeDef hsd1;
}

static SdmmcHandler sdmmc;
static FatFSInterface fatfs;
static bool sd_mount_attempted;
static bool sd_mounted;
static uint32_t sd_next_check_ms;
static uint32_t sd_check_count;
static uint8_t sd_sector[512] __attribute__((aligned(32)));

static void sd_write(uint8_t const *data, uint32_t len)
{
    uint32_t offset = 0;
    uint32_t idle_start = HAL_GetTick();

    while(offset < len)
    {
        uint32_t const written = GenericUSB_CDC_Write(data + offset, len - offset);
        if(written > 0)
        {
            offset += written;
            idle_start = HAL_GetTick();
        }
        else
        {
            if((HAL_GetTick() - idle_start) > 100)
                break;
        }

        GenericUSB_CDC_Flush();
        GenericUSB_Task();
    }

    GenericUSB_CDC_Flush();
}

static void sd_log(const char *s)
{
    if(s)
        sd_write(reinterpret_cast<uint8_t const *>(s),
                 static_cast<uint32_t>(strlen(s)));
}

static const char *sd_fresult_name(FRESULT res)
{
    switch(res)
    {
        case FR_OK: return "FR_OK";
        case FR_DISK_ERR: return "FR_DISK_ERR";
        case FR_INT_ERR: return "FR_INT_ERR";
        case FR_NOT_READY: return "FR_NOT_READY";
        case FR_NO_FILE: return "FR_NO_FILE";
        case FR_NO_PATH: return "FR_NO_PATH";
        case FR_INVALID_NAME: return "FR_INVALID_NAME";
        case FR_DENIED: return "FR_DENIED";
        case FR_EXIST: return "FR_EXIST";
        case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
        case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
        case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
        case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
        case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
        case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
        case FR_TIMEOUT: return "FR_TIMEOUT";
        case FR_LOCKED: return "FR_LOCKED";
        case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
        default: return "FR_UNKNOWN";
    }
}

static void sd_log_fresult(const char *label, FRESULT res)
{
    char line[80];
    int const len = snprintf(line,
                             sizeof(line),
                             "%s%s (%d)\r\n",
                             label,
                             sd_fresult_name(res),
                             static_cast<int>(res));

    if(len > 0)
    {
        sd_write(reinterpret_cast<uint8_t const *>(line),
                 static_cast<uint32_t>(len));
    }
}

static void sd_log_card_info(void)
{
    HAL_SD_CardInfoTypeDef info;
    if(HAL_SD_GetCardInfo(&hsd1, &info) != HAL_OK)
    {
        sd_log("SD CARD INFO FAIL\r\n");
        return;
    }

    char line[96];
    int const len = snprintf(line,
                             sizeof(line),
                             "SD CARD INFO: blocks=%lu block_size=%lu\r\n",
                             static_cast<unsigned long>(info.LogBlockNbr),
                             static_cast<unsigned long>(info.LogBlockSize));
    if(len > 0)
    {
        sd_write(reinterpret_cast<uint8_t const *>(line),
                 static_cast<uint32_t>(len));
    }
}

static uint32_t sd_le32(uint8_t const *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
           | (static_cast<uint32_t>(p[2]) << 16)
           | (static_cast<uint32_t>(p[3]) << 24);
}

static void sd_log_sector_summary(char const *label, uint8_t const *sector)
{
    char line[128];
    int len = snprintf(line,
                       sizeof(line),
                       "%s sig=%02X%02X first=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       label,
                       sector[510],
                       sector[511],
                       sector[0],
                       sector[1],
                       sector[2],
                       sector[3],
                       sector[4],
                       sector[5],
                       sector[6],
                       sector[7]);
    if(len > 0)
    {
        sd_write(reinterpret_cast<uint8_t const *>(line),
                 static_cast<uint32_t>(len));
    }
}

static void sd_log_partition_summary(uint8_t const *sector)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        uint8_t const *entry = sector + 446 + (i * 16);
        uint8_t const type = entry[4];
        uint32_t const lba = sd_le32(entry + 8);
        uint32_t const count = sd_le32(entry + 12);

        if(type == 0 && lba == 0 && count == 0)
            continue;

        char line[80];
        int const len = snprintf(line,
                                 sizeof(line),
                                 "SD PART %lu: type=%02X lba=%lu blocks=%lu\r\n",
                                 static_cast<unsigned long>(i + 1),
                                 type,
                                 static_cast<unsigned long>(lba),
                                 static_cast<unsigned long>(count));
        if(len > 0)
        {
            sd_write(reinterpret_cast<uint8_t const *>(line),
                     static_cast<uint32_t>(len));
        }
    }
}

static bool sd_read_sector(uint32_t lba)
{
    memset(sd_sector, 0, sizeof(sd_sector));
    if(HAL_SD_ReadBlocks(&hsd1, sd_sector, lba, 1, HAL_MAX_DELAY) != HAL_OK)
        return false;

    while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {}
    return true;
}

static void sd_log_raw_layout(void)
{
    if(!sd_read_sector(0))
    {
        sd_log("SD RAW READ LBA0 FAIL\r\n");
        return;
    }

    sd_log_sector_summary("SD LBA0", sd_sector);
    sd_log_partition_summary(sd_sector);

    uint8_t const first_type = sd_sector[446 + 4];
    if(first_type == 0xEE)
    {
        sd_log("SD LAYOUT: GPT MBR type EE\r\n");
        return;
    }

    uint32_t const first_lba = sd_le32(sd_sector + 446 + 8);
    if(first_lba == 0 || first_lba == 0xFFFFFFFFu)
        return;

    if(!sd_read_sector(first_lba))
    {
        sd_log("SD RAW READ PART1 FAIL\r\n");
        return;
    }

    sd_log_sector_summary("SD PART1 BOOT", sd_sector);
}

static void sd_log_path(void)
{
    char line[48];
    int const len = snprintf(line,
                             sizeof(line),
                             "SD FATFS PATH: %s\r\n",
                             fatfs.GetSDPath());
    if(len > 0)
    {
        sd_write(reinterpret_cast<uint8_t const *>(line),
                 static_cast<uint32_t>(len));
    }
}

static void sd_init_bus(void)
{
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sdmmc.Init(sd_cfg);
}

static FRESULT sd_mount_once_default(void)
{
    sd_init_bus();

    if(fatfs.Init(FatFSInterface::Config::MEDIA_SD) != FatFSInterface::Result::OK)
    {
        sd_log("SD INIT FAIL: FatFS link\r\n");
        return FR_NOT_ENABLED;
    }

    sd_log_path();
    return f_mount(&fatfs.GetSDFileSystem(), "/", 1);
}

static void sd_mount_once(void)
{
    sd_mount_attempted = true;

    FRESULT res = sd_mount_once_default();
    if(res == FR_OK)
    {
        sd_mounted = true;
        sd_log("SD MOUNT OK\r\n");
        return;
    }

    sd_log_fresult("SD MOUNT FAIL: ", res);
    sd_log_card_info();
    sd_log_raw_layout();
}

static void sd_check_file(void)
{
    static constexpr char kPath[] = "CLUSDCHK.TXT";
    FIL file;
    UINT count = 0;
    char expected[48];
    char actual[sizeof(expected)] = {};

    sd_check_count++;

    int const expected_len = snprintf(expected,
                                      sizeof(expected),
                                      "CLU SD CHECK %lu\r\n",
                                      static_cast<unsigned long>(sd_check_count));
    if(expected_len <= 0 || expected_len >= static_cast<int>(sizeof(expected)))
    {
        sd_log("SD CHECK FAIL: format\r\n");
        return;
    }

    FRESULT res = f_open(&file, kPath, FA_CREATE_ALWAYS | FA_WRITE);
    if(res != FR_OK)
    {
        sd_log_fresult("SD CHECK OPEN-W FAIL: ", res);
        return;
    }

    res = f_write(&file, expected, static_cast<UINT>(expected_len), &count);
    f_close(&file);
    if(res != FR_OK || count != static_cast<UINT>(expected_len))
    {
        sd_log_fresult("SD CHECK WRITE FAIL: ", res);
        return;
    }

    res = f_open(&file, kPath, FA_OPEN_EXISTING | FA_READ);
    if(res != FR_OK)
    {
        sd_log_fresult("SD CHECK OPEN-R FAIL: ", res);
        return;
    }

    res = f_read(&file, actual, static_cast<UINT>(expected_len), &count);
    f_close(&file);
    if(res != FR_OK || count != static_cast<UINT>(expected_len)
       || memcmp(expected, actual, static_cast<size_t>(expected_len)) != 0)
    {
        sd_log_fresult("SD CHECK READ FAIL: ", res);
        return;
    }

    char line[64];
    int const len = snprintf(line,
                             sizeof(line),
                             "SD CHECK OK: %lu\r\n",
                             static_cast<unsigned long>(sd_check_count));
    if(len > 0)
    {
        sd_write(reinterpret_cast<uint8_t const *>(line),
                 static_cast<uint32_t>(len));
    }
}

#endif

void GenericUSB_SDInit(void)
{
#if DEBUG_TEST_SD
    sd_mount_attempted = false;
    sd_mounted = false;
    sd_next_check_ms = 0;
    sd_check_count = 0;
#endif
}

void GenericUSB_SDTask(void)
{
#if DEBUG_TEST_SD
    if(!GenericUSB_CDC_Connected())
        return;

    uint32_t const now = GenericUSB_NowMs();

    if(!sd_mount_attempted)
    {
        sd_mount_once();
        sd_next_check_ms = now + 1000u;
        return;
    }

    if(!sd_mounted || static_cast<int32_t>(now - sd_next_check_ms) < 0)
        return;

    sd_next_check_ms = now + 1000u;
    sd_check_file();
#endif
}
