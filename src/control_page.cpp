#include "control_page.hpp"
#include "legacy_control.hpp"

ControlPage::ControlPage(Gtk::Window& parent, SharedStatus& shared_status)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    set_hexpand(true);
    set_vexpand(true);

    auto* controller = Gtk::make_managed<SambaControlCenterPanel>(parent, shared_status);
    controller->set_hexpand(true);
    controller->set_vexpand(true);
    append(*controller);
}
