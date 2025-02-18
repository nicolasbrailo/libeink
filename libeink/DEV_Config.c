/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2019-07-31
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of theex Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "DEV_Config.h"

#if USE_LGPIO_LIB
int GPIO_Handle;
int SPI_Handle;
#endif

/**
 * GPIO
 **/
int EPD_RST_PIN;
int EPD_DC_PIN;
int EPD_CS_PIN;
int EPD_BUSY_PIN;
int EPD_PWR_PIN;
int EPD_MOSI_PIN;
int EPD_SCLK_PIN;

/**
 * GPIO read and write
 **/
void DEV_Digital_Write(UWORD Pin, UBYTE Value) {
  lgGpioWrite(GPIO_Handle, Pin, Value);
}

UBYTE DEV_Digital_Read(UWORD Pin) {
  UBYTE Read_value = 0;
#ifdef RPI
#ifdef USE_BCM2835_LIB
  Read_value = bcm2835_gpio_lev(Pin);
#elif USE_WIRINGPI_LIB
  Read_value = digitalRead(Pin);
#elif USE_LGPIO_LIB
  Read_value = lgGpioRead(GPIO_Handle, Pin);
#elif USE_DEV_LIB
  Read_value = GPIOD_Read(Pin);
#endif
#endif

#ifdef JETSON
#ifdef USE_DEV_LIB
  Read_value = SYSFS_GPIO_Read(Pin);
#elif USE_HARDWARE_LIB
  Debug("not support");
#endif
#endif
  return Read_value;
}

/**
 * SPI
 **/
void DEV_SPI_WriteByte(uint8_t Value) {
#ifdef RPI
#ifdef USE_BCM2835_LIB
  bcm2835_spi_transfer(Value);
#elif USE_WIRINGPI_LIB
  wiringPiSPIDataRW(0, &Value, 1);
#elif USE_LGPIO_LIB
  lgSpiWrite(SPI_Handle, (char *)&Value, 1);
#elif USE_DEV_LIB
  DEV_HARDWARE_SPI_TransferByte(Value);
#endif
#endif

#ifdef JETSON
#ifdef USE_DEV_LIB
  SYSFS_software_spi_transfer(Value);
#elif USE_HARDWARE_LIB
  Debug("not support");
#endif
#endif
}

void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len) {
#ifdef RPI
#ifdef USE_BCM2835_LIB
  char rData[Len];
  bcm2835_spi_transfernb((char *)pData, rData, Len);
#elif USE_WIRINGPI_LIB
  wiringPiSPIDataRW(0, pData, Len);
#elif USE_LGPIO_LIB
  lgSpiWrite(SPI_Handle, (char *)pData, Len);
#elif USE_DEV_LIB
  DEV_HARDWARE_SPI_Transfer(pData, Len);
#endif
#endif

#ifdef JETSON
#ifdef USE_DEV_LIB
  // JETSON nano waits for hardware SPI
  //  Debug("not support");
  uint32_t i;
  for (i = 0; i < Len; i++)
    SYSFS_software_spi_transfer(pData[i]);
#elif USE_HARDWARE_LIB
  Debug("not support");
#endif
#endif
}

/**
 * GPIO Mode
 **/
void DEV_GPIO_Mode(UWORD Pin, UWORD Mode) {
#ifdef RPI
#ifdef USE_BCM2835_LIB
  if (Mode == 0 || Mode == BCM2835_GPIO_FSEL_INPT) {
    bcm2835_gpio_fsel(Pin, BCM2835_GPIO_FSEL_INPT);
  } else {
    bcm2835_gpio_fsel(Pin, BCM2835_GPIO_FSEL_OUTP);
  }
#elif USE_WIRINGPI_LIB
  if (Mode == 0 || Mode == INPUT) {
    pinMode(Pin, INPUT);
    pullUpDnControl(Pin, PUD_UP);
  } else {
    pinMode(Pin, OUTPUT);
    // Debug (" %d OUT \r\n",Pin);
  }
#elif USE_LGPIO_LIB
  if (Mode == 0 || Mode == LG_SET_INPUT) {
    lgGpioClaimInput(GPIO_Handle, LFLAGS, Pin);
    // printf("IN Pin = %d\r\n",Pin);
  } else {
    lgGpioClaimOutput(GPIO_Handle, LFLAGS, Pin, LG_LOW);
    // printf("OUT Pin = %d\r\n",Pin);
  }
#elif USE_DEV_LIB
  if (Mode == 0 || Mode == GPIOD_IN) {
    GPIOD_Direction(Pin, GPIOD_IN);
    // Debug("IN Pin = %d\r\n",Pin);
  } else {
    GPIOD_Direction(Pin, GPIOD_OUT);
    // Debug("OUT Pin = %d\r\n",Pin);
  }
#endif
#endif

#ifdef JETSON
#ifdef USE_DEV_LIB
  SYSFS_GPIO_Export(Pin);
  SYSFS_GPIO_Direction(Pin, Mode);
#elif USE_HARDWARE_LIB
  Debug("not support");
#endif
#endif
}

/**
 * delay x ms
 **/
void DEV_Delay_ms(size_t xms) {
  struct timespec ts, rem;

  if (xms > 0) {
    ts.tv_sec = xms / 1000;
    ts.tv_nsec = (xms - (1000 * ts.tv_sec)) * 1E6;

    while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem))
      ts = rem;
  }
}

void DEV_SPI_SendnData(UBYTE *Reg) {
  UDOUBLE size;
  size = sizeof(Reg);
  for (UDOUBLE i = 0; i < size; i++) {
    DEV_SPI_SendData(Reg[i]);
  }
}

void DEV_SPI_SendData(UBYTE Reg) {
  UBYTE i, j = Reg;
  DEV_GPIO_Mode(EPD_MOSI_PIN, 1);
  DEV_Digital_Write(EPD_CS_PIN, 0);
  for (i = 0; i < 8; i++) {
    DEV_Digital_Write(EPD_SCLK_PIN, 0);
    if (j & 0x80) {
      DEV_Digital_Write(EPD_MOSI_PIN, 1);
    } else {
      DEV_Digital_Write(EPD_MOSI_PIN, 0);
    }

    DEV_Digital_Write(EPD_SCLK_PIN, 1);
    j = j << 1;
  }
  DEV_Digital_Write(EPD_SCLK_PIN, 0);
  DEV_Digital_Write(EPD_CS_PIN, 1);
}

UBYTE DEV_SPI_ReadData() {
  UBYTE i, j = 0xff;
  DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
  DEV_Digital_Write(EPD_CS_PIN, 0);
  for (i = 0; i < 8; i++) {
    DEV_Digital_Write(EPD_SCLK_PIN, 0);
    j = j << 1;
    if (DEV_Digital_Read(EPD_MOSI_PIN)) {
      j = j | 0x01;
    } else {
      j = j & 0xfe;
    }
    DEV_Digital_Write(EPD_SCLK_PIN, 1);
  }
  DEV_Digital_Write(EPD_SCLK_PIN, 0);
  DEV_Digital_Write(EPD_CS_PIN, 1);
  return j;
}

/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI
protocol parameter: Info:
******************************************************************************/
UBYTE DEV_Module_Init(void) {
  printf("DEV_Module_Init\n");
  GPIO_Handle = lgGpiochipOpen(0);
  if (GPIO_Handle < 0) {
    GPIO_Handle = lgGpiochipOpen(4);
    if (GPIO_Handle < 0) {
      Debug("Can't find /dev/gpiochip[1|4]\n");
      return -1;
    }
  }

  SPI_Handle = lgSpiOpen(0, 0, 10000000, 0);
  if (SPI_Handle == LG_SPI_IOCTL_FAILED) {
    Debug("Can't open SPI\n");
    return -1;
  }

  EPD_RST_PIN = 17;
  EPD_DC_PIN = 25;
  EPD_CS_PIN = 8;
  EPD_PWR_PIN = 18;
  EPD_BUSY_PIN = 24;
  EPD_MOSI_PIN = 10;
  EPD_SCLK_PIN = 11;

  DEV_GPIO_Mode(EPD_BUSY_PIN, 0);
  DEV_GPIO_Mode(EPD_RST_PIN, 1);
  DEV_GPIO_Mode(EPD_DC_PIN, 1);
  DEV_GPIO_Mode(EPD_CS_PIN, 1);
  DEV_GPIO_Mode(EPD_PWR_PIN, 1);
  // DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
  // DEV_GPIO_Mode(EPD_SCLK_PIN, 1);

  DEV_Digital_Write(EPD_CS_PIN, 1);
  DEV_Digital_Write(EPD_PWR_PIN, 1);

  printf("/DEV_Module_Init\n");
  return 0;
}

/******************************************************************************
function:	Module exits, closes SPI and BCM2835 library
parameter:
Info:
******************************************************************************/
void DEV_Module_Exit(void) {
#ifdef RPI
#ifdef USE_BCM2835_LIB
  DEV_Digital_Write(EPD_CS_PIN, LOW);
  DEV_Digital_Write(EPD_PWR_PIN, LOW);
  DEV_Digital_Write(EPD_DC_PIN, LOW);
  DEV_Digital_Write(EPD_RST_PIN, LOW);

  bcm2835_spi_end();
  bcm2835_close();
#elif USE_WIRINGPI_LIB
  DEV_Digital_Write(EPD_CS_PIN, 0);
  DEV_Digital_Write(EPD_PWR_PIN, 0);
  DEV_Digital_Write(EPD_DC_PIN, 0);
  DEV_Digital_Write(EPD_RST_PIN, 0);
#elif USE_LGPIO_LIB
  // DEV_Digital_Write(EPD_CS_PIN, 0);
  // DEV_Digital_Write(EPD_PWR_PIN, 0);
  // DEV_Digital_Write(EPD_DC_PIN, 0);
  // DEV_Digital_Write(EPD_RST_PIN, 0);
  // lgSpiClose(SPI_Handle);
  // lgGpiochipClose(GPIO_Handle);
#elif USE_DEV_LIB
  DEV_HARDWARE_SPI_end();
  DEV_Digital_Write(EPD_CS_PIN, 0);
  DEV_Digital_Write(EPD_PWR_PIN, 0);
  DEV_Digital_Write(EPD_DC_PIN, 0);
  DEV_Digital_Write(EPD_RST_PIN, 0);
  GPIOD_Unexport(EPD_PWR_PIN);
  GPIOD_Unexport(EPD_DC_PIN);
  GPIOD_Unexport(EPD_RST_PIN);
  GPIOD_Unexport(EPD_BUSY_PIN);
  GPIOD_Unexport_GPIO();
#endif

#elif JETSON
#ifdef USE_DEV_LIB
  SYSFS_GPIO_Unexport(EPD_CS_PIN);
    SYSFS_GPIO_Unexport(EPD_PWR_PIN;
	SYSFS_GPIO_Unexport(EPD_DC_PIN);
	SYSFS_GPIO_Unexport(EPD_RST_PIN);
	SYSFS_GPIO_Unexport(EPD_BUSY_PIN);
#elif USE_HARDWARE_LIB
  Debug("not support");
#endif
#endif
}
