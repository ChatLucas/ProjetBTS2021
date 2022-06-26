#include <stdio.h>
#include <stdlib.h>
#include <bcm2835.h>

// Blinks on RPi Plug P1 pin 11 (which is GPIO pin 17)

#define pin RPI_V2_GPIO_P1_37
int main(int argc, char **argv)
{
     int high,low,BCM2835_GPIO_FSEL_OUTP;

    if (!bcm2835_init()) {
        printf ("erreur");
        return 1;
    }

    bcm2835_gpio_fsel(pin,BCM2835_GPIO_FSEL_OUTP);

    while (1)
    {
        bcm2835_gpio_write(pin,high);

        bcm2835_delay(500);

        bcm2835_gpio_write(pin,low);

        bcm2835_delay(500);
    }
    bcm2835_close();
    return 0;
}


