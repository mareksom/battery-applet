#include "battery_plugin.h"

#include <gtkmm/bin.h>
#include <gtkmm/label.h>

namespace {

template <typename Object>
gboolean ExposeEventCallback(
    GtkWidget* widget, GtkEventExpose* event, gpointer data) {
  Object* object = (Object*) data;
  cairo_t* context;
  context = gdk_cairo_create(gtk_widget_get_window(widget));
  object->ExposeEvent(context);
  cairo_destroy(context);
  return TRUE;
}

class BatteryDrawing {
 public:
  BatteryDrawing(GtkBox* box) {
    drawing_area_ = gtk_drawing_area_new();
    gtk_box_back_start(box, drawing_area_, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(drawing_area_), "expose_event",
                     G_CALLBACK(ExposeEventCallback<BatteryDrawing>), this);
  }

  void ExposeEvent(cairo_t* context) {
    cairo_set_source_rgb(context, 0, 0, 1);
    cairo_paint(context);
  }

 private:
  GtkWidget* drawing_area_;
};

}  // namespace

/*
static gboolean expose_event_callback(
    GtkWidget* widget, GdkEventExpose* event, gpointer data) {
  cairo_t* context;
  context = gdk_cairo_create(gtk_widget_get_window(widget));
  cairo_set_source_rgb(context, 0, 1, 0);
  cairo_paint(context);
  return TRUE;
}
*/

void CreateBatteryPlugin(GtkBox* box) {
  BatteryDrawing bd
  GtkWidget* da = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(da), "expose_event",
                   G_CALLBACK(expose_event_callback), NULL);
  gtk_box_pack_end(box, da, TRUE, TRUE, 0);
}
