```markdown
# Master Workspace Directory Map
Below is the flat workspace map showing how each of our 81 files is organized into nested directories on Jules's machine. This complete visual reference represents the clean, production-ready NDK compilation scheme.

```
[nacl-repository]/
├── .github/
│   └── workflows/
│       └── ndk-build.yml                   # Automates ARM64 & x86_64 cloud builds
├── host_app/
│   ├── app/
│   │   ├── build.gradle                    # Host app build configuration
│   │   └── src/
│   │       └── main/
│   │           └── java/
│   │               └── com/
│   │                   └── your/
│   │                       └── app/
│   │                           ├── bootstrap/
│   │                           │   ├── HostAppBootstrapper.java    # Key storage & Treble relocator
│   │                           │   └── MainActivity.java           # Android launcher and diagnostic hub
│   │                           ├── AudioWaveformWidget.kt          # Compose UI Audio wave visualizer
│   │                           ├── CellTowerMetricDecoder.kt       # Cellular signal diagnostic interpreter
│   │                           ├── NaclAudioBridge.kt              # Native-to-Kotlin audio routing bridge
│   │                           ├── NaclBridge.kt                   # Global native reactive flow JNI gate
│   │                           ├── NaclOverlayService.kt           # Vulkan overlay dynamic render service
│   │                           ├── NaclUsbBridge.kt                # USB direct data interop hook
│   │                           └── SignalGaugeWidget.kt            # Compose UI Cellular metric UI gauge
├── sdk/
│   ├── CMakeLists.txt                      # Master multi-ABI dynamic compilation scheme
│   ├── include/                            # Public & central subsystem C headers
│   │   ├── adb_client.h
│   │   ├── android_core.h
│   │   ├── audio.h
│   │   ├── audio_waveform_common.h
│   │   ├── automation_common.h
│   │   ├── bluetooth_ipc_common.h
│   │   ├── camera_subsystem.h
│   │   ├── camera/
│   │   │   └── NdkCameraManager.h          # Mock/Header compatibility definition
│   │   ├── display_media.h
│   │   ├── input.h
│   │   ├── ipc_common.h
│   │   ├── ipc_crypto.h
│   │   ├── location.h
│   │   ├── nacl_display.h
│   │   ├── nacl_unified_api.h              # Exported public master API header
│   │   ├── nfc_subsystem.h
│   │   ├── power_battery.h
│   │   ├── quickjs_eventfd_bridge.h
│   │   ├── routing_core.h
│   │   ├── sensor_ipc_common.h
│   │   ├── shm_common.h
│   │   ├── shm_ring_buffer.h
│   │   ├── storage.h
│   │   ├── telephony_common.h
│   │   └── usb_subsystem.h
│   │   └── vulkan_renderer.h
│   ├── src/                                # Core C dynamic implementations & wrappers
│   │   ├── adb_client.c
│   │   ├── android_core.c
│   │   ├── audio.c
│   │   ├── bluetooth_svc.cpp
│   │   ├── camera_subsystem.c
│   │   ├── client_bridge.c
│   │   ├── connectivity_automation.c
│   │   ├── display.cpp
│   │   ├── display_jni_bridge.cpp
│   │   ├── display_media.c
│   │   ├── input.c
│   │   ├── ipc_crypto.c
│   │   ├── libbluetooth_client.c
│   │   ├── location.c
│   │   ├── mock_client_main.c
│   │   ├── native_host_bridge.cpp
│   │   ├── nfc_subsystem.c
│   │   ├── power_battery.c
│   │   ├── quickjs_adb_binding.c
│   │   ├── quickjs_automation_binding.c
│   │   ├── quickjs_bluetooth_binding.c
│   │   ├── quickjs_core_binding.c
│   │   ├── quickjs_crypto_binding.c
│   │   ├── quickjs_eventfd_bridge.c
│   │   ├── quickjs_eventfd_bridge_binding.c
│   │   ├── quickjs_final_subsystems_bindings.c
│   │   ├── quickjs_routing_binding.c
│   │   ├── quickjs_sensors_binding.c
│   │   ├── quickjs_shm_binding.c
│   │   ├── quickjs_telephony_binding.c
│   │   ├── routing_core.cpp
│   │   ├── sensors_client.c
│   │   ├── sensors_daemon.c
│   │   ├── service_daemon.c
│   │   ├── shm_client.c
 guide  │   ├── shm_daemon.c
│   │   ├── storage.c
│   │   ├── telephony_client.c
│   │   ├── usb_subsystem.c
│   │   └── vulkan_renderer.c
│   └── config/                             # Automated deployment shell structures
│       ├── build_and_deploy.sh
│       ├── deploy_abi_target.sh
│       └── multi_process_debug.sh
└── verify_daemons.sh                       # Local terminal test tool to check status
```

---