// settings_panel_linux_resource.h -- dialog id for the Linux/macOS settings panel.
//
// Included by both settings_panel_linux.rc (parsed by swell_resgen.pl at build time -- see
// build-linux.sh) and smooth_wheel_scroll.cpp (which #includes the generated .rc_mac_dlg and
// calls CreateDialogParam with this id). The panel has no child controls: it is drawn by its own
// WM_PAINT (see PaintLinuxPanel in smooth_wheel_scroll.cpp), so there are no control ids to keep.
#ifndef SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H
#define SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H

#define IDD_SWS_SETTINGS_LINUX 2000

#endif // SMOOTHWHEELSCROLL_SETTINGS_PANEL_LINUX_RESOURCE_H
