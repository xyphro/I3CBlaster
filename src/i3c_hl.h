#ifndef _I3C_HL_H
#define _I3C_HL_H

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

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    i3c_hl_status_ok                     = 0,
    i3c_hl_status_ibi,          // arbitration happened during arbitration header sending or idle type of IBI signalled. Requested I3C transfer was not executed
    i3c_hl_status_nak_during_arbhdr,
    i3c_hl_status_nak_during_sdraddr, // nak occured during SDR based addressing phase
    i3c_hl_status_param_outofrange,
    i3c_hl_status_no_ibi              // used by poll function to signal that no ibi is received
} i3c_hl_status_t;

i3c_hl_status_t i3c_init(uint8_t gpiobasepin);
const char     *i3c_hl_get_errorstring(i3c_hl_status_t errcode);
i3c_hl_status_t i3c_hl_set_clkrate(uint32_t targetfreq_khz);
i3c_hl_status_t i3c_hl_targetreset(void);

i3c_hl_status_t i3c_hl_entdaa(uint8_t addr, uint8_t *pid);
i3c_hl_status_t i3c_hl_rstdaa(void);
i3c_hl_status_t i3c_hl_checkack(uint8_t addr);
i3c_hl_status_t i3c_hl_arbhdronly(void); // send START, arbhdr, STOP only

// check if interrupt or HJ request is raised
i3c_hl_status_t i3c_hl_poll(uint8_t *pdat, uint32_t *plen);

i3c_hl_status_t i3c_hl_sdr_privwrite(uint8_t addr, const uint8_t *pdat, uint32_t bytecount);
i3c_hl_status_t i3c_hl_sdr_privread(uint8_t addr, uint8_t *pdat, uint32_t *pbytecount);
i3c_hl_status_t i3c_hl_sdr_privwriteread(uint8_t addr, const uint8_t *pwritedat, uint32_t writebytecount,
                                                             uint8_t *preaddat, uint32_t *preadbytecount);

i3c_hl_status_t i3c_hl_sdr_ccc_broadcast_write(const uint8_t *pdat, uint32_t bytecount);
i3c_hl_status_t i3c_hl_sdr_ccc_direct_write(const uint8_t *pdat, uint32_t bytecount,
							       				  uint8_t addr, const uint8_t *pdirectdat, uint32_t directbytecount);
i3c_hl_status_t i3c_hl_sdr_ccc_direct_read(const uint8_t *pdat, uint32_t bytecount,
										         uint8_t addr, uint8_t *pdirectdat, uint32_t *pdirectbytecount);


i3c_hl_status_t i3c_hl_hdrexit(void);
i3c_hl_status_t i3c_hl_hdrrestart(void);

#endif //_I3C_HL_H
