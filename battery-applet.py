#!/usr/bin/python3

import gi
import math
import os
import re
import signal
import subprocess
import sys
import threading
import time

gi.require_version('Gtk', '2.0')
gi.require_version('MatePanelApplet', '4.0')

import cairo
from gi.repository import Gtk as gtk
from gi.repository import Gdk as gdk
from gi.repository import GLib as glib
from gi.repository import MatePanelApplet

def RunTlp(*args):
  p = subprocess.Popen(['sudo', '/usr/bin/tlp-stat', *args],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  return stdout.decode('utf-8')

class UpowerMonitor(threading.Thread):
  def __init__(self, callback):
    super(UpowerMonitor, self).__init__()
    self.callback = callback

  def run(self):
    p = subprocess.Popen(['/usr/bin/upower', '--monitor'],
                         stdout=subprocess.PIPE)
    assert p.stdout.readline().decode('utf-8') == \
        'Monitoring activity from the power daemon. Press Ctrl+C to cancel.\n'
    while True:
      p.stdout.readline()
      gdk.threads_enter()
      self.callback()
      gdk.threads_leave()

class Battery(object):
  def __init__(self, info):
    self.Update(info)

  def Update(self, info):
    self.BatteryName = info.get("BatteryName", "??")
    self.BatteryDescription = info.get("BatteryDescription", "??")
    self.Charge = info.get("Charge", "0")
    self.Status = info.get("Status", "Invalid")
    self.EnergyFull = info.get("EnergyFull", 0)
    self.EnergyNow = info.get("EnergyNow", 0)
    self.PowerNow = info.get("PowerNow", 0)

  def IsCharging(self):
    return self.Status == "Charging"

  def ChargeFloat(self):
    return float(self.Charge)

  def EnergyNowFloat(self):
    return float(self.EnergyNow)

  def EnergyFullFloat(self):
    return float(self.EnergyFull)

  def PowerNowFloat(self):
    return float(self.PowerNow)


class BatteryInfo(object):
  def __init__(self):
    self.history = {}
    self.re = re.compile(
        r"""\+\+\+\ ThinkPad\ Battery\ Status:
            \ (?P<BatteryName>\w+)\ \(.*?(?P<BatteryDescription>\w*)\)
            \n
            /sys/class/power_supply/(?P=BatteryName)/manufacturer\ *=\ *
                (?P<Manufacturer>\w*)\n
            /sys/class/power_supply/(?P=BatteryName)/model_name\ *=\ *
                (?P<ModelName>\w*)\n
            /sys/class/power_supply/(?P=BatteryName)/cycle_count\ *=\ *
                (?P<CycleCount>\d*)\n
            /sys/class/power_supply/(?P=BatteryName)/energy_full_design\ *=\ *
                (?P<EnergyFullDesign>\d*\ \[mWh\])\n
            /sys/class/power_supply/(?P=BatteryName)/energy_full\ *=\ *
                (?P<EnergyFull>\d*)\ \[mWh\]\n
            /sys/class/power_supply/(?P=BatteryName)/energy_now\ *=\ *
                (?P<EnergyNow>\d*)\ \[mWh\]\n
            /sys/class/power_supply/(?P=BatteryName)/power_now\ *=\ *
                (?P<PowerNow>\d*)\ \[mW\]\n
            /sys/class/power_supply/(?P=BatteryName)/status\ *=\ *
                (?P<Status>(\w|[() ])*)\n
            \n
            tpacpi-bat.(?P=BatteryName).startThreshold\ *=\ *
                (?P<StartTreshold>\d+)\ \[\%\]\n
            tpacpi-bat.(?P=BatteryName).stopThreshold\ *=\ *
                (?P<StopTreshold>\d+)\ \[\%\]\n
            tpacpi-bat.(?P=BatteryName).forceDischarge\ *=\ *
                (?P<ForceDischarge>0|1)\n
            \n
            Charge\ *=\ *
                (?P<Charge>\d+(\.\d+)?)\ \[\%\]\n
            Capacity\ *=\ *
                (?P<Capacity>\d+(\.\d+)?)\ \[\%\]\n
        """, re.UNICODE | re.VERBOSE)

  def Update(self):
    output = RunTlp('-b')
    is_cleared = False
    for match in re.finditer(self.re, output):
      battery_dict = match.groupdict()
      if "BatteryName" not in battery_dict:
        continue
      if not is_cleared:
        self.batteries = {}
        is_cleared = True
      if battery_dict["BatteryName"] in self.batteries:
        self.batteries[battery_dict["BatteryName"]].Update(battery_dict)
      else:
        self.batteries[battery_dict["BatteryName"]] = Battery(battery_dict)
    if is_cleared:
      self.error = ""
      self.UpdateHistory()
    else:
      self.error = "Retreiving information failed."

  def TotalEnergyNow(self):
    result = 0
    for battery in self.batteries.values():
      result += battery.EnergyNowFloat()
    return result

  def TotalEnergyFull(self):
    result = 0
    for battery in self.batteries.values():
      result += battery.EnergyFullFloat()
    return result

  def TotalPowerNow(self):
    result = 0
    for battery in self.batteries.values():
      result += battery.PowerNowFloat()
    return result

  def UpdateHistory(self):
    battery_list = tuple(sorted(self.batteries.keys()))
    l = []
    if battery_list not in self.history:
      self.history[battery_list] = l
    else:
      l = self.history[battery_list]
    l.append((self.TotalEnergyNow(), time.time()))
    if len(l) > 32:
      l = l[-32:]

  # Returns time in minutes.
  def EstimateTimeLeft(self):
    try:
      for battery in self.batteries.values():
        if battery.IsCharging():
          return 60 * (battery.EnergyFullFloat() - battery.EnergyNowFloat()) \
              / battery.PowerNowFloat()
      return 60 * self.TotalEnergyNow() / self.TotalPowerNow()
    except ZeroDivisionError:
      return None
#    history = self.history[tuple(sorted(self.batteries.keys()))]
#    vs = []
#    for (energy1, timestamp1), (energy2, timestamp2) in \
#        zip(history, history[1:]):
#      energy_delta = energy2 - energy1
#      time_delta = timestamp2 - timestamp1
#      assert time_delta > 0
#      if abs(energy_delta) > 0:
#        vs.append(energy_delta / time_delta)
#    if len(vs) == 0:
#      return None
#    v = vs[-1]
#    if v < 0:
#      return -(self.TotalEnergyNow() / v) / 60
#    else:
#      return ((self.TotalEnergyFull() - self.TotalEnergyNow()) / v) / 60


signal.signal(signal.SIGINT, signal.SIG_DFL)

class BatteryDrawing(gtk.DrawingArea):
  def __init__(self, battery):
    super(BatteryDrawing, self).__init__()
    self.set_size_request(60, 30)
    self.margin_to_height_ratio = 0.08
    self.corner_radius = 0.2
    self.top_width = 0.2
    self.top_height = 0.5
    self.top_radius = 0.15
    self.border_width = 0.1
    self.crevice_width = 0.02
    self.text_scale = 0.9
    self.connect("expose-event", self.on_expose)
    self.battery = battery
    self.set_events(gdk.EventMask.BUTTON_PRESS_MASK)

  def Update(self, battery):
    self.battery = battery
    self.queue_draw()

  def on_expose(self, widget, event):
    context = widget.window.cairo_create()
    w = self.allocation.width
    h = self.allocation.height
    h_after_scale = h / (1 + 2 * self.margin_to_height_ratio)
    margin_after_scale = h_after_scale * self.margin_to_height_ratio
    w_after_scale = w - 2 * margin_after_scale
    context.save()
    context.translate((w - w_after_scale) / 2, (h - h_after_scale) / 2)
    context.scale(h_after_scale, h_after_scale)
    self.draw_battery(context, w_after_scale / h_after_scale)
    context.restore()

  def get_color(self):
    status = self.battery.Status
    percent = self.battery.ChargeFloat()
    if status == 'Charging':
      return (1, 1, 0)
    elif status == 'Discharging':
      if percent < 10:
        return (0.9, 0, 0)
      else:
        return (1, 1, 1)
    elif percent < 10:
      return (0.8, 0.2, 0.2)
    else:
      return (0.5, 0.5, 0.5)

  def draw_battery(self, context, w):
    # Battery.
    context.set_source_rgb(*self.get_color())
    context.set_line_width(self.border_width)
    context.arc(self.corner_radius, self.corner_radius,
                self.corner_radius, math.pi, math.pi * 3 / 2)
    context.arc(w - self.corner_radius - self.top_width, self.corner_radius,
                self.corner_radius, math.pi * 3 / 2, 0)
    context.line_to(w - self.top_width, (1 - self.top_height) / 2)
    context.arc(w - self.top_radius,
                (1 - self.top_height) / 2 + self.top_radius,
                self.top_radius, math.pi * 3 / 2, 0)
    context.arc(w - self.top_radius,
                (1 + self.top_height) / 2 - self.top_radius,
                self.top_radius, 0, math.pi / 2)
    context.line_to(w - self.top_width, (1 + self.top_height) / 2)
    context.arc(w - self.corner_radius - self.top_width, 1 - self.corner_radius,
                self.corner_radius, 0, math.pi / 2)
    context.arc(self.corner_radius, 1 - self.corner_radius,
                self.corner_radius, math.pi / 2, math.pi)
    context.close_path()
    context.stroke()
    context.move_to(w - self.top_width, (1 - self.top_height) / 2)
    context.line_to(w - self.top_width, (1 + self.top_height) / 2)
    context.stroke()
    # Filling.
    small_radius = \
        self.corner_radius - self.border_width / 2 - self.crevice_width
    context.arc(self.corner_radius, self.corner_radius,
                small_radius, math.pi, math.pi * 3 / 2)
    context.arc(w - self.corner_radius - self.top_width, self.corner_radius,
                small_radius, math.pi * 3 / 2, 0)
    context.arc(w - self.corner_radius - self.top_width, 1 - self.corner_radius,
                small_radius, 0, math.pi / 2)
    context.arc(self.corner_radius, 1 - self.corner_radius,
                small_radius, math.pi / 2, math.pi)
    context.clip()
    context.save()
    context.translate(self.corner_radius - small_radius,
                      self.corner_radius - small_radius)
    self.draw_battery_filling(
        context, w - self.top_width - self.corner_radius * 2 + small_radius * 2,
        1 - self.corner_radius * 2 + small_radius * 2)
    context.restore()

  def draw_battery_filling(self, context, w, h):
    # Filling.
    context.set_source_rgb(*self.get_color())
    part = self.battery.ChargeFloat() / 100
    context.rectangle(0, 0, w * part, h)
    context.fill()
    # Text.
    percent_str = "{0:.2f}%".format(part * 100)
    x_bearing, y_bearing, width, height, x_advance, y_advance = \
        context.text_extents(percent_str)
    scale = min((h / height) * self.text_scale, (w / width) * self.text_scale)
    new_height = h / scale
    new_width = w / scale
    context.scale(scale, scale)
    x_bearing, y_bearing, width, height, x_advance, y_advance = \
        context.text_extents(percent_str)
    # Erased part of the text.
    context.save()
    context.set_operator(cairo.OPERATOR_XOR)
    context.move_to((new_width - width) / 2 - x_bearing,
                    (new_height - height) / 2 - y_bearing)
    context.show_text(percent_str)
    context.restore()
    # Drawn part of the text.
    context.save()
    context.rectangle(w * part / scale, 0, w * (1 - part) / scale, h / scale)
    context.clip()
    context.move_to((new_width - width) / 2 - x_bearing,
                    (new_height - height) / 2 - y_bearing)
    context.show_text(percent_str)
    context.restore()


class MainWidget(gtk.HBox):
  def __init__(self):
    super(MainWidget, self).__init__()
    self.battery_info = BatteryInfo()
    self.battery_drawings = {}
    self.BuildMenu()
    self.Update()
    glib.timeout_add_seconds(20, self.Update)
    self.upower_monitor = UpowerMonitor(self.Update)
    self.upower_monitor.start()

  def on_button_press_event(self, widget, event):
    # Not implemented :(
    return False

  def BuildMenu(self):
    # Core menu.
    #self.menu = gtk.Menu()
    # Root menu construction.
    self.menu_bar = gtk.MenuBar()
    self.root_menu = gtk.MenuItem("??:??")
    self.menu_bar.append(self.root_menu)
    #self.root_menu.set_submenu(self.menu)
    self.pack_end(self.menu_bar, expand=False, fill=False, padding=0)
    self.menu_bar.show_all()

  def Update(self):
    self.battery_info.Update()
    if self.battery_info.error != "":
      raise NotImplementedError
    # Order of the batteries.
    position_map = dict()
    def UpdatePositionMap():
      nonlocal self
      nonlocal position_map
      order = sorted(self.battery_drawings.keys())
      position_map = dict()
      for i in range(len(order)):
        position_map[order[i]] = i
    UpdatePositionMap()
    # Updates the batteries that existed earlier.
    battery_ids_to_remove = []
    for battery_id in self.battery_drawings.keys():
      if battery_id in self.battery_info.batteries:
        battery_drawing = self.battery_drawings[battery_id]
        battery_drawing.Update(self.battery_info.batteries[battery_id])
      else:
        battery_ids_to_remove.append(battery_id)
    # Removes the batteries that disappeared.
    for battery_id in battery_ids_to_remove:
      self.remove(self.battery_drawings[battery_id])
      del self.battery_drawings[battery_id]
    UpdatePositionMap()
    # Inserts the batteries that appeared.
    for battery_id, battery in self.battery_info.batteries.items():
      if battery_id not in self.battery_drawings:
        new_battery_drawing = BatteryDrawing(battery)
        new_battery_drawing.connect('button-press-event',
                                    self.on_button_press_event)
        self.battery_drawings[battery_id] = new_battery_drawing
        self.pack_start(new_battery_drawing, expand=True, fill=True, padding=0)
        UpdatePositionMap()
        self.reorder_child(new_battery_drawing, position_map[battery_id])
        new_battery_drawing.show()
    minutes_left = self.battery_info.EstimateTimeLeft()
    if minutes_left is None:
      time_left = "??:??"
    else:
      minutes_left_int = round(minutes_left)
      hours = minutes_left_int // 60
      minutes = minutes_left_int % 60
      time_left = "{:02}:{:02}".format(hours, minutes)
    self.root_menu.set_label(time_left)
    return True


def AppletFill(applet):
  main_widget = MainWidget()
  applet.add(main_widget)
  applet.show_all()


def AppletFactory(applet, iid, data):
  if iid != "MyBatteryApplet":
    return False
  AppletFill(applet)
  return True


if sys.stdout.isatty():
  applet = gtk.Window()
  if not AppletFactory(applet, "MyBatteryApplet", None):
    print("AppletFactory failed :(")
  applet.set_size_request(1200, 200)
  gtk.main()
  sys.exit(0)


MatePanelApplet.Applet.factory_main(
    "MyBatteryAppletFactory", True, MatePanelApplet.Applet.__gtype__,
    AppletFactory, None)
