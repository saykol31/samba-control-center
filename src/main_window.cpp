#include "main_window.hpp"
#include "home_page.hpp"
#include "control_page.hpp"
#include "user_page.hpp"

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

    notebook.append_page(*Gtk::make_managed<HomePage>(shared_status), "HOME");
    notebook.append_page(*Gtk::make_managed<ControlPage>(*this, shared_status), "CONTROL");
    notebook.append_page(*Gtk::make_managed<UserPage>(), "USER");

    root_box.append(notebook);
}

void MainWindow::add_menu_item(const Glib::ustring& text) {
    auto* button = Gtk::make_managed<Gtk::Button>(text);
    button->set_has_frame(false);
    menu_box.append(*button);

    // La navegación real se conectará al terminar la estructura.
}
