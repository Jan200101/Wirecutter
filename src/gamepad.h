#ifndef GAMEPAD_H
#define GAMEPAD_H

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rz;
    uint8_t gas;
    uint8_t brake;
    uint8_t hat;
    uint16_t buttons;
} gamepad_t;

extern gamepad_t gamepad;

void request_can_send_now_event(void);

#endif