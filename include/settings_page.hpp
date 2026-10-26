#pragma once

#include <gtkmm.h>

class SettingsPage : public Gtk::Box {
public:
    SettingsPage();

private:
    Gtk::Entry windows_ip_entry;
};
