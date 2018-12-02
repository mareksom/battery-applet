#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battery_info.h"

typedef struct {
  GtkWidget* drawing_area;
  BatteryInfo battery_info;
} BatteryPanelState;

static void InitializeBatteryInfo(BatteryInfo* bi) {
  bi->error = NULL;
  bi->number_of_batteries = 0;
  bi->sbis = NULL;
  bi->minutes_left = -1;
}

static void ClearBatteryInfo(BatteryInfo* bi) {
  free(bi->error);
  free(bi->sbis);
}

static void CopySingleBatteryInfo(const SingleBatteryInfo* src,
                                  SingleBatteryInfo* dst) {
  dst->charge = src->charge;
  dst->status = src->status;
}

static void CopyBatteryInfo(const BatteryInfo* src, BatteryInfo* dst) {
  int i, len;
  ClearBatteryInfo(dst);
  /* Error. */
  if (src->error == NULL) {
    dst->error = NULL;
  } else {
    len = strlen(src->error);
    dst->error = (char*) malloc(len);
    if (dst->error == NULL) {
      /* Error occurred. */
      ClearBatteryInfo(dst);
      return;
    } else {
      memcpy(dst->error, src->error, len);
    }
  }
  /* Batteries. */
  len = dst->number_of_batteries = src->number_of_batteries;
  dst->sbis = malloc(src->number_of_batteries * sizeof(SingleBatteryInfo));
  if (dst->sbis == NULL) {
    ClearBatteryInfo(dst);
    return;
  }
  for (i = 0; i < len; i++) {
    CopySingleBatteryInfo(src->sbis + i, dst->sbis + i);
  }
  /* Minutes left. */
  dst->minutes_left = src->minutes_left;
}

static void BatteryInfoSlot(const BatteryInfo* src, void* data) {
  BatteryPanelState* bps = (BatteryPanelState*) data;
  gdk_threads_enter();
  CopyBatteryInfo(src, &bps->battery_info);
  gtk_widget_queue_draw(bps->drawing_area);
  gdk_threads_leave();
}

static void SetSourceRgbForTimeLeft(cairo_t* context) {
  cairo_set_source_rgb(context, 0.7, 0.8, 0.9);
}

static double PaintTime(cairo_t* context, BatteryInfo* bi,
                        double width, double height) {
  cairo_text_extents_t extents;
  char text[6];
  double width_to_height, scale;
  if (bi->minutes_left < 0) {
    sprintf(text, "??:??");
  } else {
    sprintf(text, "%02d:%02d", bi->minutes_left / 60, bi->minutes_left % 60);
  }
  cairo_save(context);
    SetSourceRgbForTimeLeft(context);
    cairo_text_extents(context, text, &extents);
    width_to_height = extents.width / extents.height;
    scale = height / extents.height;
    cairo_translate(context, width - width_to_height * height, 0);
    cairo_scale(context, scale, scale);
    cairo_text_extents(context, text, &extents);
    cairo_move_to(context, -extents.x_bearing, -extents.y_bearing);
    cairo_show_text(context, text);
  cairo_restore(context);
  return width_to_height * height;
}

static void SetSourceRgbForBattery(cairo_t* context, SingleBatteryInfo* sbi) {
  if (sbi->status == kCharging) {
    cairo_set_source_rgb(context, 1, 1, 0);
  } else if (sbi->status == kDischarging) {
    if (sbi->charge < 10) {
      cairo_set_source_rgb(context, 0.9, 0, 0);
    } else {
      cairo_set_source_rgb(context, 1, 1, 1);
    }
  } else if (sbi->status == kFull) {
    cairo_set_source_rgb(context, 0, 1, 0);
  } else if (sbi->charge < 10) {
    cairo_set_source_rgb(context, 0.8, 0.2, 0.2);
  } else {
    cairo_set_source_rgb(context, 0.5, 0.5, 0.5);
  }
}

static void SetSourceRgbForBatteryBgFilling(cairo_t* context) {
  cairo_set_source_rgb(context, 0.0, 0.1, 0.2);
}

#define BORDER_WIDTH 0.1
#define CORNER_RADIUS 0.2
#define CREVICE_WIDTH 0.02
#define TEXT_SCALE 0.9
#define TOP_HEIGHT 0.5
#define TOP_RADIUS 0.15
#define TOP_WIDTH 0.2

#define SMALL_RADIUS (CORNER_RADIUS - BORDER_WIDTH / 2 - CREVICE_WIDTH)

static void PaintBatteryFilling(cairo_t* context, SingleBatteryInfo* sbi,
                                double width, double height) {
  cairo_text_extents_t extents;
  char charge_text[5];
  int charge_int;
  double scale, new_height, new_width;
  cairo_save(context);
    /* Bg filling. */
    SetSourceRgbForBatteryBgFilling(context);
    cairo_paint(context);
    /* Filling. */
    SetSourceRgbForBattery(context, sbi);
    cairo_rectangle(context, 0, 0, width * sbi->charge / 100, height);
    cairo_fill(context);
    /* Text. */
    charge_int = (int) round(sbi->charge);
    if (0 <= charge_int && charge_int <= 100) {
      sprintf(charge_text, "%d%%", charge_int);
    } else {
      sprintf(charge_text, "???");
    }
    cairo_text_extents(context, charge_text, &extents);
    scale = height / extents.height;
    if (width / extents.width < scale) {
      scale = width / extents.width;
    }
    scale *= TEXT_SCALE;
    new_height = height / scale;
    new_width = width / scale;
    cairo_scale(context, scale, scale);
    cairo_text_extents(context, charge_text, &extents);
    /* Erased part of the text. */
    cairo_save(context);
      SetSourceRgbForBatteryBgFilling(context);
      cairo_rectangle(context, 0, 0,
                      width * sbi->charge / 100 / scale, height / scale);
      cairo_clip(context);
      cairo_move_to(context,
                    (new_width - extents.width) / 2 - extents.x_bearing,
                    (new_height - extents.height) / 2 - extents.y_bearing);
      cairo_show_text(context, charge_text);
    cairo_restore(context);
    /* Drawn part of the text. */
    cairo_save(context);
      cairo_rectangle(context, width * sbi->charge / 100 / scale, 0,
                      width * (1 - sbi->charge / 100) / scale, height / scale);
      cairo_clip(context);
      cairo_move_to(context,
                    (new_width - extents.width) / 2 - extents.x_bearing,
                    (new_height - extents.height) / 2 - extents.y_bearing);
      cairo_show_text(context, charge_text);
    cairo_restore(context);
  cairo_restore(context);
}

static void PaintBattery(cairo_t* context, SingleBatteryInfo* sbi,
                         double width, double height) {
  cairo_scale(context, height, height);
  width /= height;
  /* Battery. */
  cairo_save(context);
    SetSourceRgbForBattery(context, sbi);
    cairo_set_line_width(context, BORDER_WIDTH);
    cairo_new_path(context);
    cairo_arc(context, CORNER_RADIUS, CORNER_RADIUS, CORNER_RADIUS,
              M_PI, M_PI * 3 / 2);
    cairo_arc(context, width - CORNER_RADIUS - TOP_WIDTH, CORNER_RADIUS,
              CORNER_RADIUS, M_PI * 3 / 2, 0);
    cairo_line_to(context, width - TOP_WIDTH, (1 - TOP_HEIGHT) / 2);
    cairo_arc(context, width - TOP_RADIUS, (1 - TOP_HEIGHT) / 2 + TOP_RADIUS,
              TOP_RADIUS, M_PI * 3 / 2, 0);
    cairo_arc(context, width - TOP_RADIUS, (1 + TOP_HEIGHT) / 2 - TOP_RADIUS,
              TOP_RADIUS, 0, M_PI / 2);
    cairo_line_to(context, width - TOP_WIDTH, (1 + TOP_HEIGHT) / 2);
    cairo_arc(context, width - CORNER_RADIUS - TOP_WIDTH, 1 - CORNER_RADIUS,
              CORNER_RADIUS, 0, M_PI / 2);
    cairo_arc(context, CORNER_RADIUS, 1 - CORNER_RADIUS, CORNER_RADIUS,
              M_PI / 2, M_PI);
    cairo_close_path(context);
    cairo_stroke(context);
    cairo_move_to(context, width - TOP_WIDTH, (1 - TOP_HEIGHT) / 2);
    cairo_line_to(context, width - TOP_WIDTH, (1 + TOP_HEIGHT) / 2);
    cairo_stroke(context);
  cairo_restore(context);
  /* Filling. */
  cairo_save(context);
    cairo_arc(context, CORNER_RADIUS, CORNER_RADIUS, SMALL_RADIUS,
              M_PI, M_PI * 3 / 2);
    cairo_arc(context, width - CORNER_RADIUS - TOP_WIDTH, CORNER_RADIUS,
              SMALL_RADIUS, M_PI * 3 / 2, 0);
    cairo_arc(context, width - CORNER_RADIUS - TOP_WIDTH, 1 - CORNER_RADIUS,
              SMALL_RADIUS, 0, M_PI / 2);
    cairo_arc(context, CORNER_RADIUS, 1 - CORNER_RADIUS, SMALL_RADIUS,
              M_PI / 2, M_PI);
    cairo_clip(context);
    cairo_translate(context, CORNER_RADIUS - SMALL_RADIUS,
                    CORNER_RADIUS - SMALL_RADIUS);
    PaintBatteryFilling(
        context, sbi, width - TOP_WIDTH - CORNER_RADIUS * 2 + SMALL_RADIUS * 2,
        1 - CORNER_RADIUS * 2 + SMALL_RADIUS * 2);
  cairo_restore(context);
}

#define MARGIN_UP 4
#define MARGIN_DOWN 4
#define MARGIN_LEFT 10
#define MARGIN_RIGHT 10
#define SPACING 10

static gboolean DrawSlot(
    GtkWidget* widget, cairo_t* context, gpointer data) {
  int i;
  double x1, y1, x2, y2;
  double width, height;
  BatteryPanelState* bps = (BatteryPanelState*) data;
  BatteryInfo* bi = &bps->battery_info;
  cairo_clip_extents(context, &x1, &y1, &x2, &y2);
  cairo_translate(context, x1 + MARGIN_LEFT, y1 + MARGIN_UP);
  width = x2 - x1 - MARGIN_LEFT - MARGIN_RIGHT;
  height = y2 - y1 - MARGIN_UP - MARGIN_DOWN;
  cairo_save(context);
    width -= PaintTime(context, bi, width, height);
    for (i = 0; i < bi->number_of_batteries; i++) {
      cairo_save(context);
        cairo_translate(context, width * i / bi->number_of_batteries, 0);
        PaintBattery(context, bi->sbis + i,
                     width / bi->number_of_batteries - SPACING, height);
      cairo_restore(context);
    }
  cairo_restore(context);
  return TRUE;
}

static BatteryPanelState* NewBatteryPanelState() {
  BatteryPanelState* bps =
      (BatteryPanelState*) malloc(sizeof(BatteryPanelState));
  if (bps == NULL) {
    return NULL;
  }
  InitializeBatteryInfo(&bps->battery_info);
  bps->drawing_area = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(bps->drawing_area), "draw",
                   G_CALLBACK(DrawSlot), (void*) bps);
  RegisterCallback(BatteryInfoSlot, (void*) bps);
  return bps;
}

#define PLUGIN_WIDTH 250

static gboolean SizeChangedSlot(XfcePanelPlugin* plugin,
                                gint size, void* data) {
  GtkOrientation orientation;
  orientation = xfce_panel_plugin_get_orientation(plugin);
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    gtk_widget_set_size_request(GTK_WIDGET(plugin), PLUGIN_WIDTH, size);
  } else {
    gtk_widget_set_size_request(GTK_WIDGET(plugin), size, PLUGIN_WIDTH);
  }
  return TRUE;
}

static void BatteryPluginConstructor(XfcePanelPlugin* plugin) {
  BatteryPanelState* bps;
  bps = NewBatteryPanelState();
  if (bps == NULL) {
    fprintf(stderr, "bps == NULL\n");
    abort();
  }
  gtk_container_add(GTK_CONTAINER(plugin), bps->drawing_area);
  g_signal_connect(G_OBJECT(plugin), "size-changed",
                   G_CALLBACK(SizeChangedSlot), bps);
  gtk_widget_show(bps->drawing_area);
  xfce_panel_plugin_set_expand(XFCE_PANEL_PLUGIN(plugin), FALSE);
}

XFCE_PANEL_PLUGIN_REGISTER_INTERNAL(BatteryPluginConstructor);
