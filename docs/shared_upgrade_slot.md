# Shared Upgrade Slot — User Guide

## Overview

In a standard MCUboot multi-image configuration every image has its own
dedicated **primary slot** (boot) and **secondary slot** (upgrade).  The
*shared upgrade slot* feature allows multiple images to share **one physical
flash region** for their secondary slots, reducing flash usage significantly
in systems where only one image is upgraded at a time.

```
Standard layout                      Shared-slot layout
┌──────────────┐ ┌──────────────┐    ┌──────────────┐ ┌──────────────┐
│  Primary 0   │ │  Primary 1   │    │  Primary 0   │ │  Primary 1   │
│  (internal)  │ │  (internal)  │    │  (internal)  │ │  (internal)  │
├──────────────┤ ├──────────────┤    ├──────────────┴─┴──────────────┤
│ Secondary 0  │ │ Secondary 1  │    │    Shared Secondary Region    │
│  (upgrade)   │ │  (upgrade)   │    │  (one physical flash area)    │
└──────────────┘ └──────────────┘    └───────────────────────────────┘
      2× secondary flash                  ~1× secondary flash
```

### Key constraint

> **Only one image may be upgraded at a time.** The secondary slots
> physically overlap; writing upgrade data for two images simultaneously
> would corrupt both.

---

## Supported Upgrade Modes

| Mode | Supported | Scratch needed | Device | Notes |
|------|-----------|----------------|--------|-------|
| Overwrite-only | **Yes** | No | `PSOCEdgeE8xSpiFlash` | Simplest layout |
| Swap-using-scratch | **Yes** | Yes (1 sector) | `PSOCEdgeE8xSpiFlash` | **Deprecated** — prefer swap-using-offset |
| Swap-using-move | **Yes** | No | `PSOCEdgeE8xSpiFlashSwapOffset` | Uses 1st sector of secondary as scratch |
| Swap-using-offset | **Yes** | No | `PSOCEdgeE8xSpiFlashSwapOffset` | Uses 1st sector of secondary as scratch |

---

## Mode 1 — Overwrite-Only

In overwrite-only mode the bootloader simply copies the new image from
the secondary slot into the primary slot.  No swap state or scratch area
is needed, so the layout is the simplest possible.

### Address formula

Each image's virtual secondary slot starts at:

```
secondary_offset(img_id) = shared_region_start + img_id × HDR_SIZE
```

where **HDR_SIZE = 32 bytes** (the MCUboot image header size).  This gives
every image a unique slot address with minimal overhead.

### Memory map (2 images, overwrite-only)

```
  DEVICE 0 — Internal Flash (4 KB sectors)
  ┌────────────────────────────────┐  ┌────────────────────────────────┐
  │     Image 0 — PRIMARY          │  │     Image 1 — PRIMARY          │
  │     0x020000 .. 0x03FFFF       │  │     0x040000 .. 0x05FFFF       │
  │          (128 KB)              │  │          (128 KB)              │
  └────────────────────────────────┘  └────────────────────────────────┘
                │                                    │
           boot_go()                            boot_go()
           copies ↓                             copies ↓

  DEVICE 1 — Shared Upgrade Flash (32-byte sectors)
  ┌───────────────────────────────────────────────────────────────────┐
  │                    ONE PHYSICAL REGION                            │
  ├──────────────────────────────┬┬───────────────────────────────────┤
  │  Secondary 0 (virtual)       ││  Secondary 1 (virtual)            │
  │  offset = 0×32 = 0x000000    ││  offset = 1×32 = 0x000020         │
  │  size   = 128 KB             ││  size   = 128 KB                  │
  │                              ││                                   │
  │  ┌────────────────────────┐  ││  ┌────────────────────────────┐   │
  │  │ HDR  (32 B)            │  ││  │ (shared with img 0 data)   │   │
  │  ├────────────────────────┤  ││  ├────────────────────────────┤   │
  │  │                        │  ││  │ HDR  (32 B)                │   │
  │  │                        │  ││  ├────────────────────────────┤   │
  │  │   image 0 payload      │◄─┼┼─►│   image 1 payload          │   │
  │  │                        │  ││  │                            │   │
  │  │   (OVERLAP — same      │  ││  │   (OVERLAP — same          │   │
  │  │    physical bytes)     │  ││  │    physical bytes)         │   │
  │  │                        │  ││  │                            │   │
  │  └────────────────────────┘  ││  │                            │   │
  │  0x020000  end of slot 0     ││  ├────────────────────────────┤   │
  │                              ││  │ 32 B tail (img 1 only)     │   │
  │                              ││  └────────────────────────────┘   │
  │                              ││  0x02001F  end of slot 1          │
  ├──────────────────────────────┴┴───────────────────────────────────┤
  │  Physical extent: 0x000000 .. 0x02001F  (~128 KB + 32 B)          │
  └───────────────────────────────────────────────────────────────────┘

  Union view — physical addresses on the shared device:
  ┌──────────┬──────────────────────┬───────────────────────────────┐
  │ Address  │  Secondary 0         │  Secondary 1                  │
  ├──────────┼──────────────────────┼───────────────────────────────┤
  │ 0x000000 │ ▓▓ img 0 HDR (32 B)  │           —                   │
  │ 0x000020 │ ░░ img 0 data        │ ▓▓ img 1 HDR (32 B)           │
  │ 0x000040 │ ░░ img 0 data ◄══════╪══► ░░ img 1 data              │
  │   ...    │      (shared physical bytes — overlap)               │
  │ 0x01FFFF │ ░░ img 0 data end    │ ░░ img 1 data                 │
  │ 0x020000 │          —           │ ░░ img 1 data (32 B tail)     │
  │ 0x02001F │          —           │ ░░ img 1 data end             │
  └──────────┴──────────────────────┴───────────────────────────────┘
    ▓▓ = unique to this image    ░░ = data region    ◄══► = overlap
```

### Sector-size requirement

The sector (erase-block) size on the shared device **must be ≤ HDR_SIZE**
so that both offset 0 and offset `HDR_SIZE` are sector-aligned.  In the
simulator this is achieved with 32-byte sectors.

On real hardware with large erase blocks this requirement is satisfied
naturally because the flash driver can present a virtual sector layout
where each "sector" covers exactly `HDR_SIZE` bytes.

### Shared region sizing

Minimum size of the shared region:

```
shared_region_size ≥ (N - 1) × HDR_SIZE + max_boot_slot_size
```

where **N** is the number of images and **max_boot_slot_size** is the
largest primary slot size among all images.

### `sysflash.h` mapping

No changes to `sysflash.h` are required.  For `MCUBOOT_IMAGE_NUMBER == 2`
the existing defines already map:

| FlashId | Slot | Role |
|---------|------|------|
| `Image0` | `FLASH_AREA_IMAGE_PRIMARY(0)` | Primary 0 |
| `Image1` | `FLASH_AREA_IMAGE_SECONDARY(0)` | Secondary 0 (shared device) |
| `Image2` | `FLASH_AREA_IMAGE_PRIMARY(1)` | Primary 1 |
| `Image3` | `FLASH_AREA_IMAGE_SECONDARY(1)` | Secondary 1 (shared device) |

Both `Image1` and `Image3` reside on the same physical flash device but
at different (overlapping) offsets.

---

## Mode 2 — Swap-Using-Scratch

Swap mode requires careful trailer placement.  Each image's boot trailer
is written at the **end** of its secondary slot.  Because the secondary
slots overlap, the trailers must land at **non-overlapping addresses**
so that one image's swap state does not corrupt another image's data or
trailer.

### Additional swap-mode constraint

> **Primary slot size must equal secondary slot size** for each image.
> This is enforced by `boot_slots_compatible()` in `swap_scratch.c`.
> If they differ, boot will print *"Cannot upgrade: slots are not
> compatible"* and refuse to swap.

### Staggering the trailers

Each image's secondary slot is shifted by a **stagger offset** so that
the trailers written at the slot's end address fall at non-overlapping
positions:

```
secondary_offset(img_id) = shared_region_start + img_id × STAGGER
```

The minimum stagger is determined by the **boot trailer size**, not by
the erase sector size:

```
STAGGER ≥ boot_trailer_sz(align)
```

Rounded up to the nearest sector boundary:

```
STAGGER = ceil(boot_trailer_sz(align) / sector_size) × sector_size
```

> On flash with large erase blocks (64–256 KB), using `boot_trailer_sz`
> instead of `erase_sector_size` for the stagger can save significant
> flash.  For example, with 256 KB erase blocks and align=1 the stagger
> is only **432 bytes** (rounded up to the sector size) instead of
> 256 KB.

### Memory map (2 images, swap-using-scratch)

```
  DEVICE 0 — Internal Flash (4 KB sectors)
  ┌────────────────────────────────┐  ┌────────────────────────────────┐
  │     Image 0 — PRIMARY          │  │     Image 1 — PRIMARY          │
  │     0x020000 .. 0x03FFFF       │  │     0x040000 .. 0x05FFFF       │
  │          (128 KB)              │  │          (128 KB)              │
  │  trailer: 0x03FE50..0x040000   │  │  trailer: 0x05FE50..0x060000   │
  └────────────────────────────────┘  └────────────────────────────────┘
                │                                    │
           swap ↕                               swap ↕
                │                                    │
  DEVICE 1 — Shared Upgrade Flash (4 KB sectors)
  ┌───────────────────────────────────────────────────────────────────┐
  │                    ONE PHYSICAL REGION                            │
  ├──────────────────────────────┬┬───────────────────────────────────┤
  │  Secondary 0 (virtual)       ││  Secondary 1 (virtual)            │
  │  offset = 0×0x1000 = 0x0000  ││  offset = 1×0x1000 = 0x1000       │
  │  size   = 128 KB             ││  size   = 128 KB                  │
  │                              ││                                   │
  │  ┌────────────────────────┐  ││                                   │
  │  │ img 0 data  (4 KB)     │  ││           —                       │
  │  │ 0x000000..0x001000     │  ││                                   │
  │  ├━━━━━━━━━━━━━━━━━━━━━━━━┤  ││  ┌────────────────────────────┐   │
  │  │                        │  ││  │                            │   │
  │  │   img 0 data           │◄─┼┼─►│   img 1 data               │   │
  │  │                        │  ││  │                            │   │
  │  │   OVERLAP ZONE         │  ││  │   OVERLAP ZONE             │   │
  │  │   0x001000..0x01F000   │  ││  │   0x001000..0x01F000       │   │
  │  │   (120 KB shared       │  ││  │   (120 KB shared           │   │
  │  │    physical bytes)     │  ││  │    physical bytes)         │   │
  │  │                        │  ││  │                            │   │
  │  ├────────────────────────┤  ││  ├────────────────────────────┤   │
  │  │ ▓▓ img 0 TRAILER       │  ││  │   img 1 data               │   │
  │  │ sector 0x01F000        │  ││  │   (still in overlap)       │   │
  │  │ trailer: 0x01FE50      │  ││  │                            │   │
  │  │          ..0x020000    │  ││  │                            │   │
  │  └────────────────────────┘  ││  ├────────────────────────────┤   │
  │  0x020000 end of slot 0      ││  │ ▓▓ img 1 TRAILER           │   │
  │                              ││  │ sector 0x020000            │   │
  │                              ││  │ trailer: 0x020E50          │   │
  │                              ││  │          ..0x021000        │   │
  │                              ││  └────────────────────────────┘   │
  │                              ││  0x021000 end of slot 1           │
  ├──────────────────────────────┴┴───────────────────────────────────┤
  │  ┌──────────────────────────────────────────────────────────────┐ │
  │  │ SCRATCH   0x021000..0x022000   (4 KB, 1 sector)              │ │
  │  └──────────────────────────────────────────────────────────────┘ │
  │  Physical extent: 0x000000 .. 0x022000  (136 KB)                  │
  └───────────────────────────────────────────────────────────────────┘

  Union view — physical addresses on the shared device (align=1):
  ┌──────────┬────────────────────────┬─────────────────────────────┐
  │ Address  │  Secondary 0           │  Secondary 1                │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x000000 │ ░░ img 0 data          │           —                 │
  │    ...   │     (4 KB unique)      │                             │
  │ 0x000FFF │ ░░                     │           —                 │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x001000 │ ░░ img 0 data ◄════════╪════► ░░ img 1 data          │
  │    ...   │      (shared physical bytes — 120 KB overlap)        │
  │ 0x01EFFF │ ░░              ◄══════╪════► ░░                     │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x01F000 │ ▓▓ TRAILER sector      │ ░░ img 1 data (overlap)     │
  │ 0x01FE50 │ ▓▓ trailer start       │ ░░                          │
  │ 0x01FFFF │ ▓▓ trailer end         │ ░░                          │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x020000 │          —             │ ▓▓ TRAILER sector           │
  │ 0x020E50 │          —             │ ▓▓ trailer start            │
  │ 0x020FFF │          —             │ ▓▓ trailer end              │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x021000 │          SCRATCH (4 KB, shared)                      │
  │ 0x021FFF │                                                      │
  └──────────┴────────────────────────┴─────────────────────────────┘
    ▓▓ = trailer (unique per image, in separate erase sectors)
    ░░ = data region    ◄════► = overlap zone
```

### Trailer separation detail

The boot trailer is written at the end of each slot and has a size that
depends on the write alignment:

```
boot_trailer_sz(align) = BOOT_STATUS_MAX_ENTRIES × BOOT_STATUS_STATE_COUNT × align
                        + BOOT_MAX_ALIGN × 4
                        + BOOT_MAGIC_ALIGN_SIZE

For the default configuration (MAX_IMG_SECTORS=128, STATE_COUNT=3):

  align=1:  128×3×1 + 32×4 + 16 = 432  bytes  (0x1B0)
  align=2:  128×3×2 + 32×4 + 16 = 816  bytes  (0x330)
  align=4:  128×3×4 + 32×4 + 16 = 1584 bytes  (0x630)
  align=8:  128×3×8 + 32×4 + 16 = 3120 bytes  (0xC30)
```

With a stagger of one 4 KB sector (0x1000):

| Component | Image 0 | Image 1 | Separation |
|-----------|---------|---------|------------|
| Slot start | 0x000000 | 0x001000 | 0x1000 |
| Slot end | 0x020000 | 0x021000 | 0x1000 |
| Trailer start (align=1) | 0x01FE50 | 0x020E50 | 0x1000 |
| Trailer end | 0x020000 | 0x021000 | 0x1000 |
| Trailer sector | 0x01F000 | 0x020000 | different sectors |

The trailers always reside in **distinct erase sectors**, so erasing one
trailer's sector during a swap operation never corrupts the other image's
trailer.

### Shared region sizing (swap mode)

```
shared_region_size ≥ (N - 1) × STAGGER + max_boot_slot_size + scratch_size
```

where:
- **N** — number of images
- **STAGGER** — offset between consecutive secondary slots (≥ `boot_trailer_sz`, sector-aligned)
- **max_boot_slot_size** — must equal the matching primary slot size
- **scratch_size** — at least one erase sector

### Scratch area placement

The scratch area must be placed **after** all secondary slot regions:

```
scratch_offset = (N - 1) × STAGGER + secondary_slot_size
```

---

## Mode 3 — Swap-Using-Move / Swap-Using-Offset (Scratchless)

Swap-using-move and swap-using-offset both use the **first sector of each
secondary slot** as a built-in scratch area, eliminating the need for a
separate scratch partition.  This simplifies the shared layout.

### How scratchless swap works

Instead of a dedicated scratch area, the algorithm:

1. Copies the first sector of the secondary slot to the primary slot's
   last sector (move) or uses it as temporary storage (offset).
2. Proceeds to swap remaining sectors in place.
3. The first sector of the secondary is effectively the "scratch".

This means each secondary slot must be the **same size** as its primary
slot — the extra sector is already embedded in the slot.

### Memory map (2 images, scratchless swap)

```
  DEVICE 0 — Internal Flash (4 KB sectors, 384 KB)
  ┌────────────────────────────────┐  ┌────────────────────────────────┐
  │     Image 0 — PRIMARY          │  │     Image 1 — PRIMARY          │
  │     0x020000 .. 0x02FFFF       │  │     0x030000 .. 0x03FFFF       │
  │          (64 KB)               │  │          (64 KB)               │
  └────────────────────────────────┘  └────────────────────────────────┘
                │                                    │
           swap ↕                               swap ↕
                │                                    │
  DEVICE 1 — Shared SPI Flash (4 KB sectors)
  ┌───────────────────────────────────────────────────────────────────┐
  │                    ONE PHYSICAL REGION                            │
  │                    No separate scratch needed                     │
  ├──────────────────────────────┬┬───────────────────────────────────┤
  │  Secondary 0 (virtual)       ││  Secondary 1 (virtual)            │
  │  offset = 0x0000             ││  offset = STAGGER                 │
  │  size   = 64 KB              ││  size   = 64 KB                   │
  │                              ││                                   │
  │  ┌────────────────────────┐  ││                                   │
  │  │ [scratch sector] 4 KB  │  ││           —                       │
  │  │ 0x000000..0x001000     │  ││                                   │
  │  ├━━━━━━━━━━━━━━━━━━━━━━━━┤  ││  ┌────────────────────────────┐   │
  │  │                        │  ││  │ [scratch sector] 4 KB      │   │
  │  │   img 0 data           │◄─┼┼─►│ + img 1 data               │   │
  │  │                        │  ││  │                            │   │
  │  │   OVERLAP ZONE         │  ││  │   OVERLAP ZONE             │   │
  │  │                        │  ││  │                            │   │
  │  ├────────────────────────┤  ││  ├────────────────────────────┤   │
  │  │ ▓▓ img 0 TRAILER       │  ││  │   img 1 data (overlap)     │   │
  │  └────────────────────────┘  ││  ├────────────────────────────┤   │
  │  end of slot 0               ││  │ ▓▓ img 1 TRAILER           │   │
  │                              ││  └────────────────────────────┘   │
  │                              ││  end of slot 1                    │
  ├──────────────────────────────┴┴───────────────────────────────────┤
  │  Physical extent: primary_sz + STAGGER                            │
  │  No scratch partition!                                            │
  └───────────────────────────────────────────────────────────────────┘

  Union view — physical addresses on the shared device (align=1):
  ┌──────────┬────────────────────────┬─────────────────────────────┐
  │ Address  │  Secondary 0           │  Secondary 1                │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x000000 │ ██ scratch sector      │           —                 │
  │ 0x000FFF │ ██                     │           —                 │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x001000 │ ░░ img 0 data ◄════════╪════► ██ scratch sector      │
  │ 0x001FFF │ ░░              ◄══════╪════► ██                     │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x002000 │ ░░ img 0 data ◄════════╪════► ░░ img 1 data          │
  │    ...   │      (shared physical bytes — overlap)               │
  │ 0x00EFFF │ ░░              ◄══════╪════► ░░                     │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x00F000 │ ▓▓ TRAILER sector      │ ░░ img 1 data (overlap)     │
  │ 0x00FE50 │ ▓▓ trailer start       │ ░░                          │
  │ 0x00FFFF │ ▓▓ trailer end         │ ░░                          │
  ├──────────┼────────────────────────┼─────────────────────────────┤
  │ 0x010000 │          —             │ ▓▓ TRAILER sector           │
  │ 0x010E50 │          —             │ ▓▓ trailer start            │
  │ 0x010FFF │          —             │ ▓▓ trailer end              │
  └──────────┴────────────────────────┴─────────────────────────────┘
    ██ = scratch sector (1st sector of each secondary, used by algorithm)
    ▓▓ = trailer (unique per image, in separate erase sectors)
    ░░ = data region    ◄════► = overlap zone
```

### Advantages over swap-using-scratch

- **No separate scratch partition** — saves one erase sector.
- **Simpler layout** — only primary and secondary slots, no third area.
- **Identical stagger formula** — same `ceil(boot_trailer_sz / sector) × sector`.

### Shared region sizing (scratchless swap)

```
shared_region_size ≥ (N - 1) × STAGGER + max_boot_slot_size
```

Note: no `+ scratch_size` term — scratch is embedded in each secondary slot.

---

## Generalisation to N Images

The layout extends to 3 or more images by adding `STAGGER` per image.

### Swap-using-scratch (3 images)

```
 Device 1 (shared):
 ┌──────────┬──────────────────────────────────────────────────┐
 │ 0x000000 │ Secondary 0: [0x000000 .. 0x020000)   128 KB     │
 │ 0x001000 │ Secondary 1: [0x001000 .. 0x021000)   128 KB     │
 │ 0x002000 │ Secondary 2: [0x002000 .. 0x022000)   128 KB     │
 ├──────────┼──────────────────────────────────────────────────┤
 │ 0x022000 │ Scratch:     [0x022000 .. 0x023000)     4 KB     │
 └──────────┴──────────────────────────────────────────────────┘

 Trailer locations (align=1, trailer=432 B):
   img 0: 0x01FE50..0x020000  — in sector [0x01F000..0x020000)
   img 1: 0x020E50..0x021000  — in sector [0x020000..0x021000)
   img 2: 0x021E50..0x022000  — in sector [0x021000..0x022000)
   All trailers in separate sectors ✓

 Total = 2 × 0x1000 + 0x20000 + 0x1000 = 0x23000 (140 KB)
 vs. standard = 3 × 0x20000 = 0x60000 (384 KB) → 64% savings
```

### Scratchless swap — move / offset (3 images)

```
 Device 1 (shared):
 ┌──────────┬──────────────────────────────────────────────────┐
 │ 0x000000 │ Secondary 0: [0x000000 .. 0x020000)   128 KB     │
 │ 0x001000 │ Secondary 1: [0x001000 .. 0x021000)   128 KB     │
 │ 0x002000 │ Secondary 2: [0x002000 .. 0x022000)   128 KB     │
 └──────────┴──────────────────────────────────────────────────┘
 (no scratch partition)

 Total = 2 × 0x1000 + 0x20000 = 0x22000 (136 KB)
 vs. standard = 3 × 0x20000 = 0x60000 (384 KB) → 65% savings
```

### General formulas

| Parameter | Overwrite-only | Swap-using-scratch | Swap-using-move / offset |
|-----------|----------------|--------------------|--------------------------|
| Slot offset | `img_id × HDR_SIZE` | `img_id × STAGGER` | `img_id × STAGGER` |
| Sector alignment | sector ≤ HDR_SIZE | STAGGER sector-aligned | STAGGER sector-aligned |
| Slot size | any | must equal primary | must equal primary |
| Stagger formula | `HDR_SIZE` | `ceil(trailer_sz / sector) × sector` | `ceil(trailer_sz / sector) × sector` |
| Scratch | not needed | ≥ 1 sector, after all slots | not needed (embedded) |
| Shared region min | `(N-1)×HDR + max_slot` | `(N-1)×STAGGER + slot + scratch` | `(N-1)×STAGGER + slot` |

---

## Checklist for Integration

1. **Set `MCUBOOT_IMAGE_NUMBER`** to the number of images (e.g. 2).

2. **Define flash areas** in your BSP so that all secondary slots map to
   the same physical flash device with the appropriate offsets.

3. **Choose the upgrade mode** and stagger:
   - Overwrite-only: `STAGGER = HDR_SIZE` (32 bytes).  No scratch.
   - Swap-using-scratch: `STAGGER = ceil(boot_trailer_sz(align) / sector_size) × sector_size`.
     Requires a scratch partition after the last secondary slot.
   - Swap-using-move / swap-using-offset:
     `STAGGER = ceil(boot_trailer_sz(align) / sector_size) × sector_size`.
     No scratch partition — the first sector of each secondary is used as
     built-in scratch by the algorithm.

4. **Ensure sector alignment**: every secondary slot's start offset must be
   aligned to the erase sector size.

5. **Size the secondary slots** equal to their corresponding primary slots
   (required for all swap modes; recommended for overwrite-only).

6. **Place scratch** (swap-using-scratch only) immediately after the last
   secondary slot region, sized to at least one erase sector.
   **Not needed** for swap-using-move or swap-using-offset.

7. **Application firmware** must ensure only one image is written to the
   shared region at a time.  Writing two upgrade images simultaneously
   will corrupt both, since the data regions overlap.

8. **Verify** with the simulator:
   ```bash
   # Overwrite-only
   cargo test --features 'sig-ecdsa,multiimage,overwrite-only' \
       shared_spi_flash_upgrade_slot

   # Swap-using-scratch
   cargo test --features 'sig-ecdsa,multiimage' \
       shared_spi_flash_upgrade_slot

   # Swap-using-move
   cargo test --features 'sig-ecdsa,multiimage,swap-move' \
       shared_spi_flash_swap_offset

   # Swap-using-offset
   cargo test --features 'sig-ecdsa,multiimage,swap-offset' \
       shared_spi_flash_swap_offset
   ```
