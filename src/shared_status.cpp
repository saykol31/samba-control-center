#include "shared_status.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

void install_dashboard_css()
{
    static bool installed = false;
    if (installed)
        return;

    auto display = Gdk::Display::get_default();
    if (!display)
        return;

    auto css = Gtk::CssProvider::create();
    css->load_from_data(
        ".scc-dashboard-title {"
        " font-size: 20px; font-weight: 800;"
        "}"
        ".scc-dashboard-subtitle { color: #687386; }"
        ".scc-overall {"
        " padding: 12px 16px; border-radius: 10px;"
        " background: #f0f2f5; border: 1px solid #d9dee7;"
        "}"
        ".scc-overall-active {"
        " color: #16803c; background: transparent; border: none;"
        "}"
        ".scc-overall-inactive {"
        " color: #d62828; background: transparent; border: none;"
        "}"
        ".scc-overall-unknown {"
        " background: #f0f2f5; border: 1px solid #d9dee7;"
        "}"
        ".scc-overall-dot { font-size: 18px; font-weight: 900; }"
        ".scc-card {"
        " padding: 12px; border: 1px solid #dfe3ea;"
        " border-radius: 10px; background: #ffffff;"
        "}"
        ".scc-card-title {"
        " font-weight: 800; padding: 2px 4px 8px 4px;"
        "}"
        ".scc-row { padding: 9px 8px; border-radius: 5px; }"
        ".scc-row-even { background: #ffffff; }"
        ".scc-row-odd { background: #f1f3f6; }"
        ".scc-row-name { font-weight: 600; }"
    );

    Gtk::StyleContext::add_provider_for_display(
        display, css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    installed = true;
}

bool status_contains(const std::string& value, const char* word)
{
    return value.find(word) != std::string::npos;
}

} // namespace

SharedStatus::SharedStatus()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12)
{
    install_dashboard_css();

    set_hexpand(true);
    set_vexpand(true);
    set_margin_start(20);
    set_margin_end(20);
    set_margin_top(18);
    set_margin_bottom(20);

    auto* heading = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 4);

    auto* title = Gtk::make_managed<Gtk::Label>(
        "SAMBA CONTROL CENTER");
    title->set_halign(Gtk::Align::START);
    title->add_css_class("scc-dashboard-title");
    heading->append(*title);

    auto* subtitle = Gtk::make_managed<Gtk::Label>(
        "Panel de estado · Debian y distribuciones compatibles");
    subtitle->set_halign(Gtk::Align::START);
    subtitle->add_css_class("scc-dashboard-subtitle");
    heading->append(*subtitle);
    append(*heading);

    auto* overall = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 10);
    overall->set_hexpand(true);
    overall->add_css_class("scc-overall");

    overall_dot_.set_text("●");
    overall_dot_.add_css_class("scc-overall-dot");
   overall_dot_.add_css_class("scc-overall-inactive");

    overall_text_.set_text(
        "INACTIVE");
    overall_text_.set_xalign(0.0f);
    overall_text_.set_hexpand(true);

    overall->append(overall_dot_);
    overall->append(overall_text_);
    append(*overall);

    auto* columns = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 14);
    columns->set_hexpand(true);
    columns->set_vexpand(true);

    auto* network_card = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 4);
    network_card->set_hexpand(true);
    network_card->add_css_class("scc-card");

    auto* network_title = Gtk::make_managed<Gtk::Label>(
        "RED E IDENTIDAD");
    network_title->set_halign(Gtk::Align::START);
    network_title->add_css_class("scc-card-title");
    network_card->append(*network_title);

    auto* system_card = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 4);
    system_card->set_hexpand(true);
    system_card->add_css_class("scc-card");

    auto* system_title = Gtk::make_managed<Gtk::Label>(
        "SISTEMA Y SERVICIOS");
    system_title->set_halign(Gtk::Align::START);
    system_title->add_css_class("scc-card-title");
    system_card->append(*system_title);

    add_row(*network_card, "Samba", samba_status_, 0);
    add_row(*network_card, "Carpeta Red", folder_status_, 1);
    add_row(*network_card, "Puertos 139/445", ports_status_, 2);
    add_row(*network_card, "Red / interfaces", network_status_, 3);

    add_row(*system_card, "Servicio smbd", smbd_status_, 0);
    add_row(*system_card, "Servicio nmbd", nmbd_status_, 1);
    add_row(*system_card, "Cortafuegos UFW", firewall_status_, 2);
    add_row(*system_card, "Herramienta tcpdump", tcpdump_status_, 3);

    columns->append(*network_card);
    columns->append(*system_card);
    append(*columns);

    samba_status_.set_text("Pendiente de comprobación");
    tcpdump_status_.set_text("Pendiente de comprobación");
    folder_status_.set_text("Pendiente de comprobación");
    smbd_status_.set_text("Pendiente de comprobación");
    nmbd_status_.set_text("Pendiente de comprobación");
    firewall_status_.set_text("SIN VERIFICAR");
    ports_status_.set_text("Pendiente de comprobación");
    network_status_.set_text("Pendiente de diagnóstico");

    // Se conservan los mismos Gtk::Label compartidos con CONTROL.
    // Al cambiar sus textos, se actualiza el indicador general.
    samba_status_.property_label().signal_changed().connect(
        [this] { refresh_overall_status(); });
    folder_status_.property_label().signal_changed().connect(
        [this] { refresh_overall_status(); });
    smbd_status_.property_label().signal_changed().connect(
        [this] { refresh_overall_status(); });
    ports_status_.property_label().signal_changed().connect(
        [this] { refresh_overall_status(); });
    network_status_.property_label().signal_changed().connect(
        [this] { refresh_overall_status(); });

    refresh_overall_status();
}

void SharedStatus::add_row(
    Gtk::Box& column, const char* name,
    Gtk::Label& value, int row_number)
{
    auto* row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 10);
    row->set_hexpand(true);
    row->add_css_class("scc-row");
    row->add_css_class(
        row_number % 2 == 0 ? "scc-row-even" : "scc-row-odd");

    auto* label = Gtk::make_managed<Gtk::Label>(name);
    label->set_xalign(0.0f);
    label->set_size_request(132, -1);
    label->add_css_class("scc-row-name");

    value.set_xalign(0.0f);
    value.set_hexpand(true);
    value.set_wrap(true);
    value.set_selectable(true);

    row->append(*label);
    row->append(value);
    column.append(*row);
}

std::string SharedStatus::normalized_status(const Gtk::Label& label)
{
    std::string value = label.get_text();
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
    return value;
}

void SharedStatus::refresh_overall_status()
{
    // Esenciales: Samba instalado, carpeta compartida, smbd,
    // puertos Samba y diagnóstico de red.
    // tcpdump, UFW y nmbd no deciden por sí solos el estado general.
    const std::string samba = normalized_status(samba_status_);
    const std::string folder = normalized_status(folder_status_);
    const std::string smbd = normalized_status(smbd_status_);
    const std::string ports = normalized_status(ports_status_);
    const std::string network = normalized_status(network_status_);

    const auto pending = [](const std::string& s) {
        return s.empty() ||
               status_contains(s, "PENDIENTE") ||
               status_contains(s, "SIN VERIFICAR") ||
               status_contains(s, "SIN COMPROBAR") ||
               status_contains(s, "DESCONOCIDO") ||
               status_contains(s, "NO DISPONIBLE");
    };

    const auto failed = [](const std::string& s) {
        return status_contains(s, "ERROR") ||
               status_contains(s, "STOPPED") ||
               status_contains(s, "DETENIDO") ||
               status_contains(s, "NO INSTALADO") ||
               status_contains(s, "NO EXISTE") ||
               status_contains(s, "NO LISTEN") ||
               status_contains(s, "FALTA");
    };

    const bool has_failure =
        failed(samba) || failed(folder) || failed(smbd) ||
        failed(ports) || failed(network);

    const bool all_confirmed =
        !pending(samba) && !pending(folder) &&
        !pending(smbd) && !pending(ports) &&
        !pending(network);

    // Samba: el chequeo puede mostrar INSTALADO o una versión.
    // Carpeta: OK/EXISTE. smbd debe estar RUNNING.
    // Puertos: al menos un puerto Samba en escucha.
    // Red: diagnóstico de interfaces/rutas confirmado.
    const bool samba_ok =
        status_contains(samba, "INSTALADO") ||
        (!samba.empty() && !failed(samba) && !pending(samba));

    const bool folder_ok =
        status_contains(folder, "OK") || status_contains(folder, "EXISTE");

    const bool ports_ok = status_contains(ports, "LISTEN");

    const bool network_ok =
        status_contains(network, "OK") || status_contains(network, "RUTAS");

    const bool all_good =
        samba_ok && folder_ok && status_contains(smbd, "RUNNING") &&
        ports_ok && network_ok;

    overall_dot_.remove_css_class("scc-overall-active");
   overall_dot_.remove_css_class("scc-overall-inactive");
   overall_dot_.remove_css_class("scc-overall-unknown");

   if (all_confirmed && all_good && !has_failure) {
       overall_text_.set_text("ACTIVE");
       overall_dot_.add_css_class("scc-overall-active");
   } else {
       overall_text_.set_text("INACTIVE");
       overall_dot_.add_css_class("scc-overall-inactive");
   }
}
