#!/bin/bash
#
# this must be the first non-commented line in this script. It ensures
# bash doesn't choke on \r on Windows
(set -o igncr) 2>/dev/null && set -o igncr; # this comment is needed

#
# This script does static code analysis using Cppcheck tool
# Copyright (c) 2019 Cypress Semiconductor.
#

# It performs Cppcheck code analysis with following inputs
# 1. CypressBootloader/sources - Code analysis is done on all the sources of CypressBootloader.
# 2. Additional source files to be analyzed are grabbed from config file that is provided as a first argument to the script.
# 3. Files to be ignored are grabbed from config file that is provided as a first argument to the script.
# 4. To ignore a file its name need to be added to the config file with word "ignore" as perfix
# 5. To add any additional files, apart the files from CypressBootloader/sources, those names need
#    to be added in a config file.
#    Example
#    A). add below entries in cpp_check.dat file
#        ignore cy_bootloader_hw.c
#        file1.c
#        file2.c
#        ignore cy_bootloader_services.c
#    B). invoke cpp_check shell script
#        cpp_check.sh cpp_check.dat
#
#    Above example performs Cppcheck analysis on CypressBootloader/sources, ignore cy_bootloader_hw.c, file1.c, file2.c and ignores cy_bootloader_services.c


app_name="$1"
platform="$2"
app_defines="$3"
app_includes="$4"
CPP_CHECK_FILES="$5"
scope="$6"
buildcfg="$7"

if [[ ${scope} != "" ]]; then
    SCOPE="--enable=${scope}"
else
    SCOPE=""
fi

#Retrieve list of files need to be ignored
while IFS= read -r line
do
    CPP_CHECK_IGNORE_FILES="$CPP_CHECK_IGNORE_FILES -i $line"
done < "cppcheck/${app_name}/ignore_files.list"

#Retrieve list of cppcheck directives
while IFS= read -r line
do
    CPP_CHECK_SUPPRESS="$CPP_CHECK_SUPPRESS --suppress=$line"
done < "cppcheck/${app_name}/suppress_types.list"

echo "-------------------------------------------"
echo "Suppress options:" "$CPP_CHECK_SUPPRESS"
echo "-------------------------------------------"
echo "Additional files:" "$CPP_CHECK_FILES"
echo "-------------------------------------------"
echo "Ignoring files:" "$CPP_CHECK_IGNORE_FILES"
echo "-------------------------------------------"
echo "CppCheck scope of messages defined with option " ${SCOPE}
echo "-------------------------------------------"
echo "Run CppCheck for platform" ${platform}
echo "-------------------------------------------"
echo "Defines passed to CppCheck:"
echo ${app_defines}
echo "-------------------------------------------"
echo "Include dirs passed to CppCheck:"
echo ${app_includes}
echo "-------------------------------------------"

mkdir -p cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_html

dos2unix cppcheck/${app_name}/suppress_messages.list

dos2unix cppcheck/${app_name}/ignore_files.list

#Generate file with list of additional files for cppcheck
CPP_CHECK_FILES_LIST_FILE=cppcheck/${app_name}/cpp-check-files.list
echo ${CPP_CHECK_FILES} | sed -e "s+ +\n+g" > ${CPP_CHECK_FILES_LIST_FILE} 

#Generate xml file
cppcheck ${SCOPE} ${CPP_CHECK_SUPPRESS} -D__GNUC__ -D${platform} ${app_defines} ${app_includes} --file-list=${CPP_CHECK_FILES_LIST_FILE} ${CPP_CHECK_IGNORE_FILES} \
    --xml 2> cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.xml

#Generate html file
python cppcheck/cppcheck-htmlreport.py --file=cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.xml --report-dir=cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_html --title=${app_name}

cppcheck ${SCOPE} ${CPP_CHECK_SUPPRESS} -D__GNUC__ -D${platform} ${app_defines} ${app_includes} --file-list=${CPP_CHECK_FILES_LIST_FILE} ${CPP_CHECK_IGNORE_FILES} \
    --template="{severity}\n{id}\n{message}\n{file}\n{line}:{column}\n{code}\n" 2> cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.full

#Generate csv file
echo "severity@id@message@file@line" > cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.csv
while IFS= read -r line
do
    read -r line2
    read -r line3
    read -r line4
    read -r line5
    line4=$(echo $line4 | sed 's/.*\\cy_mcuboot\\//' | tr '\\' '/')
    if grep -xq "${line}@${line2}@${line3}@${line4}@${line5}" cppcheck/${app_name}/suppress_messages.list
    then
        :;#suppress current warning
    else
        if grep -xq "${line4}" cppcheck/${app_name}/ignore_files.list
        then
            :;#suppress current warning
        else
            echo ${line}@${line2}@${line3}@${line4}@${line5}
        fi
    fi
    read -r line
    read -r line
    read -r line
done \
< cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.full \
>>cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.csv

#Generate log file
while IFS= read -r line
do
    read -r line2
    read -r line3
    read -r line4
    read -r line5
    line4=$(echo $line4 | sed 's/.*\\cy_mcuboot\\//' | tr '\\' '/')
    if grep -xq "${line}@${line2}@${line3}@${line4}@${line5}" cppcheck/${app_name}/suppress_messages.list
    then
        read -r line
        read -r line
        read -r line
    else
        if grep -xq "${line4}" cppcheck/${app_name}/ignore_files.list
        then
            read -r line
            read -r line
            read -r line
        else
            echo ${line} : ${line2}
            echo ${line3}
            echo "${line4} (${line5})"
            read -r line
            echo ${line}
            read -r line
            echo ${line}
            read -r line
            echo "-------------------------------------------"
        fi
    fi
done \
< cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.full \
> cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.log

rm cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.full
cat cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.log

ERRORS=$(( $(wc -l cppcheck/${app_name}/results/${platform}/${buildcfg}/cppcheck_report_${scope}.csv | cut -d' ' -f1) -1 ))
echo "${app_name} CPPCHECK FOR ${platform} KIT FOUND $ERRORS ERRORS"
exit $ERRORS