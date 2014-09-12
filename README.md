Controlling RGB LED display on Raspberry Pi GPIO via PixelPusher protocol
=========================================================================

[![Example video][vid]](http://youtu.be/ZglGuMaKvpY)


Code is (c) Henner Zeller <h.zeller@acm.org>,
license: GNU General Public License, Version 3.0

For details of the RGB Matrix library used and how to connect,
see the github over at [Raspberry Pi RGB Matrix][rgb-matrix-lib].

Compiling
---------
This project contains the RGB matrix library as submodule. So after you have
cloned this project, call:

    git submodule init
    git submodule update
	
Running
-------
Simply run the program as root (which is needed to access the GPIO pins).

     $ make
     $ sudo ./pixel-push

These are the available options

     usage: ./pixel-push <options>
     Options:
         -r <rows>     : Display rows. 16 for 16x32, 32 for 32x32. Default: 32
         -c <chained>  : Daisy-chained boards. Default: 1.
         -L            : 'Large' display, composed out of 4 times 32x32
         -p <pwm-bits> : Bits used for PWM. Something between 1..7
         -g            : Do gamma correction (experimental)
         -d            : run as daemon. Use this when starting in
                         /etc/init.d, but also when running without
                         terminal (e.g. cron).

This will advertise itself as a
PixelPusher <http://heroicrobotics.boards.net/board/5/pixelpusher> device
on the network. Number of 'strips' will be number of rows, so 16 or 32
(or, in the case of the 'Large' (Option `-L`) display, 64)
The strip-length is 32 * chained (64 for the `-L` display).

You can control these for instance with the Processing framework
<http://processing.org/>. The processing framework already has a contrib
library section that allows you to select PixelPusher supporting libs.

Large Display
-------------
For the 'Large' display (option `-L`), we are using four boards are
daisy-chained 'around the corner', see beginning of the video. This is an
example how to compose more complex and larger displays out of smaller ones.
See source for details and if you want to modify things.

![Chaining multiple displays][matrix64]

[rgb-matrix-lib]: https://github.com/hzeller/rpi-rgb-led-matrix
[matrix64]: ./img/chained-64x64.jpg
[vid]: ./img/pp-vid.jpg
