#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "btstack.h"
#include "gamepad.h"
#include "hardware/gpio.h"
#include "classic/rfcomm.h"

#define BUTTON_PIN 13

static const char hid_device_name[] = "Wirecutter Adapter";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[4090];
static uint8_t device_id_sdp_service_buffer[100];
static btstack_packet_callback_registration_t hci_event_callback_registration;
uint16_t hid_cid;

const uint16_t host_max_latency = 1600;
const uint16_t host_min_timeout = 3200;

const uint8_t hid_descriptor_gamepad[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x05, // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
    0xa1, 0x01, // COLLECTION (Application)
        // Buttons
        0x85, 0x03, // REPORT_ID (Default: 3)
        0x05, 0x09, // USAGE_PAGE (Button)
        0x15, 0x00, // LOGICAL_MINIMUM (0)
        0x25, 0x01, // LOGICAL_MAXIMUM (1)
        0x75, 0x01, // REPORT_SIZE (1)
        0x19, 0x01, // USAGE_MINIMUM (Button 1)
        0x29, 0x10, // USAGE_MAXIMUM (Up to 128 buttons possible)
        0x95, 0x10, // REPORT_COUNT (# of buttons)
        0x81, 0x02, // INPUT (Data,Var,Abs)

        // Axis
        0x05, 0x01, // USAGE_PAGE (Generic Desktop)
        0x09, 0x01, // USAGE (Pointer)
        0x16, 0x01, 0x80, // LOGICAL_MINIMUM (-32767)
        0x26, 0xFF, 0x7F, // LOGICAL_MAXIMUM (+32767)
        0x75, 0x10, // REPORT_SIZE (16)
        0x95, 0x04, // REPORT_COUNT (configuration.getAxisCount())
        0xA1, 0x00, // COLLECTION (Physical)
            0x09, 0x30, // USAGE (X)
            0x09, 0x31, // USAGE (Y)
            0x09, 0x32, // USAGE (Z)
            0x09, 0x35, // USAGE (Rz)
            0x81, 0x02, // INPUT (Data,Var,Abs)
        0xc0, // END_COLLECTION (Physical)

        // Simulation
        0x05, 0x02, // USAGE_PAGE (Simulation Controls)
        0x16, 0x00, 0x00, // LOGICAL_MINIMUM (0)
        0x26, 0xFF, 0x00, // LOGICAL_MAXIMUM (255)
        0x75, 0x08, // REPORT_SIZE (8)
        0x95, 0x02, // REPORT_COUNT (2)
        0xA1, 0x00, // COLLECTION (Physical)
            0x09, 0xC4, // USAGE (Accelerator)
            0x09, 0xC5, // USAGE (Brake)
            0x81, 0x02, // INPUT (Data,Var,Abs)
        0xc0, // END_COLLECTION (Physical)

        // HAT
        0xA1, 0x00, // COLLECTION (Physical)
            0x05, 0x01, // USAGE_PAGE (Generic Desktop)
            0x09, 0x39, // USAGE (Hat Switch)
            0x15, 0x01, // Logical Min (1)
            0x25, 0x08, // Logical Max (8)
            0x35, 0x00, // Physical Min (0)
            0x46, 0x3B, 0x01, // Physical Max (315)
            0x65, 0x12, // Unit (SI Rot : Ang Pos)
            0x75, 0x08, // Report Size (8)
            0x95, 1, // Report Count (4)
            0x81, 0x42, // Input (Data, Variable, Absolute)
        0xc0, // END_COLLECTION (Physical)
    0xc0, // END_COLLECTION (Physical)
};

gamepad_t gamepad;

void request_can_send_now_event(void)
{
    hid_device_request_can_send_now_event(hid_cid);
}

static void send_report_joystick(){
    uint8_t report[] = {
        0xa1, 0x03,
        (gamepad.buttons), (gamepad.buttons >> 8),
        (gamepad.x), (gamepad.x >> 8),
        (gamepad.y), (gamepad.y >> 8),
        (gamepad.z), (gamepad.z >> 8),
        (gamepad.rz), (gamepad.rz >> 8),
        gamepad.gas, gamepad.brake,
        gamepad.hat,
    };
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

static bd_addr_t local_addr;
static bd_addr_t event_addr;
static uint16_t rfcomm_channel_id;
static uint8_t rfcomm_channel_nr;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;


    switch(packet_type)
    {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet))
            {
                case BTSTACK_EVENT_STATE:
                        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                        gap_local_bd_addr(local_addr);
                        printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                        break;
                case GAP_EVENT_DEDICATED_BONDING_COMPLETED:
                    printf("GAP Dedicated Bonding Complete, status 0x%02x\n", packet[2]);
                    break;
                case HCI_EVENT_PIN_CODE_REQUEST:
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                        // ssp: inform about user confirmation request
                        printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", hci_event_user_confirmation_request_get_numeric_value(packet));
                        printf("SSP User Confirmation Auto accept\n");
                        break;
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_decline_connection(rfcomm_channel_id);
                    break;
                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status != ERROR_CODE_SUCCESS) {
                                // outgoing connection failed
                                printf("Connection failed, status 0x%x\n", status);
                                hid_cid = 0;
                                return;
                            }
                            hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            printf("HID Connected\n");
                            hid_device_request_can_send_now_event(hid_cid);
                            gap_discoverable_control(0);
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Disconnected\n");
                            hid_cid = 0;
                            break;
                        case HID_SUBEVENT_CAN_SEND_NOW:  
                            if(hid_cid!=0){ //Solves crash when disconnecting gamepad on android
                              send_report_joystick();
                              hid_device_request_can_send_now_event(hid_cid);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void sync_irq_handler(void)
{
    if (gpio_get_irq_event_mask(BUTTON_PIN) & GPIO_IRQ_EDGE_FALL)
    {
        gpio_acknowledge_irq(BUTTON_PIN, GPIO_IRQ_EDGE_FALL);
        hid_device_disconnect(hid_cid);
        gap_discoverable_control(1);
        printf("Set discoverable\n");
    }
}

static void setup_sync_button(void)
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_add_raw_irq_handler(BUTTON_PIN, &sync_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    gap_discoverable_control(0);
    gap_set_class_of_device(0x2508);
    gap_set_local_name(hid_device_name);
#ifdef ENABLE_BLE
    gap_set_connection_parameters(0x0060, 0x0030, 0x06, 0x06, 0, 0x0048, 2, 0x0030);
#endif

    // L2CAP
    l2cap_init();
    // SDP Server
    sdp_init();
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));

    uint8_t hid_virtual_cable = 0;
    uint8_t hid_remote_wake = 1;
    uint8_t hid_reconnect_initiate = 1;
    uint8_t hid_normally_connectable = 1;

    hid_sdp_record_t hid_params = {
        0x2508,
        0,
        0,
        1, // remote wake
        0,
        1, // normally connectable
        0, // boot device
        host_max_latency, host_min_timeout,
        3200,
        hid_descriptor_gamepad,
        sizeof(hid_descriptor_gamepad),
        hid_device_name
    };

    hid_create_sdp_record(
        hid_service_buffer,
        0x10000,
        &hid_params
    );

    hid_create_sdp_record(hid_service_buffer, sdp_create_service_record_handle(), &hid_params);
    btstack_assert(de_get_len( hid_service_buffer) <= sizeof(hid_service_buffer));
    sdp_register_service(hid_service_buffer);

     // HID Device
    hid_device_init(0, sizeof(hid_descriptor_gamepad), hid_descriptor_gamepad);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    setup_sync_button();

    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */