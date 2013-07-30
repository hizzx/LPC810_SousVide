#include "LPC8xx.h"
#include "lpc8xx_gpio.h"

#include "onewire.h"
#include "crc8.h"
#include "myuart.h"
#include "delay.h"

// http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf

/**
 * Assume the bus comprises just one DS18B20 device.
 */

uint64_t ds18b20_rom_read () {

	if ( ! ow_reset() ) {
		return -999;
	}


	// ROM read - only works with one device on the bus
	ow_byte_write (0x33);

	uint64_t addr = 0;

	int i;
	/*
	for (i = 0; i < 64; i++) {
		addr <<= 1;
		addr |= ow_bit_read();
		//delayMicroseconds(100);
	}
	*/
	for (i = 0; i < 8; i++) {
		addr <<= 8;
		addr |= ow_byte_read();
	}

	return addr;

}

int32_t ds18b20_temperature_read () {
	if ( ! ow_reset() ) {
		return -999;
	}

	// Skip ROM command
	ow_byte_write (0x55);

	delayMicroseconds(250);

	// Issue Convert command
	ow_byte_write (0x44);

	delayMicroseconds(250);

	// Poll for conversion complete
	int niter = 1;
	while ( ! ow_bit_read() ) niter++;

	int i;
	for (i = 0; i < niter; i++) {
		MyUARTSendByte(LPC_USART0,'*');
	}

	// Issue command to read scratch pad
	ow_byte_write (0xBE);

	// Read data (up to 9 bytes, but only interested in first two)
	int16_t data=0;
	for (i = 0; i < 16; i++) {
		data >>= 1;
		if (ow_bit_read()) {
			data |= 0x8000;
		}
		delayMicroseconds(100);
	}

	// 16 bit temperature data is interpreted as a signed 16 bit integer.
	// of 12 bit resolution (by default -- the DS18B20 can be configured
	// for lower resolutions). To get °C multiply by (1/16)°C
	// Return temperature * 10;
	return  (data * 10) / 16;

}