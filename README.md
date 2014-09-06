Controlling RGB LED display on Raspberry Pi GPIO via PixelPusher protocol
=========================================================================

Example video: ![Example video][vid](http://youtu.be/ZglGuMaKvpY)


Code is (c) Henner Zeller <h.zeller@acm.org>,
license: GNU General Public License, Version 3.0

For details of the RGB Matrix library used and how to connect,
see the github over at [Rasberry Pi RGB Matrix][rgb-matrix-lib].

We are using four boards are daisy-chained 'around the corner', see
beginning of the video.

![Chaining multiple displays][matrix64]

Running
-------
Simply run the program as root (which is needed to access the GPIO pins).

     $ make
     $ sudo ./pixel-push

This will advertise itself as a
PixelPusher <http://forum.heroicrobotics.com/board/5/pixelpusher> device
on the network, with 64 strips with 64 pixels. You can control these for instance
with the Processing framework <http://processing.org/>

[rgb-matrix-lib]: https://github.com/hzeller/rpi-rgb-led-matrix
[matrix64]: https://github.com/hzeller/rpi-matrix-pixelpusher/raw/master/img/chained-64x64.jpg
[vid]: https://github.com/hzeller/rpi-matrix-pixelpusher/raw/master/img/video.png
