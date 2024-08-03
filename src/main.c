#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "bsp/board_api.h"
#include "tusb.h"

const uint LED_PIN = 25;

int btstack_main(int argc, const char * argv[]);

int main()
{
    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    board_init();
    tuh_init(BOARD_TUH_RHPORT);
    btstack_main(0, NULL);

    printf("Init done\n");

    while(1)
    {
        tuh_task();
    }
}
