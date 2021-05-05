# Linux kernel for the BeagleV Starlight

## What is this?

The [BeagleV Starlight][bborg] board is a new Linux-capable 64bit RISC-V
development board. It has not shipped yet, but [beta versions][beta] are out to
developers. Consequently the board is not yet supported by upstream Linux. This
tree is meant to collect all the in-development patches for running Linux on
the board.

[bborg]: https://beagleboard.org/beaglev
[beta]: https://github.com/beagleboard/beaglev-starlight

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
board using:
```shell
make -j8 ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- beaglev_defconfig
```

There is nothing magic about this configuration other than it has all the
drivers enabled that are working for the hardware on the board. In fact it has
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
arch/riscv/boot/dts/starfive/jh7100-beaglev-starlight.dtb
```
These two files should be copied to the boot partition on the SD card. That is
onto the same file system that contains the `extlinux/extlinux.conf`. On the
default Fedora image this is mounted at `/boot`.

Now add the following entry to the `extlinux/extlinux.conf` file:
```
label My New Kernel
kernel /Image
fdt /jh7100-beaglev-starlight.dtb
append earlycon console=ttyS0,115200n8 root=/dev/mmcblk0p2 rootwait stmmac.chain_mode=1
```

This assumes your root file system is at `/dev/mmcblk0p2` which it is on the
default Fedora image. Also if your kernel is very big it might be beneficial to
use the compressed `Image.gz` rather than the uncompressed `Image`.

The `beaglev_defconfig` doesn't enable modules, but if you enabled them in
your build you'll also need to install them in `/lib/modules/` on the root file
system. How to do that best is out of scope for this README though.


## Status

#### SoC

- [x] GPIO
- [x] Serial port
- [x] I2C
- [x] SPI
- [x] MMC / SDIO / SD card
- [x] Random number generator
- [x] Temperature sensor
- [x] Ethernet, though a little flaky and `stmmac.chain_mode=1` needed on the cmdline
- [x] Framebuffer, fbdev driver so not upstreamable
- [x] NVDLA
- [ ] Clock tree, statically set up by u-boot, WIP clock driver
- [ ] Pinctrl/Pinmux, statically set up by u-boot
- [ ] Watchdog
- [ ] USB, USB 2.0 seems to work ok, but USB 3.0 is very flaky / broken
- [ ] Security Engine
- [ ] MIPI-DSI
- [ ] ISP
- [ ] MIPI-CSI
- [ ] Video Decode
- [ ] Video Encode
- [ ] NNE50
- [ ] Vision DSP

#### Board

- [x] LED
- [x] PMIC / Reboot
- [x] Ethernet PHY
- [x] HDMI, working with [some screens][hdmi]
- [x] AP6236 Wifi
- [ ] AP6236 Bluetooth
- [ ] GD25LQ256D SPI flash

[hdmi]: https://forum.beagleboard.org/t/hdmi-displays-compatible-list/

## Contributing

If you're working on cleaning up or upstreaming some of this or adding support
for more of the SoC I'd very much like to incorporate it into this tree. Either
send a pull request, mail or contact Esmil on IRC/Slack.

Also I think of this tree mostly as a collection of patches that will hopefully
mature enough to be submitted upstream. So expect regular rebases.
