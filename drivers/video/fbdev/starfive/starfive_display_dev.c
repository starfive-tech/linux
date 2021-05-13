/* driver/video/starfive/starfive_display_dev.c
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** Copyright (C) 2020 StarFive, Inc.
**
** PURPOSE:	This files contains the driver of LCD controller and VPP.
**
** CHANGE HISTORY:
**	Version		Date		Author		Description
**	0.1.0		2020-11-10	starfive		created
**
*/
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include "starfive_display_dev.h"

struct sf_fb_display_dev_data {
	struct sf_fb_display_dev* dev;
	struct list_head list;
};

static DEFINE_MUTEX(sf_fb_display_dev_lock);
static LIST_HEAD(sf_fb_display_dev_list);

int sf_fb_display_dev_register(struct sf_fb_display_dev* dev)
{
	struct sf_fb_display_dev_data *display_dev;

	display_dev = kzalloc(sizeof(struct sf_fb_display_dev_data), GFP_KERNEL);
	if (!display_dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&display_dev->list);
	display_dev->dev = dev;

	mutex_lock(&sf_fb_display_dev_lock);
	list_add_tail(&display_dev->list, &sf_fb_display_dev_list);
	mutex_unlock(&sf_fb_display_dev_lock);

	return 0;
}
EXPORT_SYMBOL(sf_fb_display_dev_register);

int sf_fb_display_dev_unregister(struct sf_fb_display_dev* dev)
{
	struct sf_fb_display_dev_data *display_dev;

	mutex_lock(&sf_fb_display_dev_lock);
	list_for_each_entry(display_dev, &sf_fb_display_dev_list, list) {
		if (display_dev->dev == dev) {
			list_del_init(&display_dev->list);
			kfree(display_dev);
		}
	}
	mutex_unlock(&sf_fb_display_dev_lock);

	return 0;
}
EXPORT_SYMBOL(sf_fb_display_dev_unregister);

static int build_dev_list(struct sf_fb_data *fb_data)
{
	int rc = 0;

	rc = of_platform_populate(fb_data->dev->of_node,
				  NULL, NULL, fb_data->dev);
	if (rc) {
		dev_err(fb_data->dev,
			"%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	}

	return rc;
}

struct sf_fb_display_dev* sf_fb_display_dev_get_by_name(char *dev_name)
{
	struct sf_fb_display_dev_data *display_dev;
	struct sf_fb_display_dev *dev = NULL;
	char *connect_panel_name;

	connect_panel_name = dev_name;
	mutex_lock(&sf_fb_display_dev_lock);
	list_for_each_entry(display_dev, &sf_fb_display_dev_list, list) {
		if(!strcmp(connect_panel_name, display_dev->dev->name)) {
			dev = display_dev->dev;
			printk(KERN_INFO "select displayer: %s\n", dev->name);
			break;
		}
	}

	if (!dev) {
			display_dev = list_first_entry(&sf_fb_display_dev_list, typeof(*display_dev), list);
			dev = display_dev->dev;
			printk(KERN_INFO "default get first displayer(%s)! \n", display_dev->dev->name);
	}
	mutex_unlock(&sf_fb_display_dev_lock);

	return dev;
}
EXPORT_SYMBOL(sf_fb_display_dev_get_by_name);

struct sf_fb_display_dev* sf_fb_display_dev_get(struct sf_fb_data *fb_data)
{
	struct sf_fb_display_dev_data *display_dev;
	struct sf_fb_display_dev *dev = NULL;
	char *connect_panel_name;

	build_dev_list(fb_data);

	connect_panel_name = fb_data->dis_dev_name;
	mutex_lock(&sf_fb_display_dev_lock);
	list_for_each_entry(display_dev, &sf_fb_display_dev_list, list) {
		if(!strcmp(connect_panel_name, display_dev->dev->name)) {
			dev = display_dev->dev;
			dev_info(fb_data->dev, "select displayer: %s\n", dev->name);
			break;
		}
	}

	if (!dev) {
			display_dev = list_first_entry(&sf_fb_display_dev_list, typeof(*display_dev), list);
			dev = display_dev->dev;
			dev_info(fb_data->dev,"default get first displayer(%s)! \n", display_dev->dev->name);
	}

	mutex_unlock(&sf_fb_display_dev_lock);

	return dev;
}
EXPORT_SYMBOL(sf_fb_display_dev_get);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("framebuffer device for StarFive");
MODULE_LICENSE("GPL");
