#!/bin/bash

PROGRAM="../../mini_hypervisor" 

gcc -m64 -ffreestanding -fno-pic -c -o guest.o guest.c

ld -T guest.ld guest.o -o guest.img


$PROGRAM -g guest.img -p 4 --memory 8