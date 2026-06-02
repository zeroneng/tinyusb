#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"

#if CFG_TUD_MSC

#define README_CONTENTS \
"Generic USB RAM MSC test.\r\n\r\n" \
"This tiny FAT12 disk lives only in Daisy RAM.\r\n" \
"Host writes are accepted but vanish on reset.\r\n"

enum {
  DISK_BLOCK_NUM  = 16,
  DISK_BLOCK_SIZE = 512
};

static bool ejected;

static uint8_t ram_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] = {
  {
    0xEB, 0x3C, 0x90, 'M', 'S', 'D', 'O', 'S', '5', '.', '0', 0x00, 0x02, 0x01, 0x01, 0x00,
    0x01, 0x10, 0x00, DISK_BLOCK_NUM, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 'G', 'E', 'N', 'U', 'S',
    'B', ' ', 'R', 'A', 'M', ' ', 'F', 'A', 'T', '1', '2', ' ', ' ', ' ', 0x00, 0x00,
    [510] = 0x55, [511] = 0xAA
  },
  {
    0xF8, 0xFF, 0xFF, 0xFF, 0x0F
  },
  {
    'G', 'E', 'N', 'U', 'S', 'B', ' ', 'R', 'A', 'M', ' ', 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ', 'T', 'X', 'T', 0x20, 0x00, 0xC6, 0x52, 0x6D,
    0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
    sizeof(README_CONTENTS) - 1, 0x00, 0x00, 0x00
  },
  { README_CONTENTS }
};

uint8_t tud_msc_get_maxlun_cb(void)
{
  return 0;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void)lun;
  memcpy(vendor_id, "Generic ", 8);
  memcpy(product_id, "RAM MSC Test    ", 16);
  memcpy(product_rev, "1.0 ", 4);
}

uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize)
{
  (void)lun;
  (void)bufsize;
  memcpy(inquiry_resp->vendor_id, "Generic ", 8);
  memcpy(inquiry_resp->product_id, "RAM MSC Test    ", 16);
  memcpy(inquiry_resp->product_rev, "1.0 ", 4);
  return sizeof(scsi_inquiry_resp_t);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  if(ejected)
    return tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);

  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  (void)lun;
  *block_count = DISK_BLOCK_NUM;
  *block_size = DISK_BLOCK_SIZE;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  (void)lun;
  return true;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void)lun;
  (void)power_condition;

  if(load_eject)
    ejected = !start;

  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  (void)lun;

  if(lba >= DISK_BLOCK_NUM || offset >= DISK_BLOCK_SIZE ||
     (lba * DISK_BLOCK_SIZE + offset + bufsize) > (DISK_BLOCK_NUM * DISK_BLOCK_SIZE))
    return -1;

  memcpy(buffer, ram_disk[lba] + offset, bufsize);
  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  (void)lun;

  if(lba >= DISK_BLOCK_NUM || offset >= DISK_BLOCK_SIZE ||
     (lba * DISK_BLOCK_SIZE + offset + bufsize) > (DISK_BLOCK_NUM * DISK_BLOCK_SIZE))
    return -1;

  memcpy(ram_disk[lba] + offset, buffer, bufsize);
  return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  (void)buffer;
  (void)bufsize;
  (void)scsi_cmd;

  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
  return -1;
}

#endif
