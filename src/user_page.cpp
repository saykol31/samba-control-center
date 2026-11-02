#include "user_page.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/*
 * ================================================================
 * PÁGINA USER — implementación (ver user_page.hpp)
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


static std::string execute_command(
    const std::string& command,
    int* exit_code = nullptr)
{
    std::array<char, 4096> buffer{};

    std::string result;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");

    if (!pipe)
    {
        if (exit_code)
            *exit_code = -1;

        return "ERROR: no fue posible ejecutar el comando.\n";
    }

    while (fgets(buffer.data(),
                 static_cast<int>(buffer.size()), pipe))
    {
        result += buffer.data();
    }

    const int status = pclose(pipe);

    if (exit_code)
    {
        if (WIFEXITED(status))
            *exit_code = WEXITSTATUS(status);
        else
            *exit_code = -1;
    }

    return result;
}


static bool command_exists(const std::string& command)
{
    const std::string cmd =
        "command -v " + shell_escape(command) + " >/dev/null 2>&1";

    return std::system(cmd.c_str()) == 0;
}


/*
 * Una sola autorización pkexec por acción, como el
 * run_admin_command del panel heredado.
 */
static bool run_admin_command(
    const std::string& command,
    std::string& output)
{
    int rc = -1;

    output = execute_command(
        "pkexec /bin/sh -c " + shell_escape(command),
        &rc);

    return rc == 0;
}


static int run_dialog_sync_local(Gtk::Dialog& dialog)
{
    auto loop = Glib::MainLoop::create();

    int response = Gtk::ResponseType::NONE;

    dialog.signal_response().connect(
        [&loop, &response](int result) {
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


static void write_config_lines(
    const std::vector<std::string>& lines)
{
    const fs::path config_path = config_file_path();

    std::error_code error;
    fs::create_directories(config_path.parent_path(), error);

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


/*
 * Escape mínimo para URIs smb:// (usuario y recurso).
 */
static std::string uri_escape(const std::string& value)
{
    std::string result;
    const char* hex = "0123456789ABCDEF";

    for (unsigned char c : value)
    {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~')
        {
            result += static_cast<char>(c);
        }
        else
        {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 0xF];
        }
    }

    return result;
}


/*
 * ------------------------------------------------------------
 * CONSTRUCCIÓN DE LA PÁGINA
 * ------------------------------------------------------------
 */

static Gtk::ScrolledWindow* make_list_scroller(
    Gtk::Widget& list,
    int min_height)
{
    auto* scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(
        Gtk::PolicyType::AUTOMATIC,
        Gtk::PolicyType::AUTOMATIC);
    scrolled->set_min_content_height(min_height);
    scrolled->set_child(list);
    return scrolled;
}


static Gtk::Label* make_column_title(const char* markup)
{
    auto* label = Gtk::make_managed<Gtk::Label>();
    label->set_markup(markup);
    label->set_halign(Gtk::Align::START);
    return label;
}


UserPage::UserPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10)
{
    set_margin_start(16);
    set_margin_end(16);
    set_margin_top(16);
    set_margin_bottom(16);
    set_hexpand(true);
    set_vexpand(true);

    auto* title = Gtk::make_managed<Gtk::Label>(
        "USER — Users and client");
    title->set_halign(Gtk::Align::START);
    append(*title);

    auto* columns = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 12);
    columns->set_homogeneous(true);
    columns->set_vexpand(true);

    // ============ Columna SERVER ============
    auto* server_box = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 8);
    server_box->append(
        *make_column_title("<b>SERVER — Samba users</b>"));

    server_box->append(*make_list_scroller(users_list_, 130));

    auto* btn_refresh_users =
        Gtk::make_managed<Gtk::Button>("REFRESH (admin)");
    btn_refresh_users->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::refresh_users));
    server_box->append(*btn_refresh_users);

    auto* add_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    new_user_entry_.set_placeholder_text("Linux user name");
    new_user_entry_.set_hexpand(true);
    add_row->append(new_user_entry_);
    auto* btn_add = Gtk::make_managed<Gtk::Button>("ADD USER");
    btn_add->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_add_user));
    add_row->append(*btn_add);
    server_box->append(*add_row);

    auto* user_actions = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* btn_password =
        Gtk::make_managed<Gtk::Button>("SET PASSWORD");
    btn_password->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_set_password));
    user_actions->append(*btn_password);
    auto* btn_enable =
        Gtk::make_managed<Gtk::Button>("ENABLE");
    btn_enable->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_enable_user));
    user_actions->append(*btn_enable);
    auto* btn_disable =
        Gtk::make_managed<Gtk::Button>("DISABLE");
    btn_disable->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_disable_user));
    user_actions->append(*btn_disable);
    auto* btn_delete =
        Gtk::make_managed<Gtk::Button>("DELETE");
    btn_delete->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_delete_user));
    user_actions->append(*btn_delete);
    server_box->append(*user_actions);

    columns->append(*server_box);

    // ============ Columna CLIENT ============
    auto* client_box = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 8);
    client_box->append(
        *make_column_title("<b>CLIENT — Connect to shares</b>"));

    auto* server_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* server_label = Gtk::make_managed<Gtk::Label>(
        "Server:");
    server_row->append(*server_label);
    server_entry_.set_placeholder_text("IP or name");
    server_entry_.set_hexpand(true);
    server_row->append(server_entry_);
    auto* btn_scan = Gtk::make_managed<Gtk::Button>("SCAN");
    btn_scan->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_scan));
    server_row->append(*btn_scan);
    client_box->append(*server_row);

    client_box->append(*make_list_scroller(hosts_list_, 70));

    auto* user_row = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* user_label = Gtk::make_managed<Gtk::Label>(
        "User:");
    user_row->append(*user_label);
    client_user_entry_.set_hexpand(true);
    user_row->append(client_user_entry_);
    auto* pass_label = Gtk::make_managed<Gtk::Label>(
        "Password:");
    user_row->append(*pass_label);
    client_password_entry_.property_placeholder_text() =
        "Asked per connection — never saved";
    client_password_entry_.set_hexpand(true);
    user_row->append(client_password_entry_);
    client_box->append(*user_row);

    auto* btn_list = Gtk::make_managed<Gtk::Button>(
        "LIST SHARES");
    btn_list->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_list_shares));
    client_box->append(*btn_list);

    client_box->append(*make_list_scroller(shares_list_, 90));

    auto* share_actions = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* btn_connect =
        Gtk::make_managed<Gtk::Button>("CONNECT");
    btn_connect->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_connect));
    share_actions->append(*btn_connect);
    auto* btn_open = Gtk::make_managed<Gtk::Button>("OPEN");
    btn_open->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_open_share));
    share_actions->append(*btn_open);
    auto* btn_disconnect =
        Gtk::make_managed<Gtk::Button>("DISCONNECT");
    btn_disconnect->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_disconnect));
    share_actions->append(*btn_disconnect);
    client_box->append(*share_actions);

    client_box->append(
        *make_column_title("Saved connections"));

    client_box->append(*make_list_scroller(saved_list_, 70));

    auto* saved_actions = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto* btn_save_conn =
        Gtk::make_managed<Gtk::Button>("SAVE CONNECTION");
    btn_save_conn->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_save_connection));
    saved_actions->append(*btn_save_conn);
    auto* btn_remove_saved =
        Gtk::make_managed<Gtk::Button>("REMOVE SAVED");
    btn_remove_saved->signal_clicked().connect(
        sigc::mem_fun(*this, &UserPage::on_remove_saved));
    saved_actions->append(*btn_remove_saved);
    client_box->append(*saved_actions);

    columns->append(*client_box);
    append(*columns);

    // ============ Log compartido ============
    auto* log_scrolled =
        Gtk::make_managed<Gtk::ScrolledWindow>();
    log_scrolled->set_policy(
        Gtk::PolicyType::AUTOMATIC,
        Gtk::PolicyType::AUTOMATIC);
    log_scrolled->set_min_content_height(100);
    log_view_.set_editable(false);
    log_view_.set_monospace(true);
    log_scrolled->set_child(log_view_);
    append(*log_scrolled);

    // Elegir un host del escaneo lo pone en el campo
    // Server. nmap reporta "nombre (IP)" o solo "IP":
    // al campo va siempre la IP.
    hosts_list_.signal_row_selected().connect(
        [this](Gtk::ListBoxRow* row) {
            if (!row)
                return;

            auto* label =
                dynamic_cast<Gtk::Label*>(row->get_child());

            if (!label)
                return;

            std::string host = label->get_text().raw();

            const auto open = host.find('(');
            const auto close = host.rfind(')');

            if (open != std::string::npos &&
                close != std::string::npos && close > open)
            {
                host = host.substr(open + 1, close - open - 1);
            }

            server_entry_.set_text(trim(host));
        });

    // Elegir una conexión guardada rellena los campos.
    saved_list_.signal_row_selected().connect(
        [this](Gtk::ListBoxRow* row) {
            if (!row)
                return;

            const int index = row->get_index();

            if (index < 0 ||
                index >= static_cast<int>(saved_.size()))
                return;

            const auto& entry = saved_[index];
            server_entry_.set_text(entry[0]);
            client_user_entry_.set_text(entry[2]);

            log_line(
                "[CLIENT] Saved connection loaded: " +
                entry[0] + "/" + entry[1] +
                " — press LIST SHARES and pick the share.");
        });

    // Valores que ya viven en la configuración.
    const auto lines = read_config_lines();
    server_entry_.set_text(config_value(lines, "windows_ip"));
    client_user_entry_.set_text(
        config_value(lines, "client_user"));

    load_saved_connections();

    log_line(
        "[USER] SERVER: REFRESH reads the Samba user list "
        "(one admin prompt). CLIENT: SCAN finds machines on "
        "your LAN; passwords are never saved.");
}


void UserPage::log_line(const std::string& text)
{
    auto buffer = log_view_.get_buffer();
    buffer->insert(buffer->end(), text + "\n");

    auto end = buffer->end();
    log_view_.scroll_to(end, 0.0, 0.0, 1.0);
}


/*
 * ------------------------------------------------------------
 * SERVER — usuarios Samba (pdbedit / smbpasswd)
 * ------------------------------------------------------------
 */

std::string UserPage::selected_user() const
{
    auto* row = users_list_.get_selected_row();

    if (!row)
        return "";

    const int index = row->get_index();

    if (index < 0 ||
        index >= static_cast<int>(user_names_.size()))
        return "";

    return user_names_[index];
}


void UserPage::refresh_users()
{
    if (!command_exists("pdbedit"))
    {
        log_line(
            "[SERVER] [FALTA] pdbedit is not installed "
            "(package: samba-common-bin).");
        return;
    }

    std::string output;

    if (!run_admin_command("pdbedit -L", output))
    {
        log_line(
            "[SERVER] [INFO] Could not read the Samba user "
            "list (cancelled or failed):");
        log_line(output);
        return;
    }

    while (auto* row = users_list_.get_row_at_index(0))
        users_list_.remove(*row);

    user_names_.clear();

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        // Formato pdbedit -L:
        // nombre:uid:LM:NT:[banderas]:LCT — la bandera D
        // en el campo 5 marca la cuenta deshabilitada.
        std::vector<std::string> fields;
        std::istringstream cells(line);
        std::string cell;

        while (std::getline(cells, cell, ':'))
            fields.push_back(cell);

        if (fields.empty() || trim(fields[0]).empty())
            continue;

        const std::string name = trim(fields[0]);

        bool disabled = false;

        if (fields.size() > 4)
        {
            disabled =
                fields[4].find('D') != std::string::npos;
        }

        user_names_.push_back(name);

        auto* label = Gtk::make_managed<Gtk::Label>(
            disabled ? name + " (disabled)" : name);
        label->set_halign(Gtk::Align::START);
        users_list_.append(*label);
    }

    log_line(
        "[SERVER] Samba users: " +
        std::to_string(user_names_.size()) + " found.");
}


bool UserPage::run_user_command(
    const std::string& command,
    const std::string& ok_message)
{
    std::string output;

    const bool ok = run_admin_command(command, output);

    if (!output.empty())
        log_line(trim(output));

    log_line(
        ok ? "[SERVER] [OK] " + ok_message
           : "[SERVER] [ERROR] The command failed.");

    return ok;
}


void UserPage::ask_password(
    const std::string& user,
    bool create)
{
    auto* root = get_root();
    auto* window = dynamic_cast<Gtk::Window*>(root);

    if (!window)
        return;

    auto dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_transient_for(*window);
    dialog->set_modal(true);
    dialog->set_title("Samba password");
    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Save", Gtk::ResponseType::OK);

    auto* content = dialog->get_content_area();
    content->set_spacing(10);
    content->set_margin(15);

    auto* label = Gtk::make_managed<Gtk::Label>(
        (create ? "New Samba user: " : "Samba user: ") + user);
    content->append(*label);

    auto* password1 = Gtk::make_managed<Gtk::PasswordEntry>();
    password1->property_placeholder_text() = "Password";
    content->append(*password1);

    auto* password2 = Gtk::make_managed<Gtk::PasswordEntry>();
    password2->property_placeholder_text() = "Repeat password";
    content->append(*password2);

    dialog->signal_response().connect(
        [this, dialog, password1, password2, user, create]
        (int response) {
            if (response != Gtk::ResponseType::OK)
            {
                dialog->close();
                return;
            }

            const std::string p1 =
                password1->get_text().raw();
            const std::string p2 =
                password2->get_text().raw();

            if (p1.empty())
            {
                log_line(
                    "[SERVER] [ERROR] The password cannot "
                    "be empty.");
                return;
            }

            if (p1 != p2)
            {
                log_line(
                    "[SERVER] [ERROR] The passwords do "
                    "not match.");
                return;
            }

            // El mismo patrón validado del panel: la
            // contraseña viaja por stdin de smbpasswd,
            // nunca a un archivo.
            const std::string command =
                "printf '%s\\n%s\\n' " +
                shell_escape(p1) + " " + shell_escape(p1) +
                " | smbpasswd -s " +
                std::string(create ? "-a " : "") +
                shell_escape(user);

            dialog->close();

            const bool ok = run_user_command(
                command,
                create
                    ? "Samba user '" + user + "' created."
                    : "Password set for '" + user + "'.");

            if (create && ok)
            {
                user_names_.push_back(user);
                auto* label_new = Gtk::make_managed<Gtk::Label>(user);
                label_new->set_halign(Gtk::Align::START);
                users_list_.append(*label_new);
            }
        });

    dialog->present();
}


void UserPage::on_add_user()
{
    const std::string user =
        trim(new_user_entry_.get_text().raw());

    if (user.empty())
    {
        log_line(
            "[SERVER] Type the Linux user name first.");
        return;
    }

    int rc = -1;
    execute_command("id " + shell_escape(user), &rc);

    if (rc != 0)
    {
        log_line(
            "[SERVER] [ERROR] Linux user '" + user +
            "' does not exist. Create the system user "
            "first (the Samba account hangs from it).");
        return;
    }

    ask_password(user, true);
}


void UserPage::on_set_password()
{
    const std::string user = selected_user();

    if (user.empty())
    {
        log_line(
            "[SERVER] Select a Samba user from the list "
            "first (REFRESH if it is empty).");
        return;
    }

    ask_password(user, false);
}


void UserPage::on_enable_user()
{
    const std::string user = selected_user();

    if (user.empty())
    {
        log_line("[SERVER] Select a Samba user first.");
        return;
    }

    run_user_command(
        "smbpasswd -e " + shell_escape(user),
        "User '" + user + "' enabled.");
}


void UserPage::on_disable_user()
{
    const std::string user = selected_user();

    if (user.empty())
    {
        log_line("[SERVER] Select a Samba user first.");
        return;
    }

    run_user_command(
        "smbpasswd -d " + shell_escape(user),
        "User '" + user + "' disabled.");
}


void UserPage::on_delete_user()
{
    const std::string user = selected_user();

    if (user.empty())
    {
        log_line("[SERVER] Select a Samba user first.");
        return;
    }

    auto* root = get_root();
    auto* window = dynamic_cast<Gtk::Window*>(root);

    if (!window)
        return;

    Gtk::MessageDialog dialog(
        *window,
        "Delete Samba user",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true);

    dialog.set_secondary_text(
        "Delete the Samba account '" + user + "'? "
        "The Linux user is NOT deleted — only its Samba "
        "account (pdbedit -x).");

    if (run_dialog_sync_local(dialog) != Gtk::ResponseType::YES)
        return;

    const bool ok = run_user_command(
        "pdbedit -x " + shell_escape(user),
        "Samba user '" + user + "' deleted.");

    if (ok)
    {
        // Sin segundo pkexec: la fila sale de la lista
        // en memoria, igual que el alta la añade.
        const auto it = std::find(
            user_names_.begin(), user_names_.end(), user);

        if (it != user_names_.end())
        {
            const int index = static_cast<int>(
                it - user_names_.begin());

            if (auto* row = users_list_.get_row_at_index(index))
                users_list_.remove(*row);

            user_names_.erase(it);
        }
    }
}


/*
 * ------------------------------------------------------------
 * CLIENT — buscar, listar, montar (GVfs), abrir, desmontar
 * ------------------------------------------------------------
 */

void UserPage::on_scan()
{
    if (!command_exists("nmap"))
    {
        log_line(
            "[CLIENT] [FALTA] nmap is not installed — "
            "SETTINGS > DEPENDENCIES can install it.");
        return;
    }

    int ip_rc = -1;
    const std::string addr_output = execute_command(
        "ip -o -4 addr show up scope global", &ip_rc);

    if (ip_rc != 0)
    {
        log_line(
            "[CLIENT] [ERROR] Could not read the IPv4 "
            "interfaces.");
        return;
    }

    // Subredes locales, con el mismo cálculo que el
    // escaneo del panel (dirección de red desde la CIDR).
    std::vector<std::string> subnets;
    std::istringstream addr_lines(addr_output);
    std::string line;

    while (std::getline(addr_lines, line))
    {
        std::istringstream fields(line);
        std::string index, iface, family, cidr;

        if (!(fields >> index >> iface >> family >> cidr))
            continue;

        if (family != "inet")
            continue;

        const auto slash = cidr.find('/');

        if (slash == std::string::npos)
            continue;

        const std::string address = cidr.substr(0, slash);

        int prefix = -1;

        try
        {
            prefix = std::stoi(cidr.substr(slash + 1));
        }
        catch (...)
        {
            continue;
        }

        if (prefix < 0 || prefix > 32)
            continue;

        in_addr parsed{};

        if (inet_pton(AF_INET, address.c_str(), &parsed) != 1)
            continue;

        const uint32_t host =
            ntohl(parsed.s_addr);
        const uint32_t mask =
            prefix == 0
                ? 0u
                : (0xFFFFFFFFu << (32 - prefix));
        const uint32_t network = host & mask;

        in_addr net_addr{};
        net_addr.s_addr = htonl(network);

        char net_text[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &net_addr, net_text, sizeof(net_text));

        const std::string subnet =
            std::string(net_text) + "/" + std::to_string(prefix);

        if (std::find(subnets.begin(), subnets.end(), subnet) ==
            subnets.end())
        {
            subnets.push_back(subnet);
        }
    }

    while (auto* row = hosts_list_.get_row_at_index(0))
        hosts_list_.remove(*row);

    int found = 0;

    for (const auto& subnet : subnets)
    {
        const std::string output = execute_command(
            "nmap -sn " + shell_escape(subnet));

        std::istringstream scan_lines(output);

        while (std::getline(scan_lines, line))
        {
            const std::string prefix = "Nmap scan report for ";

            if (line.rfind(prefix, 0) != 0)
                continue;

            const std::string host = trim(
                line.substr(prefix.size()));

            if (host.empty())
                continue;

            auto* label = Gtk::make_managed<Gtk::Label>(host);
            label->set_halign(Gtk::Align::START);
            hosts_list_.append(*label);
            ++found;
        }
    }

    log_line(
        "[CLIENT] Scan finished: " +
        std::to_string(found) + " hosts found. Pick one "
        "to fill the Server field.");
}


void UserPage::on_list_shares()
{
    const std::string server =
        trim(server_entry_.get_text().raw());

    if (server.empty())
    {
        log_line("[CLIENT] Type or pick a server first.");
        return;
    }

    if (!command_exists("smbclient"))
    {
        log_line(
            "[CLIENT] [FALTA] smbclient is not installed "
            "(package: smbclient).");
        return;
    }

    const std::string user =
        trim(client_user_entry_.get_text().raw());
    const std::string password =
        client_password_entry_.get_text().raw();

    std::string auth_args;
    fs::path auth_file;

    if (!user.empty())
    {
        // Credenciales en archivo temporal 0600 pasado
        // con -A: la contraseña no va en los argumentos
        // del proceso. Se borra al terminar.
        auth_file = fs::path(
            "/tmp/scc-auth-" +
            std::to_string(
                static_cast<long long>(getpid())));

        {
            std::ofstream create(auth_file);
        }

        std::error_code ec;
        fs::permissions(
            auth_file,
            fs::perms::owner_read | fs::perms::owner_write,
            fs::perm_options::replace, ec);

        {
            std::ofstream out(auth_file);
            out << "username=" << user << "\n";
            out << "password=" << password << "\n";
        }

        auth_args = " -A " + shell_escape(auth_file.string());
    }
    else
    {
        auth_args = " -N";
    }

    int rc = -1;
    const std::string output = execute_command(
        "smbclient -L " + shell_escape(server) +
            " -g" + auth_args,
        &rc);

    if (!auth_file.empty())
    {
        std::error_code ec;
        fs::remove(auth_file, ec);
    }

    while (auto* row = shares_list_.get_row_at_index(0))
        shares_list_.remove(*row);

    share_names_.clear();

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        // Formato -g: "Disk|<nombre>|<comentario>"
        if (line.rfind("Disk|", 0) != 0)
            continue;

        const auto end = line.find('|', 5);

        if (end == std::string::npos)
            continue;

        const std::string name =
            trim(line.substr(5, end - 5));

        if (name.empty())
            continue;

        share_names_.push_back(name);

        auto* label = Gtk::make_managed<Gtk::Label>(name);
        label->set_halign(Gtk::Align::START);
        shares_list_.append(*label);
    }

    if (share_names_.empty())
    {
        log_line(
            "[CLIENT] No shares listed on " + server +
            (rc != 0
                 ? " (smbclient failed — check server, "
                   "user and password)."
                 : "."));
    }
    else
    {
        log_line(
            "[CLIENT] " + server + ": " +
            std::to_string(share_names_.size()) +
            " shares. Pick one and press CONNECT.");
    }
}


std::string UserPage::selected_share() const
{
    auto* row = shares_list_.get_selected_row();

    if (!row)
        return "";

    const int index = row->get_index();

    if (index < 0 ||
        index >= static_cast<int>(share_names_.size()))
        return "";

    return share_names_[index];
}


std::string UserPage::share_uri() const
{
    const std::string server =
        trim(server_entry_.get_text().raw());
    const std::string share = selected_share();
    const std::string user =
        trim(client_user_entry_.get_text().raw());

    if (server.empty() || share.empty())
        return "";

    std::string uri = "smb://";

    if (!user.empty())
        uri += uri_escape(user) + "@";

    uri += server + "/" + uri_escape(share);

    return uri;
}


void UserPage::on_connect()
{
    const std::string uri = share_uri();

    if (uri.empty())
    {
        log_line(
            "[CLIENT] Pick a server and a share first "
            "(LIST SHARES).");
        return;
    }

    if (!command_exists("gio"))
    {
        log_line(
            "[CLIENT] [FALTA] gio is not installed "
            "(package: gvfs) — SETTINGS > DEPENDENCIES "
            "can install it.");
        return;
    }

    const std::string password =
        client_password_entry_.get_text().raw();

    std::string command;

    if (!password.empty())
    {
        // gio pregunta la contraseña por su entrada
        // estándar cuando el recurso la exige.
        command =
            "printf '%s\\n' " + shell_escape(password) +
            " | gio mount " + shell_escape(uri);
    }
    else
    {
        command = "gio mount " + shell_escape(uri);
    }

    int rc = -1;
    const std::string output = execute_command(command, &rc);

    if (!trim(output).empty())
        log_line(trim(output));

    log_line(
        rc == 0
            ? "[CLIENT] [OK] Connected: " + uri +
                  " — press OPEN to browse it."
            : "[CLIENT] [ERROR] Could not connect to " +
                  uri + ".");
}


void UserPage::on_open_share()
{
    const std::string uri = share_uri();

    if (uri.empty())
    {
        log_line(
            "[CLIENT] Pick a server and a share first.");
        return;
    }

    Glib::spawn_command_line_async(
        "gio open " + shell_escape(uri));
}


void UserPage::on_disconnect()
{
    const std::string uri = share_uri();

    if (uri.empty())
    {
        log_line(
            "[CLIENT] Pick a server and a share first.");
        return;
    }

    int rc = -1;
    const std::string output = execute_command(
        "gio mount -u " + shell_escape(uri), &rc);

    if (!trim(output).empty())
        log_line(trim(output));

    log_line(
        rc == 0
            ? "[CLIENT] [OK] Disconnected: " + uri
            : "[CLIENT] [ERROR] Could not disconnect " +
                  uri + ".");
}


/*
 * ------------------------------------------------------------
 * CLIENT — conexiones guardadas (sin contraseña, jamás)
 * ------------------------------------------------------------
 */

void UserPage::load_saved_connections()
{
    saved_.clear();

    for (const auto& line : read_config_lines())
    {
        const std::string clean = trim(line);

        if (clean.rfind("client_connection=", 0) != 0)
            continue;

        const std::string rest = clean.substr(18);

        const auto first = rest.find('|');
        const auto last = rest.rfind('|');

        if (first == std::string::npos ||
            last == std::string::npos || last == first)
            continue;

        saved_.push_back({
            trim(rest.substr(0, first)),
            trim(rest.substr(first + 1, last - first - 1)),
            trim(rest.substr(last + 1)),
        });
    }

    rebuild_saved_list();
}


void UserPage::rebuild_saved_list()
{
    while (auto* row = saved_list_.get_row_at_index(0))
        saved_list_.remove(*row);

    for (const auto& entry : saved_)
    {
        std::string text = entry[0] + "/" + entry[1];

        if (!entry[2].empty())
            text = entry[2] + "@" + text;

        auto* label = Gtk::make_managed<Gtk::Label>(text);
        label->set_halign(Gtk::Align::START);
        saved_list_.append(*label);
    }
}


void UserPage::store_saved_connections() const
{
    std::vector<std::string> lines;

    for (const auto& line : read_config_lines())
    {
        if (trim(line).rfind("client_connection=", 0) == 0)
            continue;

        lines.push_back(line);
    }

    for (const auto& entry : saved_)
    {
        lines.push_back(
            "client_connection=" + entry[0] + "|" +
            entry[1] + "|" + entry[2]);
    }

    write_config_lines(lines);
}


void UserPage::on_save_connection()
{
    const std::string server =
        trim(server_entry_.get_text().raw());
    const std::string share = selected_share();
    const std::string user =
        trim(client_user_entry_.get_text().raw());

    if (server.empty() || share.empty())
    {
        log_line(
            "[CLIENT] Pick a server and a share first — "
            "the saved connection stores both (never the "
            "password).");
        return;
    }

    for (const auto& entry : saved_)
    {
        if (entry[0] == server && entry[1] == share)
        {
            log_line(
                "[CLIENT] That connection is already "
                "saved.");
            return;
        }
    }

    saved_.push_back({server, share, user});
    store_saved_connections();
    rebuild_saved_list();

    log_line(
        "[CLIENT] [OK] Connection saved: " + server +
        "/" + share + " (password not saved, by design).");
}


void UserPage::on_remove_saved()
{
    auto* row = saved_list_.get_selected_row();

    if (!row)
    {
        log_line(
            "[CLIENT] Select a saved connection first.");
        return;
    }

    const int index = row->get_index();

    if (index < 0 ||
        index >= static_cast<int>(saved_.size()))
        return;

    saved_.erase(saved_.begin() + index);
    store_saved_connections();
    rebuild_saved_list();

    log_line("[CLIENT] Saved connection removed.");
}
