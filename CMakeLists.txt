#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nebula_sensor)

# --- START: Manually add NUS service source ---
# This forces the build system to compile and link the NUS implementation,
# resolving the "undefined reference" error.
# target_sources(app PRIVATE
#     ${ZEPHYR_BASE}/subsys/bluetooth/services/nus/nus.c
# )
# --- END: Manually add NUS service source ---

target_sources(app PRIVATE
  src/main.c
  src/sensor_logic.c
  src/aes_gcm.c
  )
