!!! WORK IN PROGRESS !!!  

!!! WORK IN PROGRESS !!!  

!!! WORK IN PROGRESS !!!  

# Game Boy Advance faster EWRAM hack

The GBA has 256kB of external work RAM in the form of an 128kx16 SRAM chip with 70ns (M68AS128DL70N6), 85ns ([μPD442012AGY-BB85X-MJH](http://www.dexsilicium.com/Nec_D442012AGY.pdf)), or even 120ns ([HY62LF16206A-LT12C](https://www.alldatasheet.com/datasheet-pdf/pdf/96180/HYNIX/HY62LF16206A.html)) access time, depending on the chip (good hardware database [here](https://gbhwdb.gekkio.fi/consoles/agb/). The regular EWRAM wait states are 3/3/6 clock cycles (for 1/2/4 bytes of data). Setting the [undocumented EWRAM wait states](http://problemkaputt.de/gbatek.htm#gbasystemcontrol) to 0Eh (1 wait state) allows you to speed up EWRAM access to 1 wait state (2/2/4 clock cycles). Setting it to 0Fh will lock up the GBA though, probably because 0 wait states are out of the specs of all SRAM chips ever used (access times < 62ns are needed). But now 20 years later we have SRAM chips available with as low as 10ns access time.

## The hardware

So this aims to replace the GBAs SRAM chip with a new, faster chip on a carrier board, because there seem to be no pin-compatible models anymore. The old models used a TSOP-I-48 footprint, newer models use a TSOP-II-44 footprint and all have a very different pinout.

<p align="center">
    <span>
        <img src="pcb.jpg" width=80%;" title="schematics">
    </span>
</p>

All pins have been connected straight through, with the exception of /CS1 (/CE1) and CS2 (CE2) which are permanently connected to GND resp. 3.3V anyway according to [GBATEK](http://problemkaputt.de/gbatek.htm#pinoutscpupinouts). Also the upper byte (D8-D15) is swapped for better chip placement.

<p align="center">
    <span>
        <img src="schematics.png" width=80%;" title="schematics">
    </span>
</p>

Compatible SRAM chips for the board:

| Model                                                                                                                    | min. access time | max. power draw | working |
| ------------------------------------------------------------------------------------------------------------------------ | ---------------- | --------------- | ------- |
| [IS62WV12816BLL-55TLI / -55BLI](http://www.issi.com/WW/pdf/62WV12816ALL.pdf)                                             | 55ns             | ~25mA           | ?       |
| [IS62WV12816EBLL-45TLI](http://www.issi.com/WW/pdf/62-65WV12816EALL-BLL.pdf)                                             | 45ns             | ~15mA           | ?       |
| [CY62136FV30LL-45ZSXIT / -45ZSXI / -45ZSXA](https://www.cypress.com/file/43866/download)                                 | 45ns             | ~18mA           | ?       |
| [CY62136ESL-45ZSXI](https://www.mouser.de/datasheet/2/100/001-48147_CY62136ESL_MoBL_R_2-Mbit_128_K_X_16_Stat-319203.pdf) | 45ns             | ~20mA           | ?       |

The power draw of the replacement chips seems to be comparable to that of the originals. I'd go with the IS62WV12816EBLL-45TLI if you can find it, because of the lowest power draw.

## Making your own boards

The [KiCad](KiCad) directory contains the schematics and PCB layout and also gerber files (exported for JLPCB). I chose [JLPCB](https://jlcpcb.com) as a manufacturer in this case and a board thickness of 0.8mm.

## The software

You can use the [GBA binary](MemTestGBA.gba) to test stability and performance. The source code for that can be found in the [src](src) directory.

# How to build the test binary?

## From the command line

* You **must** have [CMake](https://cmake.org/) 3.1.0 or higher, [devkitPro / devKitARM](https://devkitpro.org) r52-1 or higher [installed](https://devkitpro.org/wiki/Getting_Started).
* Navigate to the [src](src) folder, then:

```sh
mkdir build && cd build
cmake ..
make -j $(grep -c '^processor' /proc/cpuinfo 2>/dev/null)
```

## From Visual Studio Code

* You **must** have [CMake](https://cmake.org/), [devkitPro / devKitARM](https://devkitpro.org) r52-1 or higher [installed](https://devkitpro.org/wiki/Getting_Started).
* **Must**: Install the "C/C++ extension" by Microsoft.
* **Recommended**: If you want intellisense functionality install the "C++ intellisense" extension by austin.
* **Must**: Install the "CMake Tools" extension by vector-of-bool.
* Restart / Reload Visual Studio Code if you have installed extensions.
* Open the GBAmem folder using "Open folder...".
* Choose "Unspecified" as your active CMake kit if asked. It then should be autodetected correctly.
* You should be able to build now using F7 and build + run using F5.

# License

If you want to build your own soft- or hardware based on this, you can. See the [MIT LICENSE](LICENSE).
The "Modern DOS" 8x8 font is from [Jayvee Enaguas](https://notabug.org/HarvettFox96/ttf-moderndos) and used by [CC0](https://notabug.org/HarvettFox96/ttf-moderndos/src/master/LICENSE).
