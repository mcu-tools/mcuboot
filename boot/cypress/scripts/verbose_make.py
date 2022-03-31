"""VERBOSE Makefile Generator
Copyright (c) 2022 Infineon Technologies AG
"""

import os
import re
import sys


def main():
    """VERBOSE Makefile Generator"""
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <makefile>\n', file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r', encoding='UTF-8') as mk_file:
        mk_vars = {}

        for line in mk_file:
            line = line.strip()

            if re.search(r'\s*#', line):  # skip comments
                continue

            match = re.search(r'^\t*([A-Za-z_]\w+)\s*[+:?]=', line)
            if match:  # set variable
                var = match.group(1)
                mk_vars[var] = 1 | mk_vars.get(var, 0)

            match = re.findall(r'\$\(([A-Za-z_]\w+)\)', line)
            for var in match:  # get variable(s)
                mk_vars[var] = 2 | mk_vars.get(var, 0)

    if mk_vars.get('VERBOSE'):
        del mk_vars['VERBOSE']

    print('''\
###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)''')
    print(f'$(info #### {os.path.basename(sys.argv[1])} ####)')
    dirs = (None, '-->', '<--', '<->')
    for var in sorted(mk_vars.keys()):
        print(f'$(info {var} {dirs[mk_vars[var]]} $({var}))')
    print('endif')


if __name__ == '__main__':
    main()
