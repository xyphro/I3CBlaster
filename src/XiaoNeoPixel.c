#include "XiaoNeoPixel.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

void xnp_init(void)
{
	uart_init(uart0, 6666666ul);
	uart_set_format(uart0, 6, 1, UART_PARITY_NONE);
	gpio_set_function (12, GPIO_FUNC_UART);
	iobank0_hw->io[12].ctrl = (iobank0_hw->io[12].ctrl & ~(3u<<8)) | (1u<<8); // invert UART output signal
    gpio_init(11);
	gpio_set_dir(11, GPIO_OUT);    
    gpio_put(11, 1);
	gpio_set_drive_strength(11, GPIO_DRIVE_STRENGTH_12MA); // set drive strength of IO to maximum to get a bright LED
    gpio_set_function(11, GPIO_FUNC_SIO);
}

void __not_in_flash_func(xnp_send)(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t previntstate = save_and_disable_interrupts();
	uint32_t dataword = (((uint32_t)g)<<16) | (((uint32_t)r)<<8) | (((uint32_t)b)<<0);

	for (uint8_t bitcounter=0; bitcounter<24; bitcounter++)
	{
		dataword <<= 1;
		if (dataword & (1<<24))
		    uart_putc(uart0, 0x30); // send a 1  5:3
		else
		    uart_putc(uart0, 0x3C); // send a 0  3:5
	}
	restore_interrupts(previntstate);
}

void __not_in_flash_func(xnp_send24)(uint32_t color)
{
	uint32_t previntstate = save_and_disable_interrupts();
	for (uint8_t bitcounter=0; bitcounter<24; bitcounter++)
	{
		color <<= 1;
		if (color & (1<<24))
		    uart_putc(uart0, 0x3C); // send a 1
		else
		    uart_putc(uart0, 0x30); // send a 0
	}
	restore_interrupts(previntstate);
}

// Raspberry pi pico module has a voltage divider (5K6 / 10K) to detect USB VBUS
// Xiao module doesn't have this resistor. We can use this to detect if this is 
// a XIAO module by enabling a pulldown resistor on GPIO 24 and sense it is high 
// or low state.
// The idea is from Jonathan.
bool xnp_is_xiao_module(void)
{
    gpio_init(24);
    gpio_set_dir(24, GPIO_IN);
    gpio_set_function(24, GPIO_FUNC_SIO);
    gpio_set_pulls(24, false, true);
    sleep_us(100); // let GPIO24 settle. 100us is on the high side, but I am lazy characterizing it properly and execution time is not important.
    return !gpio_get(24); // if HIGH, it is raspberry pico module, so invert it
}