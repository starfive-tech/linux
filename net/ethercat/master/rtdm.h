/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C)      2012  Florian Pose <fp@igh-essen.com>
 *
 *  This file is part of the IgH EtherCAT master.
 *
 *  The IgH EtherCAT master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT master. If not, see <http://www.gnu.org/licenses/>.
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

/** \file
 * RTDM interface.
 */

#ifndef __EC_RTDM_H__
#define __EC_RTDM_H__

#include "../include/ecrt.h" /* ec_master_t */

/*****************************************************************************/

struct rtdm_device;

/*****************************************************************************/

/** EtherCAT RTDM device.
 */
typedef struct ec_rtdm_dev {
    ec_master_t *master; /**< Master pointer. */
    struct rtdm_device *dev; /**< RTDM device. */
} ec_rtdm_dev_t;

/*****************************************************************************/

int ec_rtdm_dev_init(ec_rtdm_dev_t *, ec_master_t *);
void ec_rtdm_dev_clear(ec_rtdm_dev_t *);

/****************************************************************************/

#endif
