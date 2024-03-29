#-------------------------------------------------------------------------------
# Copyright (c) 2017-2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

#Definitions to compile the "core" module.
#This file assumes it will be included from a project specific cmakefile, and
#will not create a library or executable.
#Inputs:
#	TFM_ROOT_DIR - directory where secure FW sourec is located.
#
#Outputs:
#	Will modify include directories to make the source compile.
#	ALL_SRC_C: C source files to be compiled will be added to this list.
#	  This shall be added to your add_executable or add_library command.
#	ALL_SRC_CXX: C++ source files to be compiled will be added to this list.
#	  This shall be added to your add_executable or add_library command.
#	ALL_SRC_ASM: assembly source files to be compiled will be added to this
#	  list. This shall be added to your add_executable or add_library
#	  command.
#	Include directories will be modified by using the include_directories()
#	  commands as needed.

#Get the current directory where this file is located.
set(SS_CORE_DIR ${CMAKE_CURRENT_LIST_DIR})
if(NOT DEFINED TFM_ROOT_DIR)
	message(FATAL_ERROR
		"Please set TFM_ROOT_DIR before including this file.")
endif()

set (SS_CORE_C_SRC
		"${SS_CORE_DIR}/tfm_core.c"
		"${SS_CORE_DIR}/tfm_handler.c"
		"${SS_CORE_DIR}/tfm_secure_api.c"
		"${SS_CORE_DIR}/tfm_func_api.c"
		"${SS_CORE_DIR}/tfm_spm_services.c"
		"${SS_CORE_DIR}/tfm_nspm.c"
		"${SS_CORE_DIR}/tfm_boot_data.c"
	)

#Append all our source files to global lists.
list(APPEND ALL_SRC_C ${SS_CORE_C_SRC})
unset(SS_CORE_C_SRC)

#Setting include directories
embedded_include_directories(PATH ${TFM_ROOT_DIR} ABSOLUTE)
embedded_include_directories(PATH ${TFM_ROOT_DIR}/interface/include ABSOLUTE)
embedded_include_directories(PATH ${TFM_ROOT_DIR}/secure_fw/spm ABSOLUTE)
embedded_include_directories(PATH ${TFM_ROOT_DIR}/secure_fw/core ABSOLUTE)
embedded_include_directories(PATH ${TFM_ROOT_DIR}/secure_fw/core/arch/include ABSOLUTE)

set(BUILD_CMSIS_CORE Off)
set(BUILD_RETARGET Off)
set(BUILD_NATIVE_DRIVERS Off)
set(BUILD_STARTUP Off)
set(BUILD_TARGET_CFG Off)
set(BUILD_TARGET_HARDWARE_KEYS Off)
set(BUILD_TARGET_NV_COUNTERS Off)
set(BUILD_CMSIS_DRIVERS Off)
set(BUILD_TIME Off)
set(BUILD_UART_STDOUT Off)
set(BUILD_FLASH Off)
set(BUILD_BOOT_SEED Off)
set(BUILD_DEVICE_ID Off)
set(BUILD_PLAT_TEST Off)
if(NOT DEFINED PLATFORM_CMAKE_FILE)
	message (FATAL_ERROR "Platform specific CMake is not defined. Please set PLATFORM_CMAKE_FILE.")
elseif(NOT EXISTS ${PLATFORM_CMAKE_FILE})
	message (FATAL_ERROR "Platform specific CMake \"${PLATFORM_CMAKE_FILE}\" file does not exist. Please fix value of PLATFORM_CMAKE_FILE.")
else()
	include(${PLATFORM_CMAKE_FILE})
endif()
