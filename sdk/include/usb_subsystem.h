#ifndef NATIVE_USB_SUBSYSTEM_H

#define NATIVE_USB_SUBSYSTEM_H



#include <stdint.h>

#include <stddef.h>

#include <sys/ioctl.h>

#include <linux/usbdevice_fs.h>



#ifdef __cplusplus

extern "C" {

#endif



// Custom error codes

#define USB_SUCCESS           0

#define USB_ERR_INVALID_FD   -1

#define USB_ERR_TRANSFER     -2

#define USB_ERR_TIMEOUT      -3

#define USB_ERR_OUT_OF_MEM   -4



// Packet structures packed cleanly for ARM64 memory alignment

#pragma pack(push, 1)



typedef struct {

    uint8_t  request_type;   // bmRequestType

    uint8_t  request;        // bRequest

    uint16_t value;          // wValue

    uint16_t index;          // wIndex

    uint16_t length;         // wLength

    uint32_t timeout_ms;     // Transfer timeout in milliseconds

} UsbControlSetup;



typedef struct {

    int      device_fd;      // File descriptor passed from Java UsbDeviceConnection

    uint8_t  interface_num;  // Claimed interface number

    uint8_t  bulk_in_ep;     // Bulk IN endpoint address (e.g. 0x81)

    uint8_t  bulk_out_ep;    // Bulk OUT endpoint address (e.g. 0x02)

} UsbDeviceContext;



#pragma pack(pop)



// USB Native Lifecycle and Data APIs

int usb_claim_interface(UsbDeviceContext *ctx, int fd, uint8_t interface_num);

int usb_release_interface(UsbDeviceContext *ctx);



int usb_control_transfer(const UsbDeviceContext *ctx,

                         const UsbControlSetup *setup,

                         uint8_t *data,

                         int32_t *bytes_transferred);



int usb_bulk_write(const UsbDeviceContext *ctx,

                   const uint8_t *data,

                   int32_t length,

                   uint32_t timeout_ms,

                   int32_t *bytes_written);



int usb_bulk_read(const UsbDeviceContext *ctx,

                  uint8_t *buffer,

                  int32_t max_length,

                  uint32_t timeout_ms,

                  int32_t *bytes_read);



#ifdef __cplusplus

}

#endif



#endif // NATIVE_USB_SUBSYSTEM_H