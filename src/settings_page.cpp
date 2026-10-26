#include "settings_page.hpp"

SettingsPage::SettingsPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10) {
    set_margin_start(16);
    set_margin_end(16);
    set_margin_top(16);
    set_margin_bottom(16);

    auto* title = Gtk::make_managed<Gtk::Label>(
        "SETTINGS — configuración");
    title->set_halign(Gtk::Align::START);
    append(*title);

    auto* ip_label = Gtk::make_managed<Gtk::Label>(
        "Dirección IPv4 del equipo Windows:");
    ip_label->set_halign(Gtk::Align::START);
    append(*ip_label);

    windows_ip_entry.set_placeholder_text("Ejemplo: 192.168.1.50");
    windows_ip_entry.set_max_length(15);
    windows_ip_entry.set_width_chars(20);
    windows_ip_entry.set_halign(Gtk::Align::START);
    append(windows_ip_entry);

    auto* note = Gtk::make_managed<Gtk::Label>(
        "La carga y el guardado de la configuración se "
        "implementarán al integrar el código existente.");
    note->set_wrap(true);
    note->set_halign(Gtk::Align::START);
    append(*note);
}
