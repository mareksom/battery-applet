#include <gtkmm/bin.h>
#include <gtkmm/label.h>
#include <libxfce4panel/xfce-panel-plugin.h>

namespace {

class BatteryPlugin : public Gtk::Bin {
 public:
  BatteryPlugin() : label_("Hipppoppotttamm.") {
    add(label_);
    label_.show();
  }

 private:
  Gtk::Label label_;
};

}  // namespace

extern "C" {

static void BatteryPluginConstructor(XfcePanelPlugin* plugin) {
  BatteryPlugin battery_plugin;
  gtk_container_add(GTK_CONTAINER(plugin), static_cast<Gtk::Widget&>(battery_plugin).gobj());
  battery_plugin.show();
  xfce_panel_plugin_set_expand(XFCE_PANEL_PLUGIN(plugin), TRUE);
}

XFCE_PANEL_PLUGIN_REGISTER_INTERNAL(BatteryPluginConstructor);

}  // extern "C"
