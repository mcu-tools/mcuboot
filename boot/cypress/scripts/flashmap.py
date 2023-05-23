"""MCUBoot Flash Map Converter (JSON to .h)
Copyright (c) 2022 Infineon Technologies AG
"""

import sys
import getopt
import json

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
        self.img_id = None
        self.policy = None
        self.set_core = False

        usage = 'USAGE:\n' + sys.argv[0] + \
                ''' -p <platform> -i <flash_map.json> -o <flash_map.h> -d <img_id>

OPTIONS:
-h  --help       Display the usage information
-p  --platform=  Target (e.g., PSOC_062_512K)
-i  --ifile=     JSON flash map file
-o  --ofile=     C header file to be generated
-d  --img_id     ID of application to build
-c  --policy     Policy file in JSON format
-m  --core       Detect and set Cortex-M CORE
'''

        try:
            opts, unused = getopt.getopt(
                sys.argv[1:], 'hi:o:p:d:c:m',
                ['help', 'platform=', 'ifile=', 'ofile=', 'img_id=', 'policy=', 'core'])
            if len(unused) > 0:
                print(usage, file=sys.stderr)
                sys.exit(1)
        except getopt.GetoptError:
            print(usage, file=sys.stderr)
            sys.exit(1)

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
            elif opt in ('-d', '--img_id'):
                self.img_id = arg
            elif opt in ('-c', '--policy'):
                self.policy = arg
            elif opt in ('-m', '--core'):
                self.set_core = True

        if len(self.in_file) == 0 or len(self.out_file) == 0:
            print(usage, file=sys.stderr)
            sys.exit(1)


class AreaList:
    """List of flash areas"""

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
        """Calculate minimum erase block size for int./ext. Flash """
        return self.plat['eraseSize'] if self.plat['flashSize'] > 0 \
            else self.flash['eraseSize']

    def get_img_trailer_size(self):
        """Calculate image trailer size"""
        return self.get_min_erase_size()

    def process_int_area(self, title, fa_addr, fa_size,
                         img_trailer_size, shared_slot):
        """Process internal flash area"""
        fa_device_id = 'FLASH_DEVICE_INTERNAL_FLASH'
        fa_off = fa_addr - self.plat['flashAddr']
        if img_trailer_size is not None:
            if self.use_overwrite:
                if shared_slot:
                    print('Shared slot', title,
                          'is not supported in OVERWRITE mode',
                          file=sys.stderr)
                    sys.exit(7)
            else:
                # Check trailer alignment (start at the sector boundary)
                align = (fa_off + fa_size - img_trailer_size) % \
                        self.plat['eraseSize']
                if align != 0:
                    fa_addr += self.plat['eraseSize'] - align
                    if fa_addr + fa_size <= \
                            self.plat['flashAddr'] + self.plat['flashSize']:
                        print('Misaligned', title,
                              '- suggested address', hex(fa_addr),
                              file=sys.stderr)
                    else:
                        print('Misaligned', title, file=sys.stderr)
                    sys.exit(7)
        else:
            # Check alignment (flash area should start at the sector boundary)
            if fa_off % self.plat['eraseSize'] != 0:
                print('Misaligned', title, file=sys.stderr)
                sys.exit(7)
        slot_sectors = int((fa_off % self.plat['eraseSize'] +
                            fa_size + self.plat['eraseSize'] - 1) //
                           self.plat['eraseSize'])
        return fa_device_id, fa_off, slot_sectors

    def process_ext_area(self, title, fa_addr, fa_size,
                         img_trailer_size, shared_slot):
        """Process external flash area"""
        if self.flash is None:
            print('Unspecified SPI Flash IC',
                  file=sys.stderr)
            sys.exit(3)
        if fa_addr + fa_size <= \
                self.plat['smifAddr'] + self.flash['flashSize']:
            flash_idx = 'CY_BOOT_EXTERNAL_DEVICE_INDEX'
            fa_device_id = f'FLASH_DEVICE_EXTERNAL_FLASH({flash_idx})'
            fa_off = fa_addr - self.plat['smifAddr']
        else:
            print('Misfitting', title, file=sys.stderr)
            sys.exit(7)
        if img_trailer_size is not None:
            if self.use_overwrite:
                if shared_slot:
                    print('Shared slot', title,
                          'is not supported in OVERWRITE mode',
                          file=sys.stderr)
                    sys.exit(7)
            else:
                # Check trailer alignment (start at the sector boundary)
                align = (fa_off + fa_size - img_trailer_size) % \
                        self.flash['eraseSize']
                if align != 0:
                    peer_addr = self.peers.get(fa_addr)
                    if shared_slot:
                        # Special case when using both int. and ext. memory
                        if self.plat['flashSize'] > 0 and \
                                align % self.plat['eraseSize'] == 0:
                            print('Note:', title, 'requires', align,
                                  'padding bytes before trailer',
                                  file=sys.stderr)
                        else:
                            print('Misaligned', title, file=sys.stderr)
                            sys.exit(7)
                    elif is_same_mem(fa_addr, peer_addr) and \
                            fa_addr % self.flash['eraseSize'] == \
                            peer_addr % self.flash['eraseSize']:
                        pass  # postpone checking
                    else:
                        fa_addr += self.flash['eraseSize'] - align
                        if fa_addr + fa_size <= \
                                self.plat['smifAddr'] + self.flash['flashSize']:
                            print('Misaligned', title,
                                  '- suggested address', hex(fa_addr),
                                  file=sys.stderr)
                        else:
                            print('Misaligned', title, file=sys.stderr)
                        sys.exit(7)
        else:
            # Check alignment (flash area should start at the sector boundary)
            if fa_off % self.flash['eraseSize'] != 0:
                print('Misaligned', title, file=sys.stderr)
                sys.exit(7)
        slot_sectors = int((fa_off % self.flash['eraseSize'] +
                            fa_size + self.flash['eraseSize'] - 1) //
                           self.flash['eraseSize'])
        self.external_flash = True
        if self.flash['XIP']:
            self.external_flash_xip = True
        return fa_device_id, fa_off, slot_sectors

    def chk_area(self, fa_addr, fa_size, peer_addr=None):
        """Check area location (internal/external flash)"""
        if peer_addr is not None:
            self.peers[peer_addr] = fa_addr
        fa_limit = fa_addr + fa_size
        if self.plat['flashSize'] and \
                fa_addr >= self.plat['flashAddr'] and \
                fa_limit <= self.plat['flashAddr'] + self.plat['flashSize']:
            # Internal flash
            self.internal_flash = True

    def add_area(self, title,
                 fa_id, fa_addr, fa_size,
                 img_trailer_size=None, shared_slot=False):
        """Add flash area to AreaList.
        Internal/external flash is detected by address.
        Returns number of sectors in a slot"""
        if fa_size == 0:
            print('Empty', title, file=sys.stderr)
            sys.exit(7)

        fa_limit = fa_addr + fa_size
        if self.plat['flashSize'] and \
                fa_addr >= self.plat['flashAddr'] and \
                fa_limit <= self.plat['flashAddr'] + self.plat['flashSize']:
            # Internal flash
            fa_device_id, fa_off, slot_sectors = self.process_int_area(
                title, fa_addr, fa_size, img_trailer_size, shared_slot)
            align = self.plat['eraseSize']
        elif self.plat['smifSize'] and \
                fa_addr >= self.plat['smifAddr'] and \
                fa_limit <= self.plat['smifAddr'] + self.plat['smifSize']:
            # External flash
            fa_device_id, fa_off, slot_sectors = self.process_ext_area(
                title, fa_addr, fa_size, img_trailer_size, shared_slot)
            align = self.flash['eraseSize']
        else:
            print('Invalid', title, file=sys.stderr)
            sys.exit(7)

        if shared_slot:
            assert img_trailer_size is not None
            tr_addr = fa_addr + fa_size - img_trailer_size
            tr_name = self.trailers.get(tr_addr)
            if tr_name is not None:
                print('Same trailer address for', title, 'and', tr_name,
                      file=sys.stderr)
                sys.exit(7)
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
                        sys.exit(7)
                elif over:
                    print(title, 'overlaps with', area['title'],
                          file=sys.stderr)
                    sys.exit(7)

        self.areas.append({'title': title,
                           'shared_slot': shared_slot,
                           'fa_id': fa_id,
                           'fa_device_id': fa_device_id,
                           'fa_off': fa_off,
                           'fa_size': fa_size})
        return slot_sectors

    def generate_c_source(self, params):
        """Generate C source"""
        c_array = 'flash_areas'

        try:
            with open(params.out_file, "w", encoding='UTF-8') as out_f:
                out_f.write('/* AUTO-GENERATED FILE, DO NOT EDIT.'
                            ' ALL CHANGES WILL BE LOST! */\n')
                out_f.write(f'/* Platform: {params.plat_id} */\n')
                out_f.write("#ifndef CY_FLASH_MAP_H\n")
                out_f.write("#define CY_FLASH_MAP_H\n\n")

                if self.plat.get('bitsPerCnt'):
                    out_f.write('#ifdef NEED_FLASH_MAP\n')
                out_f.write(f'static struct flash_area {c_array}[] = {{\n')
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

                if self.plat.get('bitsPerCnt'):
                    out_f.write('#endif /* NEED_FLASH_MAP */\n')
                    out_f.close()

                    # inserted here to fix misra 'header guard'
                    list_counters = process_policy_20829(params.policy)
                    if list_counters is not None:
                        form_max_counter_array(list_counters, params.out_file)
                    with open(params.out_file, "a", encoding='UTF-8') as out_f:
                        out_f.write("#endif /* CY_FLASH_MAP_H */\n")
                else:
                    out_f.write("#endif /* CY_FLASH_MAP_H */\n")

        except (FileNotFoundError, OSError):
            print('Cannot create', params.out_file, file=sys.stderr)
            sys.exit(4)


def cvt_dec_or_hex(val, desc):
    """Convert (hexa)decimal string to number"""
    try:
        return int(val, 0)
    except ValueError:
        print('Invalid value', val, 'for', desc, file=sys.stderr)
        sys.exit(6)


def get_val(obj, attr):
    """Get JSON 'value'"""
    obj = obj[attr]
    try:
        return cvt_dec_or_hex(obj['value'], obj['description'])
    except KeyError as key:
        print('Malformed JSON:', key,
              'is missing in', "'" + attr + "'",
              file=sys.stderr)
        sys.exit(5)


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
                sys.exit(6)
        except KeyError as key:
            print('Malformed JSON:', key,
                  'is missing in', "'" + attr + "'",
                  file=sys.stderr)
            sys.exit(5)
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
            sys.exit(5)
    return ret_val


class AddrSize:
    """Bootloader area"""

    def __init__(self, bootloader, addr_name, size_name):
        self.fa_addr = get_val(bootloader, addr_name)
        self.fa_size = get_val(bootloader, size_name)


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
                sys.exit(4)
    except (FileNotFoundError, OSError):
        print('Cannot open', in_file, file=sys.stderr)
        sys.exit(4)
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
                sys.exit(3)
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
                sys.exit(3)
        flash.update({'XIP': str(mode).upper() == 'XIP'})
    return flash_map['boot_and_upgrade'], flash


def process_images(area_list, boot_and_upgrade):
    """Process images"""
    app_count = 0
    slot_sectors_max = 0
    all_shared = get_bool(boot_and_upgrade['bootloader'], 'shared_slot')
    any_shared = all_shared
    app_core = None
    apps_flash_map = [None, ]

    for stage in range(2):
        for app_index in range(1, 5):

            app_flash_map = {}

            try:
                app_ident = f'application_{app_index}'
                application = boot_and_upgrade[app_ident]
                try:
                    primary_addr = get_val(application, 'address')
                    primary_size = get_val(application, 'size')
                    secondary_addr = get_val(application, 'upgrade_address')
                    secondary_size = get_val(application, 'upgrade_size')
                except KeyError as key:
                    print('Malformed JSON:', key, 'is missing',
                          file=sys.stderr)
                    sys.exit(5)
                if stage == 0:
                    if primary_size != secondary_size:
                        print('Primary and secondary slot sizes'
                              ' are different for', app_ident,
                              file=sys.stderr)
                        sys.exit(6)
                    area_list.chk_area(primary_addr, primary_size)
                    area_list.chk_area(secondary_addr, secondary_size,
                                       primary_addr)
                    if application.get('core') is None:
                        if app_index == 1:
                            app_core = area_list.plat['appCore']
                    elif app_index > 1:
                        print('"core" makes sense only for the 1st app',
                              file=sys.stderr)
                        sys.exit(6)
                    else:
                        app_core = get_str(application, 'core',
                                           area_list.plat['appCore'])
                    if app_index == 1:
                        app_core = area_list.plat['allCores'].get(app_core.lower())
                        if app_core is None:
                            print('Unknown "core"', file=sys.stderr)
                            sys.exit(6)
                else:
                    slot_sectors_max = max(
                        slot_sectors_max,
                        area_list.add_area(
                            f'{app_ident} (primary slot)',
                            f'FLASH_AREA_IMG_{app_index}_PRIMARY',
                            primary_addr, primary_size,
                            area_list.get_img_trailer_size()))
                    shared_slot = get_bool(application, 'shared_slot', all_shared)
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

                app_count = app_index

            except KeyError:
                break
        if app_count == 0:
            print('Malformed JSON: no application(s) found',
                  file=sys.stderr)
            sys.exit(5)

    return app_core, app_count, slot_sectors_max, apps_flash_map, any_shared

def process_policy_20829(in_policy):
    """Process policy file to get data of NV-counter"""
    list_counters = None

    try:
        with open(in_policy, encoding='UTF-8') as in_f:
            try:
                policy = json.load(in_f)
            except ValueError:
                print('\nERROR: Cannot parse', in_policy,'\n', file=sys.stderr)
                sys.exit(4)
            finally:
                in_f.close()
    except (FileNotFoundError, OSError):
        print('Cannot open', in_policy, file=sys.stderr)
        sys.exit(4)

    try:
        nv_cnt = policy["device_policy"]['reprovisioning']['nv_counter']
        list_values = nv_cnt["value"]
        list_counters = nv_cnt["bits_per_cnt"]
    except KeyError:
        print("\nERROR: Check path to 'nv_counter' and its correctness in policy file", in_policy,
            ".\n", file=sys.stderr)
        sys.exit(2)

    #Check correctness of NV-counter
    try:
        len_list_value = len(list_values)
        len_list_counters = len(list_counters)
    except TypeError:
        print("\nERROR: Fields 'value' and 'bits_per_cnt' of 'nv_counter' in policy file",
            in_policy,"must be arrays.\n", file=sys.stderr)
        sys.exit(2)

    if len_list_value != len_list_counters:
        print("\nERROR: Fields 'value' and 'bits_per_cnt' of 'nv_counter' in policy file",
            in_policy,"must have the same size.\n", file=sys.stderr)
        sys.exit(2)

    sum_all_counters = 0
    for i in range(len_list_value):
        sum_all_counters += list_counters[i]
        if list_values[i] > list_counters[i]:
            print("\nERROR: Field 'value' cannot be more then 'bits_per_cnt'.", file=sys.stderr)
            print("Check 'nv_counter' in policy file", in_policy,"\n", file=sys.stderr)
            sys.exit(2)

    sum_all_bit_nv_counter = 32
    if sum_all_counters != sum_all_bit_nv_counter:
        print("\nERROR: The sum of all 'bits_per_cnt' must be equal to 32.", file=sys.stderr)
        print("Check 'nv_counter' in policy file", in_policy,"\n", file=sys.stderr)
        sys.exit(2)

    return list_counters


def form_max_counter_array(in_list, out_file):
    '''Write bit_per_count array to output file
    There is expected, that "out_file" already exists'''

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
    except (FileNotFoundError, OSError):
        print('\nERROR: Cannot open ', out_file, file=sys.stderr)
        sys.exit(7)


def main():
    """Flash map converter"""
    params = CmdLineParams()

    try:
        plat = platDict[params.plat_id]
    except KeyError:
        print('Supported platforms are:', ', '.join(platDict.keys()),
              file=sys.stderr)
        sys.exit(2)

    try:
        boot_and_upgrade, flash = process_json(params.in_file)
        bootloader = boot_and_upgrade['bootloader']
        boot = AddrSize(bootloader, 'address', 'size')
    except KeyError as key:
        print('Malformed JSON:', key, 'is missing',
              file=sys.stderr)
        sys.exit(5)

    try:
        scratch = AddrSize(bootloader, 'scratch_address', 'scratch_size')
    except KeyError:
        scratch = None

    try:
        swap_status = AddrSize(bootloader, 'status_address', 'status_size')
    except KeyError:
        swap_status = None

    # Create flash areas
    area_list = AreaList(plat, flash, scratch is None and swap_status is None)
    area_list.add_area('bootloader', 'FLASH_AREA_BOOTLOADER',
                       boot.fa_addr, boot.fa_size)

    # Service RAM app (optional)
    service_app = boot_and_upgrade.get('service_app')
    app_binary = None
    input_params = None
    app_desc = None
    if service_app is not None:
        if plat['flashSize'] > 0:
            print('service_app is unsupported on this platform',
                  file=sys.stderr)
            sys.exit(7)
        try:
            app_binary = AddrSize(service_app, 'address', 'size')
            input_params = AddrSize(service_app, 'params_address', 'params_size')
            app_desc = AddrSize(service_app, 'desc_address', 'desc_size')
            if input_params.fa_addr != app_binary.fa_addr + app_binary.fa_size or \
                    app_desc.fa_addr != input_params.fa_addr + input_params.fa_size or \
                    app_desc.fa_size != 0x20:
                print('Malformed service_app definition', file=sys.stderr)
                sys.exit(7)
            area_list.add_area('service_app', None, app_binary.fa_addr,
                               app_binary.fa_size + input_params.fa_size + app_desc.fa_size)
        except KeyError as key:
            print('Malformed JSON:', key, 'is missing',
                  file=sys.stderr)
            sys.exit(5)

    # Fill flash areas
    app_core, app_count, slot_sectors_max, apps_flash_map, shared_slot = \
        process_images(area_list, boot_and_upgrade)

    cy_img_hdr_size = 0x400
    app_start = int(apps_flash_map[1].get("primary").get("address"), 0) + cy_img_hdr_size

    if app_start % plat['VTAlign'] != 0:
        print('Starting address', apps_flash_map[1].get("primary").get("address"),
              '+', hex(cy_img_hdr_size),
              'must be aligned to', hex(plat['VTAlign']),
              file=sys.stderr)
        sys.exit(7)

    slot_sectors_max = max(slot_sectors_max, 32)

    if swap_status is not None:
        status_size_min = calc_status_size(area_list.get_min_erase_size(),
                                           slot_sectors_max,
                                           app_count,
                                           scratch is not None)

        if swap_status.fa_size < status_size_min:
            print('Insufficient swap status area - suggested size',
                  hex(status_size_min),
                  file=sys.stderr)
            sys.exit(7)
        area_list.add_area('swap status partition',
                           'FLASH_AREA_IMAGE_SWAP_STATUS',
                           swap_status.fa_addr, swap_status.fa_size)

    if scratch is not None:
        area_list.add_area('scratch area',
                           'FLASH_AREA_IMAGE_SCRATCH',
                           scratch.fa_addr, scratch.fa_size)

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
            sys.exit(7)


    # Image id parameter is not used for MCUBootApp
    if params.img_id is None:
        area_list.generate_c_source(params)

    # Report necessary values back to make
    print('# AUTO-GENERATED FILE, DO NOT EDIT. ALL CHANGES WILL BE LOST!')
    print('BOOTLOADER_SIZE :=', hex(boot.fa_size))
    if params.set_core:
        print('CORE :=', plat['allCores'][plat['bootCore'].lower()])
    print('APP_CORE :=', app_core)

    if params.img_id is not None:
        primary_img_start = apps_flash_map[int(params.img_id)].get("primary").get("address")
        secondary_img_start = apps_flash_map[int(params.img_id)].get("secondary").get("address")
        slot_size = apps_flash_map[int(params.img_id)].get("primary").get("size")

        print('PRIMARY_IMG_START := ' + primary_img_start)
        print('SECONDARY_IMG_START := ' + secondary_img_start)
        print('SLOT_SIZE := ' + slot_size)
    else:
        print('MCUBOOT_IMAGE_NUMBER :=', app_count)
        print('MAX_IMG_SECTORS :=', slot_sectors_max)

    if area_list.use_overwrite:
        print('USE_OVERWRITE := 1')
    if area_list.external_flash:
        print('USE_EXTERNAL_FLASH := 1')
        if area_list.external_flash_xip:
            print('USE_XIP := 1')
    if shared_slot:
        print('USE_SHARED_SLOT := 1')
    if service_app is not None:
        print('PLATFORM_SERVICE_APP_OFFSET :=',
              hex(app_binary.fa_addr - plat['smifAddr']))
        print('PLATFORM_SERVICE_APP_INPUT_PARAMS_OFFSET :=',
              hex(input_params.fa_addr - plat['smifAddr']))
        print('PLATFORM_SERVICE_APP_DESC_OFFSET :=',
              hex(app_desc.fa_addr - plat['smifAddr']))
        print('USE_HW_ROLLBACK_PROT := 1')


if __name__ == '__main__':
    main()
