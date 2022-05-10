/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 StarFive Technology Co., Ltd
 * Author: StarFive <StarFive@starfivetech.com>
 */

#ifndef _STARFIVE_DRM_PLANE_H
#define _STARFIVE_DRM_PLANE_H

int starfive_plane_init(struct drm_device *dev, struct starfive_crtc *starfive_crtc,
						enum drm_plane_type type);
#endif /* _STARFIVE_DRM_PLANE_H */
