#ifndef _XIAONEOPIXEL_H
#define _XIAONEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * NeoPixel driver for Xiao RP2040 module.
 * 
 * The Xiao module uses GPIO11 to supply the Neopixel (WS2812) with power.
 * GPIO12 is connected to its DIN signal.
 * 
 * Normally Neopixels are controlled using PIO, However there are rare cases where 
 * projects require the full PIO program memories for other purposes.
 * 
 * This project uses UART0 TX to write to the Neopixel, giving you another option :-)
 * This is not the most efficient method to update the WS2812, but it is mostly hw
 * driven and ressource friendly.
 * 
 * It uses the UART0 in 6 Bit mode with a matching baudrate and inverts the UART0 TX 
 * signal to make the trick :-)
 * 
 * Author: Xyphro, Kai Gossner 30th January 2025
 * License: MIT
 */


// Initialize NeoPixel, call this once. Ensure that UART0 is not used for other purposes!
void xnp_init(void);

// send new R G B value (red, Green, Blue) to Neopixel
// 255 results in max brightness per color, 0 in off
void xnp_send(uint8_t r, uint8_t g, uint8_t b);

void xnp_send24(uint32_t color);

// detect if this is a xiao module
bool xnp_is_xiao_module(void);

#endif