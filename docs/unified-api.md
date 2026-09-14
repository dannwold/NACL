```markdown
# Public C API Catalog
(`nacl_unified_api.h`)

Below is the function-by-function reference of the exported public native API interface. Every external framework (such as JNI, Dart FFI, or direct C/C++ clients) interacts exclusively via this stable binary interface.

| Function Prototype | Parameter List | Return Value | Functional Description |
| :--- | :--- | :--- | :--- |
| `nacl_init` | `const char* secure_lib_path` | `int32_t` (0 = Success, Error otherwise) | Dynamically indexes and resolves all secondary modules (`.so`) in the relocations directory. |
| `nacl_register_callback` | `uint32_t module_id, void (*cb)(const NaclEventFrame*)` | `int32_t` (0 = Registered) | Binds a raw C-style callback function to receive telemetry packets from the target hardware loop. |
| `nacl_dispatch_event` | `const NaclEventFrame* frame` | `int32_t` (0 = Success) | Dispatches a command payload down the active modular router to change hardware telemetry states. |
| `nacl_shutdown` | *None* | `int32_t` (0 = Terminated) | Unmaps and closes all open shared memory maps, socket channels, and background worker loops safely. |

---