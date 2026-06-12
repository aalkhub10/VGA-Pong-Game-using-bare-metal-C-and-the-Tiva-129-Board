# VGA Pong Game (Bare-Metal C on Tiva TM4C129)

## Overview

This repository contains a simple, single-player Pong game written entirely in bare-metal C for the Tiva TM4C129 microcontroller.

Instead of using a dedicated GPU, the microcontroller acts as a software-defined VGA controller. It directly generates the Horizontal (HSYNC) and Vertical (VSYNC) synchronization pulses using hardware timers and outputs RGB color signals to drive an external monitor.

## Technical Workaround: Pixel Stretching

Driving a standard VGA display (e.g., 640x480) requires a massive amount of memory to store every pixel—far more SRAM than the Tiva TM4C129 has available.

To overcome this memory limitation, this project uses a tiny **20x30 internal framebuffer**. During the active video rendering phase, the microcontroller "stretches" these pixels. By calculating precise delay loops (`TICKS_PER_PIXEL`) and repeating rows, the firmware holds the RGB output pins for multiple clock cycles, scaling the low-resolution 20x30 grid up to fill the full monitor screen.

## Features

* **Bare-Metal VGA:** Direct manipulation of hardware timers and GPIO to generate standard VGA sync signals.
* **Memory-Efficient Rendering:** Custom pixel-stretching logic to bypass microcontroller memory limits.
* **Analog Player Control:** Uses the Tiva's built-in ADC to read a physical potentiometer/joystick on a breadboard for smooth paddle movement.
* **Hardcoded AI:** A simple, hardcoded opponent paddle that automatically tracks the ball's Y-axis.

## Hardware Setup

* **Microcontroller:** Tiva TM4C1294NCPDT Evaluation Board
* **Display:** Standard VGA monitor
* **Player Controller:** A potentiometer or analog thumbstick connected to the Tiva's ADC pins (Pin PE3).
* **VGA Interfacing:** A breadboard with a simple resistor DAC network to convert digital GPIO signals into analog RGB voltages.

## Software Requirements

* **Language:** C (Bare-Metal)
* **IDE:** Keil µVision 5
* **Libraries:** Custom headers (`ES.h`, `ES.lib`).
## Author

