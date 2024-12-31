import serial 
import serial.tools.list_ports 
import time

class i3cblaster:
    
    OKTEXT = 'OK(0)'
    USB_VID = 0x2E8A # USB vendor ID of the i3cblaster devices
    USB_PID = 0x000A # USB product ID of the i3cblaster devices
    
    _ser = None
    _serialnumber = None
    
    def _findcomport(self, serialnumber=None): 
        foundport = None 
        matchstr = 'USB VID:PID=%04x:%04x' % (self.USB_VID, self.USB_PID) 
        ports = serial.tools.list_ports.comports() 
        for port in ports: 
            if port.usb_info().find(matchstr.upper()) >= 0: 
                if foundport is None: 
                    if serialnumber is None:
                        foundport = port.name
                    elif serialnumber == port.serial_number:
                        foundport = port.name
        return foundport 
        
    def init(self, serialnumber=None): 
        self._serialnumber = serialnumber
    
    # Magic function to close all relevant serial port instances running open in current python instance right now
    def close_serialport(self, port): 
        import gc 
        a = gc.get_objects() 
        for o in a: 
            try: 
                if isinstance(o, serial.serialwin32.Serial):   # for windows 
                    if o.portstr == port: 
                        o.close() 
                elif isinstance(o, serial.serialposix.Serial): # for unix based systems 
                    if o.portstr == port: 
                        o.close() 
            except: # exceptions are expected cause we support 2 OS here and the classes are not known for both 
                pass; 
    
    # connect if not connected to the RP2040 serial port 
    def _connect(self, forcereconnect = False): 
        if forcereconnect: 
            if self._ser is not None: 
                self._ser.close() 
                del self._ser 
                self._ser = None 
        if self._ser is None: 
            comport = self._findcomport(self._serialnumber) 
            if comport is not None: 
                self.close_serialport(comport) # close port if it is open within this python instance... just to be sure 
                # now try to connect with a timeout
                openedit = False 
                timedout = False 
                starttime = time.time() 
                while not openedit and not timedout: 
                    try: 
                        self._ser = serial.Serial(comport, 115200, timeout=1) 
                        openedit = True; 
                    except: 
                        time.sleep(0.05); 
                    timedout = (time.time()-starttime) >= 2; 
                    
                    if timedout:
                        del self._ser
                        self._ser = None
                    else:
                        # suppress startup message
                        st = b''
                        done = False
                        starttime = time.time()
                        while not done:
                            st += self._ser.read_all()
                            if len(st) > 0:
                                time.sleep(0.5)
                                st += self._ser.read_all()
                                done = True
                            if time.time()-starttime > 2:
                                done = True
                
        return self._ser is not None

    def _exec(self, cmdstr): 
        respstr = ''
        if self._connect():
            self._ser.read_all() # flush
            self._ser.write(('@'+cmdstr+'\r').encode('ansi'))
            respstr = self._ser.readline()
        return respstr
    
    def _parse_response(self, resp):
        errorcode = ''
        returnvalues = []
        resp = resp.decode('ansi').strip()
        errorend = resp.find(')')
        if errorend >= 0:            
            errorcode = resp[0:errorend+1]
            values = resp[errorend+1:]
            if len(values) > 0:
                if values[0] == ',':
                    values = values[1:]
                values = values.split(',')              
                for v in values:
                    returnvalues.append(int(v, 0))                
        else:
            errorcode = resp
        return (errorcode, returnvalues)
    
    # set a gpio pin.
    # gpionum is a integer number between 0 to 15 and selects the GPIO pin to set
    # set is a character. Provide either
    #   1 or True to set the pin to HIGH
    #   0 or False to set the pin to LOW
    #   'Z' or None to set the pin to tristate = input
    def gpio_write(self, gpionum, state):        
        if (state == 1) or (state == True) or (state == '1'):
            state = '1'
        elif (state == 0) or (state == False)  or (state == '0'):
            state = '0'
        else:
            state = 'Z'
        resp = self._parse_response(self._exec('gpio_write %d %c' % (gpionum, state)))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
    
    # read the state of the full GPIO port or single GPIO pins.
    # if gpionum is given as a parameter (valid range 0..15) the state of that 
    # single pin is returned. Otherwise the state of the full port is returned as decimal number
    def gpio_read(self, gpionum=None):
        if gpionum is None:
            resp = self._parse_response(self._exec('gpio_read'))
        else:
            resp = self._parse_response(self._exec('gpio_read %d' % gpionum))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]

    # check by scanning which I3C addresses are responding on the i3c bus
    # returns a python array with integer values which are the addresses which naked in 7-bit format.
    def i3c_scan(self):
        resp = self._parse_response(self._exec('i3c_scan'))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]

    # execute a broadcast RST DAA
    def i3c_rstdaa(self):
        resp = self._parse_response(self._exec('i3c_rstdaa'))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
    
    # execute an ENTDAA CCC. returns the information of the device which is read during entdaa cycle
    # In case no devices are available for ENTDAA execution (0x7E NAKed) the value None is returned
    def i3c_entdaa(self, targetaddr):
        resp = self._parse_response(self._exec('i3c_entdaa 0x%x' % targetaddr))
        if resp[0] != self.OKTEXT:
            if resp[0] == 'ERR_NAKED(3)':
                return None
            else:
                raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]
    
    # Create a target reset pattern condition on the I3C bus
    def i3c_targetreset(self):
        resp = self._parse_response(self._exec('i3c_targetreset'))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])

    # Set the I3C clock frequency in units of kHz.
    # provide e.g. 12500 for 12.5 MHz
    def i3c_clk(self, clockrate_khz):
        resp = self._parse_response(self._exec('i3c_clock %d' % clockrate_khz))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])

    # execute a private write transfer to a I3C target.
    # writedata is an array, targetaddr is the 7-bit targetaddress to write to
    def i3c_sdr_write(self, targetaddr, writedata):
        cmd = 'i3c_sdr_write %d ' % targetaddr
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])

    # execute a private read transfer from a I3C target.
    # readbyecount is the count of bytes requested to read, 
    # targetaddr is the 7-bit targetaddress to write to
    # The function returns the read data bytes.
    # Note: This can be less as the requested bytecount in case the I3C target
    #       terminated the read earlier.
    def i3c_sdr_read(self, targetaddr, readbytecount):
        cmd = 'i3c_sdr_read %d %d' % (targetaddr, readbytecount)
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]
    
    # execute a private write transfer from a I3C target followed by a restart and read transfer.
    # targetaddr is the 7-bit targetaddress to write to.
    # write data is an array with the bytes to write during write phase.
    # readbyecount is the count of bytes requested to read.
    # The function returns the read data bytes.
    # Note: This can be less as the requested bytecount in case the I3C target
    #       terminated the read earlier.
    def i3c_sdr_writeread(self, targetaddr, writedata, readbytecount):
        cmd = 'i3c_sdr_writeread %d ' % targetaddr
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        cmd += ' %d' % readbytecount
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]
    
    # execute a broadcast ccc transfer to the I3C bus.
    # writedata is an array with payload data to write
    def i3c_sdr_ccc_bc_write(self, writedata):
        cmd = 'i3c_sdr_ccc_bc_write '
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        
    # execute a direct ccc transfer to an I3C target
    # targetaddr is the 7-bit target address    
    # bc_phase_writedata is an array with payload data to write during the broadcast phase of the transfer
    # direct_phase_writedata is an array with payload data to write during the direct addressed phase of the transfer
    def i3c_sdr_ccc_bc_write(self, targetaddr, bc_phase_writedata, direct_phase_writedata):
        cmd = 'i3c_sdr_ccc_bc_write %d ' % targetaddr
        for d in bc_phase_writedata:
            cmd += hex(d)+','
        if len(bc_phase_writedata) > 0:
            cmd = cmd[0:-1]
        cmd += ' '
        for d in direct_phase_writedata:
            cmd += hex(d)+','
        if len(direct_phase_writedata) > 0:
            cmd = cmd[0:-1]
        
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        
    # execute a direct ccc transfer to an I3C target
    # targetaddr is the 7-bit target address    
    # writedata is an array with payload data to write during the broadcast phase of the transfer
    # direct_phase_writedata is an array with payload data to write during the direct addressed phase of the transfer
    # The function returns the read data bytes.
    # Note: This can be less as the requested bytecount in case the I3C target
    #       terminated the read earlier.
    def i3c_sdr_ccc_direct_read(self, targetaddr, writedata, readbytecount):
        cmd = 'i3c_sdr_ccc_direct_read %d ' % targetaddr
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        cmd += ' %d' % readbytecount
        
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]
    
    # check if an I3C IBI or HJ request occured.
    # All transfer functions will also return an error in case an IBI or HJ was detected.
    # directly after such an error occured i3c_poll has to be called to read the IBI data or HJ code.
    # The function returns None in case no IBI was detected
    # In case an IBI/HJ was dtected, it returns the target address and read IBI data payload as an array
    def i3c_poll(self):
        resp = self._parse_response(self._exec('i3c_poll'))
        if resp[0] != self.OKTEXT:
            if 'WARN_NO_IBI(5)' == resp[0]:
                return None
            else:
                raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]        
    
    # Configure drive strength of controllers SDA/SCL pads.
    # Valid values are 2, 4, 8, 12 for either 2mA, 4mA, 8mA or 12mA drive strength
    # Note that it might be necessary to set the drivestrength of the I3C target too by doing target specific writes
    def i3c_drivestrength(self, drivestrength_mA):
        cmd = 'i3c_drivestrength %d' % drivestrength_mA
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
    
    # Configure I3C DDR specific target capabilities. Look into ENDXFER CCC for complete explanation.
    # For the values of the 3 parameters use either True or False
    def i3c_ddr_config(self, crc_word_indicator, enable_early_write_term, write_ack_enable):
        cmd = 'i3c_ddr_config %d %d %d' % (int(crc_word_indicator), int(enable_early_write_term), int(write_ack_enable))
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
    

    # execute a write transfer to a I3C target in HDR-DDR mode
    # writedata is an array, targetaddr is the 7-bit targetaddress to write to
    # Note that writedata for HDR-DDR mode are 16-bit values!
    # writecommand is the 7-bit command value which is used for the command word in the HDR-DDR write
    def i3c_ddr_write(self, targetaddr, writecommand, writedata):
        cmd = 'i3c_ddr_write %d %d ' % (targetaddr, writecommand)
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])

    # execute a read transfer from a I3C target in HDR-DDR mode.
    # readbyecount is the count of bytes requested to read, 
    # targetaddr is the 7-bit targetaddress to write to
    # The function returns the read data bytes.
    # Note: This can be less as the requested bytecount in case the I3C target
    #       terminated the read earlier.
    # readcommand is the 7-bit command value which is used for the command word in the HDR-DDR read
    def i3c_ddr_read(self, targetaddr, readcommand, readbytecount):
        cmd = 'i3c_ddr_read %d %d %d' % (targetaddr, readcommand, readbytecount)
        resp = self._parse_response(self._exec(cmd))
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        return resp[1]

    # execute a write transfer from a I3C target followed by a restart and read transfer in HDR-DDR mode.
    # targetaddr is the 7-bit targetaddress to write to.
    # write data is an array with the bytes to write during write phase.
    # readbyecount is the count of bytes requested to read.
    # The function returns the read data bytes.
    # Note: This can be less as the requested bytecount in case the I3C target
    #       terminated the read earlier.
    # The result is an array with 2 elements. 
    #   The first item contains the amount of words written successfully
    #   The second item is an array containing the read data values
    def i3c_ddr_writeread(self, targetaddr, writecommand, readcommand, writedata, readbytecount):
        cmd = 'i3c_ddr_writeread %d %d %d ' % (targetaddr, writecommand, readcommand)
        for d in writedata:
            cmd += hex(d)+','
        if len(writedata) > 0:
            cmd = cmd[0:-1]
        cmd += ' %d' % readbytecount
        resp = self._parse_response(self._exec(cmd))
        print('parsed response: ', resp)
        if resp[0] != self.OKTEXT:
            raise Exception('I3C Blaster exception: ' + resp[0])
        if len(resp[1]) > 0:
            count_written = resp[1][0]
            read_data = resp[1][1:]
            return [count_written, read_data]
        else:
            return [0, []]
    

def listdevices():
    foundserials = []
    matchstr = 'USB VID:PID=%04x:%04x' % (i3cblaster.USB_VID, i3cblaster.USB_PID) 
    ports = serial.tools.list_ports.comports() 
    for port in ports: 
        if port.usb_info().find(matchstr.upper()) >= 0: 
            foundserials.append(port.serial_number)
    return foundserials
    
