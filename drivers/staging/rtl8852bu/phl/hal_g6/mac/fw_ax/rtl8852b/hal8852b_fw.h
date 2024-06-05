/******************************************************************************
 *
 * Copyright(c) 2012 - 2020 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/
#ifdef PHL_FEATURE_NIC

#if defined(MAC_8852B_SUPPORT)
#ifdef MAC_FW_8852B_U2
#ifdef MAC_FW_CATEGORY_NICCE
extern u32 array_length_8852b_u2_nicce;
extern u8 array_8852b_u2_nicce[322408];
#endif /* MAC_FW_CATEGORY_NICCE */

#ifdef MAC_FW_CATEGORY_NIC_PLE
extern u32 array_length_8852b_u2_nic_ple;
extern u8 array_8852b_u2_nic_ple[293328];
#endif /* MAC_FW_CATEGORY_NIC_PLE */

#ifdef MAC_FW_CATEGORY_NIC
extern u32 array_length_8852b_u2_nic;
extern u8 array_8852b_u2_nic[275496];
#endif /* MAC_FW_CATEGORY_NIC */

#ifdef MAC_FW_CATEGORY_WOWLAN_PLE
extern u32 array_length_8852b_u2_wowlan_ple;
extern u8 array_8852b_u2_wowlan_ple[259488];
#endif /* MAC_FW_CATEGORY_WOWLAN_PLE */

#ifdef MAC_FW_CATEGORY_WOWLAN
extern u32 array_length_8852b_u2_wowlan;
extern u8 array_8852b_u2_wowlan[256504];
#endif /* MAC_FW_CATEGORY_WOWLAN */

#endif /* MAC_FW_8852B_U2 */
#ifdef MAC_FW_8852B_U3
#ifdef MAC_FW_CATEGORY_NICCE
extern u32 array_length_8852b_u3_nicce;
extern u8 array_8852b_u3_nicce[322328];
#endif /* MAC_FW_CATEGORY_NICCE */

#ifdef MAC_FW_CATEGORY_NIC_PLE
extern u32 array_length_8852b_u3_nic_ple;
extern u8 array_8852b_u3_nic_ple[293248];
#endif /* MAC_FW_CATEGORY_NIC_PLE */

#ifdef MAC_FW_CATEGORY_NIC
extern u32 array_length_8852b_u3_nic;
extern u8 array_8852b_u3_nic[275424];
#endif /* MAC_FW_CATEGORY_NIC */

#ifdef MAC_FW_CATEGORY_WOWLAN_PLE
extern u32 array_length_8852b_u3_wowlan_ple;
extern u8 array_8852b_u3_wowlan_ple[259408];
#endif /* MAC_FW_CATEGORY_WOWLAN_PLE */

#ifdef MAC_FW_CATEGORY_WOWLAN
extern u32 array_length_8852b_u3_wowlan;
extern u8 array_8852b_u3_wowlan[256432];
#endif /* MAC_FW_CATEGORY_WOWLAN */

#endif /* MAC_FW_8852B_U3 */
#endif /* #if MAC_XXXX_SUPPORT */
#endif /* PHL_FEATURE_NIC */
