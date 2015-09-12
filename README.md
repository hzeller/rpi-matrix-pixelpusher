Controlling RGB LED display on Raspberry Pi GPIO via PixelPusher protocol
=========================================================================

[![Example video][vid]](http://youtu.be/ZglGuMaKvpY)


Code is (c) Henner Zeller <h.zeller@acm.org>,
license: GNU General Public License, Version 3.0

For details of the RGB Matrix library used and how to connect,
see the github over at [Raspberry Pi RGB Matrix][rgb-matrix-lib].

Compiling
---------
Clone with `--recursive` to get the RGB matrix submodule when cloning
this repository:

    git clone --recursive https://github.com/hzeller/rpi-matrix-pixelpusher.git

Wiring
------
For wiring, please have a look at the [library documentation][rgb-matrix-lib].

**NOTE** The wiring changed recently, so if you have some pixel pusher wiring from before
September 2015, either compile using the 'classic pinout'

    DEFINES+="-DRGB_CLASSIC_PINOUT" make

.. or actually consider changing the wiring, as it reduces potential flickering.

Running
-------
Simply run the program as root (which is needed to access the GPIO pins).

     $ make
     $ sudo ./pixel-push

These are the available options

```
usage: ./pixel-push <options>
Options:
        -r <rows>     : Display rows. 16 for 16x32, 32 for 32x32. Default: 32
        -c <chained>  : Daisy-chained boards. Default: 1.
        -P <parallel> : For Plus-models or RPi2: parallel chains. 1..3.
        -L            : 'Large' display, composed out of 4 times 32x32
        -p <pwm-bits> : Bits used for PWM. Something between 1..11
        -l            : Switch off luminance correction.
        -i <iface>    : network interface, such as eth0, wlan0. Default eth0
        -u <udp-size> : Max UDP data/packet (default 1460)
        -d            : run as daemon. Use this when starting in /etc/init.d
```

This will advertise itself as a
PixelPusher <http://heroicrobotics.boards.net/board/5/pixelpusher> device
on the network. Number of 'strips' will be number of rows, so 16 or 32 multiplied by the
parallel panels (1 .. 3).
In the case of the 'Large' (Option `-L`) display, this is 64.
The strip-length is 32 * chained (option `-c`) (64 for the `-L` display).
For details of `-P` and `-c` refer to the [library documentation][rgb-matrix-lib].

#### Network UDP packet size
The `-u` parameter specifies the size of the allowed UDP packets. Some network switches don't
like this large packets so the default is a conservative 1460 here.
But since we have a lot of pixels, using the highest number possible is desirable so
ideally we can transmit a full frame-buffer with one packet (use something like 65535 here):

     sudo ./pixel-push -u 65535


Even if the network supports it, sometimes sending devices limit the packet size (e.g. iOS,
8192 seems to be the limit of packets to send) so we have to change:

     sudo ./pixel-push -u 8192

Controlling Software
--------------------
You can control these for instance with the Processing framework
<http://processing.org/>. The processing framework already has a contrib
library section that allows you to select PixelPusher supporting libs.
Another software supporting the PixelPusher support is L.E.D. Lab http://www.ledlabs.co/

Large Display
-------------
For the 'Large' display (option `-L`), we are using four boards are
daisy-chained 'around the corner', see beginning of the video. This is an
example how to compose more complex and larger displays out of smaller ones.
See source for details and if you want to modify things.

![Chaining multiple displays][matrix64]

If you have a Raspberry Pi 2, consider assembling a display using parallel chains.

[rgb-matrix-lib]: https://github.com/hzeller/rpi-rgb-led-matrix
[matrix64]: ./img/chained-64x64.jpg
[vid]: ./img/pp-vid.jpg
