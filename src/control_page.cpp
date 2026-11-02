#include "control_page.hpp"
#include "legacy_control.hpp"

ControlPage::ControlPage(Gtk::Window& parent, SharedStatus& shared_status)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    set_hexpand(true);
    set_vexpand(true);

    controller_ = Gtk::make_managed<SambaControlCenterPanel>(parent, shared_status);
    controller_->set_hexpand(true);
    controller_->set_vexpand(true);
    append(*controller_);
}


bool ControlPage::restart_services()
{
    if (controller_ == nullptr)
        return false;

    return controller_->restart_services();
}


void ControlPage::pick_windows_ip()
{
    if (controller_ == nullptr)
        return;

    controller_->pick_windows_ip();
}
