# rc522_utils

An Arduino Uno + RC522 utility for restoring **your own** broken fob: read the
UID from the old chip and write it onto a blank magic card.

## Hardware and wiring

| RC522 | Arduino Uno |
|-------|-------------|
| SDA/SS | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| RST | D9 |
| 3.3V | **3.3V** |
| GND | GND |

Power the RC522 from 3.3 V only. The signal lines from the Uno (5 V) are
technically out of the datasheet spec; on a breadboard it usually works
directly, but for a permanent build add 1-2 kOhm resistors in SCK/MOSI/SS/RST
or use a level shifter.

## Build and run

```bash
cd ~/Projects/rc522_utils
pio run              # build
pio run -t upload    # flash the Uno
pio device monitor   # open the serial monitor (115200)
```

## How to use

1. After flashing, the device is in read mode. Tap the old fob - the monitor
   shows its UID and card type, and the UID is stored in memory.
2. Type `W` + Enter and tap a blank magic card - the stored UID is written
   onto it.
3. `R` + Enter - go back to reading (for example, to read another fob).
   Instead of reading, you can set the UID by hand: `U 79 D6 83 63` + Enter
   (4 hex bytes, spaces/colons/dashes or no separators all work). Then `W`.
4. `D` + Enter and tap any card - diagnostics. It reports whether a data
   block is writable with the default key, whether the card answers the gen1a
   backdoor, and whether block 0 is directly writable (gen2), then prints a
   verdict: magic gen1a, magic gen2/CUID, or a normal card with a locked UID.
   The test is non-destructive - it writes each block's own bytes back.

## Things to know

- **Frequency.** RC522 is 13.56 MHz (MIFARE/NTAG). If the intercom fob is
  125 kHz (EM-Marine), this module cannot read or write it; you need a 125 kHz
  reader and a T5577 blank.
- **Magic card.** The UID can only be written onto a card whose block 0 is
  writable (Chinese "magic" gen1a/gen2). A factory MIFARE card will not let you
  change the UID - it is locked at production.
- **UID length.** The library's `MIFARE_SetUid` works with a 4-byte UID.
- **Key.** Writing uses the default key `FF FF FF FF FF FF`. If the card has a
  different key, reset sector 0 to the default first.

This tool is meant for working with your own tags (your own pass, key, or
smart-home tag).
