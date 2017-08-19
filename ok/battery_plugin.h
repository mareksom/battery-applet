#ifndef BATTERY_PLUGIN_H_
#define BATTERY_PLUGIN_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

gpointer CreateBatteryPlugin(GtkBox* box);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // BATTERY_PLUGIN_H_
