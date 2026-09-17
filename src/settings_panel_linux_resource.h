// settings_panel_linux_resource.h -- dialog/control IDs for the Linux/macOS settings panel.
//
// Included by both settings_panel_linux.rc (parsed by swell_resgen.pl at build time -- see
// build-linux.sh) and smooth_wheel_scroll.cpp (which #includes the generated .rc_mac_dlg and
// calls CreateDialogParam with these same IDs). Numbered well clear of the Windows panel's own
// IDC_S_BASE/IDC_L_BASE/IDC_K_BASE ranges (1010-1219) even though the two never coexist in the
// same binary -- keeps grep unambiguous if that ever changes.
#ifndef SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H
#define SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H

#define IDD_SWS_SETTINGS_LINUX 2000
#define IDC_SWS_PLACEHOLDER    2001

#endif // SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H
