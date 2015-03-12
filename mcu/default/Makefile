###############################################################################
# Makefile for the project CheapStat_v2
###############################################################################

## General Flags
PROJECT = CheapStat_v2
MCU = atxmega32a4
TARGET = CheapStat_v2.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -Os -std=gnu99 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=CheapStat_v2.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Libraries
LIBS = -lm 

## Objects that must be built in order to link
OBJECTS = CheapStat_v2.o adc_driver.o dac_driver.o lcd.o spi_driver.o usart_driver.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) CheapStat_v2.hex CheapStat_v2.eep CheapStat_v2.lss size

## Compile
CheapStat_v2.o: ../CheapStat_v2.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

adc_driver.o: ../adc_driver.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

dac_driver.o: ../dac_driver.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

lcd.o: ../lcd.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

spi_driver.o: ../spi_driver.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

usart_driver.o: ../usart_driver.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) CheapStat_v2.elf dep/* CheapStat_v2.hex CheapStat_v2.eep CheapStat_v2.lss CheapStat_v2.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

