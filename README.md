# SIPbell

SIPbell is "A modified batteryless SP-9621T doorbell into Wi-Fi-enabled MQTT2SIP Doorbell system".

## But why?

This is a cool experiment with electronics, MQTT, SIP being combined together :)

I also created this "product" for my own usage, as the old battery-powered system I installed
was modified as well with its receiver hooked into a UART-to-USB dongle to detect a
button press, and simple python script to dispatch a `sipexer` call for doing SIP calls.

This seemed to work just fine, until I found that when the battery is half-dead, then it sometimes
works or not, even if the receiver is quite close to the doorbell transmitter.

I walked by a local hardware store and found a "batteryless" doorbell system - the idea
is to plug the receiver into a mains outlet, and put the doorbell button near your door.
When you click on the doorbell, you hear "Beethoven" melody or something like that.

This essentially avoid the need to replace coin batteries (which are draining quite "fast"
and go completely out after a year or so) - you install everything once, checks it works, and
can be assured it will work *forever*.

I wanted to take this nice product and make it so I can hear it everywhere.

## How does this work?

There are 2 parts to this product - the doorbell button and the receiver part.

I only care about the receiver part, the doorbell button works just fine for my purposes.

There are 2 parts to this project - modifying the receiver part and connecting it
to an ESP32 board, and writing an MQTT to SIP bridge userspace program that translates
button presses on the doorbell button, to a SIP call which I can route to all my VoIP
phones using Asterisk.

## Does it work reliably?

I can say with some confidence that it is. The receiver part is working very OK.
The MQTT to SIP bridge program took about a day or so of development with the
help of ChatGPT to figure out some annoying parts of the Paho MQTT and PJSIP libraries.

It is not "battle-tested", but I did some basic testing and it does seem to work end-to-end.

## What's next?

I consider this quite complete, so there won't be any improvements besides bugfixes.
