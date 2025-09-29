#!/bin/bash

PROGRAM="../../level-C/mini_hypervisor" 

gcc -m64 -ffreestanding -fno-pic -c -o guest1.o guest1.c
gcc -m64 -ffreestanding -fno-pic -c -o guest2.o guest2.c

ld -T guest.ld guest1.o -o guest1.img
ld -T guest.ld guest2.o -o guest2.img


$PROGRAM -g guest2.img guest1.img guest2.img -p 2 --memory 2 --file  ./files/file1.txt ./files/file2.txt