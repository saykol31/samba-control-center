#pragma once

#include <gtkmm.h>
#include <string>
#include "shared_status.hpp"

class HomePage : public Gtk::Box {
public:
    explicit HomePage(SharedStatus& shared_status);

    // El botón RESET no reinicia por su cuenta: emite
    // esta señal y MainWindow la conecta con la única
    // implementación de reinicio (la del panel de
    // CONTROL, a través de ControlPage).
    sigc::signal<void()> signal_reset_requested;

private:
    SharedStatus& shared_status_;

    Gtk::Box energy_box_{Gtk::Orientation::HORIZONTAL, 12};
    Gtk::Button btn_boot_;
    Gtk::Button btn_reset_;
    Gtk::Button btn_shutdown_;

    void on_boot();
    void on_reset();
    void on_shutdown();

    // Aplica la acción (start / stop) a smbd y,
    // si está instalado, a nmbd, con privilegios de
    // administrador; al terminar refresca las etiquetas
    // de estado de ambos servicios.
    void run_service_action(const std::string& action);
    void refresh_service_status();
};
