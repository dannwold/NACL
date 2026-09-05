cmake_minimum_required(VERSION 3.22.1)
project(AndroidNativeCapabilityLibrary C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Direct CMake to use unified include directory for all target modules
include_directories(include)

# Standard Android system libraries to link against
find_library(log-lib log)
find_library(android-lib android)
find_library(dl-lib dl)

# Define Core Coordinator
add_library(android_core SHARED
    src/android_core.c
    src/client_bridge.c
)
target_link_libraries(android_core ${log-lib} ${android-lib} ${dl-lib})

# Define Sensors Module Client and Daemon
add_library(sensors_client SHARED src/sensors_client.c)
target_link_libraries(sensors_client ${log-lib})

add_executable(sensors_daemon src/sensors_daemon.c)
target_link_libraries(sensors_daemon ${log-lib} ${android-lib})

# Define Telephony Client
add_library(telephony_client SHARED src/telephony_client.c)
target_link_libraries(telephony_client ${log-lib})

# Define Bluetooth Client
add_library(bluetooth_client SHARED src/libbluetooth_client.c)
target_link_libraries(bluetooth_client ${log-lib})

# Define IPC Cryptography Gateway
add_library(ipc_crypto SHARED src/ipc_crypto.c)
target_link_libraries(ipc_crypto ${log-lib})

# Define Shared Memory Client & Daemon
add_library(shm_client SHARED src/shm_client.c)
target_link_libraries(shm_client ${log-lib})

# Define Background Shared Memory Service Daemon
add_executable(shm_daemon src/shm_daemon.c)
target_link_libraries(shm_daemon ${log-lib})

# Define Universal Event Loop Interface
add_library(quickjs_eventfd_bridge SHARED src/quickjs_eventfd_bridge_binding.c src/quickjs_eventfd_bridge.c)
target_link_libraries(quickjs_eventfd_bridge ${log-lib} quickjs_lib)




# Define Vulkan Rendering Layer
add_library(vulkan_renderer SHARED src/vulkan_renderer.c src/display.cpp)
target_link_libraries(vulkan_renderer ${log-lib} ${android-lib} -lvulkan -lEGL -lGLESv3 -lGLESv2)

# Define ADB Client Loopback Gateway
add_library(adb_client SHARED src/adb_client.c)
target_link_libraries(adb_client ${log-lib})

# Define Subsystem HAL Bridge layers
add_library(usb_subsystem SHARED src/usb_subsystem.c)
target_link_libraries(usb_subsystem ${log-lib})

add_library(camera_subsystem SHARED src/camera_subsystem.c)
target_link_libraries(camera_subsystem ${log-lib} ${android-lib} camera2ndk mediandk)

add_library(nfc_subsystem SHARED src/nfc_subsystem.c)
target_link_libraries(nfc_subsystem ${log-lib})

# Define Automation Controller Interface
add_library(connectivity_automation SHARED src/connectivity_automation.c)
target_link_libraries(connectivity_automation ${log-lib} adb_client)



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/quickjs)
add_library(quickjs_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/quickjs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/quickjs-libc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/cutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/libunicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/libregexp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/dtoa.c)
target_compile_definitions(quickjs_lib PRIVATE _GNU_SOURCE CONFIG_VERSION="2026-06-04")
