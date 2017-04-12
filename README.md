# rdos
An Arduino-based “hard drive” for the Tandy 100/102

This is based off my failed project “Arduino-tpdd”.

The projects would not work with the pre-existing clients (TS-DOS or Teeny).  But it would work with the small BASIC test programs that I created.

So I decided to write my own client(s) to communicate with my project.  This would not offer the full functionality of TS-DOS, but it will allow someone to "boot" the "dos" from the Arduino, save and load files, list files, rename, and delete.  I added directory support and the ability to seek to and read/write records.

# Hardware explanation

The base is an Arduino microcontroller.  I needed more memory than the Arduino Uno, so I went with the Arduino Mega to get enough SRAM.  The SD shield plus SoftwareSerial libraries just wouldn't fit in the Uno's small memory.

The microSD shield was a no-brainer.  The only thing with that is that I had to solder the SPI headers on since the microSD shield was made for the Uno and the usual pin outs were not compatible with the Mega.  The SPI header, though, lined up. [MicroSD Shield](https://www.sparkfun.com/products/12761)

The biggest pain was the RS-232 shifter.  RS-232 operates from 3.3V to 12V.  So we need something to 1. shift DOWN the voltage (because sending more than 5V down the Arduino's TTL lines would fry it) and 2. to shift UP the TTL voltage to something that my T102 would like to see.  After a few failures, I went with the [RS232 to 5V TTL Converter](http://www.serialcomm.com/serial_rs232_converters/rs232_rs485_to_ttl_converters/rs232_to_5v_ttl_converter/rs232_to_5v_ttl.product_general_info.aspx)
This shifter worked nice and did the hardware flow control cross over for me, so I could use a standard through 9-25 pin serial cable.  No Frankenstein cable needed.

Arduino pins used:
* 7, Gnd - Drive activity light
* A6, Gnd - Boot monentary switch
* 62 (A8) - Shifter TX
* 63 (A9) - Shifter RX
* SPI header for SD shield

Unavailable pins:
* 10-13
* 50-53
* 8 (card select)

Notes:
The Mega puts the SPI pins on 50-53 instead of 10-13.  So you need to solder the SPI header to the MicroSD Shield in order to move the pins.  As a result, pins 50-53 on the Mega are unavailable.  Also, because of how the MicroSD Shield is made, pins 10-13 are unavailable as well - if you use the stacking headers.  You can probably get around this by not using the stacking headers for pins D8-D13 and connecting stuff directly to the Mega and not through the MicroSD Shield.

Not all pins on the Mega and Mega 2560 support change interrupts, so only the following can be used for RX:
10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69
So, since 10-13 and 50-53 are unavailable, I had to put the shifter on pins 62 and 63.

# Software explanation

Much of the code is based on a [DeskLink port to Linux](http://www.bitchin100.com/).
The protocol is (reverse engineered) documented [here](http://bitchin100.com/wiki/index.php?title=TPDD_Base_Protocol)

Commands supported:
+ Type 00 - Directory Reference.
  + Options 0, 1 and 2 - Used by RLIST to list the files, and RSAVE/RLOAD to save and load files.
  + Option 3 - "request previous directory block" was repurposed to be "change directory"
  + Option 4 - "end directory reference" is not supported.
+ Type 01 - Open file - Used by RSAVE/RLOAD to save and load files
+ Type 02 - Close file - Used by RSAVE/RLOAD to save and load files
+ Type 03 - Read file - Used by RLOAD to load files
+ Type 04 - Write file - Used by RSAVE to save files
+ Type 05 - Delete file - Used by RUTIL to delete files
+ Type 06 - Format Disk - Repurposed to "seek file"
+ Type 07 - Drive Status - Repurposed to read a number of bytes from a file
+ Type 08 - DME Req - Unsupported
+ Type 0C - Drive Condition - Unsupported
+ Type 0D - Rename file - Used by RUTIL to delete files
+ Type 23 - TS-DOS Mystery Command - Unsupported
+ Type 31 - TS-DOS Mystery Command - Unsupported

File names were forced to be 6 bytes, dot, 2 bytes.  This is removed and will use the full 24 bytes available on the drive side.  You still have to deal with the Tandy side limitation.
Checksum will not be supported (since the communications is much better, it's no longer needed).

Client programs:
* RLIST.BA - List the files in the current directory
* RLOAD.BA - Load the file into the Tandy
* RSAVE.BA - Save the file onto the SD card
* RUTIL.BA - Delete/rename files, change working directory
* REF.BA - This prorgram references a file on the drive.  When you do the boot process, this is the file that will be loaded.  This is useful for programs that are too large to be on the Tandy in both untokenized and tokenized format.  Powering off the drive will reset the boot program back to RLOAD.BA.

Examples:
* I provided 2 examples on how to open a file, seek to a position and read or write a record to that position in the file.  In theory, this could be used to save/load data and make the drive "virtual memory".

Notes:
* In RSAVE, I only send 60 bytes of the file at a time.  The Tandy seems to not want to send more than that.

* Tandy BASIC does not permit you to open a .BA file for writing, so you can't load a tokenized BASIC program.  You can only load it as a .DO file and then do a LOAD "xxx.DO" in the BASIC interpreter.  Note that you can SAVE a .BA file, though.  So this project only works for data files, not binary program files.

* Don't bother trying to save a file with any extension other than .DO.  You will get an "?MN Error".  Just leave the extension off for the Tandy file name and let it default to .DO.


