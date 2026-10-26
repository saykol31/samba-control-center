#pragma once

#include <gtkmm.h>
#include "shared_status.hpp"
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();

private:
    Gtk::Box root_box{Gtk::Orientation::VERTICAL, 8};
    Gtk::Box menu_box{Gtk::Orientation::HORIZONTAL, 4};
    Gtk::Notebook notebook;
   SharedStatus shared_status;

    void add_menu_item(const Glib::ustring& text);
};
