#include "home_page.hpp"

HomePage::HomePage(SharedStatus& shared_status)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12),
      shared_status_(shared_status)
{
    set_hexpand(true);
    set_vexpand(true);
    append(shared_status_);
}
