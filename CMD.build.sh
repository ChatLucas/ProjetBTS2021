#!/bin/bash
cd programes
gcc -o serveur -l mosquitto `mariadb_config --cflags --libs` serveur.c
