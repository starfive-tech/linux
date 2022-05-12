Display Subsystem:(default FBdev)

Steps switch to DRM to hdmi:
1. Disable those config
CONFIG_FB_STARFIVE=n
CONFIG_FB_STARFIVE_HDMI_ADV7513=n
CONFIG_FRAMEBUFFER_CONSOLE=n

2. open DRM items:
CONFIG_DRM_I2C_NXP_TDA998X=y
CONFIG_PHY_M31_DPHY_TX0=y
CONFIG_DRM_STARFIVE=m

3. set hdmi or mipi pipeline on "dts"

3.1 hdmi

	hdmi_out: endpoint@0 {
		remote-endpoint = <&tda998x_0_input>;
		encoder-type = <2>;	//2-TMDS, 3-LVDS, 6-DSI, 8-DPI
		reg = <0>;
		status = "okay";
	};

	mipi_out: endpoint@1 {
		remote-endpoint = <&dsi_out_port>;
		encoder-type = <6>;	//2-TMDS, 3-LVDS, 6-DSI, 8-DPI
		reg = <1>;
		status = "failed";
	};

3.2 mipi

	hdmi_out: endpoint@0 {
		remote-endpoint = <&tda998x_0_input>;
		encoder-type = <2>;	//2-TMDS, 3-LVDS, 6-DSI, 8-DPI
		reg = <0>;
		status = "failed";
	};

	mipi_out: endpoint@1 {
		remote-endpoint = <&dsi_out_port>;
		encoder-type = <6>;	//2-TMDS, 3-LVDS, 6-DSI, 8-DPI
		reg = <1>;
		status = "okay";
	};


4. install libdrm:
make buildroot_initramfs-menuconfig
choose:
BR2_PACKAGE_LIBDRM=y
BR2_PACKAGE_LIBDRM_RADEON=y
BR2_PACKAGE_LIBDRM_AMDGPU=y
BR2_PACKAGE_LIBDRM_NOUVEAU=y
BR2_PACKAGE_LIBDRM_ETNAVIV=y
BR2_PACKAGE_LIBDRM_INSTALL_TESTS=y
