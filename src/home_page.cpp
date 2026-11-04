#include "home_page.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

/*
 * ================================================================
 * PANEL HOME — BOTONES DE ENERGÍA
 * ================================================================
 *
 * Boot     ⏻ verde   (U+23FB)  arranca  smbd y nmbd y levanta el sistema entero
 * Reset    ⟳ azul    (U+27F3)  reinicia smbd y nmbd
 * Shutdown ⏻ rojo    (U+23FB)  detiene  smbd y nmbd
 *
 * Boot y Shutdown se ejecutan con pkexec y con el gestor
 * de servicios que corresponda al init activo (systemctl
 * en systemd, service en SysVinit/OpenRC), siguiendo el
 * mismo criterio que el panel de control heredado. Al
 * terminar, el estado de los servicios se refresca con el
 * mismo formato del resto de la aplicación
 * (✓ RUNNING / ✗ STOPPED).
 *
 * Reset NO tiene implementación propia: emite una señal
 * que MainWindow conecta con restart_services() del panel
 * de CONTROL — la única implementación del reinicio.
 *
 * Boot tampoco termina en los servicios: tras arrancarlos
 * emite signal_boot_extras_requested, y MainWindow hace
 * que el panel de CONTROL corra INTERFACES, PORTS 139/445
 * y UFW FIREWALL — las acciones de sus propios botones.
 * Sin eso el veredicto global de HOME no podía pasar a
 * ACTIVE tras un encendido (red sin confirmar y puertos
 * sin revisar con los servicios ya en marcha).
 *
 * ================================================================
 */


/*
 * ------------------------------------------------------------
 * UTILIDADES (mismo criterio que legacy_control.cpp)
 * ------------------------------------------------------------
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
           ">&2; exit 127";
}


static std::string get_command_output(
    const std::string& command)
{
    return trim(
        execute_command(command)
    );
}


/*
 * ------------------------------------------------------------
 * PRESENTACIÓN
 * ------------------------------------------------------------
 */

static void configure_energy_button(
    Gtk::Button& button,
    const char* markup,
    const char* tooltip)
{
    auto* label =
        Gtk::make_managed<Gtk::Label>();

    label->set_markup(markup);
    button.set_child(*label);
    button.set_tooltip_text(tooltip);
    button.set_hexpand(true);
}


static void set_service_status(
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


/*
 * ------------------------------------------------------------
 * PANEL HOME
 * ------------------------------------------------------------
 */

HomePage::HomePage(SharedStatus& shared_status)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12),
      shared_status_(shared_status)
{
    set_hexpand(true);
    set_vexpand(true);
    append(shared_status_);

    configure_energy_button(
        btn_boot_,
        "<span foreground='green' weight='bold'>⏻</span> BOOT",
        "Arrancar smbd y nmbd"
    );

    configure_energy_button(
        btn_reset_,
        "<span foreground='blue' weight='bold'>⟳</span> RESET",
        "Reiniciar smbd y nmbd"
    );

    configure_energy_button(
        btn_shutdown_,
        "<span foreground='red' weight='bold'>⏻</span> SHUTDOWN",
        "Detener smbd y nmbd"
    );

    energy_box_.append(btn_boot_);
    energy_box_.append(btn_reset_);
    energy_box_.append(btn_shutdown_);
    append(energy_box_);

    btn_boot_.signal_clicked().connect(
        sigc::mem_fun(*this, &HomePage::on_boot)
    );

    btn_reset_.signal_clicked().connect(
        sigc::mem_fun(*this, &HomePage::on_reset)
    );

    btn_shutdown_.signal_clicked().connect(
        sigc::mem_fun(*this, &HomePage::on_shutdown)
    );

    refresh_service_status();
}


void HomePage::on_boot()
{
    run_service_action("start");

    // Con los servicios ya en marcha, el panel de
    // CONTROL levanta el resto del sistema (interfaces,
    // puertos y firewall): un solo BOOT, sistema activo.
    signal_boot_extras_requested.emit();
}


void HomePage::on_reset()
{
    // El cable de este botón va al panel de CONTROL:
    // allí vive la única implementación del reinicio.
    signal_reset_requested.emit();
}


void HomePage::on_shutdown()
{
    run_service_action("stop");
}


void HomePage::run_service_action(
    const std::string& action)
{
    std::string combined =
        service_command("smbd", action);

    if (command_exists("nmbd"))
    {
        combined +=
            "; " + service_command("nmbd", action);
    }

    execute_command(
        "pkexec /bin/sh -c " +
        shell_escape(combined)
    );

    refresh_service_status();
}


void HomePage::refresh_service_status()
{
    const bool smbd_ok =
        !get_command_output("pgrep -x smbd").empty();

    set_service_status(
        shared_status_.smbd(),
        smbd_ok
            ? "RUNNING"
            : "STOPPED",
        smbd_ok
    );

    if (command_exists("nmbd"))
    {
        const bool nmbd_ok =
            !get_command_output("pgrep -x nmbd").empty();

        set_service_status(
            shared_status_.nmbd(),
            nmbd_ok
                ? "RUNNING"
                : "STOPPED",
            nmbd_ok
        );
    }
    else
    {
        shared_status_.nmbd().set_markup(
            "<span foreground='orange' "
            "weight='bold'>⚠ NO DISPONIBLE</span>"
        );
    }
}
