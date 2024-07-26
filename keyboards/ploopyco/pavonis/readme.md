# Ploopyco Pavonis Trackpad

It's a DIY, QMK-powered trackpad!

* Keyboard Maintainer: [PloopyCo](https://github.com/ploopyco)
* Hardware Supported: RP2040
* Hardware Availability: [Store](https://ploopy.co), [GitHub](https://github.com/ploopyco)

Make example for this keyboard (after setting up your build environment):

    make ploopyco/pavonis:default
    qmk compile -kb ploopyco/pavonis -km default

Flashing example for this keyboard:

    Put the board into bootloader mode by holding the boot button while connecting the USB cable, bolding boot and shorting the reset jumper or holding SW1 and plugging in the USB cable, then drag and drop the uf2 file onto the drive that mounts.

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (usually the top left key or Escape) and plug in the keyboard
* **Physical boot button**: Hold the boot button while plugging in the USB cable, or short the reset pins while holding the boot button.
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
