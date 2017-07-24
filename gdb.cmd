target remote localhost:3333
set arm abi APCS
monitor reset halt
file ./build/ch.elf
load
monitor reset
quit
