#ifndef FLASH_MAP_H
#define FLASH_MAP_H

#include <inttypes.h>

struct flash_area {
    uint8_t fa_id;
    uint8_t fa_device_id;
    uint16_t pad16;
    uint32_t fa_off;
    uint32_t fa_size;
};

int flash_area_to_sectors(int, int *, struct flash_area *);
void flash_area_close(const struct flash_area *);
int flash_area_open(uint8_t, const struct flash_area **);
int flash_area_read(const struct flash_area *, uint32_t, void *, uint32_t);
int flash_area_write(const struct flash_area *, uint32_t, const void *, uint32_t);
int flash_area_erase(const struct flash_area *, uint32_t, uint32_t);
uint8_t flash_area_align(const struct flash_area *);
int flash_area_id_from_image_slot(int);

#endif
