.. SPDX-License-Identifier: GPL-2.0

.. include:: <isonum.txt>

================================
Starfive Camera Subsystem driver
================================

Introduction
------------

This file documents the driver for the Starfive Camera Subsystem found on
Starfive JH7110 SoC. The driver is located under drivers/media/platform/
starfive.

The driver implements V4L2, Media controller and v4l2_subdev interfaces.
Camera sensor using V4L2 subdev interface in the kernel is supported.

The driver has been successfully used on the Gstreamer 1.18.5 with
v4l2src plugin.


Starfive Camera Subsystem hardware
----------------------------------

The Starfive Camera Subsystem hardware consists of:

- MIPI DPHY Receiver: receives mipi data from a MIPI camera sensor.
- MIPI CSIRx Controller: is responsible for handling and decoding CSI2 protocol
  based camera sensor data stream.
- ISP: handles the image data streams from the MIPI CSIRx Controller.
- VIN(Video In): a top-level module, is responsible for controlling power
  and clocks to other modules, dumps the input data to memory or transfers the
  input data to ISP.


Topology
--------

The media controller pipeline graph is as follows:

.. _starfive_camss_graph:

.. kernel-figure:: starfive_camss_graph.dot
    :alt:   starfive_camss_graph.dot
    :align: center

The driver has 2 video devices:

- stf_vin0_wr_video0: capture device for images directly from the VIN module.
- stf_vin0_isp0_video1: capture device for images without scaling.

The driver has 3 subdevices:

- stf_isp0: is responsible for all the isp operations.
- stf_vin0_wr: used to dump RAW images to memory.
- stf_vin0_isp0: used to capture images for the stf_vin0_isp0_video1 device.
