
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
	}
	s_i3c_program_hdr_sdr_sm();

	i3c_hl_gpiobasepin = gpiobasepin;
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
						  (              1 << PIO_SM0_EXECCTRL_SIDE_EN_LSB);
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
    	case i3c_hl_status_ok                  : sprintf(errstring, "OK(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_ibi                 : sprintf(errstring, "ERR_IBI_ARBITRATION(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_nak_during_arbhdr   : sprintf(errstring, "ERR_NAKED_DURING_ARBHDR(%d)", (uint32_t)errcode); break;
    	case i3c_hl_status_nak_during_sdraddr  : sprintf(errstring, "ERR_NAKED(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_param_outofrange    : sprintf(errstring, "ERR_INVALID_PARAMETER(%d)", (uint32_t)errcode); break;
		case i3c_hl_status_no_ibi              : sprintf(errstring, "WARN_NO_IBI(%d)", (uint32_t)errcode); break;
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

static uint8_t inline __not_in_flash_func(s_crc5)(uint8_t crc, uint16_t *pdata, uint32_t wordcount)
{
    uint16_t i, data;
    uint8_t bit;


	while (wordcount--)
	{
		data = *pdata++;
		for (i = 0x8000; i > 0; i >>= 1)
		{
			bit = (crc & 0x10) ^ ((data & i) ? 0x10 : 0);
			crc <<= 1;
			if (bit)
			{
				crc ^= 0x05;
			}
		}
		crc &= 0x1f;
	}
    return crc;
}

