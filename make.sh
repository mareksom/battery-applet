function Run() {
  echo $@
  $@
}

Run g++ battery-plugin.cpp -o run.so -shared -fPIC -std=c++14 -Wall -DLOCAL `pkg-config gtkmm-3.0 gtk+-3.0 libxfce4panel-2.0 --cflags --libs` -fpermissive
