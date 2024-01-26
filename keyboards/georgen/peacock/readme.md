# Peacock

![georgen/peacock](imgur.com image replace me!)

Peacock is a QMK powered standalone 7" trackpad. The board features a macropad with 5 switches and 2 encoders and a completly custom designed trackpad. The two parts of the board are connected using ![a VIK interface](https://github.com/sadekbaroudi/vik/blob/master/README.md) so the trackpad could also be used with different VIK compatible keyboards or controllers.

* Keyboard Maintainer: [George Norton](https://github.com/george-norton)
* Hardware Supported: PCBA with integrated RP2040
* Hardware Availability: [Open Source](https://github.com/george-norton/peacock)

Make example for this keyboard (after setting up your build environment):

    make georgen/peacock:default

Flashing example for this keyboard:

    Put the board into bootloader mode by holding the boot button while connecting the USB cable, bolding boot and shorting the reset jumper or holding SW1 and plugging in the USB cable, then drag and drop the uf2 file onto the drive that mounts.

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (usually the top left key or Escape) and plug in the keyboard
* **Physical boot button**: Hold the boot button while plugging in the USB cable, or short the reset pins while holding the boot button.
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
