# Enchanced Simon

## Introduction
In my own implementation of the real-time Simon memorization game, “Enhanced Simon,” consists of inspired elements of the original game, but also includes new, unique features as well. In the original game, a player will have to memorize and correctly input a sequence of flashing lights in the correct order. If a player “wins” a level, they proceed to the next one. If they “lose” by submitting the wrong sequence, the game will end. In this game, a player will have to memorize and correctly input a sequence of flashing lights consisting of a random combination of four different colors in the correct order. If a player “wins” a level, they proceed to the next. If they “lose” by submitting the wrong sequence, the game will end. If the player takes too long to complete the level, they will automatically lose and the game will end. 

In my version of the game, “Enhanced Simon,” I increased the complexity of the game by doubling the amount of lights to eight lights in total. This vastly increases the different levels of light combinations the player will have to memorize. The lights are organized in a 3x3 LED matrix with the neutral point being the center LED as a reference point for the player as they traverse the matrix. The player will be playing the game through a joystick mapped to the eight different lights. In my project proposal, I mentioned that I would be using the LCD screen to project the accumulated points and a 7-segment display to show the current level. However, I decided to only use the LCD screen to communicate game states to the player because of the redundancy of using two different displays along with the concern of not having enough pins to wire it along with everything else. I was able to successfully implement all other aspects of the game I mentioned in my proposal including, dynamically calculating the accumulated score earned for each completed level, the sound effects that play during the game, and mapping the joystick to the 3x3 LED matrix.

## Hardware Components
- LCD Screen
- LED (x9)
- 220 Ohm Resistor (x10)
- Wires
- IR Remote/Receiver
- Passive Buzzer
- Shift Register

## Software Libraries Used
- <stdio.h>
- <stdlib.h>
- <time.h>
- "timerISR.h"
- "helper.h"
- "periph.h"
- "LCD.h"
- "irAVR.h"
- "pitches.h"
