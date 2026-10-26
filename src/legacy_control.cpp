#include "legacy_control.hpp"

/*
 * ================================================================
 * SAMBA CONTROL CENTER
 * ================================================================
 *
 * GUI GTKmm 4 para MX Linux / Debian
 *
 * SysVinit
 *
 * Funciones:
 *
 *  - Verificación de Samba
 *  - Verificación de dependencias
 *  - Creación de /home/$USER/Red
 *  - Permisos
 *  - Configuración smb.conf
 *  - backup de smb.conf
 *  - testparm
 *  - usuario Samba
 *  - contraseña Samba
 *  - smbd / nmbd
 *  - UFW
 *  - puertos 139 / 445
 *  - interfaces de red
 *  - rutas
 *  - ARP / neighbours
 *  - ping
 *  - prueba TCP 445
 *  - smbclient
 *  - tcpdump
 *  - Nmap
 * Compatible con:
 *
 *  MX Linux / Debian
 *  GTKmm 4
 *  C++17
 *  SysVinit
 *
 * ================================================================
 */

#include <gtkmm.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>

namespace fs = std::filesystem;


/*
 * ================================================================
 * UTILIDADES
 * ================================================================
 */

static std::string trim(const std::string& input)
{
    const char* whitespace = " \t\n\r\f\v";

    const auto begin =
        input.find_first_not_of(whitespace);

    if (begin == std::string::npos)
        return "";

    const auto end =
        input.find_last_not_of(whitespace);

    return input.substr(
        begin,
        end - begin + 1
    );
}


/*
 * Escapa una cadena para utilizarla como argumento
 * dentro de una orden shell.
 */
static std::string shell_escape(
    const std::string& value)
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


/*
 * Ejecuta una orden y devuelve stdout + stderr.
 */
static std::string execute_command(
    const std::string& command,
    int* exit_code = nullptr)
{
    std::array<char, 4096> buffer{};

    std::string result;

    const std::string full_command =
        command + " 2>&1";

    FILE* pipe =
        popen(
            full_command.c_str(),
            "r"
        );

    if (!pipe)
    {
        if (exit_code)
            *exit_code = -1;

        return
            "ERROR: no fue posible ejecutar el comando.\n";
    }

    while (
        fgets(
            buffer.data(),
            static_cast<int>(buffer.size()),
            pipe
        )
    )
    {
        result += buffer.data();
    }

    const int status =
        pclose(pipe);

    if (exit_code)
    {
        if (WIFEXITED(status))
            *exit_code =
                WEXITSTATUS(status);
        else
            *exit_code = -1;
    }

    return result;
}


static bool command_exists(
    const std::string& command)
{
    const std::string cmd =
        "command -v " +
        shell_escape(command) +
        " >/dev/null 2>&1";

    return std::system(
        cmd.c_str()
    ) == 0;
}

/*
 * Selecciona el gestor de servicios según el init activo.
 * systemd -> systemctl
 * SysVinit/OpenRC y compatibles -> service o /etc/init.d
 */
static std::string service_command(
    const std::string& service,
    const std::string& action)
{
    std::string init;
    std::ifstream init_file("/proc/1/comm");

    if (init_file)
        std::getline(init_file, init);

    init = trim(init);

    if (init == "systemd" && command_exists("systemctl"))
    {
        return "systemctl " + shell_escape(action) +
               " " + shell_escape(service);
    }

    if (command_exists("service"))
    {
        return "service " + shell_escape(service) +
               " " + shell_escape(action);
    }

    const std::string script = "/etc/init.d/" + service;

    if (fs::exists(script))
    {
        return shell_escape(script) +
               " " + shell_escape(action);
    }

    return "printf '%s\\n' "
           "'ERROR: no se encontró un gestor de servicios compatible.' "
           " >&2; exit 127";
}


static std::string get_command_output(
    const std::string& command)
{
    return trim(
        execute_command(command)
    );
}


static std::string current_user()
{
    const char* user =
        std::getenv("USER");

    if (user && *user)
        return user;

    return get_command_output(
        "id -un"
    );
}


static std::string home_directory()
{
    const char* home =
        std::getenv("HOME");

    if (home && *home)
        return home;

    const std::string user =
        current_user();

    const std::string from_getent =
        get_command_output(
            "getent passwd " +
            shell_escape(user) +
            " | cut -d: -f6"
        );

    if (!from_getent.empty())
        return from_getent;

    return "/home/" + user;
}

/*
 * ================================================================
 * funciones auxiliares
 * ================================================================
 */

/*
 * Configuración de red por usuario.
 * Archivo: $XDG_CONFIG_HOME/samba-control-center/config
 */
static fs::path windows_config_file()
{
    const char* config_dir = g_get_user_config_dir();

    if (config_dir && *config_dir)
    {
        return fs::path(config_dir)
            / "samba-control-center"
            / "config";
    }

    return fs::path(home_directory())
        / ".config"
        / "samba-control-center"
        / "config";
}

static bool is_valid_ipv4(const std::string& address)
{
    struct in_addr parsed_address{};

    return !address.empty()
        && inet_pton(
            AF_INET,
            address.c_str(),
            &parsed_address
        ) == 1;
}

static std::string load_windows_ip_config()
{
    const fs::path config_file = windows_config_file();

    std::error_code error;
    fs::create_directories(config_file.parent_path(), error);

    std::ifstream input(config_file);

    if (!input)
    {
        std::ofstream output(config_file);

        if (output)
        {
            output
                << "# Destino Windows para diagnosticos de red\n"
                << "# Introduce una direccion IPv4, por ejemplo:\n"
                << "# windows_ip=192.168.1.50\n"
                << "windows_ip=\n";
        }

        return "";
    }

    std::string line;

    while (std::getline(input, line))
    {
        line = trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        constexpr const char* key = "windows_ip=";

        if (line.rfind(key, 0) == 0)
        {
            const std::string address =
                trim(line.substr(std::char_traits<char>::length(key)));

            return is_valid_ipv4(address) ? address : "";
        }
    }

    return "";
}

/*
 * ================================================================
 * LOTE ADMINISTRATIVO (UNA SOLA AUTORIZACIÓN)
 * ================================================================
 *
 * Los pasos con privilegios de una acción se encadenan en un
 * único script para pkexec: el usuario autoriza una sola vez.
 *
 * Cada paso deja su código de salida en un marcador:
 *
 *      SCC_RC_<paso>=<código>
 *
 * Si un paso falla, los siguientes se saltan ("skip").
 */

static std::string batch_step(
    const std::string& step,
    const std::string& command)
{
    return
        "if [ $F -eq 0 ]; then " +
        command +
        "; RC=$?; else RC=skip; fi\n"
        "echo SCC_RC_" +
        step +
        "=$RC\n"
        "if [ \"$RC\" != \"skip\" ] && [ $RC -ne 0 ]; "
        "then F=1; fi\n";
}


/*
 * Extrae de la salida del lote el texto producido por un paso
 * y su código (0 = bien, -1 = sin marcador, -2 = saltado).
 * Avanza el cursor hasta después del marcador.
 */
static std::string batch_take_segment(
    const std::string& output,
    std::size_t& cursor,
    const std::string& step,
    int& code)
{
    const std::string marker =
        "SCC_RC_" +
        step +
        "=";

    const std::size_t pos =
        output.find(marker, cursor);

    if (pos == std::string::npos)
    {
        code = -1;

        const std::string rest =
            output.substr(cursor);

        cursor = output.size();

        return rest;
    }

    std::string segment =
        output.substr(
            cursor,
            pos - cursor
        );

    while (!segment.empty() &&
           (segment.back() == '\n' ||
            segment.back() == '\r'))
        segment.pop_back();

    const std::size_t value_begin =
        pos + marker.size();

    const std::size_t value_end =
        output.find(
            '\n',
            value_begin
        );

    const std::string value =
        output.substr(
            value_begin,
            value_end == std::string::npos
                ? std::string::npos
                : value_end - value_begin
        );

    code =
        value == "skip"
            ? -2
            : std::atoi(value.c_str());

    cursor =
        value_end == std::string::npos
            ? output.size()
            : value_end + 1;

    return segment;
}

/*
 * Paso de lote de SOLO LECTURA: se ejecuta siempre, sin depender de
 * que los pasos anteriores hayan tenido éxito. batch_step sigue
 * siendo el correcto para operaciones que modifican el sistema
 * (deben detenerse tras un fallo); las lecturas independientes —
 * ufw status, pdbedit -L — no deben bloquearse entre sí.
 * Mismos marcadores SCC_BEGIN / SCC_END: batch_take_segment lo
 * consume igual que un paso normal.
 */
static std::string batch_step_always(
    const std::string& name,
    const std::string& command)
{
    return
        command +
        "; RC=$?;\n"
        "echo SCC_RC_" +
        name +
        "=$RC\n";
}


/*
 * Busca un usuario en la salida de "pdbedit -L" (una línea por
 * usuario: nombre:uid:...). Devuelve true si aparece.
 * Es el bucle que antes vivía en samba_user_exists().
 */
static bool pdbedit_list_has_user(
    const std::string& output,
    const std::string& user)
{
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        const auto pos = line.find(':');

        const std::string name =
            pos == std::string::npos
                ? trim(line)
                : trim(line.substr(0, pos));

        if (name == user)
            return true;
    }

    return false;
}

/*
 * ================================================================
 * CLASE PRINCIPAL
 * ================================================================
 */

SambaControlCenterPanel::SambaControlCenterPanel(
    Gtk::Window& parent,
    SharedStatus& shared_status
)
    : Gtk::Box(Gtk::Orientation::VERTICAL),
      parent_window(&parent),
      shared_status_(shared_status),
      samba_status(shared_status.samba()),
      tcpdump_status(shared_status.tcpdump()),
      folder_status(shared_status.folder()),
      smbd_status(shared_status.smbd()),
      nmbd_status(shared_status.nmbd()),
      firewall_status(shared_status.firewall()),
      ports_status(shared_status.ports()),
      network_status(shared_status.network())
{
    shared_folder =
        home_directory() +
        "/Red";

    samba_user =
        current_user();

    windows_ip = load_windows_ip_config();

    setup_ui();

    append_log(
        "Samba Control Center iniciado."
    );

    append_log(
        "Usuario: " +
        samba_user
    );

    append_log(
        "HOME: " +
        home_directory()
    );

    append_log(
        "Carpeta: " +
        shared_folder
    );

    append_log(
        "Configuracion de red: " +
        windows_config_file().string()
    );

    if (windows_ip.empty())
    {
        append_log(
            "[INFO] No hay una IPv4 Windows valida configurada. "
            "Edita el archivo de configuracion para activar "
            "los diagnosticos dirigidos a Windows."
        );
    }
    else
    {
        append_log(
            "Destino Windows: " +
            windows_ip
        );
    }

    check_dependencies();
    check_samba();
    folder_exists();
    check_services();
    set_status_unknown(
        firewall_status,
        "SIN VERIFICAR"
    );
    check_ports();
}


/*
 * ================================================================
 * CONFIGURACIÓN DE LA INTERFAZ
 * ================================================================
 */

void SambaControlCenterPanel::setup_ui()
{
    /*
     * ================================================================
     * VENTANA PRINCIPAL
     * ================================================================
     */

    append(main_box);

    main_box.set_margin(15);
    main_box.set_spacing(10);

    /*
     * ================================================================
     * BOTONERA
     * ================================================================
     */

    auto* button_grid =
        Gtk::make_managed<Gtk::Grid>();

    button_grid->set_row_spacing(6);
    button_grid->set_column_spacing(6);

    /*
     * ------------------------------------------------
     * BOTONES PRINCIPALES
     * ------------------------------------------------
     */

    configure_button(
        btn_install,
        "DEPENDENCIAS",
        "Verificar e instalar dependencias"
    );

    configure_button(
        btn_configure,
        "CONFIGURAR SAMBA",
        "Crear carpeta y configurar Samba"
    );

    configure_button(
        btn_password,
        "CONTRASEÑA SAMBA",
        "Crear o cambiar contraseña Samba"
    );

    configure_button(
        btn_restart,
        "REINICIAR SERVICIOS",
        "Reiniciar smbd y nmbd"
    );

    configure_button(
        btn_firewall,
        "FIREWALL UFW",
        "Configurar firewall para Samba"
    );

    configure_button(
        btn_ports,
        "PUERTOS 139/445",
        "Comprobar puertos Samba"
    );

    configure_button(
        btn_network,
        "RED / RUTAS",
        "Mostrar interfaces, rutas y vecinos"
    );

    configure_button(
        btn_ping,
        "PING WINDOWS",
        "Probar conectividad con Windows"
    );

    configure_button(
        btn_smbclient,
        "PRUEBA SMB",
        "Probar smbclient"
    );

    configure_button(
        btn_tcpdump,
        "TCPDUMP",
        "Diagnóstico de tráfico SMB"
    );

    configure_button(
        btn_diagnose,
        "DIAGNÓSTICO COMPLETO",
        "Ejecutar diagnóstico completo"
    );

    configure_button(
        btn_clear,
        "LIMPIAR",
        "Limpiar consola"
    );

   // Botones adicionales de red

   configure_button(
       btn_interfaces,
       "INTERFACES",
       "Mostrar las interfaces de red disponibles"
   );

   configure_button(
       btn_routes,
       "RUTAS DE RED",
       "Mostrar la tabla de rutas del sistema"
   );

   configure_button(
       btn_gateway,
       "PUERTA DE ENLACE",
       "Mostrar la puerta de enlace de la red"
   );

   configure_button(
       btn_neighbors,
       "VECINOS DE RED",
       "Mostrar vecinos ARP y entradas de red"
   );

   configure_button(
       btn_windows,
       "RED WINDOWS",
       "Mostrar información de red del equipo Windows"
   );

   configure_button(
       btn_tcp445,
       "TCP 445 WINDOWS",
       "Probar conectividad TCP con el puerto 445 de Windows"
   );

   configure_button(
       btn_config_win,
       "CONFIG WIN",
       "Configurar la dirección IPv4 del equipo Windows"
   );


   configure_button(
       btn_scan_red,
       "ESCANEAR RED",
       "Descubrir equipos en las subredes IPv4 locales"
   );
   // Botones compactos: cuatro columnas, sin encabezados intermedios.
   button_grid->attach(btn_install,    0, 0);
   button_grid->attach(btn_configure,  1, 0);
   button_grid->attach(btn_password,   2, 0);
   button_grid->attach(btn_restart,    3, 0);

   button_grid->attach(btn_scan_red,   0, 1);
   button_grid->attach(btn_config_win, 1, 1);
   button_grid->attach(btn_ping,       2, 1);
   button_grid->attach(btn_tcp445,     3, 1);

   button_grid->attach(btn_diagnose,   0, 2);
   button_grid->attach(btn_firewall,   1, 2);
   button_grid->attach(btn_ports,      2, 2);
   button_grid->attach(btn_smbclient,  3, 2);

   button_grid->attach(btn_tcpdump,    0, 3);
   button_grid->attach(btn_network,    1, 3);
   button_grid->attach(btn_interfaces, 2, 3);
   button_grid->attach(btn_routes,     3, 3);

   button_grid->attach(btn_gateway,    0, 4);
   button_grid->attach(btn_neighbors,  1, 4);
   button_grid->attach(btn_windows,    2, 4);

   // LIMPIAR permanece junto a la consola.
   main_box.append(*button_grid);


   // Estilo visual de los botones de CONTROL.
   auto control_css = Gtk::CssProvider::create();
   control_css->load_from_data(R"CSS(
      button.scc-control-button {
         transition: background-color 120ms ease;
      }
      button.scc-control-button.scc-control-selected {
         background-color: #c7def7;
         background-image: none;
         color: #18334d;
      }
      button.scc-control-button:hover {
         background-color: #e4eaf1;
         background-image: none;
      }
   )CSS");

   if (auto display = Gdk::Display::get_default()) {
      Gtk::StyleContext::add_provider_for_display(
         display,
         control_css,
         GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
      );
   }

   // Los 20 botones, incluido LIMPIAR.
   for (auto* button : {
         &btn_install, &btn_configure, &btn_password, &btn_restart,
         &btn_firewall, &btn_ports, &btn_network, &btn_interfaces,
         &btn_routes, &btn_gateway, &btn_neighbors, &btn_windows,
         &btn_tcp445, &btn_config_win, &btn_scan_red, &btn_ping,
         &btn_smbclient, &btn_tcpdump, &btn_diagnose, &btn_clear
   }) {
      button->add_css_class("scc-control-button");
   }

   // Al hacer clic, solo cambia qué botón queda marcado.
   // Las conexiones originales de las acciones se conservan.
   for (auto* button : {
         &btn_install, &btn_configure, &btn_password, &btn_restart,
         &btn_firewall, &btn_ports, &btn_network, &btn_interfaces,
         &btn_routes, &btn_gateway, &btn_neighbors, &btn_windows,
         &btn_tcp445, &btn_config_win, &btn_scan_red, &btn_ping,
         &btn_smbclient, &btn_tcpdump, &btn_diagnose, &btn_clear
   }) {
      button->signal_clicked().connect([this, button]() {
         for (auto* candidate : {
               &btn_install, &btn_configure, &btn_password, &btn_restart,
               &btn_firewall, &btn_ports, &btn_network, &btn_interfaces,
               &btn_routes, &btn_gateway, &btn_neighbors, &btn_windows,
               &btn_tcp445, &btn_config_win, &btn_scan_red, &btn_ping,
               &btn_smbclient, &btn_tcpdump, &btn_diagnose, &btn_clear
         }) {
            candidate->remove_css_class("scc-control-selected");
         }
         button->add_css_class("scc-control-selected");
      });
   }

/*
     * ================================================================
     * CONSOLA / LOG
     * ================================================================
     */

    log_buffer =
        Gtk::TextBuffer::create();

    log_view.set_buffer(
        log_buffer
    );

    log_view.set_editable(
        false
    );

    log_view.set_cursor_visible(
        false
    );

    log_view.set_monospace(
        true
    );

    log_scroll.set_child(
        log_view
    );

    log_scroll.set_vexpand(
        true
    );

    btn_clear.set_halign(
        Gtk::Align::END
    );

    main_box.append(
        btn_clear
    );

    main_box.append(
        log_scroll
    );

    /*
     * ================================================================
     * SIGNALS
     * ================================================================
     *
     * Cada botón se conecta UNA SOLA VEZ.
     *
     */

    btn_install.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::check_dependencies
        )
    );

    btn_configure.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::configure_samba
        )
    );

    btn_password.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::show_password_dialog
        )
    );

    btn_restart.signal_clicked().connect(
        sigc::hide_return(
            sigc::mem_fun(
                *this,
                &SambaControlCenterPanel::restart_services
            )
        )
    );

    btn_firewall.signal_clicked().connect(
        sigc::hide_return(
            sigc::mem_fun(
                *this,
                &SambaControlCenterPanel::configure_firewall
            )
        )
    );

    btn_ports.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::check_ports
        )
    );

    btn_network.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::show_routing
        )
    );

    btn_ping.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::ping_windows
        )
    );

    btn_smbclient.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::test_smbclient
        )
    );

    btn_tcpdump.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::run_tcpdump_diagnostic
        )
    );

    btn_diagnose.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::full_diagnostic
        )
    );

    btn_clear.signal_clicked().connect(
        sigc::mem_fun(
            *this,
            &SambaControlCenterPanel::clear_log
        )
    );

   // Señales de los seis botones adicionales

   btn_interfaces.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::show_interfaces
       )
   );

   btn_routes.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::show_routes
       )
   );

   btn_gateway.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::show_gateway
       )
   );

   btn_neighbors.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::show_neighbors
       )
   );

   btn_windows.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::show_windows_network
       )
   );

   btn_tcp445.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::test_windows_tcp445
       )
   );

   btn_config_win.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::configure_windows
       )
   );

   btn_scan_red.signal_clicked().connect(
       sigc::mem_fun(
           *this,
           &SambaControlCenterPanel::scan_network
       )
   );
}


void SambaControlCenterPanel::configure_windows()
{
    auto dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_transient_for(*parent_window);
    dialog->set_modal(true);
    dialog->set_title("Configurar equipo Windows");

    dialog->add_button("_Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Guardar", Gtk::ResponseType::OK);
    dialog->set_default_response(Gtk::ResponseType::OK);

    auto* content = dialog->get_content_area();
    content->set_spacing(8);
    content->set_margin_start(16);
    content->set_margin_end(16);
    content->set_margin_top(16);
    content->set_margin_bottom(16);

    auto* label = Gtk::make_managed<Gtk::Label>(
        "Dirección IPv4 del equipo Windows:"
    );
    label->set_halign(Gtk::Align::START);

    auto* address_entry = Gtk::make_managed<Gtk::Entry>();
    address_entry->set_placeholder_text("Ejemplo: 192.168.1.50");
    address_entry->set_text(windows_ip);
    address_entry->set_activates_default(true);

    content->append(*label);
    content->append(*address_entry);

    dialog->signal_response().connect(
        [this, dialog, address_entry](int response) {
            if (response != static_cast<int>(Gtk::ResponseType::OK)) {
                dialog->close();
                return;
            }

            const std::string address = trim(
                address_entry->get_text().raw()
            );

            if (!is_valid_ipv4(address)) {
                show_error(
                    "Dirección no válida",
                    "Introduce una dirección IPv4 válida, por ejemplo "
                    "192.168.1.50."
                );
                return;
            }

            const fs::path config_path = windows_config_file();
            std::error_code ec;

            fs::create_directories(config_path.parent_path(), ec);
            if (ec) {
                show_error(
                    "Error de configuración",
                    "No se pudo crear el directorio de configuración: " +
                    ec.message()
                );
                return;
            }

            std::vector<std::string> lines;
            {
                std::ifstream input(config_path);
                if (input) {
                    std::string line;
                    while (std::getline(input, line)) {
                        const std::string clean = trim(line);
                        if (clean.rfind("windows_ip=", 0) == 0) {
                            continue;
                        }
                        lines.push_back(line);
                    }
                    if (input.bad()) {
                        show_error(
                            "Error de configuración",
                            "No se pudo leer el archivo: " +
                            config_path.string()
                        );
                        return;
                    }
                } else if (fs::exists(config_path, ec) && !ec) {
                    show_error(
                        "Error de configuración",
                        "No se pudo abrir el archivo: " +
                        config_path.string()
                    );
                    return;
                }
            }

            const fs::path temp_path =
                config_path.string() + ".tmp";

            {
                std::ofstream updated(temp_path, std::ios::trunc);
                if (!updated) {
                    show_error(
                        "Error de configuración",
                        "No se pudo crear el archivo temporal: " +
                        temp_path.string()
                    );
                    return;
                }

                for (const auto& line : lines) {
                    updated << line << '\n';
                }
                updated << "windows_ip=" << address << '\n';
                updated.close();

                if (!updated) {
                    fs::remove(temp_path, ec);
                    show_error(
                        "Error de configuración",
                        "No se pudo escribir el archivo temporal."
                    );
                    return;
                }
            }

            fs::rename(temp_path, config_path, ec);
            if (ec) {
                const std::string error = ec.message();
                std::error_code ignored;
                fs::remove(temp_path, ignored);
                show_error(
                    "Error de configuración",
                    "No se pudo guardar el archivo de configuración: " +
                    error
                );
                return;
            }

            windows_ip = address;
            append_section("CONFIGURACIÓN WINDOWS");
            append_log("Destino Windows: " + windows_ip);
            append_log("Configuración guardada en: " +
                       config_path.string());

            dialog->close();
        }
    );

    dialog->present();
}


void SambaControlCenterPanel::configure_button(
    Gtk::Button& button,
    const std::string& text,
    const std::string& tooltip)
{
    button.set_label(text);

    button.set_tooltip_text(
        tooltip
    );

    button.set_hexpand(true);
}


/*
 * ================================================================
 * STATUS
 * ================================================================
 */

void SambaControlCenterPanel::append_log(
    const std::string& text)
{
    if (!log_buffer)
        return;

    log_buffer->insert(
        log_buffer->end(),
        text + "\n"
    );

    auto end_iter = log_buffer->end();

    log_view.scroll_to(
        end_iter,
        0.0,
        0.0,
        1.0
    );
}


void SambaControlCenterPanel::append_section(
    const std::string& title)
{
    append_log("");

    append_log(
        "============================================================"
    );

    append_log(
        " " + title
    );

    append_log(
        "============================================================"
    );
}


void SambaControlCenterPanel::set_status(
    Gtk::Label& label,
    const std::string& text,
    bool good)
{
    const std::string color =
        good ? "green" : "red";

    const std::string symbol =
        good ? "✓" : "✗";

    label.set_markup(
        "<span foreground='" +
        color +
        "' weight='bold'>" +
        symbol +
        " " +
        text +
        "</span>"
    );
}


void SambaControlCenterPanel::set_status_unknown(
    Gtk::Label& label,
    const std::string& text)
{
    label.set_markup(
        "<span foreground='orange' "
        "weight='bold'>⚠ " +
        text +
        "</span>"
    );
}


/*
 * ================================================================
 * ADMINISTRACIÓN
 * ================================================================
 */

std::string SambaControlCenterPanel::pkexec_command(
    const std::string& command)
{
    return
        "pkexec /bin/sh -c " +
        shell_escape(command);
}


bool SambaControlCenterPanel::run_admin_command(
    const std::string& command,
    std::string& output)
{
    int rc = -1;

    output =
        execute_command(
            pkexec_command(command),
            &rc
        );

    return rc == 0;
}


/*
 * ================================================================
 * DEPENDENCIAS
 * ================================================================
 */

bool SambaControlCenterPanel::check_package(
    const std::string& command,
    const std::string& package)
{
    if (command_exists(command))
    {
        append_log(
            "[OK] " +
            command +
            " encontrado."
        );

        return true;
    }

    append_log(
        "[FALTA] " +
        command +
        " no está instalado (paquete: " +
        package +
        ")."
    );

    return false;
}


void SambaControlCenterPanel::check_dependencies()
{
    append_section(
        "VERIFICACIÓN DE DEPENDENCIAS"
    );

    const bool samba =
        check_package(
            "smbd",
            "samba"
        );

    const bool smbclient =
        check_package(
            "smbclient",
            "smbclient"
        );

    const bool smbpasswd =
        check_package(
            "smbpasswd",
            "samba-common-bin"
        );

    const bool testparm =
        check_package(
            "testparm",
            "samba-common-bin"
        );

    const bool tcpdump =
        check_package(
            "tcpdump",
            "tcpdump"
        );

    const bool ufw =
        check_package(
            "ufw",
            "ufw"
        );

    const bool ss =
        check_package(
            "ss",
            "iproute2"
        );

    const bool ping =
        check_package(
            "ping",
            "iputils-ping"
        );

    const bool nc =
        check_package(
            "nc",
            "netcat-openbsd"
        );

    set_status(
        samba_status,
        samba ? "INSTALADO" : "FALTA",
        samba
    );

    set_status(
        tcpdump_status,
        tcpdump ? "INSTALADO" : "FALTA",
        tcpdump
    );

    append_log("");

    append_log(
        "smbclient: " +
        std::string(
            smbclient ? "OK" : "FALTA"
        )
    );

    append_log(
        "smbpasswd: " +
        std::string(
            smbpasswd ? "OK" : "FALTA"
        )
    );

    append_log(
        "testparm: " +
        std::string(
            testparm ? "OK" : "FALTA"
        )
    );

    append_log(
        "ufw: " +
        std::string(
            ufw ? "OK" : "FALTA"
        )
    );

    append_log(
        "ss: " +
        std::string(
            ss ? "OK" : "FALTA"
        )
    );

    append_log(
        "ping: " +
        std::string(
            ping ? "OK" : "FALTA"
        )
    );

    append_log(
        "nc: " +
        std::string(
            nc ? "OK" : "FALTA"
        )
    );

    /*
     * Una sola instalación.
     */

    if (!samba ||
        !smbclient ||
        !smbpasswd ||
        !testparm ||
        !tcpdump ||
        !ufw ||
        !ss ||
        !ping ||
        !nc)
    {
        if (ask_install(
                "componentes Samba / red"))
        {
            install_dependencies();
        }
    }

    if (command_exists("smbd"))
    {
        append_log(
            "Samba: " +
            get_command_output(
                "smbd --version"
            )
        );
    }

    if (command_exists("tcpdump"))
    {
        append_log(
            "tcpdump: " +
            get_command_output(
                "tcpdump --version | head -n 1"
            )
        );
    }
}


void SambaControlCenterPanel::install_dependencies()
{
    append_section(
        "INSTALACIÓN DE DEPENDENCIAS"
    );

    std::string output;

    const std::string command =
        "apt-get update && "
        "DEBIAN_FRONTEND=noninteractive "
        "apt-get install -y "
        "samba "
        "samba-common-bin "
        "smbclient "
        "tcpdump "
        "ufw "
        "iproute2 "
        "iputils-ping "
        "procps "
        "netcat-openbsd "
        "coreutils";

    const bool ok =
        run_admin_command(
            command,
            output
        );

    append_log(output);

    if (ok)
    {
        append_log(
            "[OK] Dependencias instaladas."
        );

        show_info(
            "Dependencias",
            "Las dependencias fueron instaladas correctamente."
        );
    }
    else
    {
        show_error(
            "Dependencias",
            "No fue posible instalar las dependencias."
        );
    }
}


/*
 * ================================================================
 * SAMBA
 * ================================================================
 */

bool SambaControlCenterPanel::check_samba()
{
    append_section("SAMBA");

    if (!command_exists("smbd"))
    {
        set_status(
            samba_status,
            "NO INSTALADO",
            false
        );

        return false;
    }

    const std::string version =
        get_command_output(
            "smbd --version"
        );

    append_log(
        "Versión Samba: " +
        version
    );

    append_log(
        "Ruta smbd: " +
        get_command_output(
            "command -v smbd"
        )
    );

    append_log(
        "Ruta smbclient: " +
        get_command_output(
            "command -v smbclient"
        )
    );

    append_log(
        "Ruta smbpasswd: " +
        get_command_output(
            "command -v smbpasswd"
        )
    );

    append_log(
        "Ruta testparm: " +
        get_command_output(
            "command -v testparm"
        )
    );

    set_status(
        samba_status,
        version.empty()
            ? "INSTALADO"
            : version,
        true
    );

    return true;
}


bool SambaControlCenterPanel::check_smbclient()
{
    return command_exists(
        "smbclient"
    );
}


bool SambaControlCenterPanel::check_smbpasswd()
{
    return command_exists(
        "smbpasswd"
    );
}


bool SambaControlCenterPanel::check_testparm()
{
    return command_exists(
        "testparm"
    );
}


/*
 * ================================================================
 * SMB.CONF
 * ================================================================
 */


std::string SambaControlCenterPanel::prepare_samba_config_file()
{
    /*
     * Generamos dinámicamente usuario y ruta.
     * Sin privilegios: solo escribe el temporal.
     * Devuelve la ruta del temporal o "" si falla.
     */

    std::ostringstream cfg;

    cfg
        << "[global]\n"
        << "    workgroup = WORKGROUP\n"
        << "    server string = Samba Server MX Linux\n"
        << "    server role = standalone server\n"
        << "    security = user\n"
        << "    map to guest = Bad User\n"
        << "\n"
        << "    log file = /var/log/samba/log.%m\n"
        << "    max log size = 1000\n"
        << "\n"
        << "    obey pam restrictions = yes\n"
        << "    unix password sync = yes\n"
        << "    pam password change = yes\n"
        << "\n"
        << "    passwd program = /usr/bin/passwd %u\n"
        << "    passwd chat = *Enter\\snew\\s*\\spassword:* %n\\n "
           "*Retype\\snew\\s*\\spassword:* %n\\n "
           "*password\\supdated\\ssuccessfully* .\n"
        << "\n"
        << "    usershare allow guests = yes\n"
        << "\n"
        << "[homes]\n"
        << "    comment = Home Directories\n"
        << "    browseable = no\n"
        << "    read only = no\n"
        << "    valid users = %S\n"
        << "    create mask = 0700\n"
        << "    directory mask = 0700\n"
        << "\n"
        << "[printers]\n"
        << "    comment = All Printers\n"
        << "    browseable = no\n"
        << "    printable = yes\n"
        << "    path = /var/tmp\n"
        << "    create mask = 0700\n"
        << "\n"
        << "[print$]\n"
        << "    comment = Printer Drivers\n"
        << "    path = /var/lib/samba/printers\n"
        << "\n"
        << "[compartida]\n"
        << "    comment = Carpeta compartida de "
        << samba_user << "\n"
        << "    path = "
        << shared_folder << "\n"
        << "    browseable = yes\n"
        << "    read only = no\n"
        << "    writable = yes\n"
        << "    valid users = "
        << samba_user << "\n"
        << "    force user = "
        << samba_user << "\n"
        << "    create mask = 0664\n"
        << "    directory mask = 0775\n";

    const std::string config =
        cfg.str();

    const std::string temp =
        "/tmp/samba_gui_smb.conf." +
        std::to_string(
            static_cast<long long>(getpid())
        );

    {
        std::ofstream file(
            temp
        );

        if (!file)
        {
            show_error(
                "Samba",
                "No fue posible crear el archivo temporal."
            );

            return "";
        }

        file << config;
    }

    return temp;
}


bool SambaControlCenterPanel::validate_samba_config()
{
    append_section(
        "TESTPARM"
    );

    if (!command_exists(
            "testparm"))
    {
        append_log(
            "[ERROR] testparm no está instalado."
        );

        return false;
    }

    int rc = -1;

    const std::string output =
        execute_command(
            "testparm -s",
            &rc
        );

    append_log(output);

    if (rc == 0)
    {
        append_log(
            "[OK] Configuración Samba válida."
        );

        return true;
    }

    append_log(
        "[ERROR] testparm encontró problemas."
    );

    return false;
}


void SambaControlCenterPanel::configure_samba()
{
    append_section(
        "CONFIGURAR SAMBA"
    );

    if (!command_exists("testparm"))
    {
        append_log(
            "[ERROR] testparm no está instalado."
        );

        show_error(
            "Samba",
            "La configuración no pasó testparm."
        );

        return;
    }

    /*
     * Preparación sin privilegios: generar el temporal.
     */

    const std::string temp =
        prepare_samba_config_file();

    if (temp.empty())
        return;

    const bool has_nmbd =
        command_exists("nmbd");

    const bool has_ufw =
        command_exists("ufw");

    const bool has_pdbedit =
        command_exists("pdbedit");

    /*
     * Todos los pasos con privilegios viajan en UN solo
     * script: una única autorización para toda la acción.
     */

    std::string script =
        "F=0\n";

    script += batch_step(
        "folder",
        "mkdir -p " +
            shell_escape(shared_folder) +
            " && chown -R " +
            shell_escape(
                samba_user + ":" + samba_user
            ) +
            " " +
            shell_escape(shared_folder) +
            " && chmod 0775 " +
            shell_escape(shared_folder)
    );

    // Validar el temporal ANTES de tocar el smb.conf actual.
    script += batch_step(
        "testparm",
        "testparm -s " + shell_escape(temp)
    );

    // Crear un respaldo único sin sobrescribir los anteriores.
    script += batch_step(
        "backup",
        "if [ -f " + shell_escape(samba_config) + " ]; then "
        "backup_path=" + shell_escape(samba_config + ".backup-") +
            "$(date +%Y%m%d-%H%M%S)-$$; "
        "cp -a -- " + shell_escape(samba_config) +
            " \"$backup_path\" && "
            "printf '[BACKUP] %s\\n' \"$backup_path\"; "
        "else "
            "printf '%s\\n' '[INFO] No existe smb.conf anterior.'; "
        "fi"
    );

    // Instalar solamente si testparm y el respaldo tuvieron éxito.
    script += batch_step(
        "install",
        "install -o root -g root -m 0644 " +
            shell_escape(temp) +
            " " +
            shell_escape(samba_config) +
            " && rm -f " +
            shell_escape(temp)
    );

    // Solo lectura (la lista de usuarios Samba): paso "siempre",
    // no detiene ni lo detiene la cadena de modificaciones.
    if (has_pdbedit)
    {
        script += batch_step_always(
            "users",
            "pdbedit -L"
        );
    }

    script += batch_step(
        "smbd",
        "service smbd restart"
    );

    if (has_nmbd)
    {
        script += batch_step(
            "nmbd",
            "service nmbd restart"
        );
    }

    if (has_ufw)
    {
        script += batch_step(
            "ufw",
            "ufw allow samba"
        );
    }

    std::string output;

    run_admin_command(
        script,
        output
    );

    std::size_t cursor = 0;

    int rc = -1;

    std::string segment;

    /*
     * CARPETA
     */

    append_section(
        "CREACIÓN DE CARPETA RED"
    );

    segment =
        batch_take_segment(
            output,
            cursor,
            "folder",
            rc
        );

    if (!segment.empty())
        append_log(segment);

    if (rc == -1)
    {
        std::remove(temp.c_str());

        show_error(
            "Samba",
            "Autorización denegada o cancelada."
        );

        return;
    }

    if (rc != 0)
    {
        set_status(
            folder_status,
            "ERROR",
            false
        );

        std::remove(temp.c_str());

        show_error(
            "Samba",
            "No se pudo crear o preparar la carpeta compartida."
        );

        return;
    }

    append_log(
        "[OK] Carpeta creada/verificada."
    );

    append_log(
        "Ruta: " +
        shared_folder
    );

    set_status(
        folder_status,
        "OK - 775",
        true
    );

    /*
     * TESTPARM: validar el archivo temporal antes de instalarlo.
     */

    append_section(
        "TESTPARM"
    );

    segment =
        batch_take_segment(
            output,
            cursor,
            "testparm",
            rc
        );

    if (!segment.empty())
        append_log(segment);

    if (rc != 0)
    {
        append_log(
            "[ERROR] testparm rechazó la configuración temporal."
        );

        std::remove(temp.c_str());

        show_error(
            "Samba",
            "La configuración no pasó testparm."
        );

        return;
    }

    append_log(
        "[OK] Configuración temporal válida."
    );

    /*
     * SMB.CONF: respaldo e instalación
     */

    append_section(
        "CONFIGURACIÓN SMB.CONF"
    );

    segment =
        batch_take_segment(
            output,
            cursor,
            "backup",
            rc
        );

    if (!segment.empty())
        append_log(segment);

    if (rc != 0)
    {
        append_log("[ERROR] No se pudo respaldar smb.conf.");

        std::remove(temp.c_str());

        show_error(
            "Samba",
            "No se pudo respaldar smb.conf. Revisa el registro."
        );

        return;
    }

    segment =
        batch_take_segment(
            output,
            cursor,
            "install",
            rc
        );

    if (!segment.empty())
        append_log(segment);

    if (rc != 0)
    {
        std::remove(temp.c_str());

        show_error(
            "Samba",
            "No se pudo instalar smb.conf. Revisa el registro."
        );

        return;
    }

    append_log(
        "[OK] smb.conf actualizado."
    );

    append_log(
        "Share: [compartida]"
    );

    append_log(
        "Path: " +
        shared_folder
    );

    append_log(
        "User: " +
        samba_user
    );

    /*
     * USUARIO SAMBA: consume el segmento "users" del lote
     * (pdbedit -L ya corrió dentro de la única autorización).
     * -3 = pdbedit no instalado (no hubo paso en el lote).
     */

    std::string users_segment;
    int users_rc = -3;

    if (has_pdbedit)
    {
        users_segment =
            batch_take_segment(
                output,
                cursor,
                "users",
                users_rc
            );
    }

    create_samba_user(
        users_rc,
        users_segment
    );

    /*
     * SERVICIOS
     */

    append_section(
        "REINICIAR SAMBA"
    );

    segment =
        batch_take_segment(
            output,
            cursor,
            "smbd",
            rc
        );

    append_log(
        "Reiniciando smbd..."
    );

    if (!segment.empty())
        append_log(segment);

    const bool smbd_ok =
        rc == 0;

    set_status(
        smbd_status,
        smbd_ok ? "RESTART OK" : "ERROR",
        smbd_ok
    );

    if (!smbd_ok)
    {
        append_log("[ERROR] No se pudo reiniciar smbd.");

        show_error(
            "Samba",
            "Falló el reinicio de uno o más servicios."
        );

        return;
    }

    if (has_nmbd)
    {
        segment =
            batch_take_segment(
                output,
                cursor,
                "nmbd",
                rc
            );

        append_log(
            "Reiniciando nmbd..."
        );

        if (!segment.empty())
            append_log(segment);

        const bool nmbd_ok =
            rc == 0;

        set_status(
            nmbd_status,
            nmbd_ok ? "RESTART OK" : "ERROR",
            nmbd_ok
        );

        if (!nmbd_ok)
        {
            append_log("[ERROR] No se pudo reiniciar nmbd.");

            show_error(
                "Samba",
                "Falló el reinicio de uno o más servicios."
            );

            return;
        }
    }
    else
    {
        append_log(
            "[INFO] nmbd no está disponible."
        );
    }

    /*
     * FIREWALL
     */

    append_section(
        "FIREWALL UFW"
    );

    if (!has_ufw)
    {
        append_log(
            "UFW no está instalado."
        );
    }
    else
    {
        segment =
            batch_take_segment(
                output,
                cursor,
                "ufw",
                rc
            );

        if (!segment.empty())
            append_log(segment);

        if (rc != 0)
        {
            append_log("[ERROR] No se pudo aplicar la regla de UFW.");

            set_status(
                firewall_status,
                "ERROR",
                false
            );

            show_error(
                "Samba",
                "No se pudo configurar UFW. Revisa el registro."
            );

            return;
        }

        append_log(
            "[OK] Perfil Samba permitido en UFW."
        );

        set_status(
            firewall_status,
            "REGLA APLICADA",
            true
        );
    }

    check_ports();

    append_log(
        "Configuración de Samba finalizada."
    );

    show_info(
        "Samba",
        "Configuración completada."
    );
}


/*
 * ================================================================
 * CARPETA
 * ================================================================
 */

bool SambaControlCenterPanel::folder_exists()
{
    append_section(
        "CARPETA COMPARTIDA"
    );

    std::error_code ec;

    const bool exists =
        fs::exists(
            shared_folder,
            ec
        );

    const bool directory =
        exists &&
        fs::is_directory(
            shared_folder,
            ec
        );

    if (directory)
    {
        append_log(
            "[OK] Existe: " +
            shared_folder
        );

        set_status(
            folder_status,
            "EXISTE",
            true
        );

        return true;
    }

    append_log(
        "[FALTA] No existe: " +
        shared_folder
    );

    set_status(
        folder_status,
        "NO EXISTE",
        false
    );

    return false;
}


bool SambaControlCenterPanel::create_shared_folder()
{
    append_section(
        "CREACIÓN DE CARPETA RED"
    );

    std::string output;

    const std::string command =
        "mkdir -p " +
        shell_escape(shared_folder) +
        " && "
        "chown -R " +
        shell_escape(
            samba_user + ":" + samba_user
        ) +
        " " +
        shell_escape(shared_folder) +
        " && "
        "chmod 0775 " +
        shell_escape(shared_folder);

    if (run_admin_command(
            command,
            output))
    {
        append_log(
            "[OK] Carpeta creada/verificada."
        );

        append_log(
            "Ruta: " +
            shared_folder
        );

        set_status(
            folder_status,
            "OK - 775",
            true
        );
        return true;
    }
    else
    {
        append_log(output);

        set_status(
            folder_status,
            "ERROR",
            false
        );
        return false;
    }
}


void SambaControlCenterPanel::fix_folder_permissions()
{
    create_shared_folder();
}


/*
 * ================================================================
 * USUARIO SAMBA
 * ================================================================
 */

void SambaControlCenterPanel::create_samba_user(
    int pdbedit_rc,
    const std::string& pdbedit_output)
{
    append_section(
        "USUARIO SAMBA"
    );

    /*
     * El resultado de "pdbedit -L" llega del paso "users" del
     * lote (una sola autorización). pdbedit_rc:
     *
     * -3    pdbedit no está instalado.
     *  0    la consulta se hizo: pdbedit_output es la lista.
     * otro  la consulta no se pudo hacer (fallo o cancelación):
     *       NO se afirma que la cuenta no exista.
     */

    if (pdbedit_rc == -3)
    {
        append_log(
            "[ERROR] No se encontró el comando pdbedit."
        );

        return;
    }

    if (pdbedit_rc != 0)
    {
        append_log(
            "[INFO] No se pudo verificar el usuario Samba."
        );

        return;
    }

    if (pdbedit_list_has_user(
            pdbedit_output,
            samba_user))
    {
        append_log(
            "[OK] Usuario Samba '" +
            samba_user +
            "' ya existe."
        );

        return;
    }

    append_log(
        "El usuario Linux '" +
        samba_user +
        "' existe."
    );

    append_log(
        "El usuario Samba todavía no tiene una cuenta."
    );

    append_log(
        "Utilice CONTRASEÑA SAMBA para crearla."
    );
}


/*
 * ================================================================
 * CONTRASEÑA SAMBA
 * ================================================================
 */

void SambaControlCenterPanel::show_password_dialog()
{
    auto dialog =
        Gtk::make_managed<Gtk::Dialog>();

    dialog->set_transient_for(*parent_window);

    dialog->set_modal(true);

    dialog->set_title(
        "Contraseña Samba"
    );

    dialog->add_button(
        "Cancelar",
        Gtk::ResponseType::CANCEL
    );

    dialog->add_button(
        "Guardar",
        Gtk::ResponseType::OK
    );

    auto* content =
        dialog->get_content_area();

    content->set_spacing(10);
    content->set_margin(15);

    auto* label =
        Gtk::make_managed<Gtk::Label>(
            "Usuario Samba: " +
            samba_user
        );

    content->append(
        *label
    );

    auto* password1 =
        Gtk::make_managed<Gtk::PasswordEntry>();

    password1->property_placeholder_text() =
        "Nueva contraseña";

    content->append(
        *password1
    );

    auto* password2 =
        Gtk::make_managed<Gtk::PasswordEntry>();

    password2->property_placeholder_text() =
        "Repetir contraseña";

    content->append(
        *password2
    );

    dialog->signal_response().connect(
        [this,
         dialog,
         password1,
         password2]
        (int response)
        {
            if (
                response !=
                Gtk::ResponseType::OK
            )
            {
                dialog->close();
                return;
            }

            const std::string p1 =
                password1->get_text();

            const std::string p2 =
                password2->get_text();

            if (p1.empty())
            {
                show_error(
                    "Contraseña",
                    "La contraseña no puede estar vacía."
                );

                return;
            }

            if (p1 != p2)
            {
                show_error(
                    "Contraseña",
                    "Las contraseñas no coinciden."
                );

                return;
            }

            set_samba_password(
                p1
            );

            dialog->close();
        }
    );

    dialog->present();
}


void SambaControlCenterPanel::set_samba_password(
    const std::string& password)
{
    append_section(
        "CONFIGURANDO CONTRASEÑA SAMBA"
    );

    /*
     * No almacenamos la contraseña directamente
     * en un archivo permanente.
     *
     * Se utiliza stdin de smbpasswd.
     */

    const std::string escaped_password =
        shell_escape(password);

    /*
     * printf recibe la contraseña ya escapada.
     */

    const std::string command =
        "printf '%s\\n%s\\n' " +
        escaped_password +
        " " +
        escaped_password +
        " | smbpasswd -s -a " +
        shell_escape(samba_user);

    std::string output;

    const bool ok =
        run_admin_command(
            command,
            output
        );

    append_log(output);

    if (ok)
    {
        append_log(
            "[OK] Contraseña Samba configurada."
        );

        show_info(
            "Samba",
            "Contraseña configurada correctamente."
        );
    }
    else
    {
        show_error(
            "Samba",
            "No fue posible configurar la contraseña."
        );
    }
}


/*
 * ================================================================
 * SERVICIOS SYSVINIT
 * ================================================================
 */


bool SambaControlCenterPanel::restart_services()
{
    append_section(
        "REINICIAR SAMBA"
    );

    if (!command_exists("smbd"))
    {
        append_log(
            "[ERROR] smbd no está instalado."
        );

        return false;
    }

    const bool has_nmbd =
        command_exists("nmbd");

    /*
     * Un solo pkexec para los dos servicios:
     * una única autorización por acción.
     */

    std::string script =
        "F=0\n";

    script += batch_step(
        "smbd",
        "service smbd restart"
    );

    if (has_nmbd)
    {
        script += batch_step(
            "nmbd",
            "service nmbd restart"
        );
    }

    std::string output;

    run_admin_command(
        script,
        output
    );

    std::size_t cursor = 0;

    int rc_smbd = -1;

    const std::string out_smbd =
        batch_take_segment(
            output,
            cursor,
            "smbd",
            rc_smbd
        );

    append_log(
        "Reiniciando smbd..."
    );

    if (!out_smbd.empty())
        append_log(out_smbd);

    const bool smbd_ok =
        rc_smbd == 0;

    set_status(
        smbd_status,
        smbd_ok ? "RESTART OK" : "ERROR",
        smbd_ok
    );

    if (!smbd_ok)
        append_log("[ERROR] No se pudo reiniciar smbd.");

    bool nmbd_ok = true;

    if (has_nmbd)
    {
        int rc_nmbd = -1;

        const std::string out_nmbd =
            batch_take_segment(
                output,
                cursor,
                "nmbd",
                rc_nmbd
            );

        append_log(
            "Reiniciando nmbd..."
        );

        if (!out_nmbd.empty())
            append_log(out_nmbd);

        nmbd_ok =
            rc_nmbd == 0;

        set_status(
            nmbd_status,
            nmbd_ok ? "RESTART OK" : "ERROR",
            nmbd_ok
        );

        if (!nmbd_ok)
            append_log("[ERROR] No se pudo reiniciar nmbd.");
    }
    else
    {
        append_log(
            "[INFO] nmbd no está disponible."
        );
    }

    if (smbd_ok && nmbd_ok)
        check_services();

    return smbd_ok && nmbd_ok;
}


void SambaControlCenterPanel::check_services()
{
    append_section(
        "ESTADO DE SERVICIOS"
    );

    /*
     * smbd
     */

    int rc1 = -1;

    const std::string smbd =
        execute_command(
            service_command("smbd", "status"),
            &rc1
        );

    append_log(
        "---- smbd ----"
    );

    append_log(smbd);

    const std::string smbd_process =
        get_command_output(
            "pgrep -x smbd"
        );

    const bool smbd_ok =
        !smbd_process.empty();

    set_status(
        smbd_status,
        smbd_ok
            ? "RUNNING"
            : "STOPPED",
        smbd_ok
    );

    /*
     * nmbd
     */

    if (command_exists("nmbd"))
    {
        int rc2 = -1;

        const std::string nmbd =
            execute_command(
                service_command("nmbd", "status"),
                &rc2
            );

        append_log(
            "---- nmbd ----"
        );

        append_log(nmbd);

        const std::string nmbd_process =
            get_command_output(
                "pgrep -x nmbd"
            );

        const bool nmbd_ok =
            !nmbd_process.empty();

        set_status(
            nmbd_status,
            nmbd_ok
                ? "RUNNING"
                : "STOPPED",
            nmbd_ok
        );
    }
    else
    {
        append_log(
            "[INFO] nmbd no está instalado."
        );

        set_status_unknown(
            nmbd_status,
            "NO DISPONIBLE"
        );
    }
}


/*
 * ================================================================
 * UFW
 * ================================================================
 */

bool SambaControlCenterPanel::configure_firewall()
{
    append_section(
        "FIREWALL UFW"
    );

    if (!command_exists("ufw"))
    {
        append_log(
            "UFW no está instalado."
        );

        return true;
    }

    std::string output;

    const bool ok =
        run_admin_command(
            "ufw allow samba",
            output
        );

    append_log(output);

    if (!ok)
    {
        append_log("[ERROR] No se pudo aplicar la regla de UFW.");
        set_status(firewall_status, "ERROR", false);
        return false;
    }
    append_log("[OK] Perfil Samba permitido en UFW.");
    set_status(firewall_status, "REGLA APLICADA", true);
    return true;
}


/*
 * Aplica el resultado de "ufw status": imprime la salida, detecta
 * si Samba está permitido y actualiza el estado del panel.
 * La salida llega del segmento "ufw" del lote del diagnóstico
 * (una sola autorización); era la segunda mitad de
 * check_firewall().
 */
void SambaControlCenterPanel::apply_firewall_output(
    bool ok,
    const std::string& output)
{
    append_log(output);

    if (!ok)
    {
        set_status_unknown(
            firewall_status,
            "NO DISPONIBLE"
        );

        return;
    }

    /*
     * Detectamos:
     *
     * Samba
     * 139/tcp
     * 445/tcp
     */

    const bool samba =
        output.find("Samba") !=
        std::string::npos;

    const bool port139 =
        output.find("139") !=
        std::string::npos;

    const bool port445 =
        output.find("445") !=
        std::string::npos;

    const bool allowed =
        samba ||
        (port139 && port445);

    set_status(
        firewall_status,
        allowed
            ? "SAMBA PERMITIDO"
            : "SAMBA NO PERMITIDO",
        allowed
    );
}


/*
 * ================================================================
 * PUERTOS
 * ================================================================
 */

void SambaControlCenterPanel::check_ports()
{
    append_section(
        "PUERTOS SAMBA"
    );

    if (!command_exists("ss"))
    {
        append_log(
            "ss no está instalado."
        );

        set_status_unknown(
            ports_status,
            "ss NO DISPONIBLE"
        );

        return;
    }

    const std::string output =
        get_command_output(
            "ss -lntup"
        );

    std::istringstream stream(
        output
    );

    std::string line;

    std::string samba_lines;

    while (
        std::getline(
            stream,
            line
        )
    )
    {
        if (
            line.find(":139") !=
            std::string::npos ||
            line.find(":445") !=
            std::string::npos
        )
        {
            samba_lines +=
                line + "\n";
        }
    }

    if (samba_lines.empty())
    {
        append_log(
            "No se detectaron puertos Samba."
        );

        set_status(
            ports_status,
            "NO LISTEN",
            false
        );

        return;
    }

    append_log(
        samba_lines
    );

    const bool port139 =
        samba_lines.find(":139") !=
        std::string::npos;

    const bool port445 =
        samba_lines.find(":445") !=
        std::string::npos;

    if (port139 && port445)
    {
        set_status(
            ports_status,
            "139 + 445 LISTEN",
            true
        );
    }
    else if (port445)
    {
        set_status(
            ports_status,
            "445 LISTEN",
            true
        );
    }
    else if (port139)
    {
        set_status(
            ports_status,
            "139 LISTEN",
            true
        );
    }
    else
    {
        set_status(
            ports_status,
            "NO LISTEN",
            false
        );
    }
}


/*
 * ================================================================
 * RED
 * ================================================================
 */

/*
 * ================================================================
 * INFORMACIÓN DE INTERFACES
 * ================================================================
 */


/*
 * Descubre equipos en las subredes IPv4 locales.
 * Solo utiliza Nmap en modo de descubrimiento (-sn).
 */
void SambaControlCenterPanel::scan_network()
{
    append_section("ESCANEAR RED");

    if (!command_exists("nmap"))
    {
        append_log("[ERROR] Nmap no está instalado.");
        append_log("Instálalo con: sudo apt install nmap");
        return;
    }

    int ip_rc = -1;
    const std::string addr_output =
        execute_command(
            "ip -o -4 addr show up scope global",
            &ip_rc
        );

    if (ip_rc != 0)
    {
        append_log("[ERROR] No se pudieron consultar las interfaces IPv4.");
        append_log(addr_output);
        return;
    }

    std::vector<std::string> subnets;
    std::istringstream lines(addr_output);
    std::string line;

    while (std::getline(lines, line))
    {
        std::istringstream fields(line);
        std::string index;
        std::string interface_name;
        std::string family;
        std::string cidr;

        if (!(fields >> index >> interface_name >> family >> cidr))
            continue;

        if (family != "inet")
            continue;

        const std::size_t slash = cidr.find('/');
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

        // Evitar redes excesivamente grandes: /20 o más estrechas.
        if (prefix < 20)
        {
            append_log(
                "[OMITIDA] " + interface_name + " " + cidr +
                ": red demasiado grande para este escaneo."
            );
            continue;
        }

        const std::uint32_t mask =
            prefix == 0
                ? 0u
                : (0xFFFFFFFFu << (32 - prefix));

        const std::uint32_t network_host =
            ntohl(parsed.s_addr) & mask;

        in_addr network_address{};
        network_address.s_addr = htonl(network_host);

        char network_text[INET_ADDRSTRLEN]{};
        if (!inet_ntop(
                AF_INET,
                &network_address,
                network_text,
                sizeof(network_text)))
            continue;

        const std::string subnet =
            std::string(network_text) + "/" +
            std::to_string(prefix);

        if (std::find(
                subnets.begin(),
                subnets.end(),
                subnet) == subnets.end())
        {
            subnets.push_back(subnet);
        }
    }

    if (subnets.empty())
    {
        append_log(
            "[INFO] No se encontraron subredes IPv4 aptas para escanear."
        );
        append_log(
            "Comprueba que Ethernet o Wi-Fi estén conectados."
        );
        return;
    }

    append_log("Subredes detectadas:");
    for (const auto& subnet : subnets)
        append_log("  " + subnet);

    append_log("");
    append_log("Solicitando autorización para descubrir equipos...");
    append_log("Se utilizará Nmap -sn; no se escanearán puertos.");

    // Un único pkexec para todas las subredes detectadas.
    std::string script = "set +e\n";
    for (const auto& subnet : subnets)
    {
        script +=
            "echo '\\n===== SUBRED " + subnet + " ====='\n";
        script += "nmap -sn " + shell_escape(subnet) + "\n";
    }

    std::string output;
    const bool ok = run_admin_command(script, output);


    struct NetworkHost
    {
        std::string ip;
        std::string hostname = "No identificado";
        std::string mac = "No identificada";
        std::string manufacturer = "No identificado";
        std::string device = "No determinado";
    };

    std::vector<NetworkHost> hosts;
    NetworkHost current;
    bool have_host = false;

    std::istringstream nmap_lines(output);
    std::string nmap_line;

    const std::string report_prefix = "Nmap scan report for ";
    const std::string mac_prefix = "MAC Address: ";

    while (std::getline(nmap_lines, nmap_line))
    {
        if (nmap_line.compare(
                0, report_prefix.size(), report_prefix) == 0)
        {
            if (have_host)
                hosts.push_back(current);

            current = NetworkHost{};
            have_host = true;

            std::string report =
                nmap_line.substr(report_prefix.size());

            const std::size_t open_paren = report.rfind(" (");
            const std::size_t close_paren = report.rfind(')');

            if (open_paren != std::string::npos &&
                close_paren == report.size() - 1 &&
                open_paren < close_paren)
            {
                current.hostname = report.substr(0, open_paren);
                current.ip = report.substr(
                    open_paren + 2,
                    close_paren - open_paren - 2);
            }
            else
            {
                current.ip = report;
            }
        }
        else if (have_host &&
                nmap_line.compare(
                    0, mac_prefix.size(), mac_prefix) == 0)
        {
            std::string details =
                nmap_line.substr(mac_prefix.size());

            const std::size_t space = details.find(' ');
            if (space != std::string::npos)
            {
                current.mac = details.substr(0, space);

                const std::size_t open_paren = details.find('(');
                const std::size_t close_paren = details.rfind(')');

                if (open_paren != std::string::npos &&
                    close_paren != std::string::npos &&
                    close_paren > open_paren)
                {
                    current.manufacturer = details.substr(
                        open_paren + 1,
                        close_paren - open_paren - 1);
                }
            }
            else if (!details.empty())
            {
                current.mac = details;
            }
        }
    }

    if (have_host)
        hosts.push_back(current);

    // Estimación prudente basada en nombres y fabricantes conocidos.
    for (auto& host : hosts)
    {
        const std::string identity =
            host.hostname + " " + host.manufacturer;

        if (identity.find("iPhone") != std::string::npos ||
            identity.find("iphone") != std::string::npos)
            host.device = "Posible iPhone";
        else if (identity.find("iPad") != std::string::npos ||
                identity.find("ipad") != std::string::npos)
            host.device = "Posible iPad";
        else if (identity.find("MSI") != std::string::npos ||
                identity.find("Micro-Star") != std::string::npos)
            host.device = "Posible PC MSI";
        else if (identity.find("Amazon") != std::string::npos ||
                identity.find("amazon") != std::string::npos)
            host.device = "Posible dispositivo Amazon";
        else if (identity.find("Arris") != std::string::npos ||
                identity.find("arris") != std::string::npos)
            host.device = "Posible router/módem";
        else if (identity.find("HP") != std::string::npos)
            host.device = "Posible equipo HP";
    }

    auto fit_column = [](const std::string& value, std::size_t width)
    {
        std::string result = value;

        if (result.size() > width)
        {
            if (width > 1)
                result = result.substr(0, width - 1) + "~";
            else
                result = result.substr(0, width);
        }

        if (result.size() < width)
            result.append(width - result.size(), ' ');

        return result;
    };

    const std::size_t w_ip = 15;
    const std::size_t w_host = 22;
    const std::size_t w_mac = 17;
    const std::size_t w_vendor = 22;
    const std::size_t w_device = 25;

    auto table_row = [&](const std::string& ip,
                        const std::string& hostname,
                        const std::string& mac,
                        const std::string& manufacturer,
                        const std::string& device)
    {
        return fit_column(ip, w_ip) + "  " +
            fit_column(hostname, w_host) + "  " +
            fit_column(mac, w_mac) + "  " +
            fit_column(manufacturer, w_vendor) + "  " +
            fit_column(device, w_device);
    };

    append_log("");
    append_log(table_row(
        "IP", "HOSTNAME", "MAC", "FABRICANTE", "POSIBLE DISPOSITIVO"));

    append_log(std::string(
        w_ip + w_host + w_mac + w_vendor + w_device + 8, '-'));

    if (hosts.empty())
    {
        append_log("Nmap no devolvió equipos identificables.");
    }
    else
    {
        for (const auto& host : hosts)
        {
            append_log(table_row(
                host.ip,
                host.hostname,
                host.mac,
                host.manufacturer,
                host.device));
        }

        append_log("");
        append_log("Equipos encontrados: " +
                std::to_string(hosts.size()));
    }


    if (!ok)
    {
        append_log(
            "[ERROR] El escaneo falló, se canceló la autorización "
            "o Nmap devolvió un error."
        );
        return;
    }

    append_log("");
    append_log(
        "[FIN] Descubrimiento terminado. Revisa las IP y los "
        "nombres que Nmap haya podido identificar."
    );
}


void SambaControlCenterPanel::show_interfaces()
{
    append_section("INTERFACES DE RED");

    append_log("Interfaces IPv4:");
    append_log(
        get_command_output(
            "ip -4 -br addr"
        )
    );

    append_log("");

    append_log("Direcciones IPv4 completas:");
    append_log(
        get_command_output(
            "ip -4 addr show"
        )
    );

    append_log("");

    append_log("Estado de las interfaces:");
    append_log(
        get_command_output(
            "ip -br link"
        )
    );

    append_log("");

    append_log("Hostname -I:");
    append_log(
        get_command_output(
            "hostname -I"
        )
    );

    set_status(
        network_status,
        "INTERFACES OK",
        true
    );
}


/*
 * ================================================================
 * TABLA DE RUTAS
 * ================================================================
 */

void SambaControlCenterPanel::show_routes()
{
    append_section("TABLA DE RUTAS IPv4");

    append_log(
        get_command_output(
            "ip -4 route"
        )
    );

    append_section(
        "RUTA PREFERIDA HACIA INTERNET"
    );

    append_log(
        get_command_output(
            "ip route get 8.8.8.8"
        )
    );

    if (!windows_ip.empty())
    {
        append_section(
            "RUTA HACIA WINDOWS " + windows_ip
        );

        append_log(
            get_command_output(
                "ip route get " + shell_escape(windows_ip)
            )
        );
    }
    else
    {
        append_section("RUTA HACIA WINDOWS");
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
    }

    set_status(
        network_status,
        "RUTAS OK",
        true
    );
}


/*
 * ================================================================
 * GATEWAY
 * ================================================================
 */

void SambaControlCenterPanel::show_gateway()
{
    append_section("GATEWAY / ROUTER");

    std::string output =
        get_command_output(
            "ip route | grep '^default'"
        );

    append_log(output);

    append_section(
        "GATEWAY PRINCIPAL"
    );

    std::string gateway =
        get_command_output(
            "ip route | "
            "awk '$1 == \"default\" {print $3, $5}' | "
            "head -n 1"
        );

    if (gateway.empty())
    {
        append_log(
            "[ERROR] No se pudo detectar el gateway."
        );

        set_status(
            network_status,
            "GATEWAY ERROR",
            false
        );

        return;
    }

    append_log(
        "Gateway principal:"
    );

    append_log(gateway);

    /*
     * Extraemos solamente la primera dirección del gateway.
     */
    std::string gateway_ip =
        get_command_output(
            "ip route | "
            "awk '$1 == \"default\" {print $3}' | "
            "head -n 1"
        );

    if (gateway_ip.empty())
    {
        append_log(
            "[ERROR] No se pudo obtener la IP del gateway."
        );

        set_status(
            network_status,
            "GATEWAY ERROR",
            false
        );

        return;
    }

    append_log("");

    append_log(
        "Prueba de conectividad con gateway:"
    );

    std::string ping =
        get_command_output(
            "ping -c 2 -W 2 " + gateway_ip
        );

    append_log(ping);

    if (ping.find("0% packet loss") != std::string::npos)
    {
        append_log(
            "[OK] El gateway responde."
        );

        set_status(
            network_status,
            "GATEWAY OK",
            true
        );
    }
    else
    {
        append_log(
            "[ERROR] El gateway no responde."
        );

        set_status(
            network_status,
            "GATEWAY ERROR",
            false
        );
    }
}


/*
 * ================================================================
 * WINDOWS
 * ================================================================
 */

void SambaControlCenterPanel::show_windows_network()
{

        if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }

    append_section(
        "RUTA HACIA WINDOWS"
    );

    append_log(
        get_command_output(
            "ip route get " + shell_escape(windows_ip)
        )
    );

    append_section(
        "PING WINDOWS"
    );

    std::string ping =
        get_command_output(
            "ping -c 4 -W 2 " + shell_escape(windows_ip)
        );

    append_log(ping);

    if (ping.find("0% packet loss") != std::string::npos)
    {
        append_log(
            "[OK] Windows responde al ping."
        );
    }
    else
    {
        append_log(
            "[ERROR] Windows no responde al ping."
        );
    }

    append_section(
        "PUERTO SMB TCP 445"
    );

    std::string tcp445 =
        get_command_output(
            "nc -zvw 3 " + shell_escape(windows_ip) + " 445"
        );

    append_log(tcp445);

    if (
        tcp445.find("succeeded") != std::string::npos ||
        tcp445.find("open") != std::string::npos
    )
    {
        append_log(
            "[OK] TCP 445 accesible."
        );
    }
    else
    {
        append_log(
            "[ERROR] TCP 445 no accesible."
        );
    }

    append_section(
        "PUERTO NETBIOS TCP 139"
    );

    std::string tcp139 =
        get_command_output(
            "nc -zvw 3 " + shell_escape(windows_ip) + " 139"
        );

    append_log(tcp139);

    if (
        tcp139.find("succeeded") != std::string::npos ||
        tcp139.find("open") != std::string::npos
    )
    {
        append_log(
            "[OK] TCP 139 accesible."
        );
    }
    else
    {
        append_log(
            "[ERROR] TCP 139 no accesible."
        );
    }
}


/*
 * ================================================================
 * TCP 445 WINDOWS
 * ================================================================
 */

void SambaControlCenterPanel::test_windows_tcp445()
{
    if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }


    append_section(
        "PRUEBA TCP 445 - WINDOWS"
    );

    append_log(
        "Windows: " + windows_ip
    );

    append_log(
        "Probando TCP/445..."
    );

    std::string output =
        get_command_output(
            "nc -zvw 5 " + shell_escape(windows_ip) + " 445"
        );

    append_log(output);

    if (
        output.find("succeeded") != std::string::npos ||
        output.find("open") != std::string::npos
    )
    {
        append_log(
            "[OK] TCP 445 está accesible."
        );

        set_status(
            network_status,
            "TCP 445 OK",
            true
        );
    }
    else
    {
        append_log(
            "[ERROR] TCP 445 no está accesible."
        );

        set_status(
            network_status,
            "TCP 445 ERROR",
            false
        );
    }
}


/*
 * ================================================================
 * RED / ROUTING GENERAL
 * ================================================================
 */

void SambaControlCenterPanel::show_routing()
{
    append_section(
        "INFORMACIÓN GENERAL DE RED"
    );

    append_section(
        "INTERFACES IPv4"
    );

    append_log(
        get_command_output(
            "ip -4 -br addr"
        )
    );

    append_section(
        "HOSTNAME -I"
    );

    append_log(
        get_command_output(
            "hostname -I"
        )
    );

    append_section(
        "TABLA DE RUTAS"
    );

    append_log(
        get_command_output(
            "ip -4 route"
        )
    );

    if (!windows_ip.empty())
    {
        append_section(
            "RUTA HACIA WINDOWS " + windows_ip
        );

        append_log(
            get_command_output(
                "ip route get " + shell_escape(windows_ip)
            )
        );
    }
    else
    {
        append_section("RUTA HACIA WINDOWS");
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
    }

    append_section(
        "VECINOS ARP / NEIGHBOURS"
    );

    append_log(
        get_command_output(
            "ip neigh"
        )
    );

    set_status(
        network_status,
        "RED OK",
        true
    );
}


/*
 * ================================================================
 * VECINOS ARP / NEIGHBOURS
 * ================================================================
 */

void SambaControlCenterPanel::show_neighbors()
{
    append_section(
        "ARP / NEIGHBOURS"
    );

    std::string output =
        get_command_output(
            "ip neigh"
        );

    append_log(output);

    if (output.empty())
    {
        append_log(
            "[INFO] No se encontraron vecinos."
        );
    }

    set_status(
        network_status,
        "VECINOS OK",
        true
    );
}


/*
 * ================================================================
 * PING WINDOWS
 * ================================================================
 */

void SambaControlCenterPanel::ping_windows()
{
    if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }


    append_section(
        "PING WINDOWS"
    );

    append_log(
        "Destino: " + windows_ip
    );

    std::string output =
        get_command_output(
            "ping -c 4 -W 2 " + shell_escape(windows_ip)
        );

    append_log(output);

    if (
        output.find("0% packet loss") !=
        std::string::npos
    )
    {
        append_log(
            "[OK] Windows responde correctamente."
        );

        set_status(
            network_status,
            "WINDOWS ONLINE",
            true
        );
    }
    else
    {
        append_log(
            "[ERROR] Windows no responde."
        );

        set_status(
            network_status,
            "WINDOWS OFFLINE",
            false
        );
    }
}


/*
 * ================================================================
 * SMB PORT
 * ================================================================
 */

void SambaControlCenterPanel::test_smb_port()
{
    if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }


    append_section(
        "PRUEBA SMB TCP 445"
    );

    std::string output =
        get_command_output(
            "nc -zvw 5 " + shell_escape(windows_ip) + " 445"
        );

    append_log(output);

    if (
        output.find("succeeded") != std::string::npos ||
        output.find("open") != std::string::npos
    )
    {
        append_log(
            "[OK] SMB TCP 445 accesible."
        );
    }
    else
    {
        append_log(
            "[ERROR] SMB TCP 445 no accesible."
        );
    }
}


/*
 * ================================================================
 * SMBCLIENT
 * ================================================================
 */

void SambaControlCenterPanel::test_smbclient()
{
    append_section(
        "PRUEBA SMBCLIENT"
    );

    std::string output =
        get_command_output(
            "smbclient --version 2>&1"
        );

    append_log(output);

    if (output.empty())
    {
        append_log(
            "[ERROR] smbclient no está disponible."
        );

        return;
    }

    append_log(
        "[OK] smbclient disponible."
    );

    if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }

    append_section(
        "RECURSOS WINDOWS"
    );

    append_log(
        get_command_output(
            "smbclient -L " +
            shell_escape("//" + windows_ip) + " -N"
        )
    );
}


/*
 * ================================================================
 * FIN DEL BLOQUE RED
 * ================================================================
 */

void SambaControlCenterPanel::run_tcpdump_diagnostic()
{
    append_section(
        "TCPDUMP - DIAGNÓSTICO SMB"
    );

    if (windows_ip.empty())
    {
        append_log(
            "[INFO] Configura windows_ip en: " +
            windows_config_file().string()
        );
        return;
    }

    if (!command_exists("tcpdump"))
    {
        append_log(
            "tcpdump no está instalado."
        );

        return;
    }

    append_log(
        "Capturando tráfico TCP/445."
    );

    append_log(
        "Objetivo: " + windows_ip
    );

    append_log(
        "Duración: 15 segundos."
    );

    std::string output;

    const bool ok =
        run_admin_command(
            "timeout 15 "
            "tcpdump -ni any " +
            shell_escape(
                "host " + windows_ip + " and tcp port 445"
            ),
            output
        );

    append_log(output);

    if (ok)
    {
        append_log(
            "[OK] Captura terminada."
        );
    }
    else
    {
        append_log(
            "[INFO] tcpdump terminó con código no cero."
        );
    }
}


/*
 * ================================================================
 * DIAGNÓSTICO COMPLETO
 * ================================================================
 */

void SambaControlCenterPanel::full_diagnostic()
{
    append_section(
        "DIAGNÓSTICO COMPLETO SAMBA"
    );

    append_log(
        "Usuario Linux: " +
        current_user()
    );

    append_log(
        "HOME: " +
        home_directory()
    );

    append_log(
        "Carpeta compartida: " +
        shared_folder
    );

    append_log(
        "Usuario Samba: " +
        samba_user
    );

    /*
     * Autorización única del diagnóstico: las dos consultas con
     * root (ufw status y pdbedit -L) corren en UN solo pkexec.
     * Son lecturas independientes: batch_step_always no detiene
     * una si la otra falla. Los segmentos se consumen en las
     * secciones ESTADO UFW y USUARIO SAMBA, más abajo.
     */

    const bool diag_has_ufw =
        command_exists("ufw");

    const bool diag_has_pdbedit =
        command_exists("pdbedit");

    std::string diag_script;

    if (diag_has_ufw)
    {
        diag_script += batch_step_always(
            "ufw",
            "ufw status"
        );
    }

    if (diag_has_pdbedit)
    {
        diag_script += batch_step_always(
            "users",
            "pdbedit -L"
        );
    }

    std::string diag_output;
    std::size_t diag_cursor = 0;

    if (!diag_script.empty())
    {
        run_admin_command(
            diag_script,
            diag_output
        );
    }

    check_dependencies();

    check_samba();

    folder_exists();

    check_services();

    /*
     * ESTADO UFW: consume el segmento "ufw" del lote.
     */

    append_section(
        "ESTADO UFW"
    );

    if (!diag_has_ufw)
    {
        set_status_unknown(
            firewall_status,
            "NO INSTALADO"
        );
    }
    else
    {
        std::string ufw_segment;
        int ufw_rc = -1;

        ufw_segment =
            batch_take_segment(
                diag_output,
                diag_cursor,
                "ufw",
                ufw_rc
            );

        apply_firewall_output(
            ufw_rc == 0,
            ufw_segment
        );
    }

    check_ports();

    show_routing();

    ping_windows();

    test_smb_port();

    append_section(
        "CONFIGURACIÓN SMB"
    );

    validate_samba_config();

    append_section(
        "USUARIO SAMBA"
    );

    if (!diag_has_pdbedit)
    {
        append_log(
            "[ERROR] No se encontró el comando pdbedit."
        );
    }
    else
    {
        std::string users_segment;
        int users_rc = -1;

        users_segment =
            batch_take_segment(
                diag_output,
                diag_cursor,
                "users",
                users_rc
            );

        if (users_rc != 0)
        {
            // Fallo o cancelación: NO se afirma que no exista.
            append_log(
                "[INFO] No se pudo verificar el usuario Samba."
            );
        }
        else if (pdbedit_list_has_user(
                     users_segment,
                     samba_user))
        {
            append_log(
                "[OK] Usuario Samba existe."
            );
        }
        else
        {
            append_log(
                "[FALTA] Usuario Samba no existe."
            );
        }
    }

    append_section(
        "PROCESOS SAMBA"
    );

    append_log(
        get_command_output(
            "ps aux | grep '[s]mbd'"
        )
    );

    append_section(
        "SOCKETS SAMBA"
    );

    append_log(
        get_command_output(
            "ss -lntup | "
            "grep -E ':(139|445)\\b'"
        )
    );

    append_section(
        "CONFIGURACIÓN ACTUAL"
    );

    if (fs::exists(
            samba_config))
    {
        append_log(
            samba_config
        );

        append_log(
            get_command_output(
                "testparm -s 2>/dev/null"
            )
        );
    }
    else
    {
        append_log(
            "[ERROR] No existe " +
            samba_config
        );
    }

    append_section(
        "DIAGNÓSTICO FINALIZADO"
    );

    show_info(
        "Diagnóstico",
        "Diagnóstico completo terminado."
    );
}


/*
 * ================================================================
 * LIMPIAR
 * ================================================================
 */

void SambaControlCenterPanel::clear_log()
{
    if (log_buffer)
        log_buffer->set_text("");

    append_log(
        "Samba Control Center listo."
    );

    append_log(
        "Usuario: " +
        samba_user
    );

    append_log(
        "Share: " +
        shared_folder
    );
}


/*
 * ================================================================
 * DIÁLOGO SINCRÓNICO COMPATIBLE GTK4
 * ================================================================
 *
 * GTK4 eliminó Gtk::Dialog::run().
 *
 * Creamos un MainLoop temporal para poder esperar
 * la respuesta del diálogo sin utilizar APIs GTK3.
 *
 * ================================================================
 */

int SambaControlCenterPanel::run_dialog_sync(
    Gtk::Dialog& dialog)
{
    auto loop =
        Glib::MainLoop::create();

    int response =
        Gtk::ResponseType::NONE;

    dialog.signal_response().connect(
        [&loop, &response]
        (int result)
        {
            response = result;

            if (loop->is_running())
                loop->quit();
        }
    );

    dialog.present();

    loop->run();

    dialog.close();

    return response;
}


/*
 * ================================================================
 * ASK INSTALL
 * ================================================================
 */

bool SambaControlCenterPanel::ask_install(
    const std::string& package)
{
    (void)package;

    Gtk::MessageDialog dialog(
        *parent_window,
        "Dependencia no instalada",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true
    );

    dialog.set_secondary_text(
        "Faltan uno o más componentes.\n\n"
        "Se instalarán:\n"
        "Samba, smbclient, tcpdump, UFW,\n"
        "iproute2, ping, netcat y utilidades.\n\n"
        "¿Desea instalarlos ahora?"
    );

    const int response =
        run_dialog_sync(
            dialog
        );

    return
        response ==
        Gtk::ResponseType::YES;
}


/*
 * ================================================================
 * ASK YES / NO
 * ================================================================
 */

bool SambaControlCenterPanel::ask_yes_no(
    const std::string& title,
    const std::string& message)
{
    Gtk::MessageDialog dialog(
        *parent_window,
        title,
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true
    );

    dialog.set_secondary_text(
        message
    );

    const int response =
        run_dialog_sync(
            dialog
        );

    return
        response ==
        Gtk::ResponseType::YES;
}


/*
 * ================================================================
 * INFO
 * ================================================================
 */

void SambaControlCenterPanel::show_info(
    const std::string& title,
    const std::string& message)
{
    Gtk::MessageDialog dialog(
        *parent_window,
        title,
        false,
        Gtk::MessageType::INFO,
        Gtk::ButtonsType::OK,
        true
    );

    dialog.set_secondary_text(
        message
    );

    run_dialog_sync(
        dialog
    );
}


/*
 * ================================================================
 * ERROR
 * ================================================================
 */

void SambaControlCenterPanel::show_error(
    const std::string& title,
    const std::string& message)
{
    Gtk::MessageDialog dialog(
        *parent_window,
        title,
        false,
        Gtk::MessageType::ERROR,
        Gtk::ButtonsType::OK,
        true
    );

    dialog.set_secondary_text(
        message
    );

    run_dialog_sync(
        dialog
    );
}


/*
 * ================================================================
 * MAIN
 * ================================================================
 */
