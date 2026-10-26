#pragma once
#include <gtkmm.h>
#include <string>
#include <vector>
#include <filesystem>
#include "shared_status.hpp"
class SambaControlCenterPanel : public Gtk::Box
{
public:

    explicit SambaControlCenterPanel(Gtk::Window& parent, SharedStatus& shared_status);

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
   Gtk::Label& smbd_status;
   Gtk::Label& nmbd_status;
   Gtk::Label& firewall_status;
   Gtk::Label& ports_status;
   Gtk::Label& network_status;

    Gtk::Button btn_install;
    Gtk::Button btn_configure;
    Gtk::Button btn_password;
    Gtk::Button btn_restart;
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

    std::string prepare_samba_config_file();

    bool validate_samba_config();


    /*
     * ------------------------------------------------------------
     * CARPETA
     * ------------------------------------------------------------
     */

    bool folder_exists();

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


    bool restart_services();

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
