#!/usr/bin/env python

import shutil
from os import remove
from sys import argv

shutil.rmtree('../cppcheck')
shutil.rmtree('../coverity')
shutil.rmtree('../manifests')
remove('../../../.gitlab-ci.yml')
remove('../BlinkyApp/BlinkyApp_CM4_Debug.launch')
remove('../MCUBootApp/MCUBootApp_CM0P_Debug.launch')
remove('../MCUBootApp/MCUBootApp_CYW20829_Debug.launch')
remove('./cppcheck.sh')
remove('./cppcheck-htmlreport.py')
remove('./rbc_policy_and_cert_revision_modify.py')
remove('../platforms/BSP/CYW20829/cyw20829_psvp.h')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/cyw20829_xip_swap_single_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/cyw20829_xip_swap_multi2_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/cyw20829_xip_overwrite_single_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/cyw20829_xip_overwrite_multi2_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/hw_rollback_prot/cyw20829_xip_swap_single_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/hw_rollback_prot/cyw20829_xip_swap_multi2_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/hw_rollback_prot/cyw20829_xip_overwrite_single_psvp.json')
remove('../platforms/cy_flash_pal/flash_cyw20829/flashmap/hw_rollback_prot/cyw20829_xip_overwrite_multi2_psvp.json')
remove(argv[0])

print('Cleanup complete')
