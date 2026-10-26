#include "user_page.hpp"

UserPage::UserPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10) {
    set_margin_start(16);
    set_margin_end(16);
    set_margin_top(16);
    set_margin_bottom(16);

    auto* title = Gtk::make_managed<Gtk::Label>(
        "USER — usuarios Samba");
    title->set_halign(Gtk::Align::START);
    append(*title);

    auto* info = Gtk::make_managed<Gtk::Label>(
        "Este módulo mostrará las cuentas Samba y permitirá "
        "crear usuarios, cambiar contraseñas y gestionar el acceso.");
    info->set_wrap(true);
    info->set_halign(Gtk::Align::START);
    append(*info);

    auto* warning = Gtk::make_managed<Gtk::Label>(
        "Las operaciones reales se conectarán al mecanismo "
        "de autorización existente.");
    warning->set_wrap(true);
    warning->set_halign(Gtk::Align::START);
    append(*warning);
}
