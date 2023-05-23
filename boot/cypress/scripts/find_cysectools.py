"""
Copyright (c) 2019 Cypress Semiconductor Corporation

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

import subprocess
import sys

package = 'cysecuretools' 

def find_cysectools(package_name):

    """
    Check if package exist in system and return its location if it does
    """

    pip_show = subprocess.Popen([sys.executable, '-m', 'pip', 'show', package],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    stdout = pip_show.communicate()[0]
    rc = pip_show.wait()

    if (rc != 0):
        print("No package with name " + package + " found")
        exit(1)
    else:
        pip_show_info = stdout.decode("utf-8").splitlines()

    for line in pip_show_info:
        if 'Location:' in line:
            location = line.replace('Location: ', '')

    if sys.platform == 'win32':
        return location.replace('\\','/')
    else:
        return location

def main():
    """
    Call function and print output
    """
    cysecuretools_path = find_cysectools(package)

    print(cysecuretools_path)

if __name__ == "__main__":
    main()