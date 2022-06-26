#include <stdio.h>
#include <stdlib.h>
#include <bcm2835.h>

// Blinks on RPi Plug P1 pin 11 (which is GPIO pin 17)

#define PIN RPI_V2_GPIO_P1_37
int main(int argc, char **argv)
{
    int type, nbreFois;
    int i;

    // tester le nombre d'arguments
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <type_sonnerie>\n\t0 : activé\n\t1 : désactivé\n\t2 : alarme\t3 : arrêt forcé\n",argv[0]);
        return -1;
    }

    if (!bcm2835_init())
    {
        printf ("erreur");
        return 1;
    }

    bcm2835_gpio_fsel(PIN,BCM2835_GPIO_FSEL_OUTP);

    type = atoi(argv[1]);
    switch (type)
    {
    case 0 :
        bcm2835_gpio_write(PIN,1);
        bcm2835_delay(100);
        bcm2835_gpio_write(PIN,0);
        break;
    case 1 :
        for (i =0; i<2; i++ )
        {
            bcm2835_gpio_write(PIN,1);
            bcm2835_delay(100);
            bcm2835_gpio_write(PIN,0);
            bcm2835_delay(100);
        }
        break;
    case 2 :
        for (i =0; i<10; i++ )
        {
            bcm2835_gpio_write(PIN,1);
            bcm2835_delay(50);
            bcm2835_gpio_write(PIN,0);
            bcm2835_delay(50);
        }
        break;
    case 3 :
        bcm2835_gpio_write(PIN,0);
        break;
    }


    bcm2835_gpio_write(PIN,0);
    bcm2835_close();
    return 0;
}


