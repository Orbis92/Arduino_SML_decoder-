# EMH eBZD SML decoder for Arduino

## Quick and dirty SML (Smart Meter Language) decoder and MQTT transmitter for the EMH eBZD smart meter.

- Tested only with Arduino MEGA
- Tested only with ENC28J60 Phy

Special decoder for the eBZD "Bezugszähler" 1 EMH00 with IR data port. When capturing the messages from this smart meter I noticed a small difference to the other meters. The payload is not fixed in length (and padded with zeros), so I had to find the start and end of the payload myself with different, non-changing data fields. 
This works quite well, but so far the code cannot detect if its just reading the meter value or if the current power display and output is unlocked via PIN. 

## Please use the 64bit version, since there will be decode errors if your energy reader is >= 429496 kWh (32bit max 0xFFFFFFFF x 0.1Wh resolution = 429496,7295kWh)

The decoder works without current power but you have to fiddle around to transmit the decoded values if you just want the meter (e.g. after a blackout when the meter is locked again...).  The "parse"-function returns a 0 for no successful decode, a 1 for just the meter reading and a 3 for both meter reading and current power, but if you transmit with >=1 it might send one meter reading multiple times before successfully decode the power field in the message. 
I'm currently working on an unlock function for which you have to put the PIN into the Arduino code and it will "blink" the PIN into the meter after a blackout or so itself.
