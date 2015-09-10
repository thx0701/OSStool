# OSStool

This tool was developed for a Western-digital 250GB HAWK family drive (e.g. WD2500KS-??MJ??, WD2500KS-??MK? .....).
It uses the unallocated reserved-areas mapped to heads 2 through 5 to store and access data.

use 'lspci -v' to find your SATA IO address, e.g: 

00:1f.2 IDE interface: Intel Corporation N10/ICH7 Family SATA Controller [IDE mode] (rev 01) (prog-if 8a [Master SecP PriP])
	Subsystem: Giga-byte Technology Device b002
	Flags: bus master, 66MHz, medium devsel, latency 0, IRQ 19
***	I/O ports at 01f0 [size=8]
	I/O ports at 03f4 [size=1]
***	I/O ports at 0170 [size=8]
	I/O ports at 0374 [size=1]
	I/O ports at f000 [size=16]
	Capabilities: [70] Power Management version 2
	Kernel driver in use: ata_piix

Known issues with the tool:
- The tool is desinged to work on the master drive of the specific IO port.
- The code does not detect bad-sectors in the service area. If such sectors exist, the code will behave unexpectedly.
- While the tool is sending Vendor Specific Commands the OS might report errors trying to communicate with the drive. In fact, it may be required to reboot the OS (or power-cycle the drive) after the tool had completed executing.
- When reading the service-area into a file, the tool reads the entire area (mapped to heads 2 through 5) regardless of the size of the file written to the service-area. 

We believe the OS/hard-drive hanging issue can be resolved by using the SGIO library.

to compile, use: gcc -Wall -O -g -o osslab-SA osslab-SA.c

to read ID information from the drive, use: osslab-SA -p 0xport 
to write to reserved area, use: osslab-SA -p 0xport -w filename
to read from reserved area, use: osslab-SA -p 0xport -r filename


IMPORTANT: The code can render your data and hard-drive corrupted or otherwise inaccessible, use at your own risk!  

This code was rewrited www.recover.co.il's source code.
You can do below command as test:

add parameters
1. -s : Service area sectors-per-track
ex: ./osslab-SA -p 0xf0f0 -s 720

2. -d : Num of heads
ex: ./osslab-SA -p 0xf0f0 -d 6

3. -t : Service area tracks
ex: ./osslab-SA -p 0xf0f0 -t 64

4. -k : reading head num
ex: ./osslab-SA -p 0xf0f0 -r test.bin -k 0

Of course, you can do ./osslab-SA -p 0xf0f0 -r test.bin -s 720 -d 6 -t 64 -k 0

