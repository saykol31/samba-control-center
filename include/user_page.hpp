#pragma once

#include <gtkmm.h>
#include <array>
#include <string>
#include <vector>

/*
 * ================================================================
 * PÁGINA USER — IDENTIDAD Y ACCESO, EN DOS COLUMNAS
 * ================================================================
 *
 * SERVER — usuarios Samba de esta máquina, vía pdbedit y
 * smbpasswd: listar (una elevación al refrescar), alta,
 * contraseña, habilitar, deshabilitar y baja. Cada acción
 * pide su propia autorización pkexec, como el resto de la
 * app: nada de ayudantes privilegiados permanentes.
 *
 * CLIENT — la mitad cliente tipo Smb4K: buscar servidores
 * con el mismo escaneo nmap de la casa, listar shares con
 * smbclient, montar con GVfs (gio mount, sin root), abrir
 * en el gestor de archivos y desmontar. Conexiones
 * guardadas en la config XDG: servidor + recurso +
 * usuario — la contraseña NUNCA se guarda; se pide al
 * conectar.
 * ================================================================
 */

class UserPage : public Gtk::Box {
public:
    UserPage();

private:
    // ---------- SERVER ----------
    Gtk::ListBox users_list_;
    std::vector<std::string> user_names_;
    Gtk::Entry new_user_entry_;

    std::string selected_user() const;
    void refresh_users();
    void on_add_user();
    void on_set_password();
    void on_enable_user();
    void on_disable_user();
    void on_delete_user();
    void ask_password(const std::string& user, bool create);
    bool run_user_command(
        const std::string& command,
        const std::string& ok_message);

    // ---------- CLIENT ----------
    Gtk::Entry server_entry_;
    Gtk::ListBox hosts_list_;
    Gtk::Entry client_user_entry_;
    Gtk::PasswordEntry client_password_entry_;
    Gtk::ListBox shares_list_;
    std::vector<std::string> share_names_;
    Gtk::ListBox saved_list_;
    std::vector<std::array<std::string, 3>> saved_;

    std::string selected_share() const;
    std::string share_uri() const;
    void on_scan();
    void on_list_shares();
    void on_connect();
    void on_open_share();
    void on_disconnect();
    void on_save_connection();
    void on_remove_saved();
    void load_saved_connections();
    void store_saved_connections() const;
    void rebuild_saved_list();

    // ---------- compartido ----------
    Gtk::TextView log_view_;
    void log_line(const std::string& text);
};
