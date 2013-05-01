Controlling RGB LED display on Raspberry Pi GPIO via PixelPusher protocol
=========================================================================

Example: <http://youtu.be/ZglGuMaKvpY>

This is mostly experimental code. I don't have yet the update rate and PWM
accuracy I'd like to have. Yes, DMA is on the list, but it is actually slower.

Code is (c) Henner Zeller <h.zeller@acm.org>, and I grant you the permission
to do whatever you want with it :)

Overview
--------
The 32x32 RGB LED matrix panels can be scored at AdaFruit or eBay. If you are
in China, I'd try to get them directly from some manufacturer or Taobao.
They all seem to have the same standard interface, essentially controlling
two banks of 16 rows (0..15 and 16..31). There are always two rows (n and n+16),
that are controlled in parallel
(These displays are also available in 32x16 - they just have one bank).

The data for each row needs to be clocked in serially using one bit for red,
green and blue for both rows that are controlled in parallel (= 6 bits), then
a positive clock edge to shift them in - 32 pixels for one row are clocked in
like this (or more: you can chain these displays).
With 'strobe', the data is transferred to the output buffers for the row.
There are four bits that select the current row(-pair) to be displayed.
Also, there is an 'output enable' which switches if LEDs are on at all.

Since LEDs can only be on or off, we have to do our own PWM. The RGBMatrix
class in led-matrix.h does that.

Connection
----------
The RPi has 3.3V logic output level, but a display operated at 5V digests these
logic levels just fine (also, the display will work well with 4V; watch out,
they easily can sink 2 Amps if all LEDs are on. I am using a PC power supply and
short, thick leads to connect them). Since we only need pins, we don't need to
worry about level conversion back.

We need 13 IO pins. It doesn't really matter to which GPIO pins these are
connected (but the code assumes right now that the row address are adjacent
bits) - if you use a different layout, change in the `IoBits` union in
led-matrix.h if necessary (This was done with a Model B,
older versions have some different IOs on the header; check
<http://elinux.org/RPi_Low-level_peripherals> )

LED-Panel to GPIO with this code:
   * R1 (Red 1st bank)   : GPIO 17
   * G1 (Green 1st bank) : GPIO 18
   * B1 (Blue 1st bank)  : GPIO 22
   * R2 (Red 2nd bank)   : GPIO 23
   * G2 (Green 2nd bank) : GPIO 24
   * B2 (Blue 2nd bank)  : GPIO 25
   * A, B, C, D (Row address) : GPIO 7, 8, 9, 10
   * OE- (neg. Output enable) : GPIO 2
   * CLK (Serial clock) : GPIO 3
   * STR (Strobe row data) : GPIO 4

The four boards are daisy-chained 'around the corner', see beginning of the
video.

Running
-------
Simply run the program as root (which is needed to access the GPIO pins).

     $ make
     $ sudo ./led-matrix

This will advertise itself as a
PixelPusher <http://forum.heroicrobotics.com/board/5/pixelpusher> device
on the network, with 64 strips with 64 pixels. You can control these for instance
with the Processing framework <http://processing.org/>

Limitations
-----------
There seems to be a limit in how fast the GPIO pins can be controlled. Right
now, I only get about 10Mhz clock speed which ultimately limits the smallest
time constant for the PWM. Thus, only 7 bit PWM looks ok with not too much
flicker. This is the first cut of the code, I have some ideas for tricks to
make it faster.

I did a first experiment using DMA work to offload the CPU and get more accurate
timing, but it actually seems that this is slower than the manual
bit-banging done now.

Right now, I tested this with the default Linux distribution ("wheezy"). Because
this does not have any realtime patches, the PWM can look a bit uneven under
load. If you test this with realtime extensions, let me know how it works.

License
-------
GNU General Public License, Version 3.0
