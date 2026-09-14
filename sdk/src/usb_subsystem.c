#include <unistd.h>

#include <string.h>

#include <errno.h>

#include "usb_subsystem.h"



int usb_claim_interface(UsbDeviceContext *ctx, int fd, uint8_t interface_num) {

    if (fd < 0 || !ctx) return USB_ERR_INVALID_FD;



    memset(ctx, 0, sizeof(UsbDeviceContext));

    ctx->device_fd = fd;

    ctx->interface_num = interface_num;



    // Issue kernel ioctl to detach driver if already active on target interface

    struct usbdevfs_ioctl detach_ioctl;

    detach_ioctl.ifno = interface_num;

    detach_ioctl.ioctl_code = USBDEVFS_DISCONNECT;

    detach_ioctl.data = NULL;

    ioctl(ctx->device_fd, USBDEVFS_IOCTL, &detach_ioctl);



    // Claim interface directly

    int claim_num = interface_num;

    if (ioctl(ctx->device_fd, USBDEVFS_CLAIMINTERFACE, &claim_num) < 0) {

        return USB_ERR_TRANSFER;

    }



    return USB_SUCCESS;

}



int usb_release_interface(UsbDeviceContext *ctx) {

    if (!ctx || ctx->device_fd < 0) return USB_ERR_INVALID_FD;



    int iface = ctx->interface_num;

    ioctl(ctx->device_fd, USBDEVFS_RELEASEINTERFACE, &iface);

    ctx->device_fd = -1;

    return USB_SUCCESS;

}



int usb_control_transfer(const UsbDeviceContext *ctx,

                         const UsbControlSetup *setup,

                         uint8_t *data,

                         int32_t *bytes_transferred) {

    if (!ctx || ctx->device_fd < 0) return USB_ERR_INVALID_FD;

    if (!setup || !bytes_transferred) return -1;



    struct usbdevfs_ctrltransfer ctrl;

    ctrl.bRequestType = setup->request_type;

    ctrl.bRequest = setup->request;

    ctrl.wValue = setup->value;

    ctrl.wIndex = setup->index;

    ctrl.wLength = setup->length;

    ctrl.timeout = setup->timeout_ms;

    ctrl.data = data;



    int res = ioctl(ctx->device_fd, USBDEVFS_CONTROL, &ctrl);

    if (res < 0) {

        return USB_ERR_TRANSFER;

    }



    *bytes_transferred = res;

    return USB_SUCCESS;

}



int usb_bulk_write(const UsbDeviceContext *ctx,

                   const uint8_t *data,

                   int32_t length,

                   uint32_t timeout_ms,

                   int32_t *bytes_written) {

    if (!ctx || ctx->device_fd < 0) return USB_ERR_INVALID_FD;

    if (!bytes_written) return -1;



    struct usbdevfs_bulktransfer bulk;

    bulk.ep = ctx->bulk_out_ep;

    bulk.len = length;

    bulk.timeout = timeout_ms;

    bulk.data = (void *)data;



    int res = ioctl(ctx->device_fd, USBDEVFS_BULK, &bulk);

    if (res < 0) {

        return USB_ERR_TRANSFER;

    }



    *bytes_written = res;

    return USB_SUCCESS;

}



int usb_bulk_read(const UsbDeviceContext *ctx,

                  uint8_t *buffer,

                  int32_t max_length,

                  uint32_t timeout_ms,

                  int32_t *bytes_read) {

    if (!ctx || ctx->device_fd < 0) return USB_ERR_INVALID_FD;

    if (!bytes_read) return -1;



    struct usbdevfs_bulktransfer bulk;

    bulk.ep = ctx->bulk_in_ep;

    bulk.len = max_length;

    bulk.timeout = timeout_ms;

    bulk.data = buffer;



    int res = ioctl(ctx->device_fd, USBDEVFS_BULK, &bulk);

    if (res < 0) {

        return USB_ERR_TRANSFER;

    }



    *bytes_read = res;

    return USB_SUCCESS;

}