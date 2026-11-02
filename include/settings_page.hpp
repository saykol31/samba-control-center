#pragma once

#include <gtkmm.h>
#include <string>
#include <vector>

/*
 * ================================================================
 * PÁGINA SETTINGS — VALORES, NO ACCIONES
 * ================================================================
 *
 * SETTINGS guarda valores en la configuración XDG de la app
 * (la misma de siempre: clave=valor). Nunca escribe smb.conf
 * ni toca servicios: los valores de servidor se aplican con
 * CONFIGURE SAMBA en CONTROL; los de cliente se leen al
 * conectar. Sin contraseñas en claro, nunca.
 *
 * Secciones:
 *   SERVER       workgroup, permisos por defecto de shares,
 *                invitados fijo en NO por diseño.
 *   CLIENT       windows_ip (con PICK: abre el selector
 *                CONFIG WIN del panel de CONTROL) y usuario
 *                por defecto al conectar.
 *   DEPENDENCIES la tabla única comando→paquete: el chequeo
 *                y la instalación salen de la misma tabla,
 *                así nunca se desincronizan (así se escapó
 *                nmap una vez). Instalar = una sola elevación
 *                pkexec y una sola transacción apt, por nombre
 *                de paquete, jamás con versión fijada.
 * ================================================================
 */

class SettingsPage : public Gtk::Box {
public:
    SettingsPage();

    // El botón PICK no busca por su cuenta: emite esta señal
    // y MainWindow la conecta con el selector CONFIG WIN del
    // panel de CONTROL — una sola implementación.
    sigc::signal<void()> signal_pick_requested;

    // Tras el PICK, el valor pudo cambiar en el selector:
    // se relee windows_ip de la configuración.
    void reload_windows_ip();

private:
    struct Dependency {
        const char* label;
        const char* command;
        const char* package;
        bool required;
    };

    static const std::vector<Dependency>& dependency_table();

    Gtk::Entry workgroup_entry_;
    Gtk::CheckButton shares_readonly_check_{
        "Shares are read-only by default"};
    Gtk::Entry windows_ip_entry_;
    Gtk::Entry client_user_entry_;
    Gtk::Label values_status_;

    Gtk::TextView deps_view_;
    Gtk::Label deps_summary_;
    Gtk::Button btn_install_{"INSTALL MISSING"};

    void load_values();
    void set_values_feedback(const std::string& text, bool good);
    void on_save_values();
    void refresh_dependencies();
    void on_install_missing();
    std::vector<const Dependency*> missing_dependencies() const;
};
