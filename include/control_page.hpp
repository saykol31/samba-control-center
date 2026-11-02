#pragma once
#include <gtkmm.h>
#include "shared_status.hpp"

class SambaControlCenterPanel;

class ControlPage : public Gtk::Box {
public:
    ControlPage(Gtk::Window& parent, SharedStatus& shared_status);

    // Puente hacia la única implementación de reinicio,
    // la del panel heredado: la acciona el RESET de HOME.
    bool restart_services();

    // Puente hacia el selector de equipo Windows del
    // panel heredado (CONFIG WIN): lo acciona el botón
    // PICK de SETTINGS.
    void pick_windows_ip();

private:
    SambaControlCenterPanel* controller_ = nullptr;
};
