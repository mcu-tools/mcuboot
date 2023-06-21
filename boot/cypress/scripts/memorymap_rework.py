"""
Copyright 2023 Cypress Semiconductor Corporation (an Infineon company)
or an affiliate of Cypress Semiconductor Corporation. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import sys
import json
import click

APP_LIMIT = 8

settings_dict = {
        'overwrite'                 :   'USE_OVERWRITE'
    ,   'swap'                      :   'USE_SWAP'
    ,   'status'                    :   'USE_STATUS'
    ,   'scratch'                   :   'USE_SCRATCH'
    ,   'measured_boot'             :   'USE_MEASURED_BOOT'
    ,   'data_sharing'              :   'USE_DATA_SHARING'
    ,   'ram_load'                  :   'USE_MCUBOOT_RAM_LOAD'
    ,   'multi_memory_load'         :   'USE_MCUBOOT_MULTI_MEMORY_LOAD'
    ,   'shared_slot'               :   'USE_SHARED_SLOT'
    ,   'ram_load_address'          :   'IMAGE_EXECUTABLE_RAM_START'
    ,   'ram_load_size'             :   'IMAGE_EXECUTABLE_RAM_SIZE'
    ,   'shared_data_address'       :   'BOOT_SHARED_DATA_ADDRESS'
    ,   'shared_data_size'          :   'BOOT_SHARED_DATA_SIZE'
    ,   'shared_data_record_size'   :   'BOOT_SHARED_DATA_RECORD_SIZE'
    ,   'bootloader_app_address'    :   'BOOTLOADER_ORIGIN'
    ,   'bootloader_app_size'       :   'BOOTLOADER_SIZE'
    ,   'bootloader_ram_address'    :   'BOOTLOADER_RAM_ORIGIN'
    ,   'bootloader_ram_size'       :   'BOOTLOADER_RAM_SIZE'
    ,   'application_count'         :   'MCUBOOT_IMAGE_NUMBER'
    ,   'boot_image'                :   'BOOT_IMAGE_NUMBER'
    ,   'sectors_count'             :   'MAX_IMG_SECTORS'
    ,   'core'                      :   'CORE'
    ,   'image_ram_address'         :   'IMG_RAM_ORIGIN'
    ,   'image_ram_size'            :   'IMG_RAM_SIZE'
    ,   'primary_image_start'       :   'PRIMARY_IMG_START'
    ,   'secondary_image_start'     :   'SECONDARY_IMG_START'
    ,   'image_size'                :   'SLOT_SIZE'
}

def header_guard_generate(file):
    file.write('/* AUTO-GENERATED FILE, DO NOT EDIT.'
                    ' ALL CHANGES WILL BE LOST! */\n')
    file.write("#pragma once\n\n")

def is_overlap(x : int, y : int) -> bool:
    if x.start == x.stop or y.start == y.stop:
        return False
    return x.start < y.stop and y.start < x.stop

def is_aligned(addr : int, sz : int) -> bool:
    ''' Check address alignment '''
    return addr % sz == 0

class Memory:
    ''' Memory handler '''
    def __init__(self, addr, sz):
        self.addr   : int = addr
        self.sz     : int = sz

    def overlaps_with(self, other) -> bool:
        ''' Check Memory for intersection
            @return Bool
        '''
        first = range(self.addr, self.addr + self.sz)
        second = range(other.addr, other.addr + other.sz)

        return is_overlap(first, second)

    def fits_with(self, other) -> bool:
        '''

        '''
        return \
            self.addr >= other.addr and \
            self.addr + self.sz <= other.addr + other.sz

class MemoryRegion(Memory):
    ''' Memory region handler '''
    def __init__(self, addr, sz, erase_sz, erase_val, type):
        super().__init__(addr, sz)
        self.erase_sz   : int   = erase_sz
        self.erase_val  : int   = erase_val
        self.type               = type

class BootloaderLayout:
    '''
        Handler for bootloader memory layout
    '''
    def __init__(self):
        self.bootloader_area    : Memory    = None
        self.ram                : Memory    = None
        self.scratch_area       : Memory    = None
        self.status_area        : Memory    = None
        self.shared_data        : Memory    = None
        self.shared_upgrade     : Memory    = None
        self.core_name          : int       = None

    @property
    def has_shared_upgrade(self) -> bool:
        return self.shared_upgrade is not None

    @property
    def has_shared_data(self) -> bool:
        return self.shared_data is not None

    @property
    def has_scratch_area(self) -> bool:
        return self.scratch_area is not None

    @property
    def has_status_area(self) -> bool:
        return self.status_area is not None

    def parse(self, section : json):
        '''
            Parse JSON section and init fields.
        '''
        try:
            fields = ('bootloader_area', 'scratch_area', 'status_area', \
                      'shared_data', 'shared_upgrade', 'ram')
            for field in fields:
                area = section.get(field)
                if area:
                    setattr(self, field, Memory(int(area['address'], 0),
                                                int(area['size'], 0)))

            core = section.get('core')
            if core:
                self.core_name = core

        except KeyError as key:
            print('Malformed JSON:', key, 'is missing')

class MemoryAreaConfig:
    '''
        Handler for flash area configuration
    '''
    def __init__(self, area_name, device_name, offset, size):
        self.fa_id          : str   = area_name
        self.fa_device_id   : str   = device_name
        self.fa_off         : int   = offset
        self.fa_size        : int   = size

class ApplicationLayout:
    '''
        Handler for application memory layout
    '''
    def __init__(self):
        self.boot_area      : Memory        = None
        self.upgrade_area   : Memory        = None
        self.ram            : Memory        = None
        self.ram_boot       : Memory        = None
        self.core_name      : str           = None

    @property
    def has_ram_boot(self) -> bool:
        return self.ram_boot is not None

    @property
    def has_upgrade_area(self) -> bool:
        return self.upgrade_area is not None

    def overlaps_with(self, other) -> bool:
        return \
            self.boot_area.overlaps_with(other.boot_area) or \
            self.upgrade_area.overlaps_with(other.upgrade_area)

    def parse(self, section : json):
        '''
            Parse JSON section and init fields.
        '''
        try:
            slots = section['slots']
            boot_address = int(slots['boot'], 0)
            upgrade_address =  int(slots['upgrade'], 0)
            slot_size = int(slots['size'], 0)

            self.boot_area = Memory(boot_address, slot_size)
            self.upgrade_area = Memory(upgrade_address, slot_size)

            fields = ('ram', 'ram_boot')
            for field in fields:
                area = section.get(field)
                if area:
                    setattr(self, field, Memory(int(area['address'], 0),
                                                int(area['size'], 0)))

            core = section.get('core')
            if core:
                self.core_name = core

        except KeyError as key:
            print('Malformed JSON:', key, 'is missing')

class MemoryMap:
    '''
        General handler
    '''
    def __init__(self):
        self.boot_layout    : BootloaderLayout      = None
        self.regions        : MemoryRegion          = []
        self.region_types   : str                   = []
        self.apps           : ApplicationLayout     = []
        self.primary_slots  : str                   = []
        self.secondary_slots: str                   = []
        self.mem_areas      : MemoryAreaConfig      = []
        self.param_dict                             = {}
        self.map_json       : json                  = None
        self.platform_json  : json                  = None
        self.output_folder                          = None
        self.output_name                            = None
        self.max_sectors                            = 32

    def __apps_init(self):
        for image_number in range(1, APP_LIMIT):
            app_ident   = f'application_{image_number}'
            section     = self.map_json.get(app_ident)

            if section:
                app_layout = ApplicationLayout()
                app_layout.parse(section)

                self.apps.append(app_layout)
            else:
                break

    def __boot_layout_init(self):
        self.boot_layout = BootloaderLayout()
        self.boot_layout.parse(self.map_json['bootloader'])

    def __memory_regions_init(self):
        memory_regions = self.platform_json['memory_regions']
        for region in memory_regions:
            try:
                addr        = int(region['address'], 0)
                size        = int(region['size'], 0)
                erase_size  = int(region['erase_size'], 0)
                erase_value = int(region['erase_value'], 0)
                type        = str(region['type'])

                if type not in self.region_types:
                    self.region_types.append(type)

                self.regions.append(MemoryRegion(addr, size, erase_size, erase_value, type))
            except KeyError as key:
                print('Malformed JSON:', key, 'is missing')

        # Check regions for overlap
        for this in self.regions:
            for other in self.regions:
                if this is other:
                    continue
                if this.overlaps_with(other):
                    # TODO: Notify regions overlap
                    raise Exception()

    def __memory_area_find_region_id(self, area : Memory) -> int:
        for region_id, region in enumerate(self.regions):
            if area.fits_with(region):
                return region_id
        return None

    def __memory_area_config_create(self, key):
        param_dict = self.param_dict
        area = param_dict[key][0]
        area_name = param_dict[key][1]

        region_id = self.__memory_area_find_region_id(area)
        region = self.regions[region_id]
        region_name = region.type

        offset = area.addr - region.addr
        size = area.sz

        area_config = MemoryAreaConfig(area_name,\
                                       region_name, \
                                       offset, \
                                       size)

        self.mem_areas.append(area_config)

        # Update max sectors
        slot_sectors = int((offset % region.erase_sz +
                            size + region.erase_sz - 1) //
                            region.erase_sz)

        self.max_sectors = max(self.max_sectors, slot_sectors)

    def __memory_areas_create(self):
        # Generate FA indexes
        param_dict = self.param_dict
        bootloader_area = self.boot_layout.bootloader_area
        main_app = self.apps[0]
        boot_area = main_app.boot_area
        upgrade_area = main_app.upgrade_area
        scratch_area = self.boot_layout.scratch_area

        param_dict.update({'bootloader': [bootloader_area, 'FLASH_AREA_BOOTLOADER', 0]})
        param_dict.update({'app_1_boot': [boot_area, 'FLASH_AREA_IMG_1_PRIMARY', 1]})
        param_dict.update({'app_1_upgrade': [upgrade_area, 'FLASH_AREA_IMG_1_SECONDARY', 2]})

        self.primary_slots.append('FLASH_AREA_IMG_1_PRIMARY')
        self.secondary_slots.append('FLASH_AREA_IMG_1_SECONDARY')

        # Generate scratch area index
        if self.boot_layout.has_scratch_area:
            param_dict.update({'scratch_area': [scratch_area, 'FLASH_AREA_IMAGE_SCRATCH', 3]})

        # Generate multiple app area indexes
        multi_app_area_idx = 4
        if len(self.apps) > 1:
            for app_id, app in enumerate(self.apps[1:], 1):
                idx = multi_app_area_idx + (app_id-1) * 2
                app_num = app_id + 1
                key = f'app_{app_num}_boot'
                fmt = f'FLASH_AREA_IMG_{app_num}_PRIMARY'
                param_dict.update({key: [app.boot_area, fmt, idx]})

                self.primary_slots.append(fmt)

                key = f'app_{app_num}_upgrade'
                fmt = f'FLASH_AREA_IMG_{app_num}_SECONDARY'
                param_dict.update({key: [app.upgrade_area, fmt, idx+1]})

                self.secondary_slots.append(fmt)

        # Generate status area indexes
        if self.boot_layout.has_status_area:
            area = self.boot_layout.status_area
            idx = multi_app_area_idx + (len(self.apps)-1) * 2
            fmt = 'FLASH_AREA_IMAGE_SWAP_STATUS'
            param_dict.update({'status_area': [area, fmt, idx]})

        # Create areas
        for key in param_dict:
            self.__memory_area_config_create(key)

    def __source_gen(self):
        path = f'{self.output_folder}/{self.output_name}.c'
        include = f'{self.output_name}.h'

        with open(path, "w", encoding='UTF-8') as f_out:
            f_out.write(f'#include "{include}"\n')
            f_out.write(f'#include "flash_map_backend.h"\n\n')
            f_out.write('struct flash_device flash_devices[] =\n')
            f_out.write('{\n')
            for region in self.regions:
                f_out.write('\t{\n')
                f_out.write(f'\t\t.address      = {hex(region.addr)}U,\n')
                f_out.write(f'\t\t.size         = {hex(region.sz)}U,\n')
                f_out.write(f'\t\t.erase_size   = {hex(region.erase_sz)}U,\n')
                f_out.write(f'\t\t.erase_val    = {hex(region.erase_val)}U,\n')
                f_out.write(f'\t\t.device_id    = {str(region.type)},\n')
                f_out.write('\t},\n')
            f_out.write('};\n\n')

            f_out.write(f'struct flash_area flash_areas[] =\n')
            f_out.write('{\n')
            for area in self.mem_areas:
                f_out.writelines('\n'.join([
                    '\t{',
                    f"\t\t.fa_id        = {area.fa_id},",
                    f"\t\t.fa_device_id = {area.fa_device_id},",
                    f"\t\t.fa_off       = {hex(area.fa_off)}U,",
                    f"\t\t.fa_size      = {hex(area.fa_size)}U,",
                    '\t},\n']))
            f_out.write('};\n\n')

            f_out.write('struct flash_area *boot_area_descs[] =\n')
            f_out.write('{\n')
            for index, area in enumerate(self.mem_areas):
                f_out.write(f'\t&flash_areas[{index}U],\n')
            f_out.write('\tNULL\n};\n\n')

            f_out.write('uint8_t memory_areas_primary[] =\n')
            f_out.write('{\n')
            for slot in self.primary_slots:
                f_out.write(f'\t{slot}, ')
            f_out.write('\n};\n\n')

            f_out.write('uint8_t memory_areas_secondary[] =\n')
            f_out.write('{\n')
            for slot in self.secondary_slots:
                f_out.write(f'\t{slot}, ')
            f_out.write('\n};\n\n')

    def __header_gen(self):
        path = f'{self.output_folder}/{self.output_name}.h'
        with open(path, "w", encoding='UTF-8') as f_out:
            header_guard_generate(f_out)

            f_out.write(f'#include <stdint.h>\n')
            f_out.write(f'#include "flash_map_backend.h"\n\n')
            f_out.write(f'#define MEMORYMAP_GENERATED_AREAS 1\n\n')
            f_out.write('extern struct flash_device flash_devices[];\n')
            f_out.write('extern struct flash_area *boot_area_descs[];\n\n')
            f_out.write('extern uint8_t memory_areas_primary[];\n')
            f_out.write('extern uint8_t memory_areas_secondary[];\n\n')

            f_out.write('enum \n{\n')
            for id, type in enumerate(self.region_types):
                f_out.write(f'\t{type} = {id}U,\n')
            f_out.write('};\n\n')

            f_out.write('enum \n{\n')
            for area_param in self.param_dict.values():
                f_out.write(f'\t{area_param[1]} = {area_param[2]}U,\n')
            f_out.write('};\n\n')

    def __bootloader_mk_file_gen(self):
        boot = self.boot_layout
        # Upgrade mode
        if boot.scratch_area is None and boot.status_area is None:
            print(settings_dict['overwrite'], ':= 1')
        else:
            print(settings_dict['overwrite'], ':= 0')
            print(settings_dict['swap'], ':= 1')
            print(settings_dict['scratch'], f':= {0 if boot.scratch_area is None else 1}')
            print(settings_dict['status'], f':= {0 if boot.status_area is None else 1}')
        print('# Shared data')
        if boot.shared_data is not None:
            shared_data = boot.shared_data
            print(settings_dict['measured_boot'], ':= 1')
            print(settings_dict['data_sharing'], ':= 1')

            print(f'{settings_dict["shared_data_address"]} :=', hex(shared_data.addr))
            print(f'{settings_dict["shared_data_size"]} :=', hex(shared_data.sz))
            print(f'{settings_dict["shared_data_record_size"]} :=', hex(shared_data.sz))

        print('# Bootloader app area')
        print(f'{settings_dict["bootloader_app_address"]} :=', hex(boot.bootloader_area.addr))
        print(f'{settings_dict["bootloader_app_size"]} :=', hex(boot.bootloader_area.sz))

        print('# Bootloader ram area')
        if boot.ram is not None:
            print(f'{settings_dict["bootloader_ram_address"]} :=', hex(boot.ram.addr))
            print(f'{settings_dict["bootloader_ram_size"]} :=', hex(boot.ram.sz))

        print('# Application area')
        for id, app in enumerate(self.apps):
            print(f'APPLICATION_{id+1}_BOOT_SLOT_ADDRESS := {hex(app.boot_area.addr)}')
            print(f'APPLICATION_{id+1}_BOOT_SLOT_SIZE := {hex(app.boot_area.sz)}')
            print(f'APPLICATION_{id+1}_UPGRADE_SLOT_ADDRESS := {hex(app.upgrade_area.addr)}')
            print(f'APPLICATION_{id+1}_UPGRADE_SLOT_SIZE := {hex(app.upgrade_area.sz)}')

        print('# Ram load')
        # Ram load single
        if len(self.apps) == 1:
            if self.apps[0].ram_boot is not None:
                ram_boot = self.apps[0].ram_boot
                print(settings_dict['ram_load'], ':= 1')
                print(f'{settings_dict["ram_load_address"]} :=', hex(ram_boot.addr))
                print(f'{settings_dict["ram_load_size"]} :=', hex(ram_boot.sz))
        else:
        # Ram load multiple
            ram_boot_counter = 0
            ram_addr_overlap_counter = 0

            for app1 in self.apps:
                for app2 in self.apps:
                    if app1 is app2:
                        continue
                    if app1.overlaps_with(app2):
                        ram_addr_overlap_counter += 1

            for id, app in enumerate(self.apps):
                if app.ram_boot is not None:
                    ram_boot_counter += 1
                    ram_boot = app.ram_boot

                    print(f'APPLICATION_{id+1}_RAM_LOAD_ADDRESS := {hex(ram_boot.addr)}')
                    print(f'APPLICATION_{id+1}_RAM_LOAD_SIZE := {hex(ram_boot.sz)}')

            if ram_boot_counter != 0:
                print(settings_dict['ram_load'], ':= 1')

                if ram_boot_counter != len(self.apps) or ram_addr_overlap_counter == 0:
                    print(settings_dict['multi_memory_load'], ':= 1')

        print('# Mcuboot')
        print(settings_dict['application_count'], f'= {len(self.apps)}')
        print(settings_dict['sectors_count'], f'= {self.max_sectors}')

    def __application_mk_file_gen(self):
        app = self.apps[self.app_id-1]
        boot = self.boot_layout
        # Upgrade mode
        if boot.scratch_area is None and boot.status_area is None:
            print(settings_dict['overwrite'], ':= 1')
        else:
            print(settings_dict['overwrite'], ':= 0')
            print(settings_dict['swap'], ':= 1')
            print(settings_dict['scratch'], f':= {0 if boot.scratch_area is None else 1}')
            print(settings_dict['status'], f':= {0 if boot.status_area is None else 1}')

        print(settings_dict['application_count'], f'= {len(self.apps)}')
        print(settings_dict['boot_image'], ':=', self.app_id)
        print(settings_dict['primary_image_start'], ':=', hex(app.boot_area.addr))
        print(settings_dict['secondary_image_start'], ':=', hex(app.upgrade_area.addr))
        print(settings_dict['image_size'], ':=', hex(app.boot_area.sz))
        if app.ram_boot:
            print(settings_dict['ram_load'], ':= 1')
        if app.ram:
            print(settings_dict['image_ram_address'], ':=',  hex(app.ram.addr))
            print(settings_dict['image_ram_size'], ':=',  hex(app.ram.sz))
        if app.core_name:
            print(settings_dict['core'], ':=',  app.core_name)

    def parse(self, memory_map, platform_config, output_folder, output_name, app_id):
        try:
            with open(memory_map, "r", encoding='UTF-8') as f_in:
                self.map_json = json.load(f_in)

            with open(platform_config, "r", encoding='UTF-8') as f_in:
                self.platform_json = json.load(f_in)

            self.output_folder  = output_folder
            self.output_name    = output_name

            if app_id is not None:
                self.app_id = int(app_id)

            self.__memory_regions_init()
            self.__boot_layout_init()
            self.__apps_init()
            self.__memory_areas_create()

            self.__source_gen()
            self.__header_gen()

            if app_id is None:
                self.__bootloader_mk_file_gen()
            else:
                self.__application_mk_file_gen()

        except (FileNotFoundError, OSError):
            print('\nERROR: Cannot open ', f_in, file=sys.stderr)
            sys.exit(-1)


@click.group()
def cli():
    '''
        Memory map layout parser-configurator
    '''

@cli.command()
@click.option('-i', '--memory_config', required=True,
              help='memory configuration file path')
@click.option('-p', '--platform_config', required=True,
              help='platform configuration file path')
@click.option('-n', '--output_name', required=True,
              help='generated areas path')
@click.option('-o', '--output_folder', required=True,
              help='generated regions path')
@click.option('-d', '--image_id', required=False,
              help='application image number')

def run(memory_config, platform_config, output_folder, output_name, image_id):
    map = MemoryMap()
    map.parse(memory_config,
              platform_config,
              output_folder,
              output_name,
              image_id)

if __name__ == '__main__':
    cli()