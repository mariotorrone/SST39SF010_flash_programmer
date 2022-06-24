# SST39SF010_flash_programmer
## Introduction
As many other people, I have been inspired by [Ben Eater's series](https://www.youtube.com/playlist?list=PLowKtXNTBypGqImE405J2565dvjafglHU) that walks you through building an 8-bit computer on breadboards, and I wanted to replicate his work (and maybe improve the design with some ideas of my own in the future).
However, when making the list of components I needed to buy, I found out that EEPROM chips are nearly unobtainable anywhere these days and thus wondered how to solve this problem.

After some researches, I found out that the SST39SF010 flash chips are very similar to the 28C16 EEPROM chip Ben Eater uses, and with also a number of improvements, such as writing/reading speed, retention time, power consumption...
Of course, however, this calls for a different programmer than the one written by Ben Eater for his series, [`beneater/eeprom-programmer`](https://github.com/beneater/eeprom-programmer), and I found [`slu4coder\SST39SF010-FLASH-Programmer`](https://github.com/slu4coder/SST39SF010-FLASH-Programmer) could be what I needed. However, in my opinion it was written in a very different style with respect to the teaching idea that was behind Ben's project.

So I decided to write my own programmer that, although for sure less optimized than slu4coder's, I think is more suited for a beginner that wants to approach flash programming without having a lot of coding experience. For this reason, I tried to comment the code and to be as clear as possible in my style.
My project is also based on the Arduino Nano in combination with two 74HC595 chips just like the two aforementioned projects.

I might extend this software to be compatible with SST39SF0x0 generic flash chips in the future; although if you got here and you need to flash higher capacity chips, you are probably capable to extend this project on your own. :)

## Configuration
In my code, I also included a check mechanism for the data write and clear chip functions, that can be enabled and configured at compile time.
### Data write check
The macro `WRITE_CHK` is defined by default as `0`, but can be changed to `1` to enable the check. In this case, every byte of data written to the memory is automatically checked and the software will raise an error if the readback is different from the expected data.
For more information, please refer to the function `writeMem()` inside the code.
### Clear chip check
The macro `CLR_CHK` is defined by default as `0`, but can be changed to a positive number to enable the check. In this case, after the clear chip command, the number assigned to the macro will be the number of random bytes read from the flash and checked. If any one of these bytes is different from `0xFF` (default value after a clear chip), the software will raise an error.
There is another macro, called `CLR_CHK_MAX`, which defines the highest address possible coming from the random function; by default it is defined as `131071`, which is (2^17)-1, the highest addressable byte in SST39SF010. However I wanted to give the possibility to configure this macro because in some cases the flash memory is used only for the few first bytes, and thus it is useful to check only the bytes that were used in the previous write cycle.
For more information, please refer to the function `eraseAll()` inside the code.