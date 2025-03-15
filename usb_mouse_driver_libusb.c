#include <stdio.h>
#include <libusb-1.0/libusb.h>

#include "usb_mouse_driver_libusb.h"

static struct libusb_device_handle *devh = NULL;

static int do_sync_intr(unsigned char *data, int max_size, uint8_t ep_addr)
{
	int r;
	int transferred;

	r = libusb_interrupt_transfer(devh, ep_addr, data, max_size,
		&transferred, 0);
	if (r < 0) {
		fprintf(stderr, "intr error %d\n", r);
		return r;
	}
	if (transferred < max_size) {
		fprintf(stderr, "short read (%d)\n", r);
		return -1;
	}

	// printf("recv interrupt %05x\n", *((uint16_t *)data));
    for (int i = 0; i < max_size; i++)
        printf("%02x ", data[i]);
    printf("\n");
	return 0;
}

static int sync_intr(int max_size, uint8_t ep_addr)
{
	int r;
	unsigned char data[64];

	while (1) {
		r = do_sync_intr(data, max_size, ep_addr);
		if (r < 0)
			return r;
	}
}

int main()
{
    int r;

    r = libusb_init(NULL);
    if (r < 0)
        return r;

    struct libusb_device **device_list;
    int device_count = libusb_get_device_list(NULL, &device_list);
    if (device_count < 0)
        return device_count;

    struct libusb_device *device;
    int i = 0;
    int found = 0;
    int max_size = 0;
    uint8_t ep_addr = 0;
    while ((device = device_list[i++]) != NULL)
    {
		struct libusb_device_descriptor dev_desc;
		int r = libusb_get_device_descriptor(device, &dev_desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			goto exit;
		}

        printf("dev_desc.bNumConfigurations: %d\n",
            dev_desc.bNumConfigurations);

        struct libusb_config_descriptor *config_desc;
        r = libusb_get_config_descriptor(device, 0, &config_desc);
        // return zero on success
        if (r) {
            fprintf(stderr, "failed to get device descriptor");
            goto exit;
        }

        for (int interface = 0; interface < config_desc->bNumInterfaces;
            interface++)
        {
            const struct libusb_interface_descriptor *intf_desc = &
                config_desc->interface[interface].altsetting[0];

            if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID &&
                intf_desc->bInterfaceSubClass == 0x01 && // boot interface subclass
                intf_desc->bInterfaceProtocol == 0x02) // mouse
            {
                // already found a mouse
                printf("already found a mouse, endpooint address: 0x%x\n"
                    "bNumEngpoints: %d\n"
                    "bmAttributes: %x\n"
                    "wMaxPacketSize: %d\n",
                    intf_desc->endpoint[0].bEndpointAddress,
                    intf_desc->bNumEndpoints,
                    intf_desc->endpoint[0].bmAttributes,
                    intf_desc->endpoint[0].wMaxPacketSize);

                r = libusb_open(device, &devh);
                if (r < 0) {
                    fprintf(stderr, "libusb_open() failed: %d - %s\n", r, libusb_error_name(r));
                    goto exit;
                }

                r = libusb_set_auto_detach_kernel_driver(devh, 1);
                if (r < 0) {
                    fprintf(stderr, "set auto detach driver error %d - %s\n", r, libusb_strerror(r));
                    goto exit;
                }

                r = libusb_claim_interface(devh, interface);
                if (r < 0) {
                    fprintf(stderr, "claim interface error %d - %s\n", r, libusb_strerror(r));
                    goto exit;
                }

                found = 1;
                max_size = intf_desc->endpoint[0].wMaxPacketSize;
                ep_addr = intf_desc->endpoint[0].bEndpointAddress;
                break;
            }
        }

        if (found)
            break;

        libusb_free_config_descriptor(config_desc);
    }

    if (found)
        sync_intr(max_size, ep_addr);

exit:
    libusb_free_device_list(device_list, 1);
    libusb_exit(NULL);
    return 0;
}
