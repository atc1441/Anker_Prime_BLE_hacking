# AnkerPowerBankDoom

AnkerPowerBankDoom is Doom for the Anker Prime power bank. It runs on the bank's own GD32F303. This folder is the program, the shareware WAD it was built from, and the level data that WAD becomes. It is enough to compile the internal-flash image.

The engine is [DMC100-Doom](https://github.com/ArturBieniek4/DMC100-Doom) by Artur Bieniek. The board layer is `src/gd32` and `src/board`. The game code is GPL-2.0-or-later. See `COPYING` and `CREDITS.md`.

## Status: demo, not a finished firmware

**This is a demo. Do not treat it as a replacement firmware for your power bank.**

What works: Doom runs, the fuel gauge is read correctly, the pre-game pages
show live battery and port readings, and the USB power path keeps working
while Doom runs, so the bank still charges a phone.

What does not: this firmware does not reimplement the stock power manager.
It inherits the power path from whatever configured it last and is careful
not to disturb it. It cannot bring a port back up on its own after the port
has lost its USB-C contract, and it does not run the stock protection logic
(over-voltage, over-current, under-voltage). The port current and watt
figures are calibrated against a single USB meter reading and are good to
roughly ten percent, not better.

Practical consequence: if a port stops delivering, flash the stock image
back for a moment and then flash this one again. Keep a backup of your own
internal flash and of the external NOR before you write anything, and keep
an SWD adapter attached. There is no recovery path through the bank's own
UI if a write goes wrong.

This has been exercised on exactly one device.

## Hardware

| | |
|---|---|
| CPU | GD32F303VET6, Cortex-M4, no FPU, 120 MHz |
| Flash | 512 KB at `0x08000000` |
| RAM | 64 KB |
| Panel | ST7789, 240×240, SPI |
| Level data | external SPI NOR, file `DOOM.DAT` |
| Button | one, PD0, active low |

The build is soft-float (`-mfloat-abi=soft`). This chip has no hardware floating point.

## Game data

`wad/DOOM1.WAD` is id Software's Doom 1.9 shareware IWAD (4,196,020 bytes, SHA-256 `1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771`). Shareware may be redistributed. It is episode 1 only. It is not the registered game and not Freedoom.

`data/DOOM.DAT` is that WAD packed for this engine. It contains E1M1 (Hangar) and E1M5. Shareware `DEMO1` was recorded on E1M5, so the title loop can play that demo. New Game still starts at E1M1. The tables compiled into the firmware (`src/gen`) were generated from the same WAD, so this `DOOM.DAT` matches this source tree. The firmware looks for `DOOM.DAT` on the external flash. It does not read the WAD directly.

## Build

Install [GNU Arm Embedded](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) (`arm-none-eabi-gcc`) and Python 3. From this directory:

```
python src/gd32/build.py build
```

`ARM_GNU_TOOLCHAIN` can point at the toolchain root if the compiler is not on `PATH`. The output is `build/DOOM.BIN`. That file is the internal-flash image. Loading it onto the chip is up to you and your own SWD adapter.

## Controls

One button.

| Press | Action |
|---|---|
| Short tap | Shoot, and use (opens a door or switch) |
| Hold the first press | Walk forward |
| Two quick taps | Turn left a short step |
| Three quick taps | Turn right a short step |
| Hold the second press | Turn left slowly until release |
| Hold the third press | Turn right slowly until release |
| Four taps | Reset the whole bank |

On the pre-game pages the same button walks the pages instead: one tap turns
the page, two quick taps start Doom, three taps go back a page, four taps
still reset the bank.

After power-on the panel shows three pages before Doom starts.

* **BATTERY** - state of charge with two decimals, a bar, cell voltage,
  current with its direction, temperature, charge cycles and the gauge
  status byte. All of it comes from the CW2217 fuel gauge on I2C address
  0x64, bit-banged on PE10 (SCL) and PE11 (SDA). Registers are read one at
  a time: a multi-byte burst from 0x02 returns the two voltage bytes and
  then rubbish on this chip, which reads as 0 % and 87 C. The gauge also
  has to be woken the way the stock firmware does it, by writing the mode
  register 0x08 with 0x30, waiting, then 0x00.
* **PORTS** - volts, amps and watts for USB-C1 and USB-C2 from ADC0, plus
  the pack and the state of the USB-A controller. See below.
* **SYSTEM** - firmware, build date, uptime, NOR id, panel, image size,
  CPU clock, BLE counters, the I2C bus scan and the converter rail.

Text on these pages starts at a margin rather than at column 0. The glass
shows only columns 40 to 279 of the 320 the engine draws, so anything
written from column 0 falls off the left bezel. `src/gd32/ui.c` replaces the
engine port's `platform/apps/ui.c` for that reason, which also fixes the
loading and boot pages of the game itself.

## Power path

The bank's USB ports are run by three controllers of the same type, one per
port, each on its own bit-banged I2C bus: PE10/PE11 (shared with the fuel
gauge and a USB-A controller at 0x30), PB10/PB11, and PB8/PB9. A real-time
clock sits at 0x32 on PC10/PC11. The stock firmware configures all three at
boot and then leaves them alone: with a phone charging, a breakpoint on its
I2C write helper stays silent for minutes and only the fuel gauge is polled.

Replaying that boot sequence on a running system switches the bank off.
Raising PB6 is what does it - measured on the device, the converter rail
reading falls from 871 counts to 678 within two milliseconds of that write,
and the stock firmware itself uses the same write as a protection action.
The stock sequence is a cold start: it runs from reset, before the converter
is up. The hardware keeps its configuration across a processor reset, so
after a firmware swap the path is usually already running.

So this firmware reads the converter rail on ADC0 rank 1 and decides. Rail
up means the path is alive and nothing touches it. Rail down means a cold
start, and the stock sequence runs. The pin modes of the two bit-banged
buses are set either way, because without the internal pull-ups on the SDA
lines the controllers on PB8/PB9 only answer now and then.

Port readings come from ADC0 with the stock channel order (sequence 3, 2, 0,
4, 1, 9): PA0 and PA1 are the USB-C voltages, PA2 and PA4 the currents.
Voltage uses the stock formula `(raw - zero) * 30 / 819`. Current uses
`(raw - idle) * 430 / 449` for tenths of an amp, derived from the stock
limit arithmetic and pulled into line with one USB meter reading. The zero
points are tracked as a running minimum, because a port that already carries
voltage at start-up would otherwise calibrate its own voltage away.

The GD32 also speaks the bank's UART protocol to the BLE SoC (USART2, 115200, PD8 TX, PD9 RX). Group `0x11` command `0x0500` returns the state of charge and the temperature. Command `0x05D0` carries a button mask in TLV `A2`: `0x01` fire, `0x02` forward, `0x04` turn left, `0x08` turn right. A fire bit leaves the battery page. The physical button stays the control in the game.
