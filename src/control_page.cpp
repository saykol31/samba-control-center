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


void ControlPage::refresh_folder_status()
{
    if (controller_ == nullptr)
        return;

    controller_->folder_exists();
}


void ControlPage::pick_windows_ip()
{
    if (controller_ == nullptr)
        return;

    controller_->pick_windows_ip();
}


void ControlPage::boot_extras()
{
    if (controller_ == nullptr)
        return;

    // Las mismas acciones que sus botones en CONTROL,
    // en este orden: primero las dos lecturas que el
    // veredicto de HOME necesita (red confirmada y
    // puertos en escucha tras el arranque) y al final
    // el firewall, que corre en segundo plano.
    controller_->show_interfaces();
    controller_->check_ports();
    controller_->configure_firewall();
}
