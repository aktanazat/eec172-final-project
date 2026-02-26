#include "pinmux.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "gpio.h"
#include "prcm.h"

void PinMuxConfig(void)
{
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA3, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_UARTA1, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    // I2C for BMA222 (PIN_01/02)
    MAP_PinTypeI2C(PIN_01, PIN_MODE_1);
    MAP_PinTypeI2C(PIN_02, PIN_MODE_1);

    // IR receiver input
    MAP_PinTypeGPIO(PIN_03, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA1_BASE, 0x10, GPIO_DIR_MODE_IN);

    // OLED SPI pins
    MAP_PinTypeSPI(PIN_05, PIN_MODE_7); // CLK
    MAP_PinTypeSPI(PIN_07, PIN_MODE_7); // MOSI
    MAP_PinTypeSPI(PIN_08, PIN_MODE_7); // CS

    // OLED control pins
    MAP_PinTypeGPIO(PIN_18, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA3_BASE, 0x10, GPIO_DIR_MODE_OUT); // RST

    MAP_PinTypeGPIO(PIN_53, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA3_BASE, 0x40, GPIO_DIR_MODE_OUT); // DC

    // UART0 debug
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);

    // UART1 TX/RX (to Arduino)
    MAP_PinTypeUART(PIN_58, PIN_MODE_6);
    MAP_PinTypeUART(PIN_59, PIN_MODE_6);

    // Status LED
    MAP_PinTypeGPIO(PIN_64, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA1_BASE, 0x02, GPIO_DIR_MODE_OUT);
}
