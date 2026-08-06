cmake_minimum_required(VERSION 3.16)

set(FW_VER_MAJOR 1)
set(FW_VER_MINOR 1)
set(FW_VER_PATCH 5)
set(PROJECT_VER "${FW_VER_MAJOR}.${FW_VER_MINOR}.${FW_VER_PATCH}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_hmi)
