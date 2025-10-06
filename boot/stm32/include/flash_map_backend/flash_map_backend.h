#ifndef __FLASH_MAP_BACKEND_H__
#define __FLASH_MAP_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct flash_area {
    uint8_t fa_id;
    uint8_t fa_device_id;
    uint16_t pad;
    uint32_t fa_off;
    uint32_t fa_size;
};

struct flash_sector {
    uint32_t fs_off;
    uint32_t fs_size;
};


int flash_area_open(uint8_t id, const struct flash_area **fa);
void flash_area_close(const struct flash_area *fa);
int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len);
int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len);
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len);





int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors);
int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector);



int flash_area_align(const struct flash_area *fa);
int flash_area_erased_val(const struct flash_area *fa);

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa);

int flash_area_id_from_multi_image_slot(int image_index, int slot);
int flash_area_id_from_image_slot(int slot);
int flash_area_id_to_multi_image_slot(int image_index, int area_id);
static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
    return fa->fa_id;
}

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    return fa->fa_device_id;
}

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
    return fa->fa_off;
}

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
	return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
	return fs->fs_size;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
    return fa->fa_size;
}


#ifdef __cplusplus
}
#endif

#endif /* __FLASH_MAP_BACKEND_H__ */
