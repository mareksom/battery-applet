#include <libxfce4panel/xfce-panel-plugin.h>

#include <stdio.h>

#include "battery_plugin.h"

static void BatteryPluginConstructor(XfcePanelPlugin* plugin) {
  GtkWidget* box = gtk_hbox_new(FALSE, 0);
  gpointer data = CreateBatteryPlugin(GTK_BOX(box));
  gtk_container_add(GTK_CONTAINER(plugin), box);
  gtk_widget_show_all(box);
  xfce_panel_plugin_set_expand(XFCE_PANEL_PLUGIN(plugin), FALSE);
}

XFCE_PANEL_PLUGIN_REGISTER_INTERNAL(BatteryPluginConstructor);
