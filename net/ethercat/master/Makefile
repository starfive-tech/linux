# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the ethercat driver.
#

obj-$(CONFIG_ETHERCAT_MASTER)	   += ethercat_master.o
ethercat_master-y		   += cdev.o coe_emerg_ring.o datagram.o datagram_pair.o debug.o device.o \
				   domain.o doxygen.o ethernet.o fmmu_config.o foe_request.o fsm_change.o \
				   fsm_coe.o fsm_foe.o fsm_master.o fsm_pdo.o fsm_pdo_entry.o fsm_sii.o \
				   fsm_slave.o fsm_slave_config.o fsm_slave_scan.o fsm_soe.o ioctl.o \
				   mailbox.o master.o module.o pdo.o pdo_entry.o pdo_list.o reg_request.o \
				   sdo.o sdo_entry.o sdo_request.o slave.o slave_config.o \
				   soe_errors.o soe_request.o sync.o sync_config.o voe_handler.o
ethercat_master-$(CONFIG_ETHERCAT_RTDM) += rtdm.o rtdm-ioctl.o
