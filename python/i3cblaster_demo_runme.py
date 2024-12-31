import i3cblaster

# list available and connected i3c blaster devices:
devices = i3cblaster.listdevices() # this returns an array with serial numbers of connected i3cblaster devices
print(len(devices), ' i3cblaster devices connected:', devices)

if len(devices) > 0:
    i3c = i3cblaster.i3cblaster() # in case multiple devices are connected you can provide the serial number here as a parameter
    
    print('resetting dynamic addresses previously assigned')
    i3c.i3c_rstdaa()

    print('scanning for available i3c devices')
    targets = i3c.i3c_scan()
    print('found target devices on bus:', targets)
    
    print('assign a dynamic address - this only works for i3c devices supporting it, but will not cause issues')
    i3c.i3c_entdaa(0x30)
    
    print('scanning for available i3c devices')
    targets = i3c.i3c_scan()
    print('found target devices on bus:', targets)
    
    print('reading up to 10 bytes from subregister 0x00:')
    data = i3c.i3c_sdr_writeread(0x30, [0x00], 10)
    print('result: ', data)

    print('Setting controllers SDA/SCL drivestrength to 4mA')
    data = i3c.i3c_drivestrength(4)
    print('result: ', data)
    
    print('Executing a HDR-DDR Write')
    i3c.i3c_ddr_write(0x30, 0x00, [0x1234, 0x5678])

    print('Executing a HDR-DDR Read')
    data = i3c.i3c_ddr_read(0x30, 0x00, 10)
    print('result: ', data)

    print('Executing a HDR-DDR Write followed by read')
    data = i3c.i3c_ddr_writeread(0x30, 0x00, 0x00, [0x1234, 0x5678], 10)
    print('result: ', data)
