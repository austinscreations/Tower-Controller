# mini-notification-tower

As someone who has a home office and a workshop in teh agarge I wanted an easy way to have ntifications where ever I am.
traditional 5 layer led notification lighst are large and pricey.
I set out to make a smaller alternative solution. with 3d printing I made a 180 degree light, with the help of a 3d printed mold I was able to use epoxy resin and tints to create the lenses.

-

Red - flashing - immediate issue (such as a fire alarm or CO alarm)

Red - machine error (cnc or 3d printer)

Yellow - flashing - machine error / require attention (such as a 3d printer filament sensor or 3d print ends)

Yellow - equipement On - temperature related (3d printer active (hot end or heat bed) or soldering iron on)

Green - equipment on but idle (say 3d printer or soldering iron is on but not running a job

Blue - notifications (with the help of an I2C lcd I'm able to show messages

White - tied to my doorbell (one to my office door and front house door)

-

Version 1 used 5mm leds in a 3d printed insert - mosfets controlelr wired to 5v and arduino nano. the arduino nano was linked to an rf433 reciever for the doorbell.


Version 2 will include a PCB for a Wemos d1 mini and SMD leds, and I want to try to make a 360 degree model as well.
          
RF433 will be offloaded to a esp-01 to build a seprate gateway for my home assitant installation.
