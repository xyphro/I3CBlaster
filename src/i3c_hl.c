
#include "i3c_hl.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "i3c.pio.h"
#include "hardware/sync.h"

/*
MIT License

Copyright (c) 2024 xyphro, Kai Gossner, xyphro@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define inline __inline__ __attribute__((always_inline))


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
// I3c
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
static uint32_t i3c_wdata_table[512];
static uint8_t i3c_hl_arbcode;
static uint8_t i3c_hl_gpiobasepin;
static uint32_t i3c_hl_pio_program_sdr[32]; // copy of pio memory for fast exchange of SM. It is on purpose located in ram for fast copy action
static uint32_t i3c_hl_pio_program_ddr[32]; // copy of pio memory for fast exchange of SM. It is on purpose located in ram for fast copy action

// A table to help executing CRC5 CRCs using macro CRC5_CALCULATE
// generated using pycrc:
//   python pycrc.py --width=5 --poly=0x05 --reflect-in=False --xor-in=0x1f --reflect-out=False --xor-out=0x1f --algorithm=table-driven --generate=C --output=out.c
// The values in table were post execution shifted left by 3 bits afterwards for faster execution
static const uint8_t __not_in_flash("crc_table") crc5_table[256] = {
	0x00, 0x28, 0x50, 0x78, 0xa0, 0x88, 0xf0, 0xd8, 0x68, 0x40, 0x38, 0x10, 0xc8, 0xe0, 0x98, 0xb0,
	0xd0, 0xf8, 0x80, 0xa8, 0x70, 0x58, 0x20, 0x08, 0xb8, 0x90, 0xe8, 0xc0, 0x18, 0x30, 0x48, 0x60,
	0x88, 0xa0, 0xd8, 0xf0, 0x28, 0x00, 0x78, 0x50, 0xe0, 0xc8, 0xb0, 0x98, 0x40, 0x68, 0x10, 0x38,
	0x58, 0x70, 0x08, 0x20, 0xf8, 0xd0, 0xa8, 0x80, 0x30, 0x18, 0x60, 0x48, 0x90, 0xb8, 0xc0, 0xe8,
	0x38, 0x10, 0x68, 0x40, 0x98, 0xb0, 0xc8, 0xe0, 0x50, 0x78, 0x00, 0x28, 0xf0, 0xd8, 0xa0, 0x88,
	0xe8, 0xc0, 0xb8, 0x90, 0x48, 0x60, 0x18, 0x30, 0x80, 0xa8, 0xd0, 0xf8, 0x20, 0x08, 0x70, 0x58,
	0xb0, 0x98, 0xe0, 0xc8, 0x10, 0x38, 0x40, 0x68, 0xd8, 0xf0, 0x88, 0xa0, 0x78, 0x50, 0x28, 0x00,
	0x60, 0x48, 0x30, 0x18, 0xc0, 0xe8, 0x90, 0xb8, 0x08, 0x20, 0x58, 0x70, 0xa8, 0x80, 0xf8, 0xd0,
	0x70, 0x58, 0x20, 0x08, 0xd0, 0xf8, 0x80, 0xa8, 0x18, 0x30, 0x48, 0x60, 0xb8, 0x90, 0xe8, 0xc0,
	0xa0, 0x88, 0xf0, 0xd8, 0x00, 0x28, 0x50, 0x78, 0xc8, 0xe0, 0x98, 0xb0, 0x68, 0x40, 0x38, 0x10,
	0xf8, 0xd0, 0xa8, 0x80, 0x58, 0x70, 0x08, 0x20, 0x90, 0xb8, 0xc0, 0xe8, 0x30, 0x18, 0x60, 0x48,
	0x28, 0x00, 0x78, 0x50, 0x88, 0xa0, 0xd8, 0xf0, 0x40, 0x68, 0x10, 0x38, 0xe0, 0xc8, 0xb0, 0x98,
	0x48, 0x60, 0x18, 0x30, 0xe8, 0xc0, 0xb8, 0x90, 0x20, 0x08, 0x70, 0x58, 0x80, 0xa8, 0xd0, 0xf8,
	0x98, 0xb0, 0xc8, 0xe0, 0x38, 0x10, 0x68, 0x40, 0xf0, 0xd8, 0xa0, 0x88, 0x50, 0x78, 0x00, 0x28,
	0xc0, 0xe8, 0x90, 0xb8, 0x60, 0x48, 0x30, 0x18, 0xa8, 0x80, 0xf8, 0xd0, 0x08, 0x20, 0x58, 0x70,
	0x10, 0x38, 0x40, 0x68, 0xb0, 0x98, 0xe0, 0xc8, 0x78, 0x50, 0x28, 0x00, 0xd8, 0xf0, 0x88, 0xa0
};
static bool sm_is_in_ddr_mode;

// Helper macro for fast CRC exepcution. For correct usage use ast initial value 0x1f<<3 and the resulting CRC is the return value >>3
// The shift by 3 bytes helps in cycle efficiency of the CRC calculation
#define CRC5_CALCULATE(crc, data) ( crc5_table[crc5_table[(crc) ^ (uint8_t)((data)>>8)] ^ ((uint8_t)(data) & 0xffu)] )

// Calculate parity bits for a 16 bit data word. Used for HDR-DDR transfers
// Bit 1 contains PA1 = D[15] ^ D[13] ^ D[11] ^ D[9] ^ D[7] ^ D[5] ^ D[3] ^ D[1]
// Bit 0 contains PA0 = D[14] ^ D[12] ^ D[10] ^ D[8] ^ D[6] ^ D[4] ^ D[2] ^ D[0] ^ 1 
// All other bits are 0
// return datatype is enforced to be uint8_t
#define DDR_PARITY(data) ( (uint8_t)( ((uint32_t)__builtin_parity(((uint16_t)(data) & 0xaaaau)) << 1) | ((uint32_t)__builtin_parity(((uint16_t)(data) & 0x5555u))  ^ 1) ))


static inline void __not_in_flash_func(i3c_pio_wait_tx_empty)(void) 
{
    while ( (pio0->fstat & (1u << (PIO_FSTAT_TXEMPTY_LSB + 1))) == 0 )
    {
	}
}

// blocking write to pio pipeline
static inline void __not_in_flash_func(i3c_pio_put32)(uint32_t data) 
{
    while ( (pio0->fstat & (1u << (PIO_FSTAT_TXFULL_LSB + 1))) != 0 )
    {
	}
    pio0->txf[1] = data;
}

// non blocking write to pio write pipe
static inline void __not_in_flash_func(i3c_pio_put32_no_check)(uint32_t data) 
{
    pio0->txf[1] = data;
}


// blocking read from pio read pipe
static inline uint32_t __not_in_flash_func(i3c_pio_get32)(void)
{
    while ( (pio0->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + 1))) != 0 )
    {
	}
    return pio0->rxf[1];
}

// mechanism to update autopush threshold quickly on the fly
static uint32_t i3c_pio_shiftctrl_autopointer_tab[32] = { 0ul }; // prepare a table for quick autopush value update
static inline void __not_in_flash_func(i3c_pio_set_autopush)(uint8_t bitcount)
{
	pio0->sm[1].shiftctrl = i3c_pio_shiftctrl_autopointer_tab[bitcount];
}

static inline void __not_in_flash_func(i3c_pio_set_autopush_bitrev)(uint8_t bitcount)
{
	pio0->sm[1].shiftctrl = i3c_pio_shiftctrl_autopointer_tab[bitcount] & ~(1<<19);
}

// restart pio (resets e.g. shift counters and ISR/OSR content)
static inline void __not_in_flash_func(i3c_pio_restart)(void)
{
	pio0->ctrl = PIO_CTRL_CLKDIV_RESTART_BITS | (1 << (PIO_CTRL_SM_ENABLE_LSB + 1));
}

// program pio SM for SDR mode
static inline void __not_in_flash_func(s_i3c_program_hdr_sdr_sm)(void)
{
//	for (uint32_t i=0; i<i3c_program.length; i++)
//		*((volatile uint16_t *)&(pio0->instr_mem[i])) = i3c_program_instructions[i];
	memcpy((void*)(pio0->instr_mem), (void*)i3c_hl_pio_program_sdr, i3c_program.length*4);
    pio0->sm[1].execctrl = (          i3c_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB) |
	                       (   i3c_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) | 
						   (                 1 << PIO_SM0_EXECCTRL_SIDE_EN_LSB) |
						   (i3c_hl_gpiobasepin << PIO_SM0_EXECCTRL_JMP_PIN_LSB);
	pio0->sm[1].shiftctrl = (0 << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB) | 
		 				    (1 << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB) |
						    (0 << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB) |
						    (0 << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
						    (1 << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
						    (0 << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB);
	sm_is_in_ddr_mode = false;
}

// program pio SM for DDR mode
static inline void __not_in_flash_func(s_i3c_program_hdr_ddr_sm)(void)
{
	//uint32_t *src=i3c_hl_pio_program_ddr, *dst=pio0->instr_mem;
//	for (uint32_t i=0; i<i3c_ddr_program.length; i++)
//		*((volatile uint16_t *)&(pio0->instr_mem[i])) = i3c_ddr_program_instructions[i];
	memcpy((void*)(pio0->instr_mem), (void*)i3c_hl_pio_program_ddr, i3c_ddr_program.length*4);

    pio0->sm[1].execctrl = (       i3c_ddr_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB) |
	                       (i3c_ddr_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) | 
						   (                  1 << PIO_SM0_EXECCTRL_SIDE_EN_LSB) |
						   ( i3c_hl_gpiobasepin << PIO_SM0_EXECCTRL_JMP_PIN_LSB);

	pio0->sm[1].shiftctrl = (0 << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB) | 
		 				    (0 << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB) | // in DDR mode we shift out bits left out to avoid bitreversal which is slow on CM0 or adds artificially to PIO program memory size
		 				    (0 << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB) | // data enters from right
						    (0 << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
						    (1 << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
						    (0 << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB);
	sm_is_in_ddr_mode = true;
}

i3c_hl_status_t i3c_hl_set_drivestrength(uint8_t drivestrength_mA)
{
	uint8_t ds = 0xff;
	switch (drivestrength_mA)
	{
		case  2: ds = 0; break;
		case  4: ds = 1; break;
		case  8: ds = 2; break;
		case 12: ds = 3; break;
	}

	if (ds == 0xff)
	{
		return i3c_hl_status_param_outofrange;
	}
	else
	{
		gpio_set_drive_strength(i3c_hl_gpiobasepin, (enum gpio_drive_strength)ds);
		gpio_set_drive_strength(i3c_hl_gpiobasepin+1, (enum gpio_drive_strength)ds);		
	}
	return i3c_hl_status_ok;
}


i3c_hl_status_t i3c_init(uint8_t gpiobasepin)
{
    PIO pio = pio0;

	// create copies of PIO statemachines for very quick exchange during runtime when switching transfer modes
	for (uint32_t i=0; i<sizeof(i3c_hl_pio_program_sdr)/sizeof(uint32_t); i++)
	{
		if (i < (sizeof(i3c_program_instructions)/sizeof(uint16_t)) )
		{
			i3c_hl_pio_program_sdr[i] = i3c_program_instructions[i];
		}
		else
		{
			i3c_hl_pio_program_sdr[i] = 0x0ul;
		}
		if (i < (sizeof(i3c_ddr_program_instructions)/sizeof(uint16_t)) )
		{
			i3c_hl_pio_program_ddr[i] = i3c_ddr_program_instructions[i];
		}
		else
		{
			i3c_hl_pio_program_ddr[i] = 0x0ul;
		}
	}
	i3c_hl_gpiobasepin = gpiobasepin;
	s_i3c_program_hdr_sdr_sm();

    pio->sm[1].clkdiv = (uint32_t) (1.0f * (1 << 16));
    pio->sm[1].pinctrl =
            (1 << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
            ((gpiobasepin+1) << PIO_SM0_PINCTRL_SET_BASE_LSB)  |
            (2 << PIO_SM0_PINCTRL_OUT_COUNT_LSB) |
            (gpiobasepin << PIO_SM0_PINCTRL_OUT_BASE_LSB)  |
            //(1 << PIO_SM0_PINCTRL_IN_COUNT_LSB) |
            (gpiobasepin << PIO_SM0_PINCTRL_IN_BASE_LSB)  |
            (2 << PIO_SM0_PINCTRL_SIDESET_COUNT_LSB) |
            ((gpiobasepin+1) << PIO_SM0_PINCTRL_SIDESET_BASE_LSB) ;
    gpio_set_function(gpiobasepin, GPIO_FUNC_PIO0);
    gpio_set_function(gpiobasepin+1, GPIO_FUNC_PIO0);
	gpio_set_drive_strength(gpiobasepin, GPIO_DRIVE_STRENGTH_12MA);
	gpio_set_drive_strength(gpiobasepin+1, GPIO_DRIVE_STRENGTH_12MA);
	gpio_set_slew_rate(gpiobasepin, GPIO_SLEW_RATE_FAST);
	gpio_set_slew_rate(gpiobasepin+1, GPIO_SLEW_RATE_FAST);

	// set wrap target
    pio->sm[1].execctrl = (       i3c_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB) |
	                      (i3c_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) | 
						  (              1 << PIO_SM0_EXECCTRL_SIDE_EN_LSB) | 
						  (    gpiobasepin << PIO_SM0_EXECCTRL_JMP_PIN_LSB);
	pio->sm[1].shiftctrl = (0 << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB) | 
						   (1 << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB) |
						   (0 << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB) |
						   (0 << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
						   (1 << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
						   (0 << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB);
	for (uint8_t bitcount=0; bitcount<32; bitcount++)
	{
		i3c_pio_shiftctrl_autopointer_tab[bitcount] = 
		    pio->sm[1].shiftctrl | (((uint32_t)(bitcount&0x1f)) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB);
	}
    hw_set_bits(&pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + 1));	
    hw_set_bits(&pio->input_sync_bypass, (3u << (gpiobasepin)));	 // bypass input synchronizers. Very important for I3C at 12.5 MHz
	i3c_pio_put32( I3CPIO_OPCODE_SETDIR_SCLSDA_SCLHIGH(1, 0) );

    pio->sm[1].pinctrl =
            (1 << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
            (gpiobasepin << PIO_SM0_PINCTRL_SET_BASE_LSB)  |
            (1 << PIO_SM0_PINCTRL_OUT_COUNT_LSB) |
            (gpiobasepin << PIO_SM0_PINCTRL_OUT_BASE_LSB)  |
            (gpiobasepin << PIO_SM0_PINCTRL_IN_BASE_LSB)  |
            (2 << PIO_SM0_PINCTRL_SIDESET_COUNT_LSB) |
            ((gpiobasepin+1) << PIO_SM0_PINCTRL_SIDESET_BASE_LSB) ;

	// precalculate write table for fast sdr_write execution
	for (uint32_t value=0; value<256; value++)
	{
		uint8_t tbit = __builtin_parity(value)^1;
		i3c_wdata_table[(uint32_t)value*2u+0u] = I3CPIO_OPCODE_XFER(6, SDR_WBIT((value>>7)&1), SDR_WBIT((value>>6)&1), SDR_WBIT((value>>5)&1), SDR_WBIT((value>>4)&1), SDR_WBIT((value>>3)&1), SDR_WBIT((value>>2)&1));
		i3c_wdata_table[(uint32_t)value*2u+1u] = I3CPIO_OPCODE_XFER(3, SDR_WBIT((value>>1)&1), SDR_WBIT((value>>0)&1), SDR_WBIT(tbit), 0, 0, 0);
	}
	i3c_hl_arbcode = 0xfc;
	
	return i3c_hl_status_ok;
}

i3c_hl_status_t i3c_hl_set_clkrate(uint32_t targetfreq_khz)
{
	if (targetfreq_khz > 12500u)
		return i3c_hl_status_param_outofrange;
	if (targetfreq_khz < 49u)
		return i3c_hl_status_param_outofrange;

    //pio->sm[1].clkdiv = (uint32_t) (1.0f * (1 << 16));
	pio0->sm[1].clkdiv = (12500*65536) / targetfreq_khz;
	return i3c_hl_status_ok;
}

// bit 0 = pinstate falling edge
// bit 1 = pindir   falling edge
// bit 2 = opendrain delay (1=enable)
// bit 3 = pindir   rising edge

// transmit a value in i3c open drain mode
// transmit 8 bits (msb first)
// read 1 bit 

// returns TRUE on success (acked)
i3c_hl_status_t __not_in_flash_func(i3c_sdr_write_addr)(uint8_t value)
{
	uint32_t cmdword0,  cmdword1, resp;
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(9);

	cmdword0 = I3CPIO_OPCODE_XFER(6, SDR_WBIT((value>>7)&1), SDR_WBIT((value>>6)&1), SDR_WBIT((value>>5)&1), SDR_WBIT((value>>4)&1), SDR_WBIT((value>>3)&1), SDR_WBIT((value>>2)&1));
	cmdword1 = I3CPIO_OPCODE_XFER(3, SDR_WBIT((value>>1)&1), SDR_WBIT((value>>0)&1), OD_RACKBIT, 0, 0, 0);

	i3c_pio_put32_no_check(cmdword0);
	i3c_pio_put32_no_check(cmdword1);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
	resp = i3c_pio_get32();
	if (resp & 1)
	{
		retcode = i3c_hl_status_nak_during_sdraddr;
	}
	return retcode;
}

static inline void __not_in_flash_func(i3c_start)(void)
{
	i3c_pio_put32( I3CPIO_OPCODE_START ); // start
}

static inline void __not_in_flash_func(i3c_restart)(void)
{
	
	i3c_pio_put32( I3CPIO_OPCODE_RESTART ); // start
}

static inline void __not_in_flash_func(i3c_stop)(void)
{
	i3c_pio_put32( I3CPIO_OPCODE_STOP ); // stop
}

void __not_in_flash_func(i3c_sdr_write)(uint8_t value)
{
	uint32_t cmdword0,  cmdword1, tbit;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(9);
	i3c_pio_put32_no_check( i3c_wdata_table[(uint32_t)value*2+0] );
	i3c_pio_put32_no_check( i3c_wdata_table[(uint32_t)value*2+1] );	
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
	i3c_pio_get32(); // dump read data;
}

// returns the read data byte in bits 8..1 and the slave ACK phase state in Bit 0 (0 = slave did not ask to finish transfer)
// The slave will signal in in pre-Bit9 low phase if read can continue or not
//   0 => The transfer should end
//   1 => The transfer can continue
// The Master can signal back its intention in Bit 9 high state:
//   1 => Continue transfer
//   0 => STOP transfer (creates an implcit repeated start)
static uint32_t __not_in_flash_func(i3c_sdr_read)(uint8_t continuetransfer)
{
	uint32_t data;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(9);
	i3c_pio_put32_no_check( I3CPIO_OPCODE_XFER(6, SDR_RBIT(0), SDR_RBIT(0), SDR_RBIT(0), SDR_RBIT(0), SDR_RBIT(0), SDR_RBIT(0)) );
	i3c_pio_put32_no_check( I3CPIO_OPCODE_XFER(3, SDR_RBIT(0), SDR_RBIT(0), SDR_RBIT(continuetransfer), 0, 0, 0) );	
	i3c_pio_put32_no_check( I3CPIO_OPCODE_SCL0 ); // avoid high phase beeing too long
	data = i3c_pio_get32(); // dump read data;
	return data;
}

static void __not_in_flash_func(i3c_od_write)(uint8_t value)
{
	uint32_t cmdword0,  cmdword1;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(9);
	cmdword0 = I3CPIO_OPCODE_XFER(6, OD_WBIT((value>>7)&1), OD_WBIT((value>>6)&1), OD_WBIT((value>>5)&1), OD_WBIT((value>>4)&1), OD_WBIT((value>>3)&1), OD_WBIT((value>>2)&1));
	cmdword1 = I3CPIO_OPCODE_XFER(3, OD_WBIT((value>>1)&1), OD_WBIT((value>>0)&1), OD_RACKBIT, 0, 0, 0);
	
	i3c_pio_put32_no_check(cmdword0);
	i3c_pio_put32_no_check(cmdword1);	
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
	uint32_t data = i3c_pio_get32();	
}

static inline uint8_t __not_in_flash_func(i3c_od_read)(uint8_t ack)
{
	uint32_t cmdword0,  cmdword1, data;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(9);
	cmdword0 = I3CPIO_OPCODE_XFER(6, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT);
	cmdword1 = I3CPIO_OPCODE_XFER(3, OD_RBIT, OD_RBIT, OD_WBIT((ack^1)&1), 0, 0, 0);
	i3c_pio_put32_no_check(cmdword0);
	i3c_pio_put32_no_check(cmdword1);	
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
	data = i3c_pio_get32();
	return ((uint8_t)(data>>1));
}

static inline uint8_t __not_in_flash_func(i3c_od_read8)(void)
{
	uint32_t cmdword0,  cmdword1;
	uint8_t readdata;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(8);
	cmdword0 = I3CPIO_OPCODE_XFER(6, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT, OD_RBIT);
	cmdword1 = I3CPIO_OPCODE_XFER(2, OD_RBIT, OD_RBIT, 0, 0, 0, 0);
	i3c_pio_put32_no_check(cmdword0);
	i3c_pio_put32_no_check(cmdword1);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
	readdata =  (uint8_t)i3c_pio_get32();
	return readdata;
}

// returns true of SDA is low (IBI/HJ type 1)
bool __not_in_flash_func(i3c_ibi_type1_check)(void)
{
	return ((iobank0_hw->io[i3c_hl_gpiobasepin].status & IO_BANK0_GPIO0_STATUS_INFROMPAD_BITS) >> IO_BANK0_GPIO0_STATUS_INFROMPAD_LSB) == 0;
}

// returns true when acked and no arbitration issue occured.
// *parbdata will contain the sensed data during transmit, enabling to detect e.g. IBI source
static i3c_hl_status_t __not_in_flash_func(i3c_arbhdr)(uint8_t *parbdata)
{
	uint32_t cmdword0, cmdword1, cmdword2;
	uint32_t data0, data1;
	uint8_t  arbdata;
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_set_autopush(6);
	cmdword0 = I3CPIO_OPCODE_XFER(6, OD_WBIT(1), OD_WBIT(1), OD_WBIT(1), OD_WBIT(1), OD_WBIT(1), OD_WBIT(1));
	cmdword1 = I3CPIO_OPCODE_XFER(3, OD_WBIT(0), OD_WBIT(0), OD_RACKBIT, 0, 0, 0); // normal case: no arbitration happened
	cmdword2 = I3CPIO_OPCODE_XFER(3, OD_WBIT(1), OD_WBIT(1), OD_WBIT(0), 0, 0, 0); // used when arbitration won by target, this will get a read  which we need to ACK

	i3c_pio_put32_no_check(cmdword0);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // in debug build the high period would be too long and pass through i2c glitch filters, so make it low to prevent this
	data0 = i3c_pio_get32();
	i3c_pio_set_autopush(3);
	if (data0 == 0x3ful)
	{ // no arbitration occured (transmitted 6 high bits and received 6 high bits)
		i3c_pio_put32_no_check(cmdword1);	
		i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
		data1 = i3c_pio_get32();
		if (data1 & 1)
		{
			retcode = i3c_hl_status_nak_during_arbhdr;
		}
	}
	else
	{ // arbitration occured (IBI raised - collect data)
		i3c_pio_put32_no_check(cmdword2);	
		i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0); // avoid high phase beeing too long
		data1 = i3c_pio_get32();
		retcode = i3c_hl_status_ibi;
	}
	arbdata = (data0<<2) | (data1>>1); // strip away ACK bit
	if (parbdata)
		*parbdata = arbdata;

	i3c_hl_arbcode = arbdata;
	return retcode; // Bit 0 of data 1 is the last bit sampled, thus the ACK bit
}

// execute entdaa process. Return TRUE on success. FALSE when no target responded
// pid points to an 8 byte memory which receives the UID returned by the device during execution
i3c_hl_status_t __not_in_flash_func(i3c_hl_entdaa)(uint8_t addr, uint8_t *pid)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint8_t arbhdr;
	uint32_t previntstate = save_and_disable_interrupts();

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(&arbhdr);
		if ( retcode == i3c_hl_status_ok )
		{		
			i3c_sdr_write(0x07);
			i3c_restart();
			retcode = i3c_sdr_write_addr((0x7eu<<1) | 1);
			if ( retcode == i3c_hl_status_ok )
			{
				for (uint8_t i=0; i<8; i++)
				{
					uint8_t id = i3c_od_read8();
					if ( pid )
					{
						*pid++ = id;
					}
				}

				addr <<= 1;
				addr |= __builtin_parity(addr)^1;

				retcode = i3c_sdr_write_addr(addr);
			}
		}
		else
		{
			if (arbhdr != 0x7e)
			{
				//asdf
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);

	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_rstdaa)(void)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}

	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode == i3c_hl_status_ok )
			i3c_sdr_write(0x06); // RSTDAA
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_privwrite)(uint8_t addr, const uint8_t *pdat, uint32_t bytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint8_t arbdata;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(&arbdata);

		if ( retcode == i3c_hl_status_ok )
		{
			i3c_restart();
			retcode = i3c_sdr_write_addr(addr<<1);
			if (retcode == i3c_hl_status_ok)
			{
				while (bytecount--)
					i3c_sdr_write(*pdat++);
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_privwriteread)(uint8_t addr, const uint8_t *pwritedat, uint32_t writebytecount,
                                                                                  uint8_t *preaddat, uint32_t *preadbytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t readbytecount;
	bool done;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode == i3c_hl_status_ok )
		{
			i3c_restart();
			retcode = i3c_sdr_write_addr(addr<<1);
			if (retcode == i3c_hl_status_ok)
			{
				while (writebytecount--)
					i3c_sdr_write(*pwritedat++);
				// step over to read phase
				i3c_restart();
				retcode = i3c_sdr_write_addr((addr<<1) | 1);
				if (retcode == i3c_hl_status_ok)
				{
					uint32_t readlen = *preadbytecount;
					done = false;
					readbytecount = 0;
					while ( (readlen) && (!done) )
					{
						uint32_t value;
						readlen--;
						value = i3c_sdr_read(readlen==0);
						done = !(value & 1);
						*preaddat++ = value >>1;
						readbytecount++;
					}
					*preadbytecount = readbytecount;
				}
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}


i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_ccc_broadcast_write)(const uint8_t *pdat, uint32_t bytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode == i3c_hl_status_ok )
		{
			while (bytecount--)
				i3c_sdr_write(*pdat++);
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_ccc_direct_write)(const uint8_t *pdat, uint32_t bytecount,
																uint8_t addr, const uint8_t *pdirectdat, uint32_t directbytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode == i3c_hl_status_ok )
		{
			while (bytecount--)
				i3c_sdr_write(*pdat++);
			i3c_restart();
			retcode = i3c_sdr_write_addr(addr<<1);
			if (retcode == i3c_hl_status_ok)
			{
				while (directbytecount--)
					i3c_sdr_write(*pdirectdat++);
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_ccc_direct_read)(const uint8_t *pdat, uint32_t bytecount,
																uint8_t addr, uint8_t *pdirectdat, uint32_t *pdirectbytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t readbytecount = 0;
	bool done;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode == i3c_hl_status_ok )
		{
			while (bytecount--)
				i3c_sdr_write(*pdat++);
			i3c_restart();
			retcode = i3c_sdr_write_addr((addr<<1) | 1);
			if (retcode == i3c_hl_status_ok)
			{
				uint32_t readlen = *pdirectbytecount;
				done = false;
				while ( (readlen--) && (!done) )
				{
					uint32_t value;
					value = i3c_sdr_read(readlen==0);
					done = !(value & 1);
					*pdirectdat++ = value >>1;
					readbytecount++;
				}
				*pdirectbytecount = readbytecount;
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_ccc_read)(const uint8_t *pwritedata, uint32_t writelen,
                                                              uint8_t *preaddat,  uint32_t *preadlen)
{
	uint32_t previntstate = save_and_disable_interrupts();
	i3c_hl_status_t retcode;
	uint32_t readbytecount;
	uint32_t readlen = *preadlen;
	bool done;

	i3c_start();
	retcode = i3c_arbhdr(NULL);
	if ( retcode == i3c_hl_status_ok )
	{
		while (writelen--)
			i3c_sdr_write(*pwritedata++);
		i3c_restart();
		retcode = i3c_sdr_write_addr((0x7e<<1) | 1);
		if ( retcode == i3c_hl_status_ok )
		{
			done = false;
			while ( (readlen--) && (!done) )
			{
				uint32_t value;
				value = i3c_sdr_read(readlen==0);
				done = !(value & 1);
				*preaddat++ = value >>1;
				readbytecount++;
			}
			*preadlen = readbytecount;
		}
	}
	if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
		i3c_stop();
	restore_interrupts(previntstate);
	return retcode;
}



// execute a private read.
// returns the count of read data bytes.
// a slave might indicate that it ran "out of data".
i3c_hl_status_t __not_in_flash_func(i3c_hl_sdr_privread)(uint8_t addr, uint8_t *pdat, uint32_t *pbytecount)
{
	uint32_t previntstate = save_and_disable_interrupts();
	uint32_t readbytecount;
	bool done;
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t bytecount = *pbytecount;

	if (i3c_ibi_type1_check())
	{
		retcode = i3c_hl_status_ibi;
	}
	if (retcode == i3c_hl_status_ok)
	{
		readbytecount = 0;
		i3c_start();
		retcode = i3c_arbhdr(NULL);
		if ( retcode ==  i3c_hl_status_ok )
		{
			i3c_restart();
			retcode = i3c_sdr_write_addr((addr<<1) | 1);
			if ( retcode == i3c_hl_status_ok )
			{
				
				done = false;
				while ( (bytecount--) && (!done) )
				{
					uint32_t value;
					value = i3c_sdr_read(bytecount==0);
					done = !(value & 1);
					*pdat++ = value >>1;
					readbytecount++;
				}
			}
		}
		if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			i3c_stop();
		*pbytecount = readbytecount;
	}
	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_checkack)(uint8_t addr)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if (i3c_ibi_type1_check())
	{
		return i3c_hl_status_ibi;
	}

	i3c_start();
	retcode = i3c_arbhdr(NULL);
	if ( retcode ==  i3c_hl_status_ok)
	{
		i3c_restart( );
		retcode = i3c_sdr_write_addr(addr<<1);
	}
	if (retcode != i3c_hl_status_ibi) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
		i3c_stop();
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_arbhdronly)(void)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if (i3c_ibi_type1_check())
	{
		return i3c_hl_status_ibi;
	}
	i3c_start();
	retcode = i3c_arbhdr(NULL);
	i3c_stop();
	return retcode;
}


i3c_hl_status_t __not_in_flash_func(i3c_hl_targetreset)(void)
{
	uint32_t cmdword0,  cmdword1, resp;
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDASTATE(1));
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDADIR(1));  // SDA = active high

#pragma GCC unroll 8
	for (int i=0; i<7;i++)
	{
		i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(0));
		i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1));
	}
	i3c_pio_put32(I3CPIO_OPCODE_SCL1); // avoid high phase beeing too long
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(0)); 
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1)); 
	i3c_pio_put32(I3CPIO_OPCODE_SDADIR(0)); 
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_hdrexit)(void)
{
	uint32_t cmdword0,  cmdword1, resp;
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDASTATE(1));
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDADIR(1));  // SDA = active high
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDASTATE(0));  // SDA = low

#pragma GCC unroll 4
	for (int i=0; i<4;i++)
	{
		i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1));
		i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(0));
	}
	i3c_pio_put32(I3CPIO_OPCODE_SCL1); // avoid high phase beeing too long
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1)); 
	i3c_pio_put32(I3CPIO_OPCODE_SDADIR(0)); 
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_hdrrestart)(void)
{
	uint32_t cmdword0,  cmdword1, resp;
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_pio_wait_tx_empty(); // wait until tx pipe is empty. Afterwards 4 words can be written without full check
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SCL0);
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDASTATE(1));
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDADIR(1));  // SDA = active high
	i3c_pio_put32_no_check(I3CPIO_OPCODE_SDASTATE(0));  // SDA = low

	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(0));
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1));
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(0));
	i3c_pio_put32(I3CPIO_OPCODE_SDASTATE(1));

	i3c_pio_put32(I3CPIO_OPCODE_SCL1); // avoid high phase beeing too long
	i3c_pio_put32(I3CPIO_OPCODE_SDADIR(0)); // release SCL
	return retcode;
}




const char     *i3c_hl_get_errorstring(i3c_hl_status_t errcode)
{
	static char errstring[64];

	switch (errcode)
	{
    	case i3c_hl_status_ok                    : sprintf(errstring, "OK(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_ibi                   : sprintf(errstring, "ERR_IBI_ARBITRATION(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_nak_during_arbhdr     : sprintf(errstring, "ERR_NAKED_DURING_ARBHDR(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_nak_during_sdraddr    : sprintf(errstring, "ERR_NAKED(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_param_outofrange      : sprintf(errstring, "ERR_INVALID_PARAMETER(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_no_ibi                : sprintf(errstring, "WARN_NO_IBI(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_nak_ddr               : sprintf(errstring, "ERR_NAKED_DDR(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_ddr_early_termination : sprintf(errstring, "WARN_EARLY_TERMINATED(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_ddr_invalid_preamble  : sprintf(errstring, "ERR_DDR_INVALID_PREAMBLE(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_ddr_parity_wrong      : sprintf(errstring, "ERR_DDR_READ_PARITY_WRONG(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_ddr_crc_wrong         : sprintf(errstring, "ERR_DDR_READ_CRC_WRONG(%d)", (uint32_t)errcode); break;
		default:
			sprintf(errstring, "ERR_UNDEFINED(%d)", (uint32_t)errcode); 
			break;
	}

	return (char*)errstring;
}

// handle interrupt.
// at first it checks if there was an unhandled Arbitration time interrupt raised.
//    If so, it handles it by reading IBI/HJ code
// If not, it checks if START assertion type IBI/HJ was raised and handles it by reading IBI/HJ code
i3c_hl_status_t __not_in_flash_func(i3c_hl_poll)(uint8_t *pdat, uint32_t *plen)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t maxlen = *plen;
	*plen = 0;
	if (i3c_hl_arbcode != 0xfc) // last i3c transfer created an arbitration case => IBI type 2 occured
	{ // IBI - read bytes in SDR mode until end is signalled
		bool done = false;
		if (plen > 0)
		{
			*pdat++ = i3c_hl_arbcode;
			*plen = *plen + 1;
		}
		while ( (maxlen--) && (!done) )
		{
			uint32_t value;
			value = i3c_sdr_read(maxlen==0);
			done = !(value & 1);
			*pdat++ = value >>1;
			*plen = *plen + 1;
		}
		i3c_stop();
		i3c_hl_arbcode = 0xfc;
	}
	else if (i3c_ibi_type1_check())   // check if interrupt is asserted by target (SDA=0)
	{ // read IBI in OD mode
		uint8_t ibiword = i3c_od_read(1); // read and ACK
		if (plen > 0)
		{
			*pdat++ = ibiword;
			*plen = *plen + 1;
		}
		if (ibiword == 0x04) // hotjoin request (0x02 with R/W 0), no further words to read afterwards
		{
		}
		else
		{ // IBI - read bytes in SDR mode until end is signalled
			bool done = false;
			while ( (maxlen--) && (!done) )
			{
				uint32_t value;
				value = i3c_sdr_read(maxlen==0);
				done = !(value & 1);
				*pdat++ = value >>1;
				*plen = *plen + 1;
			}
		}
		i3c_stop();
	}
	else
	{
		retcode = i3c_hl_status_no_ibi;
	}
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_enthdr0)(void)
{
	uint8_t dat;
	dat = 0x7e;
	return i3c_hl_sdr_ccc_broadcast_write(&dat, 1);
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_ddr_write)(uint8_t addr, uint8_t command, uint16_t *pdat, uint32_t *pwordcount, bool finalize_with_restart,
                                                      bool ack_nack_enable, bool early_write_termination_enabled, bool send_crc_on_early_termination)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t previntstate = save_and_disable_interrupts();

	if ( !sm_is_in_ddr_mode )
	{
		if (i3c_ibi_type1_check())
		{
			retcode = i3c_hl_status_ibi;
		}
		if (retcode == i3c_hl_status_ok)
		{
			i3c_start();
			retcode = i3c_arbhdr(NULL);
			if ( retcode == i3c_hl_status_ok )
			{
				i3c_sdr_write( 0x20 );
			}
			if ( (retcode != i3c_hl_status_ibi) && (retcode != i3c_hl_status_ok) ) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			{
				i3c_stop();
			}
		}
	}

	if (retcode == i3c_hl_status_ok) // no IBI arbitration happend and arbitration header was acknowledge, so lets transfer....
	{
		s_i3c_program_hdr_ddr_sm(); // switch to DDR PIO SM. No need to restart it, as the start sequence of PIO code where the PC is at that point of time  is identical
		i3c_pio_set_autopush_bitrev(19);
		uint8_t crc5_value = 0x1f<<3; // 0x1f is the initial value for CRC5 calculation
		uint32_t cmd1, cmd2, parity, dat;
		uint32_t retval; // PIO return values end up here
		dat = ((uint32_t)addr<<1) | (((uint32_t)command&0x7f)<<8);

		// prepare DDR mode command word
		parity = ((uint32_t)__builtin_parity((dat & 0xaaaa)) << 1) |      // PA1 = D[15] ^ D[13] ^ D[11] ^ D[9] ^ D[7] ^ D[5] ^ D[3] ^ D[1]
				((uint32_t)__builtin_parity((dat & 0x5555))  ^ 1);       // PA0 = D[14] ^ D[12] ^ D[10] ^ D[8] ^ D[6] ^ D[4] ^ D[2] ^ D[0] ^ 1 

		// implement a recommendation from i3c spec which is to enforce that PA0 is 1 for faster turnaround for read transfers
		if ((parity & 1u) == 0)
		{
			parity |= 1u;
			dat ^= 1u;
		}
		// send write command
		cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(1, 0, 1, 1, 18, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_cmd_write_bits);
		cmd2 = DDR_HDR_OPCODE_WRITE_BITS_NOOPCODE(1, 18, dat<<2 | parity);
		i3c_pio_put32_no_check( cmd1 );
		i3c_pio_put32_no_check( cmd2 );
		crc5_value = CRC5_CALCULATE(crc5_value, dat); // calculate crc5 during data transfer execution to make use of parallelism
		i3c_pio_get32(); // dump read data. During command sending phase there is no ACK/NACK mechanism present in HDR-DDR mode. This is done during the NEXT preamble phase.

		// send a data word(s)
		uint16_t *pdati = pdat;
		uint32_t wordcount, prev_crc5_value;
		bool     early_termination = false;
		for (wordcount=0; wordcount < *pwordcount; wordcount++)
		{
			dat = *pdati++; // get next data word
			parity = ((uint32_t)__builtin_parity((dat & 0xaaaa)) << 1) |      // PA1 = D[15] ^ D[13] ^ D[11] ^ D[9] ^ D[7] ^ D[5] ^ D[3] ^ D[1]
					 ((uint32_t)__builtin_parity((dat & 0x5555))  ^ 1);       // PA0 = D[14] ^ D[12] ^ D[10] ^ D[8] ^ D[6] ^ D[4] ^ D[2] ^ D[0] ^ 1 
			if (wordcount == 0)
			{ // The first data word preamble phase is handled conditional cause there is an optional ACK/NACK phase
				if (ack_nack_enable)
				{ // when DDR Write ack&nack is enabled, the target signals by PRE0 state if it acks or not (introduced as optional in V1.1 spec, likely to be enforced in V1.2 spec)
					cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 0, 0, 0, 18, i3c_ddr_offset_target_nacked, i3c_ddr_offset_cmd_write_bits);
				}
				else
				{ // when ack&nack by target is not enabled, the CONTROLLER has to write PRE0 as 0. This was standard behavior in V1.0 spec
					cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 1, 1, 0, 18, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_cmd_write_bits);
				}
			}
			else
			{ // for following data words the preamble phase can optionally signal target early request
				if (early_write_termination_enabled)
				{ // when DDR Write early termination is enabled, the target signals by PRE0 state if it wants to abort (introduced as optional in V1.1 spec, likely to be enforced in V1.2 spec)
					cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 0, 0, 0, 18, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_target_nacked);
				}
				else
				{ // when DDR Write early write termination is disabled, the target cannot signal early abort (default V1.0 behavior)
					cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 1, 1, 0, 18, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_cmd_write_bits);
				}
			}
			cmd2 = DDR_HDR_OPCODE_WRITE_BITS_NOOPCODE(1, 18, (dat<<2) | parity);
			i3c_pio_put32_no_check(cmd1);
			i3c_pio_put32_no_check(cmd2);
			prev_crc5_value = crc5_value; //  store previous crc value to be able to use the correct crc value in case of early termination
			crc5_value = CRC5_CALCULATE(crc5_value, dat); // calculate crc5 during data transfer execution to make use of parallelism
			retval = i3c_pio_get32(); // dump read data. This is used as synchronization point to enable SM replacement in next step
			if (retval & 1) // on a successful transfer bit 0 is 0. If bit 1 is 0 then the target_nacked path was taken, which means the transfer stopped after preamble phase
			{
				if (wordcount == 0)
				{ // target did not ack
					retcode == i3c_hl_status_nak_ddr;
					break; // I'm not fan of break statements, but sometimes they are nice and this is no automotive qualified code at the end...
				}
				else
				{ // target did request early termination
					retcode == i3c_hl_status_ddr_early_termination;
					crc5_value = prev_crc5_value; // The last datavalue was not really sent on the bus, so restore the previous CRC value
					early_termination = true; // on early termination we might want to suppress the CRC phase as passed to this function
					break;
				}
			}
		}
		*pwordcount = wordcount; // update to callee the count of actual written words

		if (retcode != i3c_hl_status_nak_ddr)
		{ // don't send CRC word in case the target did not ACK
			// don't send CRC word in case early termination is enabled and sending CRC on early termination is disabled.
			if ( !(early_termination && !send_crc_on_early_termination) )
			{
				i3c_pio_set_autopush_bitrev(11);
				cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(1, 0, 1, 1, 10, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_cmd_write_bits);
				cmd2 = DDR_HDR_OPCODE_WRITE_BITS_NOOPCODE(1, 10, (((0xcul<<5) | (crc5_value>>3))<<1) | 1);
				i3c_pio_put32_no_check(cmd1);
				i3c_pio_put32_no_check(cmd2);
				i3c_pio_get32(); // dump read data. This is just used as synchronization point
			}
		}


		// In case no transfer error occured and a request came to finalize with a HDR-restart condition, we'll do so. This allows concatenation of 
		// transfers without SDR phase in between.
		if ( (retcode == i3c_hl_status_ok) && finalize_with_restart )
		{
			// generate HDR Exit pattern
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_DIR(1)); // set SDA to output
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_PATTERN(5, 0x15)); // create 1 0 1 0 1 pattern on SDA
			i3c_pio_put32_no_check(DDR_OPCODE_SCL1);
			i3c_pio_put32_no_check(DDR_OPCODE_SCL0);
			i3c_pio_put32(DDR_OPCODE_SDA_DIR(0)); // release SDA (open drain). Note: SDA state is still 0 which is important for any following I3C start condition
			i3c_pio_wait_tx_empty();
			while (pio0->sm[1].addr != 0); // wait until last instruction is finished processing and stay then in ddr mode
		}
		else
		{
			// generate HDR Exit pattern
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_DIR(1)); // set SDA to output
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_PATTERN(8, 0xaa)); // create 1 0 1 0 1 0 1 0 pattern on SDA
			i3c_pio_put32_no_check(DDR_OPCODE_SCL0_WAIT7); // ensure that SDA is detected low at i2c targets spike filter outputs for proper STOP condition detection
			i3c_pio_put32_no_check(DDR_OPCODE_SCL1_WAIT7); // wait a bit until SDA is released
			i3c_pio_put32(DDR_OPCODE_SDA_DIR(0)); // release SDA (open drain). Note: SDA state is still 0 which is important for any following I3C start condition
			i3c_pio_wait_tx_empty();
			while (pio0->sm[1].addr != 0); // wait until last instruction is finished processing
			s_i3c_program_hdr_sdr_sm(); // back to SDR statemachine
		}

	}

	restore_interrupts(previntstate);
	return retcode;
}

i3c_hl_status_t __not_in_flash_func(i3c_hl_ddr_read)(uint8_t addr, uint8_t command, uint16_t *pdat, uint32_t *pwordcount, bool finalize_with_restart, bool read_crc_on_early_termination)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	uint32_t previntstate = save_and_disable_interrupts();

	if (0 == *pwordcount) // defensive programming, I am proud of myself...
	{
		return i3c_hl_status_param_outofrange; // ...and my proudness fades away cause I return before the end of the function body :-)
	}

	if ( !sm_is_in_ddr_mode )
	{
		if (i3c_ibi_type1_check())
		{
			retcode = i3c_hl_status_ibi;
		}
		if (retcode == i3c_hl_status_ok)
		{
			i3c_start();
			retcode = i3c_arbhdr(NULL);
			if ( retcode == i3c_hl_status_ok )
			{
				i3c_sdr_write( 0x20 );
			}
			if ( (retcode != i3c_hl_status_ibi) && (retcode != i3c_hl_status_ok) ) // on a IBI getting received, don't terminate the transfer -> it has to be handled by i3c_poll function
			{
				i3c_stop();
			}
		}
	}

	if (retcode == i3c_hl_status_ok)
	{ // let's talk HDR-DDR
		uint32_t wordcount = *pwordcount;
		s_i3c_program_hdr_ddr_sm(); // switch to DDR PIO SM. No need to restart it, as the start sequence of PIO code where the PC is at that point of time  is identical
		i3c_pio_set_autopush_bitrev(19);

		uint32_t cmd1, cmd2, cmd3, cmd4, parity;
		uint16_t dat;
		uint8_t  crc5_value = 0x1f<<3;
		command |= 0x80; // for read transfers bit 7 of command has to be set to 1
		dat = ((uint16_t)addr<<1) | (((uint16_t)command)<<8);

		// calculate parity for cmdword
		parity = ((uint32_t)__builtin_parity((dat & 0xaaaaul)) << 1) |      // PA1 = D[15] ^ D[13] ^ D[11] ^ D[9] ^ D[7] ^ D[5] ^ D[3] ^ D[1]
				 ((uint32_t)__builtin_parity((dat & 0x5555ul))  ^ 1);       // PA0 = D[14] ^ D[12] ^ D[10] ^ D[8] ^ D[6] ^ D[4] ^ D[2] ^ D[0] ^ 1 

		// implement a recommendation from i3c spec which is to enforce that PA0 is 1 for faster turnaround
		if ((parity & 1u) == 0)
		{
			parity |= 1u;
			dat ^= 1u;
		}
		cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(1, 0, 1, 1, 18, i3c_ddr_offset_cmd_write_bits, i3c_ddr_offset_cmd_write_bits);
		cmd2 = DDR_HDR_OPCODE_WRITE_BITS_NOOPCODE(1, 18, ((uint32_t)dat<<2) | parity);
		i3c_pio_put32_no_check(cmd1);
		i3c_pio_put32_no_check(cmd2);
		crc5_value = CRC5_CALCULATE(crc5_value, dat); // calculate crc5 during data transfer execution to make use of parallelism
		//printf("%x %x\r\n", dat, crc5_value);
		i3c_pio_get32(); // dump read data. This is used as synchronization point only

		// read first word This one is special, because the target acknowledges during its preamble phase
		uint32_t pioretval;
		cmd1 = DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 0, 0, 0, 18, i3c_ddr_offset_target_nacked, i3c_ddr_offset_cmd_write_bits);
		cmd2 = DDR_HDR_OPCODE_WRITE_BITS_NOOPCODE(0, 18, 0x00000);
		i3c_pio_put32_no_check(cmd1);
		i3c_pio_put32_no_check(cmd2);
		pioretval = i3c_pio_get32(); // dump read data. This is used as synchronization point to enable SM replacement in next step
		dat = (uint16_t)(pioretval >> 3);
		wordcount = 0;
		if (pioretval & 1)
		{  // target NACKed after retreiving READ command (I know, NAK is a passive action)
			retcode = i3c_hl_status_nak_ddr;
		}
		else
		{ // target ACKed
			*pdat++ = dat;
			wordcount = 1;
			crc5_value = CRC5_CALCULATE(crc5_value, dat); // update crc5 with latest dataword
			// check Parity bits
			if ( ((pioretval>>1) & 3) != DDR_PARITY(dat) )
			{
				retcode = i3c_hl_status_ddr_parity_wrong;
				printf("dat: %d pioretval: %d calculated_parity: %d\r\n", dat, pioretval, DDR_PARITY(dat));
			}
		}

		if (retcode == i3c_hl_status_ok)
		{ // read the (potential) rest of the data words word by word...
			bool crc_received = false;     // Set to true in case the target returned a crc value during read. This happens when more words are read as the target can return
			bool early_terminated = false; // Set to true in case the controller executed an early termination action.
			uint8_t preamble_value;        // This will receive the preamble value
			
			cmd1 = DDR_HDR_OPCODE_READ_BITS(12); // Prepare for reading 12 Bits. This is the first part of a transfer where we don't know if its a CRC or data word.
			cmd2 = DDR_HDR_OPCODE_READ_BITS(8);  // Read another 8 bits in case we know a data word is to be received.

			for (wordcount=1; wordcount < *pwordcount; wordcount++)
			{ // During this inner look no early termination has to be signalled by controller. There are only 2 cases the target will signal: Either CRC is received or Data.
				// as we don't know if the target sends a CRC or data, we first make the worst case assumption in terms 
				// of data length, which is CRC with its 10 bits.
				// After those 12 bits are received, we check the preamble. 
				//	 If it is 01 a CRC word was sent and we stop.
				//   If it is 11 a Data word is being send and we transfer 8 more bits. Unfortunately this scheme creates a timing gap 
				//   during data word reading, but we live for it for now until I am more motivated to improve it.
				i3c_pio_set_autopush_bitrev(13);
				i3c_pio_put32_no_check(cmd1);
				pioretval = i3c_pio_get32();      // dump read data. This is used as synchronization point to enable SM replacement in next step
				preamble_value = (pioretval >> (1+10)); // 01 in case of CRC, 11 in case of data word, other values: report error
				if (preamble_value == 3)
				{ // we need to transfer 8 more bits
					i3c_pio_set_autopush_bitrev(9);
					i3c_pio_put32_no_check(cmd2);
					dat = ((uint16_t)pioretval<<5) & 0xffc0;   // The 10 lower bits of pioretval contain the 10 MSBs of the read data word
					pioretval = i3c_pio_get32();               // Dump read data. This is used as synchronization point to enable SM replacement in next step
					dat |= (uint16_t)pioretval>>3;             // Complement the last 6 missing bits. The 2 skipped bits are the parity
					*pdat++ = dat;
					crc5_value = CRC5_CALCULATE(crc5_value, dat); // Update crc5 with latest dataword
					// check Parity bits
					if ( ((pioretval>>1) & 3) != DDR_PARITY(dat) )
					{
						retcode = i3c_hl_status_ddr_parity_wrong;
						break;
					}
				}
				else if (preamble_value == 1)
				{ // CRC received, no further transfer requred
					crc_received = true;
					// check if CRC is correct. If not -> raise an error. Note that the data is still written to the receive buffer.
					if ( (crc5_value>>3) != ((pioretval>>2) & 0x1f) )
					{
						retcode == i3c_hl_status_ddr_crc_wrong;
					}
					break;
				}
				else
				{ // we received an invalid preamble, e.g. 00 or 10 -> raise an error and go to HDR EXIT phase...
					retcode = i3c_hl_status_ddr_invalid_preamble;
					break;
				}
			}

			// TODO: Check if early abortion is required and if so ... do it by transmitting the 2 magic bits...
			// Early termination is required when no CRC was received in previous phase OR
			// a Parity or CRC error occured
			if ( ((retcode == i3c_hl_status_ok) && (!crc_received)) ||
			     (retcode == i3c_hl_status_ddr_invalid_preamble) || (retcode == i3c_hl_status_ddr_crc_wrong) )
			 
			{ // let's terminate "early" -> This means transmitting 2 preamble bits with state 10 as binary value. Only the 0 is actively driven by controller
				i3c_pio_put32_no_check( DDR_HDR_OPCODE_WRITE_PREAMBLE(0, 0, 1, 0, 18, i3c_ddr_offset_target_nacked, i3c_ddr_offset_target_nacked) );
				i3c_pio_put32_no_check(0x00000000ul);
				pioretval = i3c_pio_get32(); // dump read data. This is used as synchronization point to enable SM replacement in next step

				// If the target supports CRC transmission after early termination AND the reason of the early termination was no error...
				// ...let's read the CRC and check it
				if  ( (retcode == i3c_hl_status_ok) && (read_crc_on_early_termination) )
				{
					i3c_pio_set_autopush_bitrev(13);
					i3c_pio_put32_no_check(cmd1);
					pioretval = i3c_pio_get32();      // dump read data. This is used as synchronization point to enable SM replacement in next step
					if ( (crc5_value>>3) != ((pioretval>>2) & 0x1f) )
					{
						retcode == i3c_hl_status_ddr_crc_wrong; // When we get here, it can also mean that the target does NOT support sending a CRC on early termination. For the V1.0 target I test this is the case. I suspect this feature got only introduced in >= V1.1 spec version
					}
				}
			}
		}

		*pwordcount = wordcount;

		// In case no transfer error occured and a request came to finalize with a HDR-restart condition, we'll do so. This allows concatenation of 
		// transfers without SDR phase in between.
		if ( (retcode == i3c_hl_status_ok) && finalize_with_restart )
		{
			// generate HDR Exit pattern
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_DIR(1)); // set SDA to output
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_PATTERN(5, 0x15)); // create 1 0 1 0 pattern on SDA
			i3c_pio_put32_no_check(DDR_OPCODE_SCL0);
			i3c_pio_put32(DDR_OPCODE_SDA_DIR(0)); // release SDA (open drain). Note: SDA state is still 0 which is important for any following I3C start condition
			i3c_pio_wait_tx_empty();
			while (pio0->sm[1].addr != 0); // wait until last instruction is finished processing
		}
		else
		{
			// generate HDR Exit pattern
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_DIR(1)); // set SDA to output
			i3c_pio_put32_no_check(DDR_OPCODE_SDA_PATTERN(8, 0xaa)); // create 1 0 1 0 1 0 1 0 pattern on SDA
			i3c_pio_put32_no_check(DDR_OPCODE_SCL0_WAIT7); // ensure that SDA is detected low at i2c targets spike filter outputs for proper STOP condition detection
			i3c_pio_put32_no_check(DDR_OPCODE_SCL1_WAIT7); // wait a bit until SDA is released
			i3c_pio_put32(DDR_OPCODE_SDA_DIR(0)); // release SDA (open drain). Note: SDA state is still 0 which is important for any following I3C start condition
			i3c_pio_wait_tx_empty();
			while (pio0->sm[1].addr != 0); // wait until last instruction is finished processing
			s_i3c_program_hdr_sdr_sm();
		}


	}

	restore_interrupts(previntstate);
	return retcode;
}
