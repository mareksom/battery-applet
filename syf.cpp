#include <fstream>
#include <gtkmm/application.h>
#include <gtkmm/label.h>
#include <gtkmm/plug.h>
#include <gtkmm/socket.h>
#include <gtkmm/window.h>

#include "makra.h"
//#include "slot_lambda.h"

class MyPlug : public Gtk::Plug {
 public:
  MyPlug() {
    auto label = Gtk::manage(new Gtk::Label("I am the plug."));
    add(*label);
    /*
    signal_embedded().connect(
        []() -> void {
          debug() << "Yay! I have been embedded. :(";
        });
    */
    show_all_children();
  }
};

class MySocketWindow : public Gtk::Window {
 public:
  MySocketWindow() {
    std::ifstream id("swyg.txt");
    if (id) {
      auto socket = Gtk::manage(new Gtk::Socket());
      add(*socket);
      /*
      socket->signal_plug_added().connect(
          []() -> void {
            debug() << "Plug added.";
          });
      socket->signal_plug_removed().connect(
          []() -> void {
            debug() << "Plug removed.";
          });
      */
      ::Window plug_id = 0;
      id >> plug_id;
      id.close();
      socket->add_id(plug_id);
    } else {
      auto label = Gtk::manage(new Gtk::Label("Plug id file not found :("));
      add(*label);
    }
    show_all();
  }
};

int main(int argc, char** argv) {
  bool plugin = true;
  if (argc == 2) plugin = false;
  argc = 1;
  if (plugin) {
    debug() << "Plugin";
    auto app = Gtk::Application::create(argc, argv);
    MyPlug plug;
    plug.show();
    std::ofstream id("swyg.txt");
    id << plug.get_id();
    id.close();
    debug() << imie(plug.get_id());
    return app->run(plug);
  } else {
    debug() << "Socket";
    auto app = Gtk::Application::create(argc, argv);
    MySocketWindow window;
    return app->run(window);
  }
}
