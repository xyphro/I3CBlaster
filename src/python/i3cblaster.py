import serial 
import serial.tools.list_ports 
import time

class i3cblaster:
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


def listdevices():
    foundserials = []
    matchstr = 'USB VID:PID=%04x:%04x' % (i3cblaster.USB_VID, i3cblaster.USB_PID) 
    ports = serial.tools.list_ports.comports() 
    for port in ports: 
        if port.usb_info().find(matchstr.upper()) >= 0: 
            foundserials.append(port.serial_number)
    return foundserials
    

print(listdevices())

a = i3cblaster()
print(a._connect(True))
