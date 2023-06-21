"""MCUBoot Flash Map Converter (JSON to .h)
Copyright (c) 2022 Infineon Technologies AG
"""

import sys
import getopt
import json
from enum import Enum
import os.path

class Error(Enum):
    ''' Application error codes '''
    ARG             = 1
    POLICY          = 2
    FLASH           = 3
    IO              = 4
    JSON            = 5
    VALUE           = 6
    CONFIG_MISMATCH = 7

SERVICE_APP_SZ = 0x20

c_array = 'flash_areas'

# Supported Platforms
cm0pCore = {
    'cortex-m0+': 'CM0P',
    'cm0+': 'CM0P',
    'm0+': 'CM0P',
    'cortex-m0p': 'CM0P',
    'cm0p': 'CM0P',
    'm0p': 'CM0P',
    'cortex-m0plus': 'CM0P',
    'cm0plus': 'CM0P',
    'm0plus': 'CM0P'
}

cm4Core = {
    'cortex-m4': 'CM4',
    'cm4': 'CM4',
    'm4': 'CM4'
}

cm33Core = {
    'cortex-m33': 'CM33',
    'cm33': 'CM33',
    'm33': 'CM33'
}

cm7Core = {
    'cortex-m7': 'CM7',
    'cm7': 'CM7',
    'm7': 'CM7'
}

allCores_PSOC_06x = {**cm0pCore, **cm4Core}

common_PSOC_061 = {
    'flashAddr': 0x10000000,
    'eraseSize': 0x200,  # 512 bytes
    'smifAddr': 0x18000000,
    'smifSize': 0x8000000,  # i.e., window size
    'VTAlign': 0x400,  # Vector Table alignment
    'allCores': cm4Core,
    'bootCore': 'Cortex-M4',
    'appCore': 'Cortex-M4'
}

common_PSOC_06x = {
    'flashAddr': 0x10000000,
    'eraseSize': 0x200,  # 512 bytes
    'smifAddr': 0x18000000,
    'smifSize': 0x8000000,  # i.e., window size
    'VTAlign': 0x400,  # Vector Table alignment
    'allCores': allCores_PSOC_06x,
    'bootCore': 'Cortex-M0+',
    'appCore': 'Cortex-M4'
}

common_XMC7000 = {
    'flashAddr': 0x10000000,
    'eraseSize': 0x8000,  # 512 bytes
    'smifAddr': 0x18000000,
    'smifSize': 0x8000000,  # i.e., window size
    'VTAlign': 0x400,  # Vector Table alignment
    'allCores': cm7Core,
    'bootCore': 'Cortex-M7',
    'appCore': 'Cortex-M7'
}

common_PSE84 = {
    'flashAddr': 0x32000000,
    'flashSize': 0x40000,
    'eraseSize': 0x20,  # 32 bytes
    'smifAddr': 0x60000000, #secure address 
    'smifSize': 0x4000000,  # i.e., window size
    'VTAlign': 0x400,  # Vector Table alignment
    'allCores': cm33Core,
    'bootCore': 'Cortex-M33',
    'appCore': 'Cortex-M33'
}

platDict = {
    'PSOC_061_2M': {
        'flashSize': 0x200000,  # 2 MBytes
        **common_PSOC_061
    },
    'PSOC_061_1M': {
        'flashSize': 0x100000,  # 1 MByte
        **common_PSOC_061
    },
    'PSOC_061_512K': {
        'flashSize': 0x80000,  # 512 KBytes
        **common_PSOC_061
    },

    'PSOC_062_2M': {
        'flashSize': 0x200000,  # 2 MBytes
        **common_PSOC_06x
    },
    'PSOC_062_1M': {
        'flashSize': 0x100000,  # 1 MByte
        **common_PSOC_06x
    },
    'PSOC_062_512K': {
        'flashSize': 0x80000,  # 512 KBytes
        **common_PSOC_06x
    },

    'PSOC_063_1M': {
        'flashSize': 0x100000,  # 1 MByte
        **common_PSOC_06x
    },

    'XMC7200': {
        'flashSize': 0x100000,  # 1 MByte
        **common_XMC7000
    },

    'XMC7100': {
        'flashSize': 0x100000,  # 1 MByte
        **common_PSOC_06x
    },

    'CYW20829': {
        'flashSize': 0,  # n/a
        'smifAddr': 0x60000000,
        'smifSize': 0x8000000,  # i.e., window size
        'VTAlign': 0x200,  # Vector Table alignment
        'allCores': cm33Core,
        'bootCore': 'Cortex-M33',
        'appCore': 'Cortex-M33',
        'bitsPerCnt': False
    },

    'PSE84_L4': {
        **common_PSE84
    },

    'PSE84_L2': {
        **common_PSE84
    }
}

# Supported SPI Flash ICs
flashDict = {
    # Fudan
    'FM25Q04': {
        'flashSize': 0x80000,  # 4 Mbits
        'eraseSize': 0x1000,  # 128 uniform sectors with 4K-byte each
    },
    'FM25W04': {
        'flashSize': 0x80000,  # 4 Mbits
        'eraseSize': 0x1000,  # 128 uniform sectors with 4K-byte each
    },
    'FM25Q08': {
        'flashSize': 0x100000,  # 8 Mbits
        'eraseSize': 0x1000,  # 256 uniform sectors with 4K-byte each
    },
    'FM25W08': {
        'flashSize': 0x100000,  # 8 Mbits
        'eraseSize': 0x1000,  # 256 uniform sectors with 4K-byte each
    },
    # Puya
    'P25Q05H': {
        'flashSize': 0x10000,  # 512 Kbits
        'eraseSize': 0x1000,  # Uniform 4K-byte Sector Erase
    },
    'P25Q10H': {
        'flashSize': 0x20000,  # 1 Mbit
        'eraseSize': 0x1000,  # Uniform 4K-byte Sector Erase
    },
    'P25Q20H': {
        'flashSize': 0x40000,  # 2 Mbits
        'eraseSize': 0x1000,  # Uniform 4K-byte Sector Erase
    },
    'P25Q40H': {
        'flashSize': 0x80000,  # 4 Mbits
        'eraseSize': 0x1000,  # Uniform 4K-byte Sector Erase
    },
    # Infineon
    'S25HS256T': {
        'flashSize': 0x2000000,  # 256 Mbits
        'eraseSize': 0x40000,  # Uniform Sector Architecture
    },
    'S25HS512T': {
        'flashSize': 0x4000000,  # 512 Mbits
        'eraseSize': 0x40000,  # Uniform Sector Architecture
    },
    'S25HS01GT': {
        'flashSize': 0x8000000,  # 1 Gbit
        'eraseSize': 0x40000,  # Uniform Sector Architecture
    }
}


def is_overlap(fa1off, fa1size, fa2off, fa2size, align):
    """Check if two flash areas on the same device overlap"""
    mask = align - 1
    assert align > 0 and (align & mask) == 0  # ensure align is a power of 2
    fa1end = (fa1off + fa1size + mask) & ~mask
    fa2end = (fa2off + fa2size + mask) & ~mask
    fa1off = fa1off & ~mask
    fa2off = fa2off & ~mask
    return fa1off < fa2end and fa2off < fa1end


def is_same_mem(fa1addr, fa2addr):
    """Check if two addresses belong to the same memory"""
    if fa1addr is None or fa2addr is None:
        return False
    mask = 0xFF000000
    return (fa1addr & mask) == (fa2addr & mask)


class CmdLineParams:
    """Command line parameters"""

    def __init__(self):
        self.plat_id = ''
        self.in_file = ''
        self.out_file = ''
        self.fa_file = ''
        self.img_id = None
        self.policy = None
        self.set_core = False
        self.image_boot_config = False

        usage = 'USAGE:\n' + sys.argv[0] + \
                ''' -p <platform> -i <flash_map.json> -o <memorymap.c> -a <memorymap.h> -d <img_id> -c <policy.json>

OPTIONS:
-h  --help       Display the usage information
-p  --platform=  Target (e.g., PSOC_062_512K)
-i  --ifile=     JSON flash map file
-o  --ofile=     C file to be generated
-a  --fa_file=   path where to create 'memorymap.h'
-d  --img_id     ID of application to build
-c  --policy     Policy file in JSON format
-m  --core       Detect and set Cortex-M CORE
-x  --image_boot_config Generate image boot config structure
'''

        try:
            opts, unused = getopt.getopt(
                sys.argv[1:], 'hi:o:a:p:d:c:x:m',
                ['help', 'platform=', 'ifile=', 'ofile=', "fa_file=", 'img_id=', 'policy=', 'core', 'image_boot_config'])
        except getopt.GetoptError:
            print(usage, file=sys.stderr)
            sys.exit(Error.ARG)

        for opt, arg in opts:
            if opt in ('-h', '--help'):
                print(usage, file=sys.stderr)
                sys.exit()
            elif opt in ('-p', '--platform'):
                self.plat_id = arg
            elif opt in ('-i', '--ifile'):
                self.in_file = arg
            elif opt in ('-o', '--ofile'):
                self.out_file = arg
            elif opt in ('-a', '--fa_file'):
                self.fa_file = arg
            elif opt in ('-d', '--img_id'):
                self.img_id = arg
            elif opt in ('-c', '--policy'):
                self.policy = arg
            elif opt in ('-m', '--core'):
                self.set_core = True
            elif opt in ('x', '--image_boot_config'):
                self.image_boot_config = True

        if len(self.in_file) == 0 or len(self.out_file) == 0 or len(self.fa_file) == 0:
            print(usage, file=sys.stderr)
            sys.exit(Error.ARG)


class AreaList:
    '''
    A List of flash areas
    ...

    Attributes
    ----------
    plat : dict
        Platform settings

    flash : dict
        External flash settings

    use_overwrite : bool
        Overwrite configuration in use

    areas : list
        Flash area parameter list

    peers : set
        Peers

    trailers : set
        Flash area trailers

    internal_flash : bool
        Internal flash in use

    external_flash : bool
        External flash in use

    external_flash_xip : bool
        External XIP in use

    Methods
    -------
    get_min_erase_size:
        Calculate minimum erase block size for int./ext. Flash

    get_img_trailer_size:
        Calculate image trailer size

    process_int_area:
        Process internal flash area

    process_ext_area:
        Process external flash area

    chk_area:
        Check area location (internal/external flash)

    add_area:
        Add flash area to AreaList.
        Internal/external flash is detected by address.

    generate_c_source:
        Generate C source

    create_flash_area_id:
        Creates flash_area_id.h file.
    '''

    def __init__(self, plat, flash, use_overwrite):
        self.plat = plat
        self.flash = flash
        self.use_overwrite = use_overwrite
        self.areas = []
        self.peers = {}
        self.trailers = {}
        self.internal_flash = False
        self.external_flash = False
        self.external_flash_xip = False

    def get_min_erase_size(self):
        '''Calculate minimum erase block size for int./ext. Flash '''
        return self.plat['eraseSize'] if self.plat['flashSize'] > 0 \
            else self.flash['eraseSize']

    def get_img_trailer_size(self):
        '''Calculate image trailer size'''
        return self.get_min_erase_size()

    def process_int_area(self, title, addr, fa_size,
                         img_trailer_size, shared_slot):
        '''
        Process internal flash area
        Parameters:
        ----------
        title : str
            Area name

        addr : int
            Area address

        fa_size : int
            Area size

        img_trailer_size : int
            Trailer size

        shared_slot : bool
            Shared slot option in use

        Returns:
        ----------
        fa_device_id : str

        fa_off : int

        slot_sectors : int
        '''
        fa_device_id = 'FLASH_DEVICE_INTERNAL_FLASH'
        fa_off = addr - self.plat['flashAddr']
        if img_trailer_size is not None:
            if self.use_overwrite:
                if shared_slot:
                    print('Shared slot', title,
                          'is not supported in OVERWRITE mode',
                          file=sys.stderr)
                    sys.exit(Error.CONFIG_MISMATCH)
            else:
                # Check trailer alignment (start at the sector boundary)
                align = (fa_off + fa_size - img_trailer_size) % \
                        self.plat['eraseSize']
                if align != 0:
                    addr += self.plat['eraseSize'] - align
                    if addr + fa_size <= \
                            self.plat['flashAddr'] + self.plat['flashSize']:
                        print('Misaligned', title,
                              '- suggested address', hex(addr),
                              file=sys.stderr)
                    else:
                        print('Misaligned', title, file=sys.stderr)
                    sys.exit(Error.CONFIG_MISMATCH)
        else:
            # Check alignment (flash area should start at the sector boundary)
            if fa_off % self.plat['eraseSize'] != 0:
                print('Misaligned', title, file=sys.stderr)
                sys.exit(Error.CONFIG_MISMATCH)
        slot_sectors = int((fa_off % self.plat['eraseSize'] +
                            fa_size + self.plat['eraseSize'] - 1) //
                           self.plat['eraseSize'])
        return fa_device_id, fa_off, slot_sectors

    def process_ext_area(self, title, addr, fa_size,
                         img_trailer_size, shared_slot):
        '''
        Process external flash area
        Parameters:
        ----------
        title : str
            Area name

        addr : int
            Area address

        fa_size : int
            Area size

        img_trailer_size : int
            Trailer size

        shared_slot : bool
            Shared slot option in use

        Returns:
        ----------
        fa_device_id : str

        fa_off : int

        slot_sectors : int
        '''
        if self.flash is None:
            print('Unspecified SPI Flash IC',
                  file=sys.stderr)
            sys.exit(Error.FLASH)
        if addr + fa_size <= \
                self.plat['smifAddr'] + self.flash['flashSize']:
            flash_idx = 'CY_BOOT_EXTERNAL_DEVICE_INDEX'
            fa_device_id = f'FLASH_DEVICE_EXTERNAL_FLASH({flash_idx})'
            fa_off = addr - self.plat['smifAddr']
        else:
            print('Misfitting', title, file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)
        if img_trailer_size is not None:
            if self.use_overwrite:
                if shared_slot:
                    print('Shared slot', title,
                          'is not supported in OVERWRITE mode',
                          file=sys.stderr)
                    sys.exit(Error.CONFIG_MISMATCH)
            else:
                # Check trailer alignment (start at the sector boundary)
                align = (fa_off + fa_size - img_trailer_size) % \
                        self.flash['eraseSize']
                if align != 0:
                    peer_addr = self.peers.get(addr)
                    if shared_slot:
                        # Special case when using both int. and ext. memory
                        if self.plat['flashSize'] > 0 and \
                                align % self.plat['eraseSize'] == 0:
                            print('Note:', title, 'requires', align,
                                  'padding bytes before trailer',
                                  file=sys.stderr)
                        else:
                            print('Misaligned', title, file=sys.stderr)
                            sys.exit(Error.CONFIG_MISMATCH)
                    elif is_same_mem(addr, peer_addr) and \
                            addr % self.flash['eraseSize'] == \
                            peer_addr % self.flash['eraseSize']:
                        pass  # postpone checking
                    else:
                        addr += self.flash['eraseSize'] - align
                        if addr + fa_size <= \
                                self.plat['smifAddr'] + self.flash['flashSize']:
                            print('Misaligned', title,
                                  '- suggested address', hex(addr),
                                  file=sys.stderr)
                        else:
                            print('Misaligned', title, file=sys.stderr)
                        sys.exit(Error.CONFIG_MISMATCH)
        else:
            # Check alignment (flash area should start at the sector boundary)
            if fa_off % self.flash['eraseSize'] != 0:
                print('Misaligned', title, file=sys.stderr)
                sys.exit(Error.CONFIG_MISMATCH)
        slot_sectors = int((fa_off % self.flash['eraseSize'] +
                            fa_size + self.flash['eraseSize'] - 1) //
                           self.flash['eraseSize'])
        self.external_flash = True
        if self.flash['XIP']:
            self.external_flash_xip = True
        return fa_device_id, fa_off, slot_sectors

    def chk_area(self, addr, fa_size, peer_addr=None):
        '''
        Check area location (internal/external flash)
        Parameters:
        ----------
        addr : int
            Area address

        fa_size : int
            Area size

        peer_addr : bool (optional)
            Shared slot option in use

        Returns:
        ----------
        None
        '''
        if peer_addr is not None:
            self.peers[peer_addr] = addr
        fa_limit = addr + fa_size
        if self.plat['flashSize'] and \
                addr >= self.plat['flashAddr'] and \
                fa_limit <= self.plat['flashAddr'] + self.plat['flashSize']:
            # Internal flash
            self.internal_flash = True

    def add_area(self, title,
                 fa_id, addr, fa_size,
                 img_trailer_size=None, shared_slot=False):
        '''
        Add flash area to AreaList.
        Internal/external flash is detected by address.
        Parameters:
        ----------
        title : str
            Area name

        fa_id : str
            Area id

        addr : int
            Area address

        fa_size : int
            Area size

        img_trailer_size : int
            Trailer size (optional)

        shared_slot : bool
            Shared slot option in use (optional)

        Returns:
        ----------
        slot_sectors : int
            Number of sectors in a slot
        '''
        if fa_size == 0:
            print('Empty', title, file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)

        fa_limit = addr + fa_size
        if self.plat['flashSize'] and \
                addr >= self.plat['flashAddr'] and \
                fa_limit <= self.plat['flashAddr'] + self.plat['flashSize']:
            # Internal flash
            fa_device_id, fa_off, slot_sectors = self.process_int_area(
                title, addr, fa_size, img_trailer_size, shared_slot)
            align = self.plat['eraseSize']
        elif self.plat['smifSize'] and \
                addr >= self.plat['smifAddr'] and \
                fa_limit <= self.plat['smifAddr'] + self.plat['smifSize']:
            # External flash
            fa_device_id, fa_off, slot_sectors = self.process_ext_area(
                title, addr, fa_size, img_trailer_size, shared_slot)
            align = self.flash['eraseSize']
        else:
            print('Invalid', title, file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)

        if shared_slot:
            assert img_trailer_size is not None
            tr_addr = addr + fa_size - img_trailer_size
            tr_name = self.trailers.get(tr_addr)
            if tr_name is not None:
                print('Same trailer address for', title, 'and', tr_name,
                      file=sys.stderr)
                sys.exit(Error.CONFIG_MISMATCH)
            self.trailers[tr_addr] = title

        # Ensure no flash areas on this device will overlap, except the
        # shared slot
        for area in self.areas:
            if fa_device_id == area['fa_device_id']:
                over = is_overlap(fa_off, fa_size,
                                  area['fa_off'], area['fa_size'],
                                  align)
                if shared_slot and area['shared_slot']:
                    if not over:  # images in shared slot should overlap
                        print(title, 'is not shared with', area['title'],
                              file=sys.stderr)
                        sys.exit(Error.CONFIG_MISMATCH)
                elif over:
                    print(title, 'overlaps with', area['title'],
                          file=sys.stderr)
                    sys.exit(Error.CONFIG_MISMATCH)

        self.areas.append({'title': title,
                           'shared_slot': shared_slot,
                           'fa_id': fa_id,
                           'fa_device_id': fa_device_id,
                           'fa_off': fa_off,
                           'fa_size': fa_size})
        return slot_sectors

    def generate_c_source(self, params):
        '''
        Generate C source
        Parameters:
        ----------
        params : CmdLineParams
            Application parameters

        Returns:
        ----------
        None
        '''
        c_array = 'flash_areas'

        try:
            with open(params.out_file, "w", encoding='UTF-8') as out_f:

                out_f.write(f'#include "{params.fa_file}"\n')
                out_f.write(f'#include "flash_map_backend.h"\n\n')
                out_f.write(f'#include "flash_map_backend_platform.h"\n\n')
                out_f.write(f'struct flash_area {c_array}[] = {{\n')
                comma = len(self.areas)
                area_count = 0
                for area in self.areas:
                    comma -= 1
                    if area['fa_id'] is not None:
                        sss = ' /* Shared secondary slot */' \
                            if area['shared_slot'] else ''
                        out_f.writelines('\n'.join([
                            '    {' + sss,
                            f"        .fa_id        = {area['fa_id']},",
                            f"        .fa_device_id = {area['fa_device_id']},",
                            f"        .fa_off       = {hex(area['fa_off'])}U,",
                            f"        .fa_size      = {hex(area['fa_size'])}U",
                            '    },' if comma else '    }', '']))
                        area_count += 1
                out_f.write('};\n\n'
                            'struct flash_area *boot_area_descs[] = {\n')
                for area_index in range(area_count):
                    out_f.write(f'    &{c_array}[{area_index}U],\n')
                out_f.write('    NULL\n};\n')                
                out_f.close()

        except (FileNotFoundError, OSError):
            print('Cannot create', params.out_file, file=sys.stderr)
            sys.exit(Error.IO)

    def create_flash_area_id(self, img_number, params):
        """ Get 'img_number' and generate flash_area_id.h file' """

        #check if params.fa_file already exists and it has FLASH_AREA_ID
        if os.path.exists(params.fa_file):
            with open(params.fa_file, "r", encoding='UTF-8') as fa_f:
                content = fa_f.read()
                res = content.find(f"FLASH_AREA_IMG_{img_number}_SECONDARY")
                if res != -1:
                    fa_f.close()
                    return

                fa_f.close()

        try:
            with open(params.fa_file, "w", encoding='UTF-8') as fa_f:
                fa_f.write("#ifndef MEMORYMAP_H\n")
                fa_f.write("#define MEMORYMAP_H\n\n")
                fa_f.write('/* AUTO-GENERATED FILE, DO NOT EDIT.'
                            ' ALL CHANGES WILL BE LOST! */\n')
                fa_f.write(f'#include "flash_map_backend.h"\n\n')

                fa_f.write(f'extern struct flash_area {c_array}[];\n')
                fa_f.write(f'extern struct flash_area *boot_area_descs[];\n')

                #we always have BOOTLOADER and IMG_1_
                fa_f.write("#define FLASH_AREA_BOOTLOADER          ( 0u)\n\n")
                fa_f.write("#define FLASH_AREA_IMG_1_PRIMARY       ( 1u)\n")
                fa_f.write("#define FLASH_AREA_IMG_1_SECONDARY     ( 2u)\n\n")

                fa_f.write("#define FLASH_AREA_IMAGE_SCRATCH       ( 3u)\n")
                fa_f.write("#define FLASH_AREA_IMAGE_SWAP_STATUS   ( 7u)\n\n")

                for img in range(2, img_number + 1):
                    """ img_id_primary and img_id_secondary must be aligned with the
                        flash_area_id, calculated in the functions
                        __STATIC_INLINE uint8_t FLASH_AREA_IMAGE_PRIMARY(uint32_t img_idx) and
                        __STATIC_INLINE uint8_t FLASH_AREA_IMAGE_SECONDARY(uint32_t img_idx),
                        in boot/cypress/platforms/memory/sysflash/sysflash.h
                    """

                    slots_for_image = 2
                    img_id_primary = None
                    img_id_secondary = None

                    if img == 2:
                        img_id_primary = int(slots_for_image * img)
                        img_id_secondary = int(slots_for_image * img + 1)

                    #number 7 is used for FLASH_AREA_IMAGE_SWAP_STATUS, so the next is 8
                    if img >= 3:
                        img_id_primary = int(slots_for_image * img + 2)
                        img_id_secondary = int(slots_for_image * img + 3)

                    fa_f.write(f"#define FLASH_AREA_IMG_{img}_PRIMARY       ( {img_id_primary}u)\n")
                    fa_f.write(f"#define FLASH_AREA_IMG_{img}_SECONDARY     ( {img_id_secondary}u)\n\n")
                
                if self.plat.get('bitsPerCnt'):
                    fa_f.close()
                    
                    list_counters = process_policy_20829(params.policy)
                    if list_counters is not None:
                        form_max_counter_array(list_counters, params.fa_file)
                else:
                    fa_f.write("#endif /* MEMORYMAP_H */")
                fa_f.close()

        except (FileNotFoundError, OSError):
            print('\nERROR: Cannot create ', params.fa_file, file=sys.stderr)
            sys.exit(Error.IO)


def cvt_dec_or_hex(val, desc):
    """Convert (hexa)decimal string to number"""
    try:
        return int(val, 0)
    except ValueError:
        print('Invalid value', val, 'for', desc, file=sys.stderr)
        sys.exit(Error.VALUE)


def get_val(obj, attr):
    """Get JSON 'value'"""
    obj = obj[attr]
    try:
        return cvt_dec_or_hex(obj['value'], obj['description'])
    except KeyError as key:
        print('Malformed JSON:', key,
              'is missing in', "'" + attr + "'",
              file=sys.stderr)
        sys.exit(Error.JSON)


def get_bool(obj, attr, def_val=False):
    """Get JSON boolean value (returns def_val if it is missing)"""
    ret_val = def_val
    obj = obj.get(attr)
    if obj is not None:
        try:
            val = str(obj['value']).lower()
            desc = obj['description']
            if val == 'true':
                ret_val = True
            elif val == 'false':
                ret_val = False
            else:
                print('Invalid value', val, 'for', desc, file=sys.stderr)
                sys.exit(Error.VALUE)
        except KeyError as key:
            print('Malformed JSON:', key,
                  'is missing in', "'" + attr + "'",
                  file=sys.stderr)
            sys.exit(Error.JSON)
    return ret_val


def get_str(obj, attr, def_val=None):
    """Get JSON string value (returns def_val if it is missing)"""
    ret_val = def_val
    obj = obj.get(attr)
    if obj is not None:
        try:
            ret_val = str(obj['value'])
        except KeyError as key:
            print('Malformed JSON:', key,
                  'is missing in', "'" + attr + "'",
                  file=sys.stderr)
            sys.exit(Error.JSON)
    return ret_val


class AddrSize:
    """Bootloader area"""

    def __init__(self, bootloader, addr_name, size_name):
        self.addr = get_val(bootloader, addr_name)
        self.size = get_val(bootloader, size_name)


def calc_status_size(boot_swap_status_row_sz, max_img_sectors,
                     img_number, scratch_flag=True):
    """Estimate status size, see swap_status.h"""
    boot_swap_status_cnt_sz = 4
    boot_swap_status_crc_sz = 4
    boot_swap_status_mgcrec_sz = 4
    boot_swap_status_trailer_size = 64
    boot_swap_status_payld_sz = \
        boot_swap_status_row_sz - boot_swap_status_mgcrec_sz - \
        boot_swap_status_cnt_sz - boot_swap_status_crc_sz
    boot_swap_status_sect_rows_num = \
        int((max_img_sectors - 1) //
            boot_swap_status_payld_sz) + 1
    boot_swap_status_trail_rows_num = \
        int((boot_swap_status_trailer_size - 1) //
            boot_swap_status_payld_sz) + 1
    boot_swap_status_d_size = \
        boot_swap_status_row_sz * \
        (boot_swap_status_sect_rows_num + boot_swap_status_trail_rows_num)
    boot_swap_status_mult = 2
    boot_swap_status_size = boot_swap_status_mult * boot_swap_status_d_size
    status_zone_cnt = 2 * img_number
    if scratch_flag:
        status_zone_cnt += 1
    return boot_swap_status_size * status_zone_cnt


def process_json(in_file):
    """Process JSON"""
    try:
        with open(in_file, encoding='UTF-8') as in_f:
            try:
                flash_map = json.load(in_f)
            except ValueError:
                print('Cannot parse', in_file, file=sys.stderr)
                sys.exit(Error.IO)
    except (FileNotFoundError, OSError):
        print('Cannot open', in_file, file=sys.stderr)
        sys.exit(Error.IO)
    flash = flash_map.get('external_flash')
    if flash is not None:
        flash = flash[0]
        model = flash.get('model')
        mode = flash.get('mode')
        if model is not None:
            try:
                flash = flashDict[model]
            except KeyError:
                print('Supported SPI Flash ICs are:',
                      ', '.join(flashDict.keys()),
                      file=sys.stderr)
                sys.exit(Error.FLASH)
        else:
            try:
                flash = {'flashSize': cvt_dec_or_hex(flash['flash-size'],
                                                     'flash-size'),
                         'eraseSize': cvt_dec_or_hex(flash['erase-size'],
                                                     'erase-size')}
            except KeyError as key:
                print('Malformed JSON:', key,
                      "is missing in 'external_flash'",
                      file=sys.stderr)
                sys.exit(Error.FLASH)
        flash.update({'XIP': str(mode).upper() == 'XIP'})
    return flash_map.get('boot_and_upgrade', None), flash_map.get('ram_app_staging', None), flash

def process_boot_type(boot_and_upgrade):
    image_boot_mode = []

    for app_index in range(1, 5):
        app_ident = f'application_{app_index}'
        application = boot_and_upgrade.get(app_ident)

        if application:
            ram = application.get('ram', application.get('ram_boot'))

            if ram:
                image_boot_mode.append(
                    {
                        'mode': 'IMAGE_BOOT_MODE_FLASH' if application.get('ram') else 'IMAGE_BOOT_MODE_RAM',
                        'address': ram.get('address', {}).get('value', 0),
                        'size': ram.get('size', {}).get('value', 0),
                    }
                )

    return image_boot_mode

def generate_boot_type(image_boot_mode):
    c_file = "image_boot_config.c"
    h_file = "image_boot_config.h"
    try:
        with open(c_file, "w", encoding='UTF-8') as out_f:
            out_f.write('/* AUTO-GENERATED FILE, DO NOT EDIT.'
                        ' ALL CHANGES WILL BE LOST! */\n')
            
            out_f.write(f'#include "{h_file}"\n')
            out_f.write('\nimage_boot_config_t image_boot_config[BOOT_IMAGE_NUMBER] = {\n')
            for mode in image_boot_mode:
                out_f.writelines('\n'.join([
                    '\t{\n'
                    f"\t\t.mode     = {mode['mode']},",
                    f"\t\t.address  = {mode['address']},",
                    f"\t\t.size     = {mode['size']},",
                    '\t},\n']))
            out_f.write('};\n')

        with open(h_file, "w", encoding='UTF-8') as out_f:
            out_f.write('/* AUTO-GENERATED FILE, DO NOT EDIT.'
                        ' ALL CHANGES WILL BE LOST! */\n')
            out_f.write('#ifndef IMAGE_BOOT_CONFIG_H\n')
            out_f.write('#define IMAGE_BOOT_CONFIG_H\n')
            out_f.write('#include "bootutil/bootutil.h"\n')
            out_f.writelines('\n'.join([
                ' ',
                'typedef enum',
                '{',
                    '\tIMAGE_BOOT_MODE_FLASH = 0U,',
                    '\tIMAGE_BOOT_MODE_RAM = 1U,',
                '} image_boot_mode_t;',
                '',
                'typedef struct image_boot_config_s {',
                    '\timage_boot_mode_t mode;',
                    '\tuint32_t address;',
                    '\tuint32_t size;',
                '} image_boot_config_t;',
                '',
                'extern image_boot_config_t image_boot_config[BOOT_IMAGE_NUMBER];'
            ]))
            out_f.write('\n#endif /* IMAGE_BOOT_CONFIG_H */\n')

    except (FileNotFoundError, OSError):
        print('Cannot create', out_f, file=sys.stderr)
        sys.exit(Error.IO)
                         

def process_images(area_list, boot_and_upgrade):
    """Process images"""
    app_count = 0
    slot_sectors_max = 0
    all_shared = get_bool(boot_and_upgrade['bootloader'], 'shared_slot')
    any_shared = all_shared
    app_core = None
    apps_flash_map = [None, ]
    apps_ram_map = [None, ]

    for stage in range(2):
        for app_index in range(1, 5):

            app_flash_map = {}
            app_ram_map = {}
            app_ram_boot = False

            try:
                app_ident = f'application_{app_index}'
                application = boot_and_upgrade[app_ident]

                try:
                    flash = application['flash']
                except KeyError:
                    #Backward compatibility
                    flash = application

                try:
                    config = application['config']
                except KeyError:
                    #Backward compatibility
                    config = application

                try:
                    ram = application['ram']
                except KeyError:
                    try:
                        ram = application['ram_boot']
                        app_ram_boot = True
                    except KeyError:
                        ram = None

                try:
                    primary_addr = get_val(flash, 'address')
                    primary_size = get_val(flash, 'size')
                    secondary_addr = get_val(flash, 'upgrade_address')
                    secondary_size = get_val(flash, 'upgrade_size')

                    if ram is not None:
                        app_ram_addr = get_val(ram, 'address')
                        app_ram_size = get_val(ram, 'size')
                    else:
                        app_ram_addr = None
                        app_ram_size = None

                except KeyError as key:
                    print('Malformed JSON:', key, 'is missing',
                          file=sys.stderr)
                    sys.exit(Error.JSON)
                if stage == 0:
                    if primary_size != secondary_size:
                        print('Primary and secondary slot sizes'
                              ' are different for', app_ident,
                              file=sys.stderr)
                        sys.exit(Error.VALUE)
                    area_list.chk_area(primary_addr, primary_size)
                    area_list.chk_area(secondary_addr, secondary_size,
                                       primary_addr)
                    if config is None or config.get('core') is None:
                        if app_index == 1:
                            app_core = area_list.plat['appCore']
                    elif app_index > 1:
                        print('"core" makes sense only for the 1st app',
                              file=sys.stderr)
                        sys.exit(Error.VALUE)
                    else:
                        app_core = get_str(config, 'core',
                                           area_list.plat['appCore'])
                    if app_index == 1:
                        app_core = area_list.plat['allCores'].get(app_core.lower())
                        if app_core is None:
                            print('Unknown "core"', file=sys.stderr)
                            sys.exit(Error.VALUE)
                else:
                    slot_sectors_max = max(
                        slot_sectors_max,
                        area_list.add_area(
                            f'{app_ident} (primary slot)',
                            f'FLASH_AREA_IMG_{app_index}_PRIMARY',
                            primary_addr, primary_size,
                            area_list.get_img_trailer_size()))
                    shared_slot = get_bool(flash, 'shared_slot', all_shared)
                    any_shared = any_shared or shared_slot
                    slot_sectors_max = max(
                        slot_sectors_max,
                        area_list.add_area(
                            f'{app_ident} (secondary slot)',
                            f'FLASH_AREA_IMG_{app_index}_SECONDARY',
                            secondary_addr, secondary_size,
                            area_list.get_img_trailer_size(),
                            shared_slot))

                    app_slot_prim = {"address": hex(primary_addr), "size": hex(primary_size)}
                    app_slot_sec = {"address": hex(secondary_addr), "size": hex(secondary_size)}

                    app_flash_map.update({"primary": app_slot_prim, "secondary": app_slot_sec})
                    apps_flash_map.append(app_flash_map)

                    if ram is not None:
                        app_ram_map.update({"address": app_ram_addr
                                            , "size": app_ram_size
                                            , "ram_boot": app_ram_boot})
                        apps_ram_map.append(app_ram_map)
                    else:
                        apps_ram_map = None

                app_count = app_index

            except KeyError:
                break
        if app_count == 0:
            print('Malformed JSON: no application(s) found',
                  file=sys.stderr)
            sys.exit(Error.JSON)

    return app_core, app_count, slot_sectors_max, apps_flash_map, apps_ram_map, any_shared


def process_policy_20829(in_policy):
    """Process policy file to get data of NV-counter"""
    list_counters = None

    try:
        with open(in_policy, encoding='UTF-8') as in_f:
            try:
                policy = json.load(in_f)
            except ValueError:
                print('\nERROR: Cannot parse', in_policy,'\n', file=sys.stderr)
                sys.exit(Error.IO)
            finally:
                in_f.close()
    except (FileNotFoundError, OSError):
        print('Cannot open', in_policy, file=sys.stderr)
        sys.exit(Error.IO)

    try:
        nv_cnt = policy["device_policy"]['reprovisioning']['nv_counter']
        list_values = nv_cnt["value"]
        list_counters = nv_cnt["bits_per_cnt"]
    except KeyError:
        print("\nERROR: Check path to 'nv_counter' and its correctness in policy file", in_policy,
            ".\n", file=sys.stderr)
        sys.exit(Error.POLICY)

    #Check correctness of NV-counter
    try:
        len_list_value = len(list_values)
        len_list_counters = len(list_counters)
    except TypeError:
        print("\nERROR: Fields 'value' and 'bits_per_cnt' of 'nv_counter' in policy file",
            in_policy,"must be arrays.\n", file=sys.stderr)
        sys.exit(Error.POLICY)

    if len_list_value != len_list_counters:
        print("\nERROR: Fields 'value' and 'bits_per_cnt' of 'nv_counter' in policy file",
            in_policy,"must have the same size.\n", file=sys.stderr)
        sys.exit(Error.POLICY)

    sum_all_counters = 0
    for i in range(len_list_value):
        sum_all_counters += list_counters[i]
        if list_values[i] > list_counters[i]:
            print("\nERROR: Field 'value' cannot be more then 'bits_per_cnt'.", file=sys.stderr)
            print("Check 'nv_counter' in policy file", in_policy,"\n", file=sys.stderr)
            sys.exit(Error.POLICY)

    sum_all_bit_nv_counter = 32
    if sum_all_counters != sum_all_bit_nv_counter:
        print("\nERROR: The sum of all 'bits_per_cnt' must be equal to 32.", file=sys.stderr)
        print("Check 'nv_counter' in policy file", in_policy,"\n", file=sys.stderr)
        sys.exit(Error.POLICY)

    return list_counters


def form_max_counter_array(in_list, out_file):
    '''Write bit_per_count array to output file
    There is expected, that "out_file" already exists'''

    #ifdef here is needed to fix Rule 12.2 MISRA violation
    out_array_str = "\n#ifdef NEED_MAX_COUNTERS\nstatic const uint8_t bits_per_cnt[] = {"

    #in_list is checked in prior function 'process_policy()'
    for i, list_member in enumerate(in_list):
        out_array_str += str(list_member)
        if i < len(in_list) - 1:
            out_array_str += ", "
    out_array_str += "};\n#endif\n"

    try:
        with open(out_file, "a", encoding='UTF-8') as out_f:
            out_f.write(out_array_str)
            out_f.write("\n#endif /* MEMORYMAP_H */")
    except (FileNotFoundError, OSError):
        print('\nERROR: Cannot open ', out_file, file=sys.stderr)
        sys.exit(Error.CONFIG_MISMATCH)


def main():
    """Flash map converter"""
    params = CmdLineParams()

    try:
        plat = platDict[params.plat_id]
    except KeyError:
        print('Supported platforms are:', ', '.join(platDict.keys()),
              file=sys.stderr)
        sys.exit(Error.POLICY)

    try:
        boot_and_upgrade, ram_app_staging, flash = process_json(params.in_file)
        bootloader = boot_and_upgrade['bootloader']
        try:
            bootloader_config = bootloader['config']
        except KeyError:
            #Backward compatibility
            bootloader_config = bootloader

        if ram_app_staging is not None:
            try:
                ram_app_staging_size = get_val(ram_app_staging, 'size')
                ram_app_staging_ext_mem_addr = get_val(ram_app_staging, 'external_mememory_address')
                ram_app_staging_sram_stage_addr = get_val(ram_app_staging, 'sram_stage_address')
                ram_app_staging_reset_trigger = get_bool(ram_app_staging, 'reset_after_staging')
            except KeyError:
                ram_app_staging = None

        try:
            bootloader_flash = bootloader['flash']
        except KeyError:
            #Backward compatibility
            bootloader_flash = bootloader

        try:
            bootloader_ram = bootloader['ram']
        except KeyError:
            #Backward compatibility
            bootloader_ram = bootloader

        boot_flash_area = AddrSize(bootloader_flash, 'address', 'size')
        boot_ram_area = AddrSize(bootloader_ram, 'address', 'size')
    except KeyError as key:
        print('Malformed JSON:', key, 'is missing',
              file=sys.stderr)
        sys.exit(Error.JSON)

    try:
        scratch = AddrSize(bootloader_flash, 'scratch_address', 'scratch_size')
    except KeyError:
        scratch = None

    try:
        swap_status = AddrSize(bootloader_flash, 'status_address', 'status_size')
    except KeyError:
        swap_status = None

    try:
        bootloader_shared_data = bootloader['shared_data']
        boot_shared_data_area = AddrSize(bootloader_shared_data, 'address', 'size')
    except KeyError:
        boot_shared_data_area = None

    try:
        bootloader_startup = get_bool(bootloader_config, 'startup')
    except KeyError:
        bootloader_startup = None

    try:
        ram_app_area = AddrSize(bootloader_config['ram_boot'], 'address', 'size')
    except  KeyError:
        ram_app_area = None


    # Create flash areas
    area_list = AreaList(plat, flash, scratch is None and swap_status is None)
    area_list.add_area('bootloader', 'FLASH_AREA_BOOTLOADER',
                       boot_flash_area.addr, boot_flash_area.size)

    # Service RAM app (optional)
    service_app = boot_and_upgrade.get('service_app')
    app_binary = None
    input_params = None
    app_desc = None
    if service_app is not None:
        if plat['flashSize'] > 0:
            print('service_app is unsupported on this platform',
                  file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)
        try:
            app_binary = AddrSize(service_app, 'address', 'size')
            input_params = AddrSize(service_app, 'params_address', 'params_size')
            app_desc = AddrSize(service_app, 'desc_address', 'desc_size')
            if input_params.addr != app_binary.addr + app_binary.size or \
                    app_desc.addr != input_params.addr + input_params.size or \
                    app_desc.size != SERVICE_APP_SZ:
                print('Malformed service_app definition', file=sys.stderr)
                sys.exit(Error.CONFIG_MISMATCH)
            area_list.add_area('service_app', None, app_binary.addr,
                               app_binary.size + input_params.size + app_desc.size)
        except KeyError as key:
            print('Malformed JSON:', key, 'is missing',
                  file=sys.stderr)
            sys.exit(Error.JSON)

    # Fill flash areas
    app_core, app_count, slot_sectors_max, apps_flash_map, apps_ram_map, shared_slot = \
        process_images(area_list, boot_and_upgrade)

    if params.image_boot_config:
        image_boot_mode = process_boot_type(boot_and_upgrade)

        if image_boot_mode:
            generate_boot_type(image_boot_mode)

    cy_img_hdr_size = 0x400
    app_start = int(apps_flash_map[1].get("primary").get("address"), 0) + cy_img_hdr_size

    if app_start % plat['VTAlign'] != 0:
        print('Starting address', apps_flash_map[1].get("primary").get("address"),
              '+', hex(cy_img_hdr_size),
              'must be aligned to', hex(plat['VTAlign']),
              file=sys.stderr)
        sys.exit(Error.CONFIG_MISMATCH)

    slot_sectors_max = max(slot_sectors_max, 32)

    if swap_status is not None:
        status_size_min = calc_status_size(area_list.get_min_erase_size(),
                                           slot_sectors_max,
                                           app_count,
                                           scratch is not None)

        if swap_status.size < status_size_min:
            print('Insufficient swap status area - suggested size',
                  hex(status_size_min),
                  file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)
        area_list.add_area('swap status partition',
                           'FLASH_AREA_IMAGE_SWAP_STATUS',
                           swap_status.addr, swap_status.size)

    if scratch is not None:
        area_list.add_area('scratch area',
                           'FLASH_AREA_IMAGE_SCRATCH',
                           scratch.addr, scratch.size)

    # Compare size 'bit_per_cnt' and number of images.
    # 'service_app' is used only when HW rollback counter exists
    if plat.get('bitsPerCnt') is not None and service_app is not None:
        plat['bitsPerCnt'] = True
        list_counters = process_policy_20829(params.policy)
        if list_counters is not None and len(list_counters) != app_count:
            print("\nERROR: 'bits_per_cnt' must be present for each image!",
                file=sys.stderr)
            print("Please, check secure provisioning and reprovisioning policies.\n",
                file=sys.stderr)
            sys.exit(Error.CONFIG_MISMATCH)

    # Image id parameter is not used for MCUBootApp
    if params.img_id is None:
        area_list.generate_c_source(params)

    area_list.create_flash_area_id(app_count, params)

    # Report necessary values back to make
    print('# AUTO-GENERATED FILE, DO NOT EDIT. ALL CHANGES WILL BE LOST!')
    if params.set_core:
        print('CORE :=', plat['allCores'][plat['bootCore'].lower()])

    if ram_app_staging is not None:
        print('USE_STAGE_RAM_APPS := 1')
        print('RAM_APP_STAGING_EXT_MEM_ADDR := ', hex(ram_app_staging_ext_mem_addr))
        print('RAM_APP_STAGING_SRAM_MEM_ADDR :=', hex(ram_app_staging_sram_stage_addr))
        print('RAM_APP_STAGING_SIZE := ', hex(ram_app_staging_size))
        if ram_app_staging_reset_trigger is True:
            print('RAM_APP_RESET_TRIGGER := 1')

    if bootloader_startup is True:
        print('BOOTLOADER_STARTUP := 1')

    if ram_app_area is not None:
        print('USE_MCUBOOT_RAM_LOAD := 1')
        print('IMAGE_EXECUTABLE_RAM_START :=', hex(ram_app_area.addr))
        print('IMAGE_EXECUTABLE_RAM_SIZE :=', hex(ram_app_area.size))

    if boot_shared_data_area is not None:
        print('USE_MEASURED_BOOT := 1')
        print('USE_DATA_SHARING := 1')
        print('BOOT_SHARED_DATA_ADDRESS :=', hex(boot_shared_data_area.addr)+'U')
        print('BOOT_SHARED_DATA_SIZE :=', hex(boot_shared_data_area.size)+'U')
        print('BOOT_SHARED_DATA_RECORD_SIZE :=', hex(boot_shared_data_area.size)+'U')

    print('BOOTLOADER_ORIGIN :=', hex(boot_flash_area.addr))
    print('BOOTLOADER_SIZE :=', hex(boot_flash_area.size))
    print('BOOTLOADER_RAM_ORIGIN :=', hex(boot_ram_area.addr))
    print('BOOTLOADER_RAM_SIZE :=', hex(boot_ram_area.size))
    print('APP_CORE :=', app_core)

    if params.img_id is not None:
        primary_img_start = apps_flash_map[int(params.img_id)].get("primary").get("address")
        secondary_img_start = apps_flash_map[int(params.img_id)].get("secondary").get("address")
        slot_size = apps_flash_map[int(params.img_id)].get("primary").get("size")

        if apps_ram_map:
            image_ram_address = apps_ram_map[int(params.img_id)].get("address")
            image_ram_size = apps_ram_map[int(params.img_id)].get("size")
            image_ram_boot = apps_ram_map[int(params.img_id)].get("ram_boot")
            if image_ram_address and image_ram_size:
                print('IMG_RAM_ORIGIN := ' + hex(image_ram_address))
                print('IMG_RAM_SIZE := ' + hex(image_ram_size))
                if image_ram_boot is True:
                    print('USE_MCUBOOT_RAM_LOAD := 1')

        print('PRIMARY_IMG_START := ' + primary_img_start)
        print('SECONDARY_IMG_START := ' + secondary_img_start)
        print('SLOT_SIZE := ' + slot_size)
    else:
        if apps_ram_map:
            ram_load_counter = 0
            for img in apps_ram_map:
                if img is not None and img.get("ram_boot"):
                    ram_load_counter += 1

            if ram_load_counter != 0:
                if ram_load_counter == 1 and app_count == 1:
                    print('USE_MCUBOOT_RAM_LOAD := 1')
                    print(f'IMAGE_EXECUTABLE_RAM_START := {hex(apps_ram_map[1].get("address"))}')
                    print(f'IMAGE_EXECUTABLE_RAM_SIZE := {hex(apps_ram_map[1].get("size"))}')
                else:
                    print('USE_MCUBOOT_MULTI_MEMORY_LOAD := 1')

        print('MAX_IMG_SECTORS :=', slot_sectors_max)

    print('MCUBOOT_IMAGE_NUMBER :=', app_count)
    if area_list.external_flash:
        print('USE_EXTERNAL_FLASH := 1')
    if area_list.external_flash_xip:
        print('USE_XIP := 1')

    if area_list.use_overwrite:
        print('USE_OVERWRITE := 1')
    if shared_slot:
        print('USE_SHARED_SLOT := 1')
    if service_app is not None:
        print('PLATFORM_SERVICE_APP_OFFSET :=',
              hex(app_binary.addr - plat['smifAddr']))
        print('PLATFORM_SERVICE_APP_INPUT_PARAMS_OFFSET :=',
              hex(input_params.addr - plat['smifAddr']))
        print('PLATFORM_SERVICE_APP_DESC_OFFSET :=',
              hex(app_desc.addr - plat['smifAddr']))
        print('USE_HW_ROLLBACK_PROT := 1')


if __name__ == '__main__':
    main()
