import 'dart:ffi' as ffi;

import 'dart:typed_data';

import 'dart:async';

import 'package:ffi/ffi.dart';



// C-Struct representations matching nacl_unified_api.h

base class NaclEventFrame extends ffi.Struct {

  @ffi.Uint32()

  external int moduleId;



  @ffi.Uint32()

  external int eventType;



  @ffi.Uint64()

  external int timestampNs;



  @ffi.Size()

  external int payloadSize;



  external ffi.Pointer<ffi.Uint8> payload;

}



// Dart Callback Signature

typedef NaclEventCallbackDart = void Function(ffi.Pointer<NaclEventFrame> frame);

// C Callback Signature

typedef NaclEventCallbackC = ffi.Void Function(ffi.Pointer<NaclEventFrame> frame);



// Dart FFI bindings to libandroid_core.so APIs

typedef _InitFunc = ffi.Bool Function(ffi.Pointer<Utf8>);

typedef _InitFuncDart = bool Function(ffi.Pointer<Utf8>);



typedef _SetMockFunc = ffi.Void Function(ffi.Uint32, ffi.Bool);

typedef _SetMockFuncDart = void Function(int, bool);



typedef _RegisterListenerFunc = ffi.Void Function(ffi.Pointer<ffi.NativeFunction<NaclEventCallbackC>>);

typedef _RegisterListenerFuncDart = void Function(ffi.Pointer<ffi.NativeFunction<NaclEventCallbackC>>);



class NaclFfi {

  late ffi.DynamicLibrary _lib;

  late _InitFuncDart _initialize;

  late _SetMockFuncDart _setMockMode;

  late _RegisterListenerFuncDart _registerListener;



  final _eventController = StreamController<NaclDartEvent>.broadcast();

  Stream<NaclDartEvent> get events => _eventController.stream;



  NaclFfi() {

    // Dynamically load the core loader on Android

    _lib = ffi.DynamicLibrary.open('libandroid_core.so');



    _initialize = _lib

        .lookup<ffi.NativeFunction<_InitFunc>>('nacl_initialize')

        .asFunction<_InitFuncDart>();



    _setMockMode = _lib

        .lookup<ffi.NativeFunction<_SetMockFunc>>('nacl_set_subsystem_mock_mode')

        .asFunction<_SetMockFuncDart>();



    _registerListener = _lib

        .lookup<ffi.NativeFunction<_RegisterListenerFunc>>('nacl_register_event_listener')

        .asFunction<_RegisterListenerFuncDart>();



    _setupEventListener();

  }



  bool initialize(String privateDirPath) {

    final pathPtr = privateDirPath.toNativeUtf8();

    final result = _initialize(pathPtr);

    malloc.free(pathPtr);

    return result;

  }



  void setMockMode(int moduleId, bool enabled) {

    _setMockMode(moduleId, enabled);

  }



  // Global static pointer context required for C FFI callback routing

  static late StreamController<NaclDartEvent> _staticController;



  void _setupEventListener() {

    _staticController = _eventController;

    // Map our Dart-side static receiver to the native function pointer callback

    final callbackPtr = ffi.Pointer.fromFunction<NaclEventCallbackC>(_nativeCallbackReceiver);

    _registerListener(callbackPtr);

  }



  // Receives raw structures directly from native OS threads

  static void _nativeCallbackReceiver(ffi.Pointer<NaclEventFrame> framePtr) {

    final frame = framePtr.ref;



    // Read raw payload from memory into native Dart Typed Data views safely

    final rawPayload = frame.payload.asTypedList(frame.payloadSize);

    final payloadBytes = Uint8List.fromList(rawPayload);



    _staticController.add(NaclDartEvent(

      moduleId: frame.moduleId,

      eventType: frame.eventType,

      timestampNs: frame.timestampNs,

      payload: payloadBytes,

    ));

  }

}



// Standardized structured Dart representation

class NaclDartEvent {

  final int moduleId;

  final int eventType;

  final int timestampNs;

  final Uint8List payload;



  NaclDartEvent({

    required this.moduleId,

    required this.eventType,

    required this.timestampNs,

    required this.payload,

  });

}