
path=C:\devkitPro\devkitARM\bin
@ECHO OFF

arm-none-eabi-gcc -Wall -O3 -mthumb -mthumb-interwork -specs=gba.specs main.c -o main.elf -lm
arm-none-eabi-objcopy -O binary main.elf main.gba

del main.elf
pause