/*********************************************************************
SSD1306 I2C Library for Raspberry Pi.
Based on Adafruit SSD1306 Arduino library. Some functions came from Adafruit GFX lib

Modified by Ilia Penev
Tested on Raspberry Pi 2 with 0.96 Yellow/Blue OLED
*********************************************************************/

/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

#include <stdio.h>
#include <string.h>

#include "ssd1306_i2c.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "oled_fonts.h"

#define I2C_DEV "/dev/i2c-1"

#define true 1
#define false 0

#define rotation 0

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

int cursor_y = 0;
int cursor_x = 0;

// the memory buffer for the LCD. Displays Adafruit logo
int buffer[SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x80,
	0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x80, 0x80, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xF8, 0xE0,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x00, 0xFF,
#if (SSD1306_LCDHEIGHT * SSD1306_LCDWIDTH > 96*16)
	0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
	0x80, 0x80, 0x00, 0x00,
	0x80, 0xFF, 0xFF, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x80, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x8C, 0x8E, 0x84,
	0x00, 0x00, 0x80, 0xF8,
	0xF8, 0xF8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
	0xE0, 0xE0, 0xC0, 0x80,
	0x00, 0xE0, 0xFC, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE,
	0xFF, 0xC7, 0x01, 0x01,
	0x01, 0x01, 0x83, 0xFF, 0xFF, 0x00, 0x00, 0x7C, 0xFE, 0xC7, 0x01, 0x01,
	0x01, 0x01, 0x83, 0xFF,
	0xFF, 0xFF, 0x00, 0x38, 0xFE, 0xC7, 0x83, 0x01, 0x01, 0x01, 0x83, 0xC7,
	0xFF, 0xFF, 0x00, 0x00,
	0x01, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x07, 0x01, 0x01, 0x01,
	0x00, 0x00, 0x7F, 0xFF,
	0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x01, 0xFF,
	0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x03, 0x0F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xE7, 0xC7, 0xC7, 0x8F,
	0x8F, 0x9F, 0xBF, 0xFF, 0xFF, 0xC3, 0xC0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFC, 0xFC, 0xFC,
	0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xF8, 0xF8, 0xF0, 0xF0, 0xE0, 0xC0, 0x00,
	0x01, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x01, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03,
	0x03, 0x03, 0x01, 0x01,
	0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01,
	0x03, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x01,
	0x00, 0x00, 0x00, 0x03,
	0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
#if (SSD1306_LCDHEIGHT == 64)
	0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF9, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x3F, 0x1F, 0x0F,
	0x87, 0xC7, 0xF7, 0xFF, 0xFF, 0x1F, 0x1F, 0x3D, 0xFC, 0xF8, 0xF8, 0xF8,
	0xF8, 0x7C, 0x7D, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x0F, 0x07, 0x00,
	0x30, 0x30, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xE0, 0xC0, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0xC0, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x7F, 0x7F, 0x3F, 0x1F,
	0x0F, 0x07, 0x1F, 0x7F, 0xFF, 0xFF, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFE, 0xF8, 0xE0,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFE, 0x00, 0x00,
	0x00, 0xFC, 0xFE, 0xFC, 0x0C, 0x06, 0x06, 0x0E, 0xFC, 0xF8, 0x00, 0x00,
	0xF0, 0xF8, 0x1C, 0x0E,
	0x06, 0x06, 0x06, 0x0C, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFE, 0xFE, 0x00,
	0x00, 0x00, 0x00, 0xFC,
	0xFE, 0xFC, 0x00, 0x18, 0x3C, 0x7E, 0x66, 0xE6, 0xCE, 0x84, 0x00, 0x00,
	0x06, 0xFF, 0xFF, 0x06,
	0x06, 0xFC, 0xFE, 0xFC, 0x0C, 0x06, 0x06, 0x06, 0x00, 0x00, 0xFE, 0xFE,
	0x00, 0x00, 0xC0, 0xF8,
	0xFC, 0x4E, 0x46, 0x46, 0x46, 0x4E, 0x7C, 0x78, 0x40, 0x18, 0x3C, 0x76,
	0xE6, 0xCE, 0xCC, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x0F, 0x1F, 0x1F, 0x3F, 0x3F, 0x3F,
	0x3F, 0x1F, 0x0F, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x0F, 0x00, 0x00,
	0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00,
	0x03, 0x07, 0x0E, 0x0C,
	0x18, 0x18, 0x0C, 0x06, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x0F, 0x0E,
	0x0C, 0x18, 0x0C, 0x0F,
	0x07, 0x01, 0x00, 0x04, 0x0E, 0x0C, 0x18, 0x0C, 0x0F, 0x07, 0x00, 0x00,
	0x00, 0x0F, 0x0F, 0x00,
	0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F,
	0x00, 0x00, 0x00, 0x07,
	0x07, 0x0C, 0x0C, 0x18, 0x1C, 0x0C, 0x06, 0x06, 0x00, 0x04, 0x0E, 0x0C,
	0x18, 0x0C, 0x0F, 0x07,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
#endif
#endif
};

int _vccstate;
int i2cd = -1;

#define ssd1306_swap(a, b) { int t = a; a = b; b = t; }

// the most basic function, set a single pixel
void ssd1306_drawPixel(int x, int y, unsigned int color)
{
	if ((x < 0) || (x >= WIDTH) || (y < 0) || (y >= HEIGHT))
		return;

	// check rotation, move pixel around if necessary
	switch (rotation) {
	case 1:
		ssd1306_swap(x, y);
		x = WIDTH - x - 1;
		break;
	case 2:
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		break;
	case 3:
		ssd1306_swap(x, y);
		y = HEIGHT - y - 1;
		break;
	}

	// x is which column
	switch (color) {
	case WHITE:
		buffer[x + (y / 8) * SSD1306_LCDWIDTH] |= (1 << (y & 7));
		break;
	case BLACK:
		buffer[x + (y / 8) * SSD1306_LCDWIDTH] &= ~(1 << (y & 7));
		break;
	case INVERSE:
		buffer[x + (y / 8) * SSD1306_LCDWIDTH] ^= (1 << (y & 7));
		break;
	}
}

// Init SSD1306. Returns 1 when the display responds, 0 when it is absent.
int ssd1306_begin(unsigned int vccstate, unsigned int i2caddr)
{
	int ok = 0;

	// I2C Init

	_vccstate = vccstate;

	i2cd = open(I2C_DEV, O_RDWR);
	if (i2cd < 0) {
		fprintf(stderr, "ssd1306_i2c : Unable to open %s: %s\n", I2C_DEV, strerror(errno));
		return 0;
	}
	if (ioctl(i2cd, I2C_SLAVE, i2caddr) < 0) {
		fprintf(stderr, "ssd1306_i2c : Unable to set I2C slave 0x%X: %s\n", i2caddr, strerror(errno));
		close(i2cd);
		i2cd = -1;
		return 0;
	}
	// Probe the display by writing one command. If nothing ACKs on the bus,
	// the write fails with ENXIO and the display is treated as "not found".
	{
		unsigned char probe[2] = {0x00, SSD1306_DISPLAYON};
		if (write(i2cd, probe, 2) < 0) {
			fprintf(stderr, "ssd1306_i2c : No SSD1306 display found at 0x%X: %s\n", i2caddr, strerror(errno));
			close(i2cd);
			i2cd = -1;
			return 0;
		}
	}
	ok = 1;
	// Init sequence
	ssd1306_command(SSD1306_DISPLAYOFF);	// 0xAE
	ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);	// 0xD5
	ssd1306_command(0x80);	// the suggested ratio 0x80

	ssd1306_command(SSD1306_SETMULTIPLEX);	// 0xA8
	ssd1306_command(SSD1306_LCDHEIGHT - 1);

	ssd1306_command(SSD1306_SETDISPLAYOFFSET);	// 0xD3
	ssd1306_command(0x0);	// no offset
	ssd1306_command(SSD1306_SETSTARTLINE | 0x0);	// line #0
	ssd1306_command(SSD1306_CHARGEPUMP);	// 0x8D
	if (vccstate == SSD1306_EXTERNALVCC) {
		ssd1306_command(0x10);
	} else {
		ssd1306_command(0x14);
	}
	ssd1306_command(SSD1306_MEMORYMODE);	// 0x20
	ssd1306_command(0x00);	// 0x0 act like ks0108
	ssd1306_command(SSD1306_SEGREMAP | 0x1);
	ssd1306_command(SSD1306_COMSCANDEC);

#if defined SSD1306_128_32
	ssd1306_command(SSD1306_SETCOMPINS);	// 0xDA
	ssd1306_command(0x02);
	ssd1306_command(SSD1306_SETCONTRAST);	// 0x81
	ssd1306_command(0x8F);

#elif defined SSD1306_128_64
	ssd1306_command(SSD1306_SETCOMPINS);	// 0xDA
	ssd1306_command(0x12);
	ssd1306_command(SSD1306_SETCONTRAST);	// 0x81
	if (vccstate == SSD1306_EXTERNALVCC) {
		ssd1306_command(0x9F);
	} else {
		ssd1306_command(0xCF);
	}

#elif defined SSD1306_96_16
	ssd1306_command(SSD1306_SETCOMPINS);	// 0xDA
	ssd1306_command(0x2);	// ada x12
	ssd1306_command(SSD1306_SETCONTRAST);	// 0x81
	if (vccstate == SSD1306_EXTERNALVCC) {
		ssd1306_command(0x10);
	} else {
		ssd1306_command(0xAF);
	}

#endif
	ssd1306_command(SSD1306_SETPRECHARGE);	// 0xd9
	if (vccstate == SSD1306_EXTERNALVCC) {
		ssd1306_command(0x22);
	} else {
		ssd1306_command(0xF1);
	}
	ssd1306_command(SSD1306_SETVCOMDETECT);	// 0xDB
	ssd1306_command(0x40);
	ssd1306_command(SSD1306_DISPLAYALLON_RESUME);	// 0xA4
	ssd1306_command(SSD1306_NORMALDISPLAY);	// 0xA6

	ssd1306_command(SSD1306_DEACTIVATE_SCROLL);

	ssd1306_command(SSD1306_DISPLAYON);	// --turn on oled panel

	return ok;
}

void ssd1306_invertDisplay(unsigned int i)
{
	if (i) {
		ssd1306_command(SSD1306_INVERTDISPLAY);
	} else {
		ssd1306_command(SSD1306_NORMALDISPLAY);
	}
}

void ssd1306_command(unsigned int c)
{
	// I2C: control byte 0x00 (Co = 0, D/C = 0) followed by the command
	unsigned char data[2];
	data[0] = 0x00;
	data[1] = (unsigned char)c;
	if (i2cd >= 0)
		write(i2cd, data, 2);
}

void ssd1306_display(void)
{
	ssd1306_command(SSD1306_COLUMNADDR);
	ssd1306_command(0);	// Column start address (0 = reset)
	ssd1306_command(SSD1306_LCDWIDTH - 1);	// Column end address (127 
	// = reset)

	ssd1306_command(SSD1306_PAGEADDR);
	ssd1306_command(0);	// Page start address (0 = reset)
#if SSD1306_LCDHEIGHT == 64
	ssd1306_command(7);	// Page end address
#endif
#if SSD1306_LCDHEIGHT == 32
	ssd1306_command(3);	// Page end address
#endif
#if SSD1306_LCDHEIGHT == 16
	ssd1306_command(1);	// Page end address
#endif

	// I2C: control byte 0x40 (Co = 0, D/C = 1) followed by frame-buffer data.
	// The buffer is split into chunks to speed up the transfer.
	if (i2cd >= 0) {
		int total = SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8;
		int offset = 0;
		unsigned char data[33];
		data[0] = 0x40;
		while (offset < total) {
			int chunk = total - offset;
			int i;
			if (chunk > 32)
				chunk = 32;
			for (i = 0; i < chunk; i++)
				data[i + 1] = (unsigned char)buffer[offset + i];
			write(i2cd, data, chunk + 1);
			offset += chunk;
		}
	}
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// ssd1306_scrollright(0x00, 0x0F)
void ssd1306_startscrollright(unsigned int start, unsigned int stop)
{
	ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
	ssd1306_command(0X00);
	ssd1306_command(start);
	ssd1306_command(0X00);
	ssd1306_command(stop);
	ssd1306_command(0X00);
	ssd1306_command(0XFF);
	ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// ssd1306_scrollright(0x00, 0x0F)
void ssd1306_startscrollleft(unsigned int start, unsigned int stop)
{
	ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
	ssd1306_command(0X00);
	ssd1306_command(start);
	ssd1306_command(0X00);
	ssd1306_command(stop);
	ssd1306_command(0X00);
	ssd1306_command(0XFF);
	ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// ssd1306_scrollright(0x00, 0x0F)
void ssd1306_startscrolldiagright(unsigned int start, unsigned int stop)
{
	ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
	ssd1306_command(0X00);
	ssd1306_command(SSD1306_LCDHEIGHT);
	ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
	ssd1306_command(0X00);
	ssd1306_command(start);
	ssd1306_command(0X00);
	ssd1306_command(stop);
	ssd1306_command(0X01);
	ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// ssd1306_scrollright(0x00, 0x0F)
void ssd1306_startscrolldiagleft(unsigned int start, unsigned int stop)
{
	ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
	ssd1306_command(0X00);
	ssd1306_command(SSD1306_LCDHEIGHT);
	ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
	ssd1306_command(0X00);
	ssd1306_command(start);
	ssd1306_command(0X00);
	ssd1306_command(stop);
	ssd1306_command(0X01);
	ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void ssd1306_stopscroll(void)
{
	ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void ssd1306_dim(unsigned int dim)
{
	unsigned int contrast;

	if (dim) {
		contrast = 0;	// Dimmed display
	} else {
		if (_vccstate == SSD1306_EXTERNALVCC) {
			contrast = 0x9F;
		} else {
			contrast = 0xCF;
		}
	}
	// the range of contrast to too small to be really useful
	// it is useful to dim the display
	ssd1306_command(SSD1306_SETCONTRAST);
	ssd1306_command(contrast);
}

// clear everything
void ssd1306_clearDisplay(void)
{
	memset(buffer, 0,
	       (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8) * sizeof(int));
	cursor_y = 0;
	cursor_x = 0;
}

void ssd1306_drawFastHLineInternal(int x, int y, int w, unsigned int color)
{
	// Do bounds/limit checks
	if (y < 0 || y >= HEIGHT) {
		return;
	}
	// make sure we don't try to draw below 0
	if (x < 0) {
		w += x;
		x = 0;
	}
	// make sure we don't go off the edge of the display
	if ((x + w) > WIDTH) {
		w = (WIDTH - x);
	}
	// if our width is now negative, punt
	if (w <= 0) {
		return;
	}
	// set up the pointer for movement through the buffer
	unsigned int *pBuf = buffer;
	// adjust the buffer pointer for the current row
	pBuf += ((y / 8) * SSD1306_LCDWIDTH);
	// and offset x columns in
	pBuf += x;

	unsigned int mask = 1 << (y & 7);

	switch (color) {
	case WHITE:
		while (w--) {
			*pBuf++ |= mask;
		};
		break;
	case BLACK:
		mask = ~mask;
		while (w--) {
			*pBuf++ &= mask;
		};
		break;
	case INVERSE:
		while (w--) {
			*pBuf++ ^= mask;
		};
		break;
	}
}

void ssd1306_drawFastVLineInternal(int x, int __y, int __h, unsigned int color)
{

	// do nothing if we're off the left or right side of the screen
	if (x < 0 || x >= WIDTH) {
		return;
	}
	// make sure we don't try to draw below 0
	if (__y < 0) {
		// __y is negative, this will subtract enough from __h to account
		// for __y being 0
		__h += __y;
		__y = 0;

	}
	// make sure we don't go past the height of the display
	if ((__y + __h) > HEIGHT) {
		__h = (HEIGHT - __y);
	}
	// if our height is now negative, punt
	if (__h <= 0) {
		return;
	}
	// this display doesn't need ints for coordinates, use local byte
	// registers for faster juggling
	unsigned int y = __y;
	unsigned int h = __h;

	// set up the pointer for fast movement through the buffer
	unsigned int *pBuf = buffer;
	// adjust the buffer pointer for the current row
	pBuf += ((y / 8) * SSD1306_LCDWIDTH);
	// and offset x columns in
	pBuf += x;

	// do the first partial byte, if necessary - this requires some
	// masking
	unsigned int mod = (y & 7);
	if (mod) {
		// mask off the high n bits we want to set
		mod = 8 - mod;

		// note - lookup table results in a nearly 10% performance
		// improvement in fill* functions
		// register unsigned int mask = ~(0xFF >> (mod));
		static unsigned int premask[8] =
		    { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
		unsigned int mask = premask[mod];

		// adjust the mask if we're not going to reach the end of this
		// byte
		if (h < mod) {
			mask &= (0XFF >> (mod - h));
		}

		switch (color) {
		case WHITE:
			*pBuf |= mask;
			break;
		case BLACK:
			*pBuf &= ~mask;
			break;
		case INVERSE:
			*pBuf ^= mask;
			break;
		}

		// fast exit if we're done here!
		if (h < mod) {
			return;
		}

		h -= mod;

		pBuf += SSD1306_LCDWIDTH;
	}
	// write solid bytes while we can - effectively doing 8 rows at a time
	if (h >= 8) {
		if (color == INVERSE) {	// separate copy of the code so we don't
			// impact performance of the black/white
			// write version with an extra comparison
			// per loop
			do {
				*pBuf = ~(*pBuf);

				// adjust the buffer forward 8 rows worth of data
				pBuf += SSD1306_LCDWIDTH;

				// adjust h & y (there's got to be a faster way for me to
				// do this, but this should still help a fair bit for now)
				h -= 8;
			}
			while (h >= 8);
		} else {
			// store a local value to work with
			register unsigned int val = (color == WHITE) ? 255 : 0;

			do {
				// write our value in
				*pBuf = val;

				// adjust the buffer forward 8 rows worth of data
				pBuf += SSD1306_LCDWIDTH;

				// adjust h & y (there's got to be a faster way for me to
				// do this, but this should still help a fair bit for now)
				h -= 8;
			}
			while (h >= 8);
		}
	}
	// now do the final partial byte, if necessary
	if (h) {
		mod = h & 7;
		// this time we want to mask the low bits of the byte, vs the high 
		// bits we did above
		// register unsigned int mask = (1 << mod) - 1;
		// note - lookup table results in a nearly 10% performance
		// improvement in fill* functions
		static unsigned int postmask[8] =
		    { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
		unsigned int mask = postmask[mod];
		switch (color) {
		case WHITE:
			*pBuf |= mask;
			break;
		case BLACK:
			*pBuf &= ~mask;
			break;
		case INVERSE:
			*pBuf ^= mask;
			break;
		}
	}
}

void ssd1306_drawFastHLine(int x, int y, int w, unsigned int color)
{
	unsigned int bSwap = false;
	switch (rotation) {
	case 0:
		// 0 degree rotation, do nothing
		break;
	case 1:
		// 90 degree rotation, swap x & y for rotation, then invert x
		bSwap = true;
		ssd1306_swap(x, y);
		x = WIDTH - x - 1;
		break;
	case 2:
		// 180 degree rotation, invert x and y - then shift y around for
		// height.
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		x -= (w - 1);
		break;
	case 3:
		// 270 degree rotation, swap x & y for rotation, then invert y and 
		// adjust y for w (not to become h)
		bSwap = true;
		ssd1306_swap(x, y);
		y = HEIGHT - y - 1;
		y -= (w - 1);
		break;
	}

	if (bSwap) {
		ssd1306_drawFastVLineInternal(x, y, w, color);
	} else {
		ssd1306_drawFastHLineInternal(x, y, w, color);
	}
}

void ssd1306_drawFastVLine(int x, int y, int h, unsigned int color)
{
	unsigned int bSwap = false;
	switch (rotation) {
	case 0:
		break;
	case 1:
		// 90 degree rotation, swap x & y for rotation, then invert x and
		// adjust x for h (now to become w)
		bSwap = true;
		ssd1306_swap(x, y);
		x = WIDTH - x - 1;
		x -= (h - 1);
		break;
	case 2:
		// 180 degree rotation, invert x and y - then shift y around for
		// height.
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		y -= (h - 1);
		break;
	case 3:
		// 270 degree rotation, swap x & y for rotation, then invert y
		bSwap = true;
		ssd1306_swap(x, y);
		y = HEIGHT - y - 1;
		break;
	}

	if (bSwap) {
		ssd1306_drawFastHLineInternal(x, y, h, color);
	} else {
		ssd1306_drawFastVLineInternal(x, y, h, color);
	}
}

void ssd1306_fillRect(int x, int y, int w, int h, int fillcolor)
{

	// Bounds check
	if ((x >= WIDTH) || (y >= HEIGHT))
		return;

	// Y bounds check
	if (y + h > HEIGHT) {
		h = HEIGHT - y - 1;
	}
	// X bounds check
	if (x + w > WIDTH) {
		w = WIDTH - x - 1;
	}

	switch (rotation) {
	case 1:
		ssd1306_swap(x, y);
		x = WIDTH - x - 1;
		break;
	case 2:
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		break;
	case 3:
		ssd1306_swap(x, y);
		y = HEIGHT - y - 1;
		break;
	}
	int i;
	for (i = 0; i < h; i++)
		ssd1306_drawFastHLine(x, y + i, w, fillcolor);
}

int textsize = 1;
int wrap = 1;

void ssd1306_setTextSize(int s)
{
	textsize = (s > 0) ? s : 1;
}

void ssd1306_write(int c)
{

	if (c == '\n') {
		cursor_y += textsize * 8;
		cursor_x = 0;
	} else if (c == '\r') {
		// skip em
	} else {
		ssd1306_drawChar(cursor_x, cursor_y, c, WHITE, textsize);
		cursor_x += textsize * 6;
		if (wrap && (cursor_x > (WIDTH - textsize * 6))) {
			cursor_y += textsize * 8;
			cursor_x = 0;
		}
	}
}

void ssd1306_drawString(char *str)
{
	int i, end;
	end = strlen(str);
	for (i = 0; i < end; i++)
		ssd1306_write(str[i]);
}

void ssd1306_drawText(int x, int y, char *str)
{
	int i, end;
	end = strlen(str);
	int point_x = x;
	int point_y = y;
	for (i = 0; i < end; i++)
	{
		if (str[i] == '\n')
		{
			point_y += textsize * 8;
			point_x = 0;
		}
		else if (str[i] == '\r')
		{
			// skip em
		}
		else
		{
			ssd1306_drawChar(point_x, point_y, str[i], WHITE, textsize);
			point_x += textsize * 6;
			if (wrap && (point_x > (WIDTH - textsize * 6)))
			{
				point_y += textsize * 8;
				point_x = 0;
			}
		}
	}
}

// Draw a character
void ssd1306_drawChar(int x, int y, unsigned char c, int color, int size)
{

	if ((x >= WIDTH) ||	// Clip right
	    (y >= HEIGHT) ||	// Clip bottom
	    ((x + 6 * size - 1) < 0) ||	// Clip left
	    ((y + 8 * size - 1) < 0))	// Clip top
		return;
	int i;
	int j;
	for (i = 0; i < 6; i++) {
		int line;
		if (i == 5)
			line = 0x0;
		else
			line = pgm_read_byte(font + (c * 5) + i);
		for (j = 0; j < 8; j++) {
			if (line & 0x1) {
				if (size == 1)	// default size
					ssd1306_drawPixel(x + i, y + j, color);
				else {	// big size
					ssd1306_fillRect(x + (i * size),
							 y + (j * size), size,
							 size, color);
				}
			}
			line >>= 1;
		}
	}
}
