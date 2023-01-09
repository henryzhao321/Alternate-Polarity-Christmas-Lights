# Alternate-Polarity-Christmas-Lights
PIC microcontroller code for controller common fairy lights, christmas lights etc 

Most are battery powered, or solar/battery or even mains. These have the LEDs wired in alternate polarity either. Multicolour tend to group blue&green and red&yellow.

The usual circuit is a 8 pin controller with a 32.768Khz crystal (for the timer function), connected to a small H-Bridge to drive the LEDs. A button used to select 1 of 8 modes.

The patterns are harsh, the settings usually get forgotten, the timing drifts, the pattern fades to black at points and when the fade does occur, its usually done at such a low resulition you can see the "steps".

This code is an attempt to improve on it. It's based around a PIC12F1612 (what was available at the time) and usings two timers/PWM units to control each alternate polarity. The last selected pattern is saved to the last page of flash memory. MCC is used to configure the chip. If the button is held for enough time the next pattern is selected. Each time it is pressed it will reset the On Timer start point.
