cmake_minimum_required(VERSION 3.22.1)
project(AndroidNativeCapabilityLibrary C CXX)

add_definitions(-DANDROID_PLATFORM=1 -D__ANDROID__=1 -DANDROID=1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Direct CMake to use unified include directory for all target modules
include_directories(include)

# Standard Android system libraries to link against
find_library(log-lib log)
find_library(android-lib android)
find_library(dl-lib dl)

# 1. Core Coordinator & JNI Bridge
add_library(android_core SHARED
    src/android_core.c
    src/client_bridge.c
)
target_link_libraries(android_core ${log-lib} ${android-lib} ${dl-lib})

# 2. Sensors Subsystem Client & Daemon
add_library(sensors_client SHARED src/sensors_client.c)
target_link_libraries(sensors_client ${log-lib})

add_executable(sensors_daemon src/sensors_daemon.c)
target_link_libraries(sensors_daemon ${log-lib} ${android-lib})

# 3. Telephony Subsystem
add_library(telephony_client SHARED src/telephony_client.c)
target_link_libraries(telephony_client ${log-lib})

# 4. Bluetooth Subsystem
add_library(bluetooth_client SHARED src/libbluetooth_client.c)
target_link_libraries(bluetooth_client ${log-lib})

add_library(bluetooth_svc SHARED src/bluetooth_svc.cpp)
target_link_libraries(bluetooth_svc ${log-lib})

# 5. IPC Cryptography Gateway
add_library(ipc_crypto SHARED src/ipc_crypto.c)
target_link_libraries(ipc_crypto ${log-lib})

# 6. Shared Memory Subsystem Client & Daemon
add_library(shm_client SHARED src/shm_client.c)
target_link_libraries(shm_client ${log-lib})

add_executable(shm_daemon src/shm_daemon.c)
target_link_libraries(shm_daemon ${log-lib})

# 7. Vulkan Rendering Layer & Display Bridges
add_library(vulkan_renderer SHARED src/vulkan_renderer.c src/display.cpp)
target_link_libraries(vulkan_renderer ${log-lib} ${android-lib})

add_library(display_jni_bridge SHARED src/display_jni_bridge.cpp)
target_link_libraries(display_jni_bridge ${log-lib} ${android-lib})

add_library(display_media SHARED src/display_media.c)
target_link_libraries(display_media ${log-lib} ${android-lib})

# 8. ADB Loopback Client Gateway
add_library(adb_client SHARED src/adb_client.c)
target_link_libraries(adb_client ${log-lib})

# 9. Dynamic Routing & Core Control
add_library(routing_core SHARED src/routing_core.c)
target_link_libraries(routing_core ${log-lib} ${dl-lib})

# 10. Hardware Subsystems (USB, Camera, NFC, Input, Audio, Location, Storage, Power)
add_library(usb_subsystem SHARED src/usb_subsystem.c)
target_link_libraries(usb_subsystem ${log-lib})

add_library(camera_subsystem SHARED src/camera_subsystem.c)
target_link_libraries(camera_subsystem ${log-lib} ${android-lib})

add_library(nfc_subsystem SHARED src/nfc_subsystem.c)
target_link_libraries(nfc_subsystem ${log-lib})

add_library(nacl_input SHARED src/input.c)
target_link_libraries(nacl_input ${log-lib} ${android-lib})

add_library(nacl_audio SHARED src/audio.c)
target_link_libraries(nacl_audio ${log-lib} ${android-lib})

add_library(nacl_location SHARED src/location.c)
target_link_libraries(nacl_location ${log-lib})

add_library(nacl_storage SHARED src/storage.c)
target_link_libraries(nacl_storage ${log-lib})

add_library(power_battery SHARED src/power_battery.c)
target_link_libraries(power_battery ${log-lib})

# 11. Connectivity & Automation Controller
add_library(connectivity_automation SHARED src/connectivity_automation.c)
target_link_libraries(connectivity_automation ${log-lib} adb_client)

# 12. Host App Native Bridge
add_library(native_host_bridge SHARED src/native_host_bridge.cpp)
target_link_libraries(native_host_bridge ${log-lib} ${android-lib})

# 13. Service Daemon & Mock Client Executables
add_executable(service_daemon src/service_daemon.c)
target_link_libraries(service_daemon ${log-lib})

add_executable(mock_client_main src/mock_client_main.c)
target_link_libraries(mock_client_main ${log-lib})

# 14. Universal QuickJS Binding Engine Module
add_library(quickjs_bindings SHARED
    src/quickjs_eventfd_bridge_binding.c
    src/quickjs_eventfd_bridge.c
    src/quickjs_adb_binding.c
    src/quickjs_automation_binding.c
    src/quickjs_bluetooth_binding.c
    src/quickjs_core_binding.c
    src/quickjs_crypto_binding.c
    src/quickjs_final_subsystems_bindings.c
    src/quickjs_routing_binding.c
    src/quickjs_sensors_binding.c
    src/quickjs_shm_binding.c
    src/quickjs_telephony_binding.c
)
target_link_libraries(quickjs_bindings ${log-lib} ${dl-lib})
