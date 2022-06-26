# SPDX-License-Identifier: Apache-2.0

list(
  APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/hx711_driver
  ${CMAKE_CURRENT_SOURCE_DIR}/max7219_driver
)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(weight-scale)

set(app_sources src/main.c src/board/stm32f1)
if(${SOC_SERIES} STREQUAL "stm32f1")
  list(APPEND app_sources src/board/stm32f1/pm.c)
endif()
target_sources(app PRIVATE ${app_sources})
