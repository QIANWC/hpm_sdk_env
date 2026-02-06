# Copyright (c) 2024 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

# If you do not want to use flash to simulate eeprom. It requests to regenerate
# the SSC code, please refer to the README
set(CONFIG_EEPROM_EMULATION 1) # using eeprom emulation compenent
set(CONFIG_HPM_MONITOR 1)
# set(CONFIG_MONITOR_INTERFACE "uart")
set(CONFIG_MONITOR_INTERFACE "usb")

if("${CONFIG_MONITOR_INTERFACE}" STREQUAL "uart")

elseif("${CONFIG_MONITOR_INTERFACE}" STREQUAL "usb")
    set(CONFIG_CHERRYUSB 1)
    set(CONFIG_USB_DEVICE 1)
    set(CONFIG_USB_DEVICE_CDC 1)
endif()

if("${HPM_BUILD_TYPE}" STREQUAL "")
    SET(HPM_BUILD_TYPE flash_xip)
endif()

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

if("${CONFIG_MONITOR_INTERFACE}" STREQUAL "uart")
    sdk_compile_definitions("-DCONFIG_UART_CHANNEL=1")
    sdk_compile_definitions("-DCONFIG_USE_CONSOLE_UART=1")
    sdk_compile_definitions("-DCONFIG_MONITOR_DBG_LEVEL=0")
elseif("${CONFIG_MONITOR_INTERFACE}" STREQUAL "usb")
    sdk_compile_definitions("-DCONFIG_USB_CHANNEL=1")
endif()

project(ecat_io)

sdk_inc(inc)
sdk_inc(inc/config)

sdk_inc(SSC/Src)
sdk_src(SSC/Src/coeappl.c)
sdk_src(SSC/Src/ecatappl.c)
sdk_src(SSC/Src/ecatcoe.c)
sdk_src(SSC/Src/ecatslv.c)
sdk_src(SSC/Src/mailbox.c)
sdk_src(SSC/Src/objdef.c)
sdk_src(SSC/Src/sdoserv.c)

sdk_inc(../port)
sdk_src(../port/hpm_ecat_hw.c)
sdk_src(../port/hpm_ecat_phy.c)
if(CONFIG_EEPROM_EMULATION)
sdk_src(../port/hpm_ecat_e2p_emulation.c)
endif()

sdk_src(application/digital_io.c)

sdk_inc(.)
sdk_app_src(ecat.c)
generate_ide_projects()
