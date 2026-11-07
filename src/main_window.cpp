#include "main_window.hpp"
#include "home_page.hpp"
#include "file_page.hpp"
#include "control_page.hpp"
#include "user_page.hpp"
#include "settings_page.hpp"
#include "help_page.hpp"

MainWindow::MainWindow() {
    set_title("Samba Control Center");
    set_default_size(1050, 720);
    set_child(root_box);

    notebook.set_hexpand(true);
    notebook.set_vexpand(true);

    auto* home_page =
        Gtk::make_managed<HomePage>(shared_status);

    auto* control_page =
        Gtk::make_managed<ControlPage>(*this, shared_status);

    auto* file_page =
        Gtk::make_managed<FilePage>();

    // El RESET de HOME acciona el reinicio del panel de
    // CONTROL: una sola implementación para los dos sitios.
    home_page->signal_reset_requested.connect(
        sigc::hide_return(
            sigc::mem_fun(
                *control_page,
                &ControlPage::restart_services
            )
        )
    );

    // El BOOT de HOME levanta el sistema entero: tras
    // arrancar smbd y nmbd, el panel de CONTROL corre
    // INTERFACES, PORTS 139/445 y UFW FIREWALL — las
    // acciones de sus propios botones, sin duplicar
    // implementaciones.
    home_page->signal_boot_extras_requested.connect(
        sigc::mem_fun(
            *control_page,
            &ControlPage::boot_extras
        )
    );

    // El REFRESH de FILE también refresca el
    // veredicto de carpeta que muestra HOME (vía el
    // panel de CONTROL): la carpeta de FILE y la de
    // HOME son la misma y deben decir lo mismo.
    file_page->signal_refresh_requested.connect(
        sigc::mem_fun(
            *control_page,
            &ControlPage::refresh_folder_status
        )
    );

    auto* settings_page =
        Gtk::make_managed<SettingsPage>(shared_status);

    // El PICK de SETTINGS acciona el selector CONFIG WIN
    // del panel de CONTROL: una sola implementación de
    // escaneo y guardado. Al volver, SETTINGS relee el
    // windows_ip que el selector haya guardado.
    settings_page->signal_pick_requested.connect(
        [control_page, settings_page]() {
            control_page->pick_windows_ip();
            settings_page->reload_windows_ip();
        }
    );

    notebook.append_page(*home_page, "HOME");
    notebook.append_page(*file_page, "FILE");
    notebook.append_page(*control_page, "CONTROL");
    notebook.append_page(*Gtk::make_managed<UserPage>(), "USER");
    notebook.append_page(*settings_page, "SETTINGS");
    notebook.append_page(*Gtk::make_managed<HelpPage>(), "HELP");

    root_box.append(notebook);
}

