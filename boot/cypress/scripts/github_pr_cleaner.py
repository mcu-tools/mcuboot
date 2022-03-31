#!/usr/bin/env python

import shutil
from os import remove
from sys import argv

shutil.rmtree('../cppcheck')
shutil.rmtree('../coverity')
remove('../../../.gitlab-ci.yml')
remove('../BlinkyApp/BlinkyApp_CM4_Debug.launch')
remove('../MCUBootApp/MCUBootApp_CM0P_Debug.launch')
remove('../MCUBootApp/MCUBootApp_CYW20829_Debug.launch')
remove('../cy_flash_pal/flash_cyw208xx/flashmap/cyw20829_xip_swap_single_psvp.json')
remove('../cy_flash_pal/flash_cyw208xx/flashmap/cyw20829_xip_swap_multi2_psvp.json')
remove('../cy_flash_pal/flash_cyw208xx/flashmap/cyw20829_xip_overwrite_single_psvp.json')
remove('../cy_flash_pal/flash_cyw208xx/flashmap/cyw20829_xip_overwrite_multi2_psvp.json')
remove('./cppcheck-htmlreport.py')
remove('./rbc_policy_and_cert_revision_modify.py')
remove('../platforms/CYW20829/cyw20829_psvp.h')
remove(argv[0])

print('Cleanup complete')
