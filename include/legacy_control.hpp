#pragma once
#include <gtkmm.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <thread>
#include "shared_status.hpp"
class SambaControlCenterPanel : public Gtk::Box
{
public:

    explicit SambaControlCenterPanel(Gtk::Window& parent, SharedStatus& shared_status);

    ~SambaControlCenterPanel();

    // Reinicio de smbd y nmbd: única implementación,
    // vive en este panel. El botón RESET de HOME la
    // invoca a través de ControlPage y MainWindow.
    bool restart_services();

    // Selector de equipo Windows (CONFIG WIN): única
    // implementación, vive en este panel. El botón
    // PICK de SETTINGS lo invoca a través de
    // ControlPage y MainWindow.
    void pick_windows_ip();

    // Veredicto de carpeta (el chip que muestra
    // HOME): público para que el REFRESH de FILE
    // pueda refrescarlo vía ControlPage, igual que
    // el RESET y el PICK cruzan entre páginas.
    bool folder_exists();

private:
    Gtk::Window* parent_window = nullptr;
   SharedStatus& shared_status_;


    /*
     * ------------------------------------------------------------
     * WIDGETS
     * ------------------------------------------------------------
     */

    Gtk::Box main_box{
        Gtk::Orientation::VERTICAL
    };



    Gtk::ScrolledWindow log_scroll;

    Gtk::TextView log_view;

    Glib::RefPtr<Gtk::TextBuffer>
        log_buffer;



   Gtk::Label& samba_status;
   Gtk::Label& tcpdump_status;
   Gtk::Label& folder_status;
   Gtk::Label& folder_title;
   Gtk::Label& smbd_status;
   Gtk::Label& nmbd_status;
   Gtk::Label& firewall_status;
   Gtk::Label& ports_status;
   Gtk::Label& network_status;

    Gtk::Button btn_configure;
    Gtk::Button btn_password;
    Gtk::Button btn_firewall;
    Gtk::Button btn_ports;
    Gtk::Button btn_network;
    Gtk::Button btn_interfaces;
    Gtk::Button btn_routes;
    Gtk::Button btn_gateway;
    Gtk::Button btn_neighbors;
    Gtk::Button btn_windows;
    Gtk::Button btn_tcp445;
   Gtk::Button btn_config_win;
   Gtk::Button btn_scan_red;
    Gtk::Button btn_ping;
    Gtk::Button btn_smbclient;
    Gtk::Button btn_tcpdump;
    Gtk::Button btn_diagnose;
    Gtk::Button btn_clear;

    /*
     * ------------------------------------------------------------
     * CONFIGURACIÓN
     * ------------------------------------------------------------
     */


    const std::string samba_config =
        "/etc/samba/smb.conf";

    std::string shared_folder;
    std::string samba_user;

    // IP del equipo Windows cargada desde la configuración del usuario.
    std::string windows_ip;

    // Shares emitidos por la última generación de smb.conf
    // (rebanada 4, CONFIGURAR multi-share): prepare los
    // rellena y configure_samba los usa para el paso de
    // carpetas del lote y para el registro final.
    struct GeneratedShare
    {
        std::string name;
        std::string folder;
    };

    std::vector<GeneratedShare> generated_shares_;

    /*
     * ------------------------------------------------------------
     * GUI
     * ------------------------------------------------------------
     */

    void setup_ui();


    void configure_button(
        Gtk::Button& button,
        const std::string& text,
        const std::string& tooltip
    );

    void append_log(
        const std::string& text
    );

    void append_section(
        const std::string& title
    );

    void set_status(
        Gtk::Label& label,
        const std::string& text,
        bool good
    );

    void set_status_unknown(
        Gtk::Label& label,
        const std::string& text
    );

    /*
     * ------------------------------------------------------------
     * DEPENDENCIAS
     * ------------------------------------------------------------
     */

    bool check_package(
        const std::string& command,
        const std::string& package
    );

    void check_dependencies();

    void install_dependencies();

    /*
     * ------------------------------------------------------------
     * SAMBA
     * ------------------------------------------------------------
     */

    bool check_samba();

    bool check_smbclient();

    bool check_smbpasswd();

    bool check_testparm();

    void configure_samba();

    // Remate de configure_samba en el hilo GTK, cuando el
    // lote vuelve del hilo de trabajo (rebanada 7).
    void finish_configure_samba(
        const std::string& output,
        const std::string& temp,
        bool has_pdbedit,
        bool has_nmbd,
        bool has_ufw);

    // Remate del escaneo de red en el hilo GTK.
    void finish_scan_network(
        bool ok,
        const std::string& output);

    std::string prepare_samba_config_file();

    bool validate_samba_config();


    /*
     * ------------------------------------------------------------
     * CARPETA
     * ------------------------------------------------------------
     */

    bool create_shared_folder();

    void fix_folder_permissions();

    /*
     * ------------------------------------------------------------
     * USUARIO
     * ------------------------------------------------------------
     */

    void create_samba_user(
        int pdbedit_rc,
        const std::string& pdbedit_output
    );

    void show_password_dialog();

   void configure_windows();

    void set_samba_password(
        const std::string& password
    );

    /*
     * ------------------------------------------------------------
     * SERVICIOS
     * ------------------------------------------------------------
     */


    void check_services();

    /*
     * ------------------------------------------------------------
     * FIREWALL
     * ------------------------------------------------------------
     */

    bool configure_firewall();

    void apply_firewall_output(
        bool ok,
        const std::string& output
    );

    /*
     * ------------------------------------------------------------
     * RED
     * ------------------------------------------------------------
     */

    void check_ports();

    void show_interfaces();
    void show_routes();
    void show_gateway();

    void show_windows_network();

    void test_windows_tcp445();
    void show_network();


    void ping_windows();


    void test_smb_port();

    void show_routing();

    void show_neighbors();
   void scan_network();

    /*
     * ------------------------------------------------------------
     * SMB
     * ------------------------------------------------------------
     */

    void test_smbclient();

    /*
     * ------------------------------------------------------------
     * TCPDUMP
     * ------------------------------------------------------------
     */

    void run_tcpdump_diagnostic();

    /*
     * ------------------------------------------------------------
     * DIAGNÓSTICO
     * ------------------------------------------------------------
     */

    void full_diagnostic();

    void clear_log();

    /*
     * ------------------------------------------------------------
     * ADMINISTRACIÓN
     * ------------------------------------------------------------
     */

    std::string pkexec_command(
        const std::string& command
    );

    bool run_admin_command(
        const std::string& command,
        std::string& output
    );

    /*
     * ------------------------------------------------------------
     * ASÍNCRONO (rebanada 7)
     * ------------------------------------------------------------
     * Una sola operación larga a la vez: el comando con
     * privilegios corre en un hilo (la ventana sigue viva)
     * y el Dispatcher avisa al hilo GTK, que recoge el
     * resultado y ejecuta el remate. Mientras dura, la
     * botonera queda insensible.
     */

    Gtk::Grid* button_grid_ = nullptr;

    Glib::Dispatcher async_signal_;
    std::thread async_thread_;
    bool async_busy_ = false;
    bool async_ok_ = false;
    std::string async_output_;
    std::function<void(bool, const std::string&)>
        async_done_;

    void start_async(
        const std::string& admin_command,
        std::function<void(bool, const std::string&)> done
    );

    void on_async_finished();

    /*
     * ------------------------------------------------------------
     * DIÁLOGOS GTK4
     * ------------------------------------------------------------
     */

    int run_dialog_sync(
        Gtk::Dialog& dialog
    );

    bool ask_install(
        const std::string& package
    );

    bool ask_yes_no(
        const std::string& title,
        const std::string& message
    );

    void show_info(
        const std::string& title,
        const std::string& message
    );

    void show_error(
        const std::string& title,
        const std::string& message
    );
};
