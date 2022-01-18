# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-portenta-x8 INTERFACE)

target_sources(mbed-portenta-x8
    INTERFACE
        PeripheralPins.c
)

target_include_directories(mbed-portenta-x8
    INTERFACE
        .
)

target_link_libraries(mbed-portenta-x8 INTERFACE mbed-stm32h747xi-cm4)
