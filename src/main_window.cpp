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

    menu_box.set_margin_start(8);
    menu_box.set_margin_end(8);
    menu_box.set_margin_top(6);

    add_menu_item("File");
    add_menu_item("Edit");
    add_menu_item("Panel");
    add_menu_item("Home");
    add_menu_item("User");
    add_menu_item("Settings");
    add_menu_item("Help");

    root_box.append(menu_box);

    notebook.set_hexpand(true);
    notebook.set_vexpand(true);

    auto* home_page =
        Gtk::make_managed<HomePage>(shared_status);

    auto* control_page =
        Gtk::make_managed<ControlPage>(*this, shared_status);

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

    auto* settings_page =
        Gtk::make_managed<SettingsPage>();

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
    notebook.append_page(*Gtk::make_managed<FilePage>(), "FILE");
    notebook.append_page(*control_page, "CONTROL");
    notebook.append_page(*Gtk::make_managed<UserPage>(), "USER");
    notebook.append_page(*settings_page, "SETTINGS");
    notebook.append_page(*Gtk::make_managed<HelpPage>(), "HELP");

    root_box.append(notebook);
}

void MainWindow::add_menu_item(const Glib::ustring& text) {
    auto* button = Gtk::make_managed<Gtk::Button>(text);
    button->set_has_frame(false);
    menu_box.append(*button);

    // La navegación real se conectará al terminar la estructura.
}
