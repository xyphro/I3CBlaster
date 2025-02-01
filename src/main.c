#include <stdio.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/structs/adc.h"
#include "hardware/adc.h"
#include "i3c_hl.h"
#include "ucli.h"
#include "tusb.h"
#include "hardware/timer.h"
#include <stdlib.h>

#include "hardware/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"
#include "XiaoNeoPixel.h"

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


// some global runtime user adjustable settings (see i3c_ddr_config function)
static bool i3c_ddr_config_crc_word_indicator = true;
static bool i3c_ddr_config_enable_early_write_term = false;
static bool i3c_ddr_config_write_ack_enable = false;



static void parse_array_string(const char *str, uint8_t *ppayload, uint32_t *ppayloadlen)
{
	uint8_t payload[1024];
	uint32_t maxpayloadlen = *ppayloadlen;
	uint32_t payloadlen=0;
	const char *pstart, *pstr;

	if (str)
	{
		bool stop = false;
		pstr = str;
		pstart = str;
		do
		{
			if ((*pstart != ',') && (*pstart))
			{
				int32_t num = strtol(pstart, (char **)&(pstr), 0);
				if ((num >= 0) && (num <= 0xff))
				{
					if (payloadlen < maxpayloadlen)
					{
						ppayload[payloadlen++] = (uint8_t)num;
					}
					else
					{
						ucli_error("payload too long");
						stop = true;
					}
					
				}
				else
				{
					ucli_error("payload byte values have to be in range 0..255");
					stop = true;
				}
				//pstr++;
				pstart = pstr;
			}
			else if (*pstart)
			{
				pstr++;
				pstart++;
			}
		}
		while (*pstr && !stop); // while end of string is not reached
	}
	*ppayloadlen = payloadlen;
}

static void parse_array_string_uint16(const char *str, uint16_t *ppayload, uint32_t *ppayloadlen)
{
	uint8_t payload[1024];
	uint32_t maxpayloadlen = *ppayloadlen;
	uint32_t payloadlen=0;
	const char *pstart, *pstr;

	if (str)
	{
		bool stop = false;
		pstr = str;
		pstart = str;
		do
		{
			if ((*pstart != ',') && (*pstart))
			{
				int32_t num = strtol(pstart, (char **)&(pstr), 0);
				if ((num >= 0) && (num <= 0xffff))
				{
					if (payloadlen < maxpayloadlen)
					{
						ppayload[payloadlen++] = (uint16_t)num;
					}
					else
					{
						ucli_error("payload too long");
						stop = true;
					}
					
				}
				else
				{
					ucli_error("payload byte values have to be in range 0..65535");
					stop = true;
				}
				//pstr++;
				pstart = pstr;
			}
			else if (*pstart)
			{
				pstr++;
				pstart++;
			}
		}
		while (*pstr && !stop); // while end of string is not reached
	}
	*ppayloadlen = payloadlen;
}


void ucli_print(const char *str)
{
	printf("%s", str);
}

UCLI_COMMAND_DEF(i3c_sdr_write, "Execute a private write transfer to a target",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_OPTIONAL_STR_ARG_DEF(payload, "Optional payload data - byte values seperated with comma without whitespaces (e.g. 0x12,0x43,0x56)")
)
{
	uint8_t payload[1024];
	uint32_t payloadlen=sizeof(payload);
	i3c_hl_status_t retcode;

	parse_array_string(args->payload, payload, &payloadlen);
	
	retcode = i3c_hl_sdr_privwrite(args->addr, payload, payloadlen);
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_sdr_ccc_bc_write, "Execute a ccc broadcast write transfer",
    UCLI_STR_ARG_DEF(payload, "The data payload to write in the broadcast transfer - byte values seperated with comma without whitespaces (e.g. 0x12,0x43,0x56)")
)
{
	uint8_t payload[1024];
	uint32_t payloadlen=sizeof(payload);
	i3c_hl_status_t retcode;

	parse_array_string(args->payload, payload, &payloadlen);

	retcode = i3c_hl_sdr_ccc_broadcast_write(payload, payloadlen);
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_sdr_ccc_direct_write, "Execute a ccc direct write transfer",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_STR_ARG_DEF(bc_payload, "The data payload to write during broadcast phase - byte values seperated with comma without whitespaces (e.g. 0x12,0x43,0x56)"),
    UCLI_STR_ARG_DEF(dir_payload, "The data payload to write during slave addressing phase - byte values seperated with comma without whitespaces (e.g. 0x12,0x43,0x56)")
)
{
	uint8_t  bc_payload[1024];
	uint32_t bc_payloadlen=sizeof(bc_payload);
	uint8_t  direct_payload[1024];
	uint32_t direct_payloadlen=sizeof(direct_payload);
	i3c_hl_status_t retcode;

	parse_array_string(args->bc_payload, bc_payload, &bc_payloadlen);
	parse_array_string(args->dir_payload, direct_payload, &direct_payloadlen);
	retcode =  i3c_hl_sdr_ccc_direct_write(bc_payload, bc_payloadlen,
												args->addr, direct_payload, direct_payloadlen);
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_sdr_ccc_direct_read, "Execute a ccc direct read transfer",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_STR_ARG_DEF(payload, "The data payload to write during write phase of transfer - byte values seperated with comma without whitespaces (e.g. 0x12,0x43,0x56)"),
    UCLI_INT_ARG_DEF(len, "The count of bytes to read")
)
{
	uint8_t  payload[1024];
	uint32_t payloadlen=sizeof(payload);
	uint8_t  rxdata[1024];
	uint32_t rxlen;
	i3c_hl_status_t retcode;

	rxlen = args->len;
	parse_array_string(args->payload, payload, &payloadlen);
	retcode = i3c_hl_sdr_ccc_direct_read(payload, payloadlen, args->addr, rxdata, &rxlen);
	printf("%s", i3c_hl_get_errorstring(retcode));
	if (retcode == i3c_hl_status_ok)
	{
		for (uint32_t i=0; i<rxlen; i++)
			printf(",0x%02x", rxdata[i]);
	}
	printf("\r\n");
}

UCLI_COMMAND_DEF(i3c_sdr_writeread, "Execute a private combined write read transfer from a target",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_STR_ARG_DEF(payload, "The data payload to write during write phase of transfer"),
    UCLI_INT_ARG_DEF(len, "The count of bytes to read (0..255)")
)
{
	uint8_t  payload[1024];
	uint32_t payloadlen=sizeof(payload);
	uint8_t  rxdata[1024];
	uint32_t rxlen;
	i3c_hl_status_t retcode;

	rxlen = args->len;
	parse_array_string(args->payload, payload, &payloadlen);
	retcode = i3c_hl_sdr_privwriteread(args->addr, payload, payloadlen, rxdata, &rxlen);
	printf("%s", i3c_hl_get_errorstring(retcode));
	if (retcode == i3c_hl_status_ok)
	{
		for (uint32_t i=0; i<rxlen; i++)
			printf(",0x%02x", rxdata[i]);
	}
	printf("\r\n");
}

UCLI_COMMAND_DEF(i3c_sdr_read, "Execute a private read transfer from a target",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_INT_ARG_DEF(len, "The count of bytes to read (0..255)")
)
{
	bool success;
	const char *pstart, *pstr;
	uint8_t  payload[1024];
	uint32_t payloadlen=0;
	i3c_hl_status_t retcode;

	payloadlen = args->len;
	retcode = i3c_hl_sdr_privread(args->addr, payload, &payloadlen);
	printf("%s", i3c_hl_get_errorstring(retcode));
	if (retcode == i3c_hl_status_ok)
	{
		for (uint32_t i=0; i<payloadlen; i++)
			printf(",0x%02x", payload[i]);
	}
	printf("\r\n");
}


UCLI_COMMAND_DEF(i3c_rstdaa, "Execute i3crstdaa broadcast transfer. This will unassign all targets dynamic addresses"
)
{
	i3c_hl_rstdaa();
	printf("%s\r\n", i3c_hl_get_errorstring(i3c_hl_status_ok));
}

UCLI_COMMAND_DEF(i3c_scan, "Scan for available i3c addreses"
)
{
	bool notfirst = false;
	printf("%s,", i3c_hl_get_errorstring(i3c_hl_status_ok));
	for (uint8_t addr=0; addr<0x80; addr++)
	{
		if ( (addr != 0x7F) && (addr != 0x7C) && (addr != 0x7A) && (addr != 0x76) && (addr != 0x6E) && (addr != 0x5E) && (addr != 0x3E) )
		{
			if ( i3c_hl_checkack(addr) == i3c_hl_status_ok )
			{
				if (notfirst)
					printf(",");
				printf("0x%02x", addr);
				notfirst = true;
			}
		}
	}
	i3c_hl_arbhdronly(); // free the bus
	printf("\r\n");
}



UCLI_COMMAND_DEF(i3c_entdaa, "Execute i3c entdaa procedure",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address to assign to the target")
)
{
	uint8_t id[8];
	i3c_hl_status_t retcode;
	if ( (args->addr < 0) || (args->addr >= 0x80) )
	{
		ucli_error("address has to be in range  0..0x7f");
		return;
	}
	retcode = i3c_hl_entdaa((uint8_t)args->addr, id);
	printf("%s", i3c_hl_get_errorstring(retcode));
	if ( retcode == i3c_hl_status_ok )
	{
		for (int i=0; i<8; i++)
		{
			printf(",0x%02x", id[i]);
		}
	}
	printf("\r\n");
}

UCLI_COMMAND_DEF(i3c_clk, "Set I3C clock frequency",
    UCLI_INT_ARG_DEF(freq_khz, "The Clock frequency as integer number in units of kHz. Valid range: 49..12500")
)
{
	i3c_hl_status_t retcode;
	retcode = i3c_hl_set_clkrate(args->freq_khz);
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_targetreset, "Execute a Targetreset sequence on I3C Bus."
)
{
	i3c_hl_status_t retcode;
	retcode = i3c_hl_targetreset();
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}


UCLI_COMMAND_DEF(i3c_poll, "Poll / handle interrupt"
)
{
	i3c_hl_status_t retcode;
	uint8_t ibidata[16];
	uint32_t ibidatalen;

	ibidatalen = sizeof(ibidata);
    retcode = i3c_hl_poll(ibidata, &ibidatalen);
	printf("%s", i3c_hl_get_errorstring(retcode));
	if (retcode == i3c_hl_status_ok)
	{
		uint32_t cnt=0;
		while (ibidatalen--)
		{
			printf(",0x%02x", ibidata[cnt++]);
		}		
	}
	printf("\r\n");
}


UCLI_COMMAND_DEF(info, "Show information about this version")
{
    ucli_print("Supported protocols: I3C SDR, I3C OD\r\n");
	printf("Automated command mode:\r\n");
	printf("-----------------------\r\n");
	printf("By sending @ as first character for each command, there is \r\nno character echoed back for easier automation fromout PC\r\n" );

	uint32_t addr = PIO0_BASE;

	for (addr = PIO0_BASE; addr <= (PIO0_BASE+0x128); addr+=4)
	{
		uint32_t data = *((volatile uint32_t *)addr);
		printf("[%04x] = %08x\n", addr-PIO0_BASE, data);

	}
}

UCLI_COMMAND_DEF(clkinfo, "Show information of the MCUs clock setup")
{
    uint f_pll_sys  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc     = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    printf("pll_usb  = %dkHz\n", f_pll_usb);
    printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\n", f_clk_sys);
    printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb  = %dkHz\n", f_clk_usb);
    printf("clk_adc  = %dkHz\n", f_clk_adc);
    printf("clk_rtc  = %dkHz\n", f_clk_rtc);
}

UCLI_COMMAND_DEF(gpio_write, "Sets a GPIO pin state and direction",
    UCLI_INT_ARG_DEF(port, "The GPIO pin number (0..29)"),
	UCLI_STR_ARG_DEF(state, "0 to set pin to OUTPUT LOW, 1 to set pin to OUTPUT HIGH, Z to set pin to tristate (input)")
)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	if ( (strlen(args->state) == 1) && (args->port >= 0) && (args->port < 32) )
	{
		char c = toupper(args->state[0]);
		if (c == '0')
		{
			gpio_put(args->port, 0);
			gpio_set_dir(args->port, true);
		}
		else if (c == '1')
		{
			gpio_put(args->port, 1);
			gpio_set_dir(args->port, true);
		}
		else if (c == 'Z')
		{
			gpio_set_dir(args->port, false);
		}
		else
		{
			retcode = i3c_hl_status_param_outofrange;
		}
	}
	else
	{
		retcode = i3c_hl_status_param_outofrange;
	}
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(gpio_read, "Get gpio state. If no parameter is given a 32 Bit number containing all GPIOs states is returned",
    UCLI_OPTIONAL_INT_ARG_DEF(gpionum, "Optional gpio number to read GPIO state from (0..29)")
)
{
	uint32_t gpiostate, i;
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	gpiostate = 0;
	for (i=0; i<30; i++)
	{
		if ((iobank0_hw->io[i].status & IO_BANK0_GPIO0_STATUS_INFROMPAD_BITS) >> IO_BANK0_GPIO0_STATUS_INFROMPAD_LSB > 0)
		{
			gpiostate |= (1<<i);
		}
	}
	if (args->gpionum != UCLI_INT_ARG_DEFAULT)
	{ 
		if ( (args->gpionum > 31) || (args->gpionum < 0) )
			retcode = i3c_hl_status_param_outofrange;
		gpiostate = (gpiostate >> args->gpionum) & 1;
	}
	printf("%s,%d\r\n", i3c_hl_get_errorstring(retcode), gpiostate);
}

UCLI_COMMAND_DEF(i3c_ddr_config, "Configure I3C behavior/ I3C target capability. Look into ENDXFER CCC for complete explanation",
    UCLI_INT_ARG_DEF(crc_word_indicator, "specify 1 for 'No CRC Word follows Early termination request' (default). Specify 0 for 'CRC Word follows early Termination request'."),
    UCLI_INT_ARG_DEF(enable_early_write_term, "Enable (1) or disable (0=default) early write termination request."),
	UCLI_INT_ARG_DEF(write_ack_enable, "Enable (1) or disable (0=default) ack/nack capabilify for write commands (V1.0 targets don't support this).")
)
{
	i3c_hl_status_t retcode = i3c_hl_status_ok;

	i3c_ddr_config_crc_word_indicator = args->crc_word_indicator != 0;
	i3c_ddr_config_enable_early_write_term = args->enable_early_write_term != 0;
	i3c_ddr_config_write_ack_enable = args->write_ack_enable != 0;

	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_ddr_write, "Execute a HDR-DDR mode write transfer to a target. The function returns error code and how many words have actually been written.",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_INT_ARG_DEF(cmd, "The 7-bit  command parameter (used in command phase)."),
    UCLI_STR_ARG_DEF(payload, "Payload data - 16-bit word values seperated with comma without whitespaces (e.g. 0x1234,0x5678)")
)
{
	uint16_t payload[1024];
	uint32_t payloadlen=sizeof(payload);
	i3c_hl_status_t retcode;

	parse_array_string_uint16(args->payload, payload, &payloadlen);
	
	retcode =  i3c_hl_ddr_write((uint8_t)args->addr, (uint8_t)args->cmd, payload, &payloadlen, false,
                                 i3c_ddr_config_write_ack_enable, i3c_ddr_config_enable_early_write_term, i3c_ddr_config_crc_word_indicator); // those settings can be adjusted by the user calling the i3c_ddr_config function and have to match the targets spec version / ENDXFER CCC setting

	printf("%s,%d\r\n", i3c_hl_get_errorstring(retcode), payloadlen);
}

UCLI_COMMAND_DEF(i3c_ddr_read, "Execute a HDR-DDR mode read transfer from a target. The function returns error code and the read data words",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_INT_ARG_DEF(cmd, "The 7-bit  command parameter (used in command phase). The 7th bit of the command value will be forced high internally to signal a read transfer"),
    UCLI_INT_ARG_DEF(wordcount, "The count of bytes to read")
)
{
	uint16_t payload[1024];
	uint32_t payloadlen=sizeof(payload);
	i3c_hl_status_t retcode;

	payloadlen = args->wordcount;
	retcode = i3c_hl_ddr_read(args->addr , args->cmd, payload, &payloadlen, false, i3c_ddr_config_enable_early_write_term);

	printf("%s", i3c_hl_get_errorstring(retcode));
	if (retcode == i3c_hl_status_ok)
	{
		for (uint32_t i=0; i<payloadlen; i++)
			printf(",0x%04x", payload[i]);
	}
	printf("\r\n");
}

UCLI_COMMAND_DEF(i3c_ddr_writeread, "Execute a HDR-DDR mode write transfer followed by a read. The function returns error code and how many words have actually been written and read data words",
    UCLI_INT_ARG_DEF(addr, "The 7-Bit address of the target"),
    UCLI_INT_ARG_DEF(wrcmd, "The 7-bit command parameter used for the write phase (used in command phase)."),
    UCLI_INT_ARG_DEF(rdcmd, "The 7-bit command parameter used for the readphase (used in command phase)."),
    UCLI_STR_ARG_DEF(payload, "Payload data - 16-bit word values seperated with comma without whitespaces (e.g. 0x1234,0x5678)"),
    UCLI_INT_ARG_DEF(wordcount, "The count of bytes to read")
)
{
	uint16_t payload[1024];
	uint32_t payloadlen=sizeof(payload), readpayloadlen;
	i3c_hl_status_t retcode;

	parse_array_string_uint16(args->payload, payload, &payloadlen);
	
	retcode =  i3c_hl_ddr_write((uint8_t)args->addr, (uint8_t)args->wrcmd, payload, &payloadlen, true,
                                 i3c_ddr_config_write_ack_enable, i3c_ddr_config_enable_early_write_term, i3c_ddr_config_crc_word_indicator); // those settings can be adjusted by the user calling the i3c_ddr_config function and have to match the targets spec version / ENDXFER CCC setting
	if (retcode == i3c_hl_status_ok)
	{
		readpayloadlen = args->wordcount;
		retcode = i3c_hl_ddr_read(args->addr , args->rdcmd, payload, &readpayloadlen, false, i3c_ddr_config_enable_early_write_term);
		if (retcode != i3c_hl_status_ok)
		{
			readpayloadlen = 0;
		}
	}

	printf("%s,%d", i3c_hl_get_errorstring(retcode), payloadlen);

	if (retcode == i3c_hl_status_ok)
	{
		for (uint32_t i=0; i<payloadlen; i++)
			printf(",0x%04x", payload[i]);
	}
	printf("\r\n");
}


UCLI_COMMAND_DEF(i3c_drivestrength, "Set the drive strength of SDA and SCL outputs of the controller. This helps in addressing signal integrity issues e.g. minimize crosstalk",
    UCLI_INT_ARG_DEF(strength, "2 for 2mA, 4 for 4mA, 8 for 8mA, 12 for 12mA (default)")
	)
{
	i3c_hl_status_t retcode;
	retcode = i3c_hl_set_drivestrength(args->strength);
	printf("%s\r\n", i3c_hl_get_errorstring(retcode));
}

UCLI_COMMAND_DEF(i3c_gpiobase, "set gpio base pin for i3c. The specified GPIO pin will be the SDA line, the SCL line will be the specified GPIO+1.",
    UCLI_INT_ARG_DEF(gpiobase, "The gpio base pin. Valid range: 0..27.")
)
{
	uint8_t id[8];
	i3c_hl_status_t retcode = i3c_hl_status_ok;
	if ( (args->gpiobase > 27) )
	{
		ucli_error("gpiobase has to be in range 0..27");
		return;
	}
	i3c_init(args->gpiobase);
	printf("%s", i3c_hl_get_errorstring(retcode));
	printf("\r\n");
}

UCLI_COMMAND_DEF(i3c_signaltest, "Some tests for characterizing level translation. The module needs to be reset after using it. To avoid misuse, I disable it below, but I want to conserve the code",
    UCLI_INT_ARG_DEF(togglemode, "0 for scl toggle, 1 for sda push-pull toggle, 2 for scl push-pull toggle, 3 for SDA reverse toggle test.")
)
{
	uint8_t id[8];

	i3c_hl_status_t retcode = i3c_hl_status_ok;
	if ( args->togglemode == 0 )
	{
    	gpio_init(7);
		gpio_set_dir(7, GPIO_OUT);
		gpio_set_function(7, GPIO_FUNC_SIO);

		for (uint8_t i=0; i<100; i++)
		{
			gpio_xor_mask(1<<7);
			sleep_us(1);
		}
	}
	else if (args->togglemode == 1)
	{
    	gpio_init(6);
		gpio_set_dir(6, GPIO_OUT);
		gpio_set_function(6, GPIO_FUNC_SIO);

		for (uint8_t i=0; i<100; i++)
		{
			gpio_xor_mask(1<<6);
			sleep_us(1);
		}
	}
	else if (args->togglemode == 2)
	{
    	gpio_init(6);
		gpio_set_function(6, GPIO_FUNC_SIO);

		gpio_put(6, 0);
		for (uint8_t i=0; i<100; i++)
		{
			gpio_set_dir(6, GPIO_OUT);
			sleep_us(1);
			gpio_set_dir(6, GPIO_IN);
			sleep_us(1);
		}
	}
	else if (args->togglemode == 3)
	{
    	gpio_init(29);
		gpio_set_dir(29, GPIO_OUT);
		gpio_set_function(29, GPIO_FUNC_SIO);
		gpio_put(29, 0);

		for (uint8_t i=0; i<100; i++)
		{
			gpio_set_dir(29, GPIO_OUT);
			sleep_us(1);
			gpio_set_dir(29, GPIO_IN);
			sleep_us(1);
		}
	}
	
	printf("%s", i3c_hl_get_errorstring(retcode));
	printf("\r\n");
}



bool usb_newly_connected(void)
{
	static bool was_connected = false;
	bool is_connected, result;
	is_connected = tud_cdc_connected();

	result = is_connected && !was_connected;
	was_connected = is_connected;
	return result;
}

bool is_xiao = true;
uint8_t comm_active = 0;
uint64_t last_timer;

int main()
{
	stdio_init_all();

	is_xiao = xnp_is_xiao_module();

	// initialize gpio 0..15 as general purpose GPIOs with state = input
	for (uint32_t gpio=0; gpio < 16; gpio++)
	{
		gpio_init(gpio);
	}

    ucli_init();
    ucli_cmd_register(gpio_write);
	ucli_cmd_register(gpio_read);
    ucli_cmd_register(info);	
	ucli_cmd_register(i3c_drivestrength);
	ucli_cmd_register(i3c_targetreset);
	ucli_cmd_register(i3c_clk);
	ucli_cmd_register(i3c_scan);
	ucli_cmd_register(i3c_entdaa);
	ucli_cmd_register(i3c_rstdaa);
	ucli_cmd_register(i3c_sdr_write);
	ucli_cmd_register(i3c_sdr_read);
	ucli_cmd_register(i3c_sdr_writeread);
	ucli_cmd_register(i3c_sdr_ccc_bc_write);
	ucli_cmd_register(i3c_sdr_ccc_direct_write);
	ucli_cmd_register(i3c_sdr_ccc_direct_read);
	ucli_cmd_register(i3c_poll);
	ucli_cmd_register(i3c_ddr_config);
	ucli_cmd_register(i3c_ddr_write);
	ucli_cmd_register(i3c_ddr_read);
	ucli_cmd_register(i3c_ddr_writeread);
	//ucli_cmd_register(i3c_gpiobase); // With xiao module autodetection this function is not required anymore.
	//ucli_cmd_register(i3c_signaltest); // enable only during development phase
	
	if (is_xiao)
	{
		xnp_init();
		xnp_send(0, 0, 0);

		adc_init();
    	adc_gpio_init(26);
		adc_select_input(0); // GPIO26 = ADC0 channel. Maps to PA02_A0_D0 pin of Xiao rp2040 module
		adc_run(true);
		last_timer = time_us_64();

		// enable edge accelerator
    	gpio_init(0);
		gpio_set_dir(0, GPIO_OUT);
		gpio_put(0, 1);
		gpio_set_function(0, GPIO_FUNC_SIO);

	}
	
	// initialize i3c to use GPIO16=SDA and GPIO17=SCL (raspberry pico board)
	// xiao rp2040 base i2c pin is gpio6
	if (is_xiao)
		i3c_init(6);
	else
		i3c_init(16);

	while (1) 
	{
		if (usb_newly_connected())
		{
			sleep_ms(500);
			ucli_init(); // this reprints the welcome message
		}
		else if (!tud_cdc_connected())
		{ // not connected
			sleep_ms(10);
		}

		// put received char from stdin to ucli lib
		int ch = getchar_timeout_us(0);
		if (ch >= 0)
		{
        	ucli_process((char)ch);
			comm_active = 3; // keep Neolight >=300ms in ON state every time a character was received
		}

		if (is_xiao)
		{
			uint64_t tval = time_us_64();
			if ( (tval-last_timer) >= (100ul * 1000ul) ) // update light every 100ms
			{ // triggers every 100ms
				//printf("adc %d\n", (adc_hw->result));
				last_timer = tval;
				if (comm_active)
				{
					xnp_send(0, 0, 255);
					comm_active--;
				}
				else
				{					
					uint8_t col = (adc_hw->result) >> 4; // 8 bit adc value. 255 = 6.6V
					if (col < 33) // if VIO < 0.9V show only a dark red light
						xnp_send(64, 0, 0);
					else // if VIO >= 0.9V show a green light with scaled brightness (the higher the brighter)
						//uint8_t scaler = ((uint32_t)col-34)*255(/(255-34);
						xnp_send(0, col, (col-34)/4);
				}
			}
		}
	}
}

/*
i3c_drivestrength 2
i3c_sdr_write 0x70 0x13,0x04
i3c_ddr_write 0x70 0x00 0x1234,0x2345
i3c_ddr_read 0x70 0x00 10
*/