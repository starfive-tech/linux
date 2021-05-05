# Linux kernel for StarFive's JH7100 RISC-V SoC

## What is this?

The [JH7100][soc] is a Linux-capable dual-core 64bit RISC-V SoC and this tree
is meant to collect all the in-development patches for running Linux on boards
using this. So far there are two such boards and both are supported by this tree:

1) [StarFive VisionFive][visionfive]
2) [BeagleV Starlight Beta][starlight]

The VisionFive boards aren't quite shipping yet, but you can already
[register interest][interest] and ask questions on the [forum][].

About 300 BeagleV Starlight Beta boards were sent out to developers in
April 2021 in preparation for an eventual BeagleV branded board using the
updated JH7110 chip. The BeagleBoard organization has since [cancelled that
project][beaglev] though.


[visionfive]: https://github.com/starfive-tech/VisionFive
[interest]: http://starfive.mikecrm.com/doQXj99
[forum]: https://forum.rvspace.org/c/visionfive/6
[starlight]: https://github.com/beagleboard/beaglev-starlight
[soc]: https://github.com/starfive-tech/JH7100_Docs
[beaglev]: https://beaglev.org/blog/2021-07-30-the-future-of-beaglev-community

## Cross-compiling

Cross-compiling the Linux kernel is surprisingly easy since it doesn't depend
on any (target) libraries and most distributions already have packages with a
working cross-compiler. We'll also need a few other tools to build everything:
```shell
# Debian/Ubuntu
sudo apt-get install libncurses-dev libssl-dev bc flex bison make gcc gcc-riscv64-linux-gnu
# Fedora
sudo dnf install ncurses-devel openssl openssl-devel bc flex bison make gcc gcc-riscv64-linux-gnu
# Archlinux
sudo pacman -S --needed ncurses openssl bc flex bison make gcc riscv64-linux-gnu-gcc
```

The build system needs to know that we want to cross-compile a kernel for
RISC-V by setting `ARCH=riscv`. It also needs to know the prefix of our
cross-compiler using `CROSS_COMPILE=riscv64-linux-gnu-`. Also let's assume
we're building on an 8-core machine so compilation can be greatly sped up by
telling make to use all 8 cores with `-j8`.

First we need to configure the kernel though. Linux has a *very* extensive
configuration system, but you can get a good baseline configuration for the
boards using:
```shell
make -j8 ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- visionfive_defconfig
```

There is nothing magic about this configuration other than it has all the
drivers enabled that are working for the hardware on the boards. In fact it has
very little extra features enabled which is great for compile times, but you
are very much encouraged to add additional drivers and configure your kernel
further using
```shell
make -j8 ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- nconfig
```

Now compile the whole thing with
```
make -j8 ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-
```


## Installing

Once the build has finished the resulting kernel can be found at
```shell
arch/riscv/boot/Image
```
You'll also need the matching device tree at
```shell
arch/riscv/boot/dts/starfive/jh7100-starfive-visionfive-v1.dtb
```
(If you have a Starlight board you should instead be using `jh7100-beaglev-starlight.dtb`.)

These two files should be copied to the boot partition on the SD card. In the
default [Fedora image][fedora] this is `/dev/mmcblk0p3` and is mounted at `/boot`.

Now add the following entry to the `grub.cfg` file:
```
menuentry 'My New Kernel' {
    linux /Image earlycon console=ttyS0,115200n8 stmmac.chain_mode=1 root=/dev/mmcblk0p4 rootwait
    devicetree /jh7100-starfive-visionfive-v1.dtb
}
```

This assumes your root file system is at `/dev/mmcblk0p4` which it is in the
default [Fedora image][fedora].

The `visionfive_defconfig` doesn't enable modules, but if you enabled them in
your build you'll also need to install them in `/lib/modules/` on the root file
system. How to do that best is out of scope for this README though.

[fedora]: https://github.com/starfive-tech/Fedora_on_StarFive/

## Status

#### SoC

- [x] Clock tree
- [x] Resets
- [x] Pinctrl/Pinmux
- [x] GPIO
- [x] Serial port
- [x] I2C
- [x] SPI
- [x] MMC / SDIO / SD card
- [x] Random number generator
- [x] Temperature sensor
- [x] Ethernet, though `stmmac.chain_mode=1` needed on the cmdline
- [x] USB, USB 3.0 is broken with `CONFIG_PM=y`
- [x] DRM driver
- [x] NVDLA
- [x] Watchdog
- [x] PWM DAC for sound through the minijack, only 16kHz samplerate for now
- [ ] I2S [WIP]
- [ ] TDM [WIP]
- [ ] MIPI-DSI [WIP]
- [ ] MIPI-CSI [WIP]
- [ ] ISP [WIP]
- [ ] Video Decode [WIP]
- [ ] Video Encode [WIP]
- [ ] QSPI
- [ ] Security Engine
- [ ] NNE50
- [ ] Vision DSP

#### Board

- [x] LED
- [x] PMIC / Reboot
- [x] Ethernet PHY
- [x] HDMI
- [x] AP6236 Wifi
- [x] AP6236 Bluetooth, with a [userspace tool][patchram]
- [x] I2C EEPROM (VisionFive only)
- [ ] GD25LQ128DWIG (VisionFive) / GD25LQ256D (Starlight) flash

[patchram]: https://github.com/AsteroidOS/brcm-patchram-plus

## Contributing

If you're working on cleaning up or upstreaming some of this or adding support
for more of the SoC I'd very much like to incorporate it into this tree. Either
send a pull request, mail or contact Esmil on IRC/Slack.

Also think of this tree mostly as a collection of patches that will hopefully
mature enough to be submitted upstream eventually. So expect regular rebases.
