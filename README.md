## JH7110 Upstream Status ##

To get the latest status of each upstreaming patch series, please visit
our RVspace.

https://rvspace.org/en/project/JH7110_Upstream_Plan

## Test Status ##

| Component | Test Result | Note |
| :---------| :-----------| :----|
| Clock tree / Reset | Pass |  |
| Pinctrl | Pass |  |
| Watchdog | Pass |  |
| Timer | Pass |  |
| PLL clock | Pass |  |
| Temperature sensor | Pass |  |
| DMA | Pass |  |
| PWM | Pass |  |
| GMAC | Pass |  |
| SDIO / EMMC | Pass |  |
| I2C | Pass |  |
| SPI | Pass |  |
| QSPI | Pass |  |
| USB | Pass |  |
| PCIe | Pass | Please use a power supply with strong driving capability |
| PMU | Pass |  |
| Hibernation | Pass |  |
| PMIC | Pass |  |
| CPU freq scaling | Pass |  |
| Crypto | Fail | Cause kernel crash at startup |
| TRNG | Pass |  |
| PWMDAC | Pass |  |
| I2S | Pass |  |
| TDM | Pass |  |
| HDMI / DC8200 | Pass | You need to boot Linux with the U-Boot in the mainline |
| MIPI CSI | Pass |  |
| ISP | WIP |  |
| MIPI CSI PHY | Pass |  |

## Build Instructions ##

1. Configure Kconfig options

```shell
# Use the provided starfive_visionfive2_defconfig
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- starfive_visionfive2_defconfig
```

or

```shell
# Select options manually
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- nconfig
```

To boot up the VisionFive 2 board, please make sure **SOC_STARFIVE**, 
**CLK_STARFIVE_JH7110_SYS**, **PINCTRL_STARFIVE_JH7110_SYS**, 
**SERIAL_8250_DW** are selected.
> If you need MMC and GMAC drivers, you should also select
**MMC_DW_STARFIVE** and **DWMAC_STARFIVE**.

2. Build
```shell
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-
```

## How to Run on VisionFive 2 Board via Network ##

1. Power on, enter u-boot and set enviroment parameters
```
setenv fdt_high 0xffffffffffffffff; setenv initrd_high 0xffffffffffffffff;
setenv scriptaddr 0x88100000; setenv script_offset_f 0x1fff000; setenv script_size_f 0x1000;
setenv kernel_addr_r 0x84000000; setenv kernel_comp_addr_r 0x90000000; setenv kernel_comp_size 0x10000000;
setenv fdt_addr_r 0x88000000; setenv ramdisk_addr_r 0x88300000;
```
2. Set IP addresses for the board and your tftp server
```
setenv serverip 192.168.w.x; setenv gatewayip 192.168.w.y; setenv ipaddr 192.168.w.z; setenv hostname starfive; setenv netdev eth0;
```
3. Upload dtb, image and file system to DDR from your tftp server
```
tftpboot ${fdt_addr_r} jh7110-starfive-visionfive-2-v1.3b.dtb; tftpboot ${kernel_addr_r} Image.gz; tftpboot ${ramdisk_addr_r} initramfs.cpio.gz;
```
> If your VisionFive 2 is v1.2A, you should upload jh7110-starfive-visionfive-2-v1.2a.dtb instead.
4. Load and boot the kernel
```
booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r};
```
When you see the message "buildroot login:", the launch was successful.
You can just input the following accout and password, then continue.
```
buildroot login: root
Password: starfive
```
