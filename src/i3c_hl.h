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
    i3c_hl_status_ibi,                   // arbitration happened during arbitration header sending or idle type of IBI signalled. Requested I3C transfer was not executed
    i3c_hl_status_nak_during_arbhdr,
    i3c_hl_status_nak_during_sdraddr,    // nak occured during SDR based addressing phase
    i3c_hl_status_param_outofrange,
    i3c_hl_status_no_ibi,                // used by poll function to signal that no ibi is received
    i3c_hl_status_nak_ddr,               // nak occured during DDR transfer
    i3c_hl_status_ddr_early_termination, // Early termination requested by target during ddr write. This does not necessarily mean that an error occured
    i3c_hl_status_ddr_invalid_preamble,  // Unexpected preamble value received
    i3c_hl_status_ddr_parity_wrong,      // Incorrect HDR-CCC value received during HDR-DDR read transfer
    i3c_hl_status_ddr_crc_wrong          // Incorrect CCC received during HDR-DDR read transfer
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

// Execute a DDR write transfer. 
// The parameter finalize_with_restart allows contatenation of transfers by finalizing with a HDR-restart condition instead of an HDR-exit in case no transfer errors occured
// Due to compatibility to V1.0 spec which most DDR targets on the market comply right now (2024) and ENDXFER settings this 
// creates a bit complexity for the user.
// ack_nack_enable                 Selects if the target supports sending ACKs. This is the case for targets that comply with V1.1 spec,
//                                 have flow control implemented and have Bit 4 of ENDXFER subcommand 0xf7 bit 4 set to 1.
// early_write_termination_enabled selects if the target supports sending early termination requests. This is the case for targets that 
//                                 comply with V1.1 spec, have flow control implemented and ENDXFER subcommand 0xf7 bit 5 set to 1.
// send_crc_on_early_termination   selects if the target supports sending early termination requests. This is the case for targets that 
//                                 comply with V1.1 spec, have flow control implemented and ENDXFER subcommand 0xf7 bit 7..6 set to bitvalue 01.
// For V1.0 targets you can set all those 3 parameters to false.
i3c_hl_status_t i3c_hl_ddr_write(uint8_t addr, uint8_t command, uint16_t *pdat, uint32_t *pwordcount, bool finalize_with_restart,  
                                 bool ack_nack_enable, bool early_write_termination_enabled, bool send_crc_on_early_termination); // These 3 configuration parameters are coming partly due to legacy nightmare to V1.0 spec. See ENDXFER CCC / "Target Ends or continues the write transfer" section of spec

// Execute a DDR read transfer.
// The parameter finalize_with_restart allows contatenation of transfers by finalizing with a HDR-restart condition instead of an HDR-exit in case no transfer errors occured
// Due to compatibility to V1.0 spec which most DDR targets on the market comply right now (2024) and ENDXFER settings this 
// creates a bit complexity for the user.
// send_crc_on_early_termination   This selects if when the controller requests early termination, if the target will still send a CRC word.
//                                 This has to match the ENDXFER sucommand 0xf7 bit 7..6 setting. In case this is binary 01, this parameter
//                                 must be set to true.
// For V1.0 targets, set this parameter to false.
i3c_hl_status_t i3c_hl_ddr_read(uint8_t addr, uint8_t command, uint16_t *pdat, uint32_t *preadcount, bool finalize_with_restart,
                                bool read_crc_on_early_termination);

// set drive strength for SDA and SCL outputs. Valid inputs are 2, 4, 8, 12. The units is in mA
i3c_hl_status_t i3c_hl_set_drivestrength(uint8_t drivestrength_mA);

#endif //_I3C_HL_H
