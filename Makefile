main: usb_mouse_driver_libusb.c
	gcc -o $@ $< -lusb-1.0
