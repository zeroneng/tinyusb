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
static bool sd_using_1bit;
static uint32_t sd_next_check_ms;
static uint32_t sd_check_count;

static void sd_log(const char *s)
{
    GenericUSB_CDC_WriteString(s);
    GenericUSB_CDC_Flush();
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
        GenericUSB_CDC_Write(reinterpret_cast<uint8_t const *>(line),
                             static_cast<uint32_t>(len));
        GenericUSB_CDC_Flush();
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
        GenericUSB_CDC_Write(reinterpret_cast<uint8_t const *>(line),
                             static_cast<uint32_t>(len));
        GenericUSB_CDC_Flush();
    }
}

static void sd_init_bus(SdmmcHandler::BusWidth width)
{
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.width = width;
    sd_cfg.speed = SdmmcHandler::Speed::STANDARD;
    sdmmc.Init(sd_cfg);
}

static FRESULT sd_mount_once_with_width(SdmmcHandler::BusWidth width)
{
    sd_init_bus(width);
    return f_mount(&fatfs.GetSDFileSystem(), fatfs.GetSDPath(), 1);
}

static void sd_mount_once(void)
{
    sd_mount_attempted = true;

    if(fatfs.Init(FatFSInterface::Config::MEDIA_SD) != FatFSInterface::Result::OK)
    {
        sd_log("SD INIT FAIL: FatFS link\r\n");
        return;
    }

    FRESULT res = sd_mount_once_with_width(SdmmcHandler::BusWidth::BITS_4);
    if(res == FR_OK)
    {
        sd_mounted = true;
        sd_using_1bit = false;
        sd_log("SD MOUNT OK: 4-bit\r\n");
        return;
    }

    sd_log_fresult("SD MOUNT 4-bit FAIL: ", res);

    res = sd_mount_once_with_width(SdmmcHandler::BusWidth::BITS_1);
    if(res == FR_OK)
    {
        sd_mounted = true;
        sd_using_1bit = true;
        sd_log("SD MOUNT OK: 1-bit\r\n");
        return;
    }

    sd_log_fresult("SD MOUNT 1-bit FAIL: ", res);
    sd_log_card_info();
}

static void sd_check_file(void)
{
    static constexpr char kPath[] = "0:/CLUSDCHK.TXT";
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
                             "SD CHECK OK: %lu %s\r\n",
                             static_cast<unsigned long>(sd_check_count),
                             sd_using_1bit ? "1-bit" : "4-bit");
    if(len > 0)
    {
        GenericUSB_CDC_Write(reinterpret_cast<uint8_t const *>(line),
                             static_cast<uint32_t>(len));
        GenericUSB_CDC_Flush();
    }
}

#endif

void GenericUSB_SDInit(void)
{
#if DEBUG_TEST_SD
    sd_mount_attempted = false;
    sd_mounted = false;
    sd_using_1bit = false;
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
