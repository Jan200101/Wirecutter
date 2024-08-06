#include "tusb.h"
#include "xinput_host.h"
#include "gamepad.h"

#define XINPUT_GAMEPAD_SHOULDER (XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER)
#define XINPUT_GAMEPAD_DPAD (XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT)
#define XINPUT_GAMEPAD_THUMB (XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB)

tusb_desc_device_t dev_desc;

//Since https://github.com/hathach/tinyusb/pull/2222, we can add in custom vendor drivers easily
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count){
    *driver_count = 1;
    return &usbh_xinput_driver;
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf, uint16_t len)
{
    const xinput_gamepad_t *p = &xid_itf->pad;

    if (xid_itf->last_xfer_result == XFER_RESULT_SUCCESS)
    {
        if (xid_itf->connected && xid_itf->new_pad_data)
        {
            gamepad.x = p->sThumbLX;
            gamepad.y = p->sThumbLY;
            gamepad.z = p->sThumbRX;
            gamepad.rz = p->sThumbRY;
            gamepad.gas = p->bRightTrigger;
            gamepad.brake = p->bLeftTrigger;

            uint8_t dpad = (p->wButtons & XINPUT_GAMEPAD_DPAD);

            if (dpad & XINPUT_GAMEPAD_DPAD_UP)
            {
                if (dpad & XINPUT_GAMEPAD_DPAD_LEFT)
                    gamepad.hat = 8;
                else if (dpad & XINPUT_GAMEPAD_DPAD_RIGHT)
                    gamepad.hat = 2;
                else
                    gamepad.hat = 1;
            }
            else if (dpad & XINPUT_GAMEPAD_DPAD_DOWN)
            {
                if (dpad & XINPUT_GAMEPAD_DPAD_LEFT)
                    gamepad.hat = 6;
                else if (dpad & XINPUT_GAMEPAD_DPAD_RIGHT)
                    gamepad.hat = 4;
                else
                    gamepad.hat = 5;
            }
            else if (dpad & XINPUT_GAMEPAD_DPAD_LEFT)
                gamepad.hat = 7;
            else if (dpad & XINPUT_GAMEPAD_DPAD_RIGHT)
                gamepad.hat = 3;
            else
                gamepad.hat = 0;

            gamepad.buttons = \
                (p->wButtons & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B)) >> 12 |
                (p->wButtons & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y)) >> 11 |
                (p->wButtons & XINPUT_GAMEPAD_SHOULDER) >> 2 |
                (p->wButtons & XINPUT_GAMEPAD_BACK) << 5 |
                (p->wButtons & XINPUT_GAMEPAD_START) << 7 |
                (p->wButtons & XINPUT_GAMEPAD_THUMB) << 7 |
                (p->wButtons & XINPUT_GAMEPAD_GUIDE) << 2 |
            0;

            request_can_send_now_event();
        }
    }
    tuh_xinput_receive_report(dev_addr, instance);
}

void handle_device_descriptor(tuh_xfer_t* xfer)
{
    if (XFER_RESULT_SUCCESS != xfer->result)
    {
        TU_LOG1("Failed to get usb descriptor\n");
        return;
    }

    change_device_id(dev_desc.idVendor, dev_desc.idProduct);
    TU_LOG1("Updated device id to %04x:%04x\n", dev_desc.idVendor, dev_desc.idProduct);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf)
{
    TU_LOG1("Controller attached\n");
    // If this is a Xbox 360 Wireless controller we need to wait for a connection packet
    // on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
    if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false)
    {
        tuh_xinput_receive_report(dev_addr, instance);
        return;
    }
    tuh_descriptor_get_device(dev_addr, &dev_desc, sizeof(dev_desc), handle_device_descriptor, 0);
    tuh_xinput_set_led(dev_addr, instance, 0, true);
    tuh_xinput_set_led(dev_addr, instance, 1, true);
    tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    TU_LOG1("Controller removed, clearing controls\n");
    memset(&gamepad, sizeof(gamepad), 0);
}