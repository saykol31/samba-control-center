#include "settings_page.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/*
 * ================================================================
 * PÁGINA SETTINGS — implementación (ver settings_page.hpp)
 * ================================================================
 */


/*
 * ------------------------------------------------------------
 * UTILIDADES (mismo criterio que el resto de la app)
 * ------------------------------------------------------------
 */

static std::string trim(const std::string& input)
{
    const char* whitespace = " \t\n\r\f\v";

    const auto begin = input.find_first_not_of(whitespace);

    if (begin == std::string::npos)
        return "";

    const auto end = input.find_last_not_of(whitespace);

    return input.substr(begin, end - begin + 1);
}


static std::string shell_escape(const std::string& value)
{
    std::string result = "'";

    for (char c : value)
    {
        if (c == '\'')
            result += "'\\''";
        else
            result += c;
    }

    result += "'";

    return result;
}


static bool command_exists(const std::string& command)
{
    const std::string cmd =
        "command -v " + shell_escape(command) + " >/dev/null 2>&1";

    return std::system(cmd.c_str()) == 0;
}


/*
 * El mismo archivo de configuración de siempre:
 * $XDG_CONFIG_HOME/samba-control-center/config
 */
static fs::path config_file_path()
{
    const char* config_dir = g_get_user_config_dir();

    if (config_dir && *config_dir)
    {
        return fs::path(config_dir)
            / "samba-control-center"
            / "config";
    }

    return fs::path(Glib::get_home_dir())
        / ".config"
        / "samba-control-center"
        / "config";
}


static std::vector<std::string> read_config_lines()
{
    std::vector<std::string> lines;

    std::ifstream input(config_file_path());
    std::string line;

    while (input && std::getline(input, line))
        lines.push_back(line);

    return lines;
}


static std::string config_value(
    const std::vector<std::string>& lines,
    const std::string& key)
{
    const std::string prefix = key + "=";

    for (const auto& line : lines)
    {
        const std::string clean = trim(line);

        if (clean.rfind(prefix, 0) == 0)
            return trim(clean.substr(prefix.size()));
    }

    return "";
}


/*
 * Guarda pares clave=valor conservando las demás líneas
 * (windows_ip ajeno, share= de FILE, comentarios): solo se
 * reemplazan las claves pedidas. Escritura atómica con
 * temporal + rename, como el resto de la app.
 */
static void save_config_values(
    const std::vector<std::pair<std::string, std::string>>& values)
{
    const fs::path config_path = config_file_path();

    std::error_code error;
    fs::create_directories(config_path.parent_path(), error);

    std::vector<std::string> lines = read_config_lines();

    for (const auto& [key, value] : values)
    {
        const std::string prefix = key + "=";
        bool replaced = false;

        for (auto& line : lines)
        {
            if (trim(line).rfind(prefix, 0) == 0)
            {
                line = key + "=" + value;
                replaced = true;
                break;
            }
        }

        if (!replaced)
            lines.push_back(key + "=" + value);
    }

    const fs::path temp_path =
        fs::path(config_path.string() + ".tmp");

    {
        std::ofstream output(temp_path);

        if (!output)
            return;

        for (const auto& line : lines)
            output << line << '\n';
    }

    fs::rename(temp_path, config_path, error);
}


static bool is_valid_ipv4(const std::string& address)
{
    struct in_addr parsed{};

    return !address.empty()
        && inet_pton(AF_INET, address.c_str(), &parsed) == 1;
}


/*
 * Muestra un diálogo y espera la respuesta, con el mismo
 * truco de bucle principal que run_dialog_sync del panel
 * heredado.
 */
static int run_dialog_sync_local(Gtk::Dialog& dialog)
{
    auto loop = Glib::MainLoop::create();

    int response = Gtk::ResponseType::NONE;

    dialog.signal_response().connect(
        [&loop, &response](int result) {
            // El diálogo emite DOS respuestas: la del
            // botón (-8/-9) y, al cerrarse él mismo, un
            // DELETE_EVENT (-4). Vale la PRIMERA; la
            // segunda pisaba la respuesta y todo clic
            // parecía un "no" (cazado el 2026-11-02:
            // ningún diálogo de la app instalaba ni
            // confirmaba nada).
            if (response == Gtk::ResponseType::NONE)
                response = result;

            if (loop->is_running())
                loop->quit();
        });

    dialog.present();
    loop->run();
    dialog.close();

    return response;
}


/*
 * ------------------------------------------------------------
 * LA TABLA ÚNICA DE DEPENDENCIAS
 * ------------------------------------------------------------
 * Chequeo e instalación leen ESTA tabla y ninguna otra
 * lista. Agregar una dependencia = agregar una fila.
 */

const std::vector<SettingsPage::Dependency>&
SettingsPage::dependency_table()
{
    static const std::vector<Dependency> table = {
        {"Samba server",        "smbd",       "samba",            true},
        {"Samba client",        "smbclient",  "smbclient",        true},
        {"Samba password tool", "smbpasswd",  "samba-common-bin", true},
        {"Samba config tester", "testparm",   "samba-common-bin", true},
        {"Packet capture",      "tcpdump",    "tcpdump",          true},
        {"Firewall",            "ufw",        "ufw",              true},
        {"Network status",      "ss",         "iproute2",         true},
        {"Ping",                "ping",       "iputils-ping",     true},
        {"TCP check",           "nc",         "netcat-openbsd",   true},
        {"Process tools",       "pgrep",      "procps",           true},
        {"Admin authorization", "pkexec",     "pkexec",           true},
        {"Network scan",        "nmap",       "nmap",             false},
        {"CIFS mount",          "mount.cifs", "cifs-utils",       false},
        {"GVfs",                "gio",        "gvfs",             false},
        {"Install console",     "konsole",    "konsole",          false},
        {"Noto fonts",          "Noto Sans",  "fonts-noto-core",  false,
            "fc-list | grep -qi 'noto sans'"},
        {"Emoji font",          "Noto Color Emoji",
            "fonts-noto-color-emoji", false,
            "fc-list | grep -qi 'noto color emoji'"},
        {"DejaVu fonts",        "DejaVu Sans", "fonts-dejavu-core", false,
            "fc-list | grep -qi 'dejavu sans'"},
    };

    return table;
}


/*
 * ------------------------------------------------------------
 * PÁGINA SETTINGS
 * ------------------------------------------------------------
 */

SettingsPage::SettingsPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    install_signal_.connect(
        sigc::mem_fun(
            *this, &SettingsPage::on_install_finished));

    set_hexpand(true);
    set_vexpand(true);

    auto* content = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 10);
    content->set_margin_start(16);
    content->set_margin_end(16);
    content->set_margin_top(16);
    content->set_margin_bottom(16);

    auto* scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(
        Gtk::PolicyType::NEVER,
        Gtk::PolicyType::AUTOMATIC);
    scrolled->set_vexpand(true);
    scrolled->set_child(*content);
    append(*scrolled);

    auto* title = Gtk::make_managed<Gtk::Label>(
        "SETTINGS — Advanced settings");
    title->set_halign(Gtk::Align::START);
    content->append(*title);

    auto* note = Gtk::make_managed<Gtk::Label>(
        "SETTINGS saves values only — it never writes smb.conf "
        "or touches services. Server values take effect with "
        "CONFIGURE SAMBA in CONTROL.");
    note->set_wrap(true);
    note->set_halign(Gtk::Align::START);
    content->append(*note);

    // ---------------- SERVER ----------------

    auto* server_title = Gtk::make_managed<Gtk::Label>();
    server_title->set_markup("<b>🖳 SERVER</b>");
    server_title->set_halign(Gtk::Align::START);
    content->append(*server_title);

    auto* workgroup_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* workgroup_label = Gtk::make_managed<Gtk::Label>(
        "Workgroup:");
    workgroup_row->append(*workgroup_label);
    workgroup_entry_.set_width_chars(20);
    workgroup_entry_.set_halign(Gtk::Align::START);
    workgroup_row->append(workgroup_entry_);
    content->append(*workgroup_row);

    shares_readonly_check_.set_halign(Gtk::Align::START);
    content->append(shares_readonly_check_);

    auto* guest_note = Gtk::make_managed<Gtk::Label>(
        "Guest access: No — fixed by design. Shares always "
        "require a Samba user and password.");
    guest_note->set_wrap(true);
    guest_note->set_halign(Gtk::Align::START);
    content->append(*guest_note);

    // ---------------- CLIENT ----------------

    auto* client_title = Gtk::make_managed<Gtk::Label>();
    client_title->set_markup("<b>🖥 CLIENT</b>");
    client_title->set_halign(Gtk::Align::START);
    content->append(*client_title);

    auto* ip_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* ip_label = Gtk::make_managed<Gtk::Label>(
        "Windows IPv4:");
    ip_row->append(*ip_label);
    windows_ip_entry_.set_placeholder_text("Example: 192.168.1.50");
    windows_ip_entry_.set_max_length(15);
    windows_ip_entry_.set_width_chars(20);
    ip_row->append(windows_ip_entry_);

    auto* btn_pick = Gtk::make_managed<Gtk::Button>("PICK");
    btn_pick->set_tooltip_text(
        "Scan the network and pick the Windows machine "
        "(the CONFIG WIN selector from CONTROL)");
    btn_pick->signal_clicked().connect(
        [this]() { signal_pick_requested.emit(); });
    ip_row->append(*btn_pick);
    content->append(*ip_row);

    auto* user_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* user_label = Gtk::make_managed<Gtk::Label>(
        "Default user:");
    user_row->append(*user_label);
    client_user_entry_.set_placeholder_text(
        "User for outgoing connections (password is asked "
        "when connecting)");
    client_user_entry_.set_hexpand(true);
    user_row->append(client_user_entry_);
    content->append(*user_row);

    auto* save_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* btn_save = Gtk::make_managed<Gtk::Button>(
        "SAVE VALUES");
    btn_save->signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsPage::on_save_values));
    save_row->append(*btn_save);
    values_status_.set_halign(Gtk::Align::START);
    save_row->append(values_status_);
    content->append(*save_row);

    // ---------------- DEPENDENCIES ----------------

    auto* deps_title = Gtk::make_managed<Gtk::Label>();
    deps_title->set_markup("<b>DEPENDENCIES</b>");
    deps_title->set_halign(Gtk::Align::START);
    content->append(*deps_title);

    auto* deps_scrolled =
        Gtk::make_managed<Gtk::ScrolledWindow>();
    deps_scrolled->set_policy(
        Gtk::PolicyType::AUTOMATIC,
        Gtk::PolicyType::AUTOMATIC);
    deps_scrolled->set_min_content_height(170);
    deps_view_.set_editable(false);
    deps_view_.set_monospace(true);

    // Los marcadores de la lista se pintan con etiquetas
    // del búfer, con la misma paleta que el log de CONTROL:
    // [OK] verde, [FALTA] naranja, en negrita. Sin esto la
    // lista salía en texto plano (detectado el 2026-11-03).
    {
        auto dep_buffer = deps_view_.get_buffer();
        auto tag_ok = dep_buffer->create_tag("dep_ok");
        tag_ok->property_foreground().set_value("green");
        tag_ok->property_weight().set_value(700);
        auto tag_falta = dep_buffer->create_tag("dep_falta");
        tag_falta->property_foreground().set_value("orange");
        tag_falta->property_weight().set_value(700);
    }

    deps_scrolled->set_child(deps_view_);
    content->append(*deps_scrolled);

    auto* deps_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    deps_summary_.set_hexpand(true);
    deps_summary_.set_halign(Gtk::Align::START);
    deps_row->append(deps_summary_);

    auto* btn_check = Gtk::make_managed<Gtk::Button>("CHECK");
    btn_check->signal_clicked().connect(
        [this]() { refresh_dependencies(); });
    deps_row->append(*btn_check);

    btn_install_.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsPage::on_install_missing));
    deps_row->append(btn_install_);
    content->append(*deps_row);

    load_values();
    refresh_dependencies();
}


/*
 * ------------------------------------------------------------
 * VALORES
 * ------------------------------------------------------------
 */

void SettingsPage::load_values()
{
    const auto lines = read_config_lines();

    const std::string workgroup = config_value(lines, "workgroup");
    workgroup_entry_.set_text(
        workgroup.empty() ? "WORKGROUP" : workgroup);

    shares_readonly_check_.set_active(
        config_value(lines, "shares_read_only_default") == "1");

    windows_ip_entry_.set_text(config_value(lines, "windows_ip"));
    client_user_entry_.set_text(config_value(lines, "client_user"));
}


void SettingsPage::reload_windows_ip()
{
    windows_ip_entry_.set_text(
        config_value(read_config_lines(), "windows_ip"));
}


void SettingsPage::set_values_feedback(
    const std::string& text,
    bool good)
{
    values_status_.set_markup(
        "<span foreground='" +
        std::string(good ? "green" : "red") +
        "'>" + text + "</span>");
}


void SettingsPage::on_save_values()
{
    const std::string workgroup =
        trim(workgroup_entry_.get_text().raw());
    const std::string windows_ip =
        trim(windows_ip_entry_.get_text().raw());
    const std::string client_user =
        trim(client_user_entry_.get_text().raw());

    if (workgroup.empty() ||
        workgroup.find_first_of(" \t") != std::string::npos)
    {
        set_values_feedback(
            "Workgroup is empty or contains spaces: not saved.",
            false);
        return;
    }

    if (!windows_ip.empty() && !is_valid_ipv4(windows_ip))
    {
        set_values_feedback(
            "Windows IPv4 is not a valid IPv4 address: not saved.",
            false);
        return;
    }

    save_config_values({
        {"workgroup", workgroup},
        {"shares_read_only_default",
         shares_readonly_check_.get_active() ? "1" : "0"},
        {"windows_ip", windows_ip},
        {"client_user", client_user},
    });

    set_values_feedback(
        "Values saved. Server values take effect with "
        "CONFIGURE SAMBA in CONTROL.",
        true);
}


/*
 * ------------------------------------------------------------
 * DEPENDENCIAS
 * ------------------------------------------------------------
 */

bool SettingsPage::dependency_present(
    const Dependency& dep) const
{
    if (dep.probe && dep.probe[0])
    {
        const std::string probe_cmd =
            std::string(dep.probe) + " >/dev/null 2>&1";

        return std::system(probe_cmd.c_str()) == 0;
    }

    return command_exists(dep.command);
}


std::vector<const SettingsPage::Dependency*>
SettingsPage::missing_dependencies() const
{
    std::vector<const Dependency*> missing;

    for (const auto& dep : dependency_table())
    {
        if (!dependency_present(dep))
            missing.push_back(&dep);
    }

    return missing;
}


void SettingsPage::refresh_dependencies()
{
    int satisfied = 0;
    const auto& table = dependency_table();

    auto buffer = deps_view_.get_buffer();
    buffer->set_text("");

    auto tag_ok = buffer->get_tag_table()->lookup("dep_ok");
    auto tag_falta = buffer->get_tag_table()->lookup("dep_falta");

    for (const auto& dep : table)
    {
        const bool present = dependency_present(dep);

        if (present)
            ++satisfied;

        // El marcador se inserta con su etiqueta de color;
        // el resto de la línea va en texto plano.
        if (present)
            buffer->insert_with_tag(buffer->end(), "[OK] ", tag_ok);
        else
            buffer->insert_with_tag(buffer->end(), "[FALTA] ", tag_falta);

        std::string line = dep.command;
        line += " — ";
        line += dep.label;
        line += " (package: ";
        line += dep.package;
        line += dep.required ? ")" : ", optional)";
        line += "\n";

        buffer->insert(buffer->end(), line);
    }

    const auto total =
        static_cast<int>(table.size());

    deps_summary_.set_text(
        "Dependencies satisfied: " +
        std::to_string(satisfied) + " of " +
        std::to_string(total) + ".");

    btn_install_.set_sensitive(satisfied < total);
}


void SettingsPage::on_install_missing()
{
    if (install_busy_)
        return;

    const auto missing = missing_dependencies();

    if (missing.empty())
    {
        refresh_dependencies();
        return;
    }

    // Paquetes únicos, en orden de tabla.
    std::vector<std::string> packages;

    for (const auto* dep : missing)
    {
        if (std::find(packages.begin(), packages.end(),
                      dep->package) == packages.end())
        {
            packages.push_back(dep->package);
        }
    }

    std::string package_list;

    for (const auto& package : packages)
    {
        if (!package_list.empty())
            package_list += " ";
        package_list += package;
    }

    auto* root = get_root();
    auto* window = dynamic_cast<Gtk::Window*>(root);

    if (!window)
        return;

    Gtk::MessageDialog dialog(
        *window,
        "Dependencies not found",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true);

    dialog.set_secondary_text(
        "Missing packages:\n" + package_list +
        "\n\nInstall them now? The installation runs "
        "in a konsole window that stays open, next to "
        "the interface. One apt transaction, packages "
        "by name (no pinned versions).");

    if (run_dialog_sync_local(dialog) != Gtk::ResponseType::YES)
        return;

    // El mismo tratamiento que el guarda de
    // CONTROL — el mecanismo del Update de idec: la
    // instalación se ve en una konsole que no se
    // cierra (--noclose), al lado de la interfaz,
    // con sudo pidiendo la contraseña en la propia
    // terminal. La vía anterior (pkexec en un hilo,
    // con la salida capturada y volcada al final)
    // era invisible; y en WSL pkexec ni siquiera
    // está instalado, así que moría en silencio.
    auto buffer = deps_view_.get_buffer();

    if (!command_exists("konsole"))
    {
        buffer->insert(
            buffer->end(),
            "\n[ERROR] konsole is not installed: "
            "the installation cannot be shown.\n"
            "Install it from a terminal with:\n"
            "sudo apt install konsole\n");

        return;
    }

    const std::string script =
        "echo '=== Dependencies — Samba Control "
        "Center ==='; "
        "echo; "
        "sudo apt-get update && "
        "sudo apt-get install -y " +
        package_list +
        "; echo; "
        "echo '=== Done: check the result "
        "above ==='; "
        "read -p 'Press ENTER to close this "
        "console'";

    try
    {
        Glib::spawn_command_line_async(
            "konsole --noclose -e bash -c " +
            shell_escape(script));

        buffer->insert(
            buffer->end(),
            "\nInstall console opened in konsole: "
            "the installation runs in that "
            "window.\nWhen it finishes, press "
            "CHECK to re-verify.\n");
    }
    catch (const Glib::Error& error)
    {
        buffer->insert(
            buffer->end(),
            std::string(
                "\n[ERROR] Could not open "
                "konsole: ") +
            error.what() + "\n");
    }
}


void SettingsPage::on_install_finished()
{
    if (install_thread_.joinable())
        install_thread_.join();

    install_busy_ = false;
    btn_install_.set_sensitive(true);

    auto buffer = deps_view_.get_buffer();
    buffer->insert(
        buffer->end(),
        "\n--- apt output ---\n" + install_output_);

    refresh_dependencies();
}


SettingsPage::~SettingsPage()
{
    if (install_thread_.joinable())
        install_thread_.join();
}
