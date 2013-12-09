CsrUsbSpiDeviceRE
===
Description
---
This is a reverse engineered re-implementation of CSR's USB<->SPI Converter on a TI Stellaris Launchpad. It is compatible with CSR's own drivers and BlueSuite tools, and should work on any BlueCore chip that supports programming through SPI.

Disclaimer
---
I make no guarantees about this code. For me it worked, but it might not for you. If you break your BlueCore module or anything else by using this software this is your own responsibility.

How to use
---
* Get a TI Stellaris Launchpad ($10), StellarisWare, TI's Code Composer (free with the launchpad), and an extra USB<->microUSB cable.
* Import this project into Code composer as usual.
* Ensure SW_ROOT is set to your StellarisWare directory in *both* of these locations:
    * Project -> Properties -> Resource -> Linked Resources -> Path Variables -> SW_ROOT
    * Window -> Preferences -> C/C++ -> Build -> Environment -> SW_ROOT.
* Build using Project -> Build Project
* Connect your BlueCore module. 3.3v -> 3.3v, GND -> GND, PC4 -> SPI_CSB, PC5 -> SPI_MOSI, PC6 -> SPI_MISO, PC7 -> SPI_CLK. (PC4 - PC7 are located on the second column from the right)
* Connect your Stellaris Launchpad with the microUSB port at the top (next to power select), and set power select to DEBUG.
* Debug using Run -> Debug
* When paused on main, do Run -> Resume
* Plug in the other microUSB port.
* Device should be recognized. Drivers can be found at csrsupport.com (needs registration), under Browse category tree -> Bluetooth PC Software/Tools -> USB-SPI Converter.
* Use any of the BlueSuite or BlueLabs tools to play with your bluecore module! (BlueSuite can be found on CSRSupport.com under Browser category tree -> Bluetooth PC Software/Tools -> Current BlueSuite Development Tools)
* Green led will light up during reading, Red during writing, Blue when using BCCMD's.

Notes
---
* The SPI clock may not be accurate.
* The code was written to match CSR's original firmware as closely as possible, it might not be the most efficient way to things on the Stellaris.
* If something does or does not work, let me know.

TODO
---
* Clean up: The code works, but should still be cleaned up, and some bits aren't fully reverse engineered yet (e.g. the BCCMD timeout)
* Refactor/rewrite: The code as it is now is as close to CSR's code as I could get, but it's not the most readable. I intend to refactor this at some point (maybe in a different project) to make it more readable and portable to other platforms.

Ports to other platforms
---
* Tiva C Launchpad (EK-TM4C123GXL) by Richard Aplin: https://github.com/raplin/CsrUsbSpiDeviceRE
