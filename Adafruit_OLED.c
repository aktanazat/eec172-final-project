

#include <string.h>
#include <stdio.h>

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "gpio.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"

#include "uart_if.h"
#include "pinmux.h"

#include "Adafruit_SSD1351.h"
#include "Adafruit_GFX.h"
#include "glcdfont.h"

#define OLED_DC_PIN           GPIO_PIN_6
#define OLED_RST_PIN          GPIO_PIN_4

void writeCommand(unsigned char c) {
  unsigned long ulDummy;

  MAP_SPICSEnable(GSPI_BASE);
  GPIOPinWrite(GPIOA3_BASE, OLED_DC_PIN, 0x00);  // DC low

  MAP_SPIDataPut(GSPI_BASE, c);
  MAP_SPIDataGet(GSPI_BASE, &ulDummy);

  MAP_SPICSDisable(GSPI_BASE);
}

void writeData(unsigned char c) {
    unsigned long ulDummy;

    MAP_SPICSEnable(GSPI_BASE);
    GPIOPinWrite(GPIOA3_BASE, OLED_DC_PIN, OLED_DC_PIN);  // DC high

    MAP_SPIDataPut(GSPI_BASE, c);
    MAP_SPIDataGet(GSPI_BASE, &ulDummy);

    MAP_SPICSDisable(GSPI_BASE);
}

void Adafruit_Init(void){
  volatile unsigned long delay;

  GPIOPinWrite(GPIOA3_BASE, OLED_RST_PIN, 0);	// RESET = RESET_LOW

  for(delay=0; delay<100; delay=delay+1);// delay minimum 100 ns

  GPIOPinWrite(GPIOA3_BASE, OLED_RST_PIN, OLED_RST_PIN);	// RESET = RESET_HIGH

	// Initialization Sequence
  writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
  writeData(0x12);
  writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
  writeData(0xB1);

  writeCommand(SSD1351_CMD_DISPLAYOFF);

  writeCommand(SSD1351_CMD_CLOCKDIV);
  writeData(0xF1);

  writeCommand(SSD1351_CMD_MUXRATIO);
  writeData(127);

  writeCommand(SSD1351_CMD_SETREMAP);
  writeData(0x74);

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(0x00);
  writeData(0x7F);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(0x00);
  writeData(0x7F);

  writeCommand(SSD1351_CMD_STARTLINE);
  if (SSD1351HEIGHT == 96) {
    writeData(96);
  } else {
    writeData(0);
  }


  writeCommand(SSD1351_CMD_DISPLAYOFFSET);
  writeData(0x0);

  writeCommand(SSD1351_CMD_SETGPIO);
  writeData(0x00);

  writeCommand(SSD1351_CMD_FUNCTIONSELECT);
  writeData(0x01);

  writeCommand(SSD1351_CMD_PRECHARGE);
  writeData(0x32);

  writeCommand(SSD1351_CMD_VCOMH);
  writeData(0x05);

  writeCommand(SSD1351_CMD_NORMALDISPLAY);

  writeCommand(SSD1351_CMD_CONTRASTABC);
  writeData(0xFF);
  writeData(0xFF);
  writeData(0xFF);

  writeCommand(SSD1351_CMD_CONTRASTMASTER);
  writeData(0x0F);

  writeCommand(SSD1351_CMD_SETVSL);
  writeData(0xA0);
  writeData(0xB5);
  writeData(0x55);

  writeCommand(SSD1351_CMD_PRECHARGE2);
  writeData(0x01);

  writeCommand(SSD1351_CMD_DISPLAYON);
}

void goTo(int x, int y) {
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;

  // set x and y coordinate
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(SSD1351WIDTH-1);

  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(SSD1351HEIGHT-1);

  writeCommand(SSD1351_CMD_WRITERAM);
}

unsigned int Color565(unsigned char r, unsigned char g, unsigned char b) {
  unsigned int c;
  c = r >> 3;
  c <<= 6;
  c |= g >> 2;
  c <<= 5;
  c |= b >> 3;

  return c;
}

void fillScreen(unsigned int fillcolor) {
  fillRect(0, 0, SSD1351WIDTH, SSD1351HEIGHT, fillcolor);
}

void fillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int fillcolor)
{
  unsigned int i;

  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
	return;

  if (y+h > SSD1351HEIGHT)
  {
    h = SSD1351HEIGHT - y - 1;
  }

  if (x+w > SSD1351WIDTH)
  {
    w = SSD1351WIDTH - x - 1;
  }

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w*h; i++) {
    writeData(fillcolor >> 8);
    writeData(fillcolor);
  }
}

void drawFastVLine(int x, int y, int h, unsigned int color) {
  unsigned int i;

  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
	return;

  if (y+h > SSD1351HEIGHT)
  {
    h = SSD1351HEIGHT - y - 1;
  }

  if (h < 0) return;

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < h; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}



void drawFastHLine(int x, int y, int w, unsigned int color) {
  unsigned int i;

  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
	return;

  if (x+w > SSD1351WIDTH)
  {
    w = SSD1351WIDTH - x - 1;
  }

  if (w < 0) return;

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}




void drawPixel(int x, int y, unsigned int color)
{
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if ((x < 0) || (y < 0)) return;

  goTo(x, y);

  writeData(color >> 8);
  writeData(color);
}


void  invert(char v) {
   if (v) {
     writeCommand(SSD1351_CMD_INVERTDISPLAY);
   } else {
     	writeCommand(SSD1351_CMD_NORMALDISPLAY);
   }
 }


