CC = xtensa-lx106-elf-gcc
CFLAGS = -I. -mlongcalls -I/home/victor/Coding/sst/include -I/home/victor/Documents/esp-dev/esp-open-sdk/ESP8266_NONOS_SDK_V1.5.4_16_05_20/driver_lib/include/driver
LDLIBS = -nostdlib -Wl,--start-group -lmain -lnet80211 -lwpa -llwip -lpp -lphy -Wl,--end-group -lgcc -ldriver -L/home/victor/Documents/esp-dev/esp-open-sdk/ESP8266_NONOS_SDK_V1.5.4_16_05_20/lib
LDFLAGS = -Teagle.app.v6.ld

blinky-0x00000.bin: blinky
	esptool.py elf2image $^

blinky: blinky.o sst.o

blinky.o: blinky.c

sst.o: ../src/sst.c
	$(CC) $(CFLAGS) -c ../src/sst.c

flash: blinky-0x00000.bin
	esptool.py write_flash 0 blinky-0x00000.bin 0x40000 blinky-0x40000.bin
	putty -load nodeMCU

serial:
	putty -load nodeMCU

clean:
	rm -f blinky blinky.o sst.o blinky-0x00000.bin blinky-0x40000.bin
