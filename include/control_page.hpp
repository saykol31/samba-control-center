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

    // Puente al veredicto de carpeta del panel
    // heredado (folder_exists): lo acciona el botón
    // REFRESH de FILE, para que el chip que muestra
    // HOME se entere de los cambios de carpeta sin
    // esperar a un reinicio de la app.
    void refresh_folder_status();

    // Puente a las tres acciones que completan el
    // BOOT de HOME (interfaces, puertos y firewall
    // del panel heredado): las acciona el botón BOOT
    // tras arrancar los servicios, para levantar el
    // sistema entero de un solo encendido.
    void boot_extras();

private:
    SambaControlCenterPanel* controller_ = nullptr;
};
