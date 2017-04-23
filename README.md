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

(If you did a `git pull` to get the latest state, also make sure to get the latest matrix code:
type `git submodule update`).

Wiring
------
For wiring, please have a look at the [library documentation][rgb-matrix-lib].

You can choose the wiring you are using with the option `--led-gpio-mapping`
on the command line. E.g. if you are connected to an Adafruit HAT, then it is
`--led-gpio-mapping=adafruit-hat` or `--led-gpio-mapping=adafruit-hat-pwm`
depending on if you did the PWM modification (which you absolutely should to
reduce flicker.

Running
-------
Simply run the program as root (which is needed to access the GPIO pins). It
will drop its privileges once it has set up the hardware.

     $ make
     $ sudo ./pixel-push

These are the available options

```
usage: ./pixel-push <options>
Options:
        -i <iface>    : network interface, such as eth0, wlan0. Default eth0
        -a <artnet-universe,artnet-channel>: if used with artnet. Default 0,0
        -u <udp-size> : Max UDP data/packet (default 1460)
                        Best use the maximum that works with your network (up to 65507).
        -d            : run as daemon. Use this when starting in /etc/init.d
        -U            : Panel with each chain arranged in an sidways U. This gives you double the height and half the width.
        -R <rotation> : Rotate display by given degrees (steps of 90).
        --led-gpio-mapping=<name> : Name of GPIO mapping used. Default "regular"
        --led-rows=<rows>         : Panel rows. 8, 16, 32 or 64. (Default: 32).
        --led-chain=<chained>     : Number of daisy-chained panels. (Default: 1).
        --led-parallel=<parallel> : For A/B+ models or RPi2,3b: parallel chains. range=1..3 (Default: 1).
        --led-pwm-bits=<1..11>    : PWM bits (Default: 11).
        --led-brightness=<percent>: Brightness in percent (Default: 100).
        --led-scan-mode=<0..1>    : 0 = progressive; 1 = interlaced (Default: 0).
        --led-show-refresh        : Show refresh rate.
        --led-inverse             : Switch if your matrix has inverse colors on.
        --led-swap-green-blue     : Switch if your matrix has green/blue swapped on.
        --led-pwm-lsb-nanoseconds : PWM Nanoseconds for LSB (Default: 130)
        --led-no-hardware-pulse   : Don't use hardware pin-pulse generation.
        --led-slowdown-gpio=<0..2>: Slowdown GPIO. Needed for faster Pis and/or slower panels (Default: 1).
        --led-daemon              : Make the process run in the background as daemon.
        --led-no-drop-privs       : Don't drop privileges from 'root' after initializing the hardware.
```

This will advertise itself as a
PixelPusher <http://www.heroicrobotics.com/products/pixelpusher> device
on the network. Number of 'strips' will be number of rows, so 16 or 32 multiplied by the parallel panels (1 .. 3).

#### Network UDP packet size
The `-u` parameter specifies the size of the allowed UDP packets. Some network
switches (and the original PixelPusher hardware) don't like large packets
so the default is a conservative 1460 here.

But since we have a lot of pixels, using the highest number possible is
desirable so ideally we can transmit a full frame-buffer with one packet (use
something like 65507 here):

```
     sudo ./pixel-push -u 65507
```

Even if the network supports it, sometimes sending devices limit the packet size (e.g. iOS, 8192 seems to be the limit of packets to send; important if you use
LED labs softare) so we have to change:

```
     sudo ./pixel-push -u 8192
```

Controlling Software
--------------------
You can control these for instance with the Processing framework
<http://processing.org/>. The processing framework already has a contrib
library section that allows you to select PixelPusher supporting libs.

Another software supporting the PixelPusher support is L.E.D. Lab http://www.ledlabs.co/

Artnet / sACN
-------------
If you use the [artnet bridge][artnet], you can specify the artnet-universe and the
artnet-channel with the `-a` option:

    sudo ./pixel-push -a1,1


Larger displays
---------------

Generally, if you want larger displays, it is suggested to first use the
feature of connecting multiple parallel chains to one Raspberry Pi; the [adapter]
in the underlying project provides three outputs.

If you have the Adafruit HAT, then you only can do one chain, but you can
arrange them in a sideways 'U' shape to get a more square display. This is what
the `-U` option is for.

Here are four panels arranged in a square on a single
connector, typically something you might want do do if you want a 64x64
arrangement of four 32x32 displays on an Adafruit HAT (which only provides one
chain):

```
   [<][<]  }--- Pi connector #1 (looking from the front)
   [>][>]
```

(`-U --led-chain=4 --led-parallel=1`).

This is how it looks wired up from the back:

![Chaining multiple displays][matrix64]

How about 6 panels ?
```
   [<][<][<]  }--- Pi connector #1
   [>][>][>]
```

(`-U --led-chain=6 --led-parallel=1`).


This even works if you have multiple parallel chains. Here is an arrangement
with two chains with 8 panels each:

```
   [<][<][<][<]  }--- Pi connector #1
   [>][>][>][>]
   [<][<][<][<]  }--- Pi connector #2
   [>][>][>][>]
```

(`-U --led-chain=8 --led-parallel=2`).


The `-U` option essentially gives you half the width of a panel, but double
the height.

If you have a Raspberry Pi 2 or later consider assembling a display using
parallel chains, for instance using the [adapter] that is provided in the
RGB matrix project

[rgb-matrix-lib]: https://github.com/hzeller/rpi-rgb-led-matrix
[matrix64]: ./img/chained-64x64.jpg
[vid]: ./img/pp-vid.jpg
[artnet]: http://heroicrobotics.boards.net/thread/39/artnet-support-sacn
[adapter]: https://github.com/hzeller/rpi-rgb-led-matrix/tree/master/adapter/active-3