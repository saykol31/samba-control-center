#pragma once
#include <gtkmm.h>
#include "shared_status.hpp"
class ControlPage : public Gtk::Box {
public:
    ControlPage(Gtk::Window& parent, SharedStatus& shared_status);
};
