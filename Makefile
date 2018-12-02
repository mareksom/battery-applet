CFLAGS = -fPIC -O3
CXXFLAGS = -std=c++14 -fPIC -O3
CXXFLAGS += -Wno-write-strings
CXXLDFLAGS = -pthread

libbatteryapplet.so: xfce_plugin.o battery_info.o
	g++ $^ -o $@ -shared $(CXXLDFLAGS) \
	    $(shell pkg-config gtk+-3.0 libxfce4panel-2.0 --libs)
	sudo cp libbatteryapplet.so /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/libbatteryapplet.so

run.e: main.cpp battery_info.o
	g++ $^ -o $@ $(CXXFLAGS) $(CXXLDFLAGS)

battery_info.o: battery_info.cpp battery_info.h
	g++ $< -o $@ -c $(CXXFLAGS)

xfce_plugin.o: xfce_plugin.c battery_info.h
	gcc $< -o $@ -c $(CFLAGS) \
	    $(shell pkg-config gtk+-3.0 libxfce4panel-2.0 --cflags)

.PHONY: clean
clean:
	rm -f run.e battery_info.o libbatteryapplet.so xfce_plugin.o
