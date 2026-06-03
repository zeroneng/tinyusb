#include "usb_sd.h"

#include <stdio.h>
#include <string.h>

#include "daisy_seed.h"
#include "ff.h"
#include "usb_cdc.h"
#include "usb_port.h"
#include "global.h"
#include "per/sdmmc.h"
#include "sys/fatfs.h"

using namespace daisy;

#if DEBUG_TEST_SD

static SdmmcHandler sdmmc;
static FatFSInterface fatfs;
static bool sd_mount_attempted;
static bool sd_mounted;
static uint32_t sd_next_check_ms;
static uint32_t sd_check_count;

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
