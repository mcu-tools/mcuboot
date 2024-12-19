# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Parse config files (.conf, format: <CONFIG_NAME>=<VALUE>) and set each as
# definitions and variables
function(parse_and_set_config_file CONFIG_FILE)
    file(STRINGS ${CONFIG_FILE} BOOTLOADER_CONF)
    foreach(config ${BOOTLOADER_CONF})
        if (NOT (${config} MATCHES "#"))
            string(REGEX REPLACE "^[ ]+" "" config ${config})
            string(REGEX MATCH "^[^=]+" CONFIG_NAME ${config})
            string(REPLACE "${CONFIG_NAME}=" "" CONFIG_VALUE ${config})
            # Overrides if already defined (definitions from latter file prevails over the former)
            if (DEFINED ${CONFIG_NAME})
                set(CONFIG_OLD "${CONFIG_NAME}")
                remove_definitions(-D${CONFIG_NAME}=${${CONFIG_OLD}})
            endif()
            if (NOT ("${CONFIG_VALUE}" STREQUAL "n"
                OR "${CONFIG_VALUE}" STREQUAL "N"))

                if (("${CONFIG_VALUE}" STREQUAL "y")
                    OR ("${CONFIG_VALUE}" STREQUAL "Y"))
                    set(CONFIG_VALUE 1)
                endif()
                add_compile_definitions(${CONFIG_NAME}=${CONFIG_VALUE})
                set(${CONFIG_NAME} ${CONFIG_VALUE} PARENT_SCOPE)
            endif()
        endif()
    endforeach()
endfunction()

# Auxiliar function to get IDF version from esp_idf_version.h file
function(get_version_from_header VAR_NAME HEADER_FILE VERSION_OUT)
    # Read the header file and extract the value of the specified macro
    file(READ "${HEADER_FILE}" CONTENTS)
    string(REGEX MATCH "#define ${VAR_NAME}[ ]+([0-9]+)" MATCH "${CONTENTS}")
    if(MATCH)
        string(REGEX REPLACE "#define ${VAR_NAME}[ ]+([0-9]+)" "\\1" VERSION "${MATCH}")
        set(${VERSION_OUT} "${VERSION}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Could not find ${VAR_NAME} in ${HEADER_FILE}")
    endif()
endfunction()
