function Run() {
  echo $@
  $@ || exit 1
}

Run gcc xfce_register.c -o xfce_register.o -c -fPIC `pkg-config gtk+-3.0 libxfce4panel-2.0 --cflags`
Run gcc battery_plugin.cpp xfce_register.o -o libbatteryapplet.so -shared -fPIC -std=c++14 `pkg-config gtkmm-3.0 gtk+-3.0 libxfce4panel-2.0 --cflags --libs`
sudo cp libbatteryapplet.so /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/libbatteryapplet.so
