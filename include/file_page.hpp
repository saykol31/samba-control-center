#pragma once

#include <gtkmm.h>
#include <functional>
#include <string>
#include <vector>

/*
 * ================================================================
 * PÁGINA FILE — CARPETAS QUE COMPARTE ESTA MÁQUINA
 * ================================================================
 *
 * El usuario elige qué carpetas se comparten; ya no hay una
 * carpeta forzada. La tabla vive aquí; CONFIGURAR (página
 * CONTROL) aplicará la lista con el flujo validado.
 *
 * Regla de validación por carpeta: la que no exista se marca
 * [FALTA] y queda excluida de active_shares() — nunca tumba
 * el resto. Sin lista guardada, el valor por defecto es la
 * carpeta ~/Red de siempre (compatibilidad).
 *
 * Persistencia: el mismo archivo de configuración XDG de la
 * app ($XDG_CONFIG_HOME/samba-control-center/config), con
 * líneas clave=valor:
 *
 *      share=<nombre>|<ruta>|<rw|ro>|<1|0 visible>
 * ================================================================
 */

class FilePage : public Gtk::Box {
public:
    FilePage();

    struct ShareDef {
        std::string folder;
        std::string name;
        bool read_only = false;
        bool visible = true;
    };

    // Todas las filas de la tabla, en orden.
    std::vector<ShareDef> shares() const;

    // Solo las carpetas que existen: las [FALTA] quedan
    // fuera. Es la lista que consumirá CONFIGURAR.
    std::vector<ShareDef> active_shares() const;

private:
    class ShareRow : public Glib::Object {
    public:
        std::string folder;
        std::string name;
        bool read_only = false;
        bool visible = true;

        static Glib::RefPtr<ShareRow> create(
            const std::string& folder,
            const std::string& name,
            bool read_only,
            bool visible)
        {
            return Glib::make_refptr_for_instance<ShareRow>(
                new ShareRow(folder, name, read_only, visible));
        }

    private:
        ShareRow(const std::string& folder_,
                 const std::string& name_,
                 bool read_only_,
                 bool visible_)
            : folder(folder_), name(name_),
              read_only(read_only_), visible(visible_) {}
    };

    Gtk::ColumnView column_view_;
    Glib::RefPtr<Gio::ListStore<ShareRow>> store_;
    Glib::RefPtr<Gtk::SingleSelection> selection_;

    Gtk::Entry name_entry_;
    Gtk::CheckButton readonly_check_{"Read only"};
    Gtk::CheckButton visible_check_{"Visible"};

    Gtk::Entry new_folder_entry_;
    Gtk::Button btn_add_folder_{"ADD FOLDER"};
    Gtk::Button btn_create_{"CREATE"};
    Gtk::Button btn_remove_{"REMOVE"};
    Gtk::Button btn_refresh_{"REFRESH"};

    // FileChooserNative (gtkmm 4.0+) en vez de FileDialog
    // (gtkmm 4.10+): el mismo código compila en la MX
    // (gtkmm 4.8, Debian 12) y en la Surface (4.18).
    Glib::RefPtr<Gtk::FileChooserNative> folder_dialog_;
    bool syncing_ = false;

    Glib::RefPtr<Gtk::SignalListItemFactory> make_text_factory(
        const std::function<
            Glib::ustring(const ShareRow&)>& getter);
    Glib::RefPtr<Gtk::SignalListItemFactory> make_state_factory();

    void load_shares();
    void save_shares() const;
    void add_folder(const std::string& path);
    Glib::RefPtr<ShareRow> selected_row() const;
    void sync_editors();
    void touch_row(const Glib::RefPtr<ShareRow>& row);
    void refresh_all_rows();

    void on_add_folder();
    void on_create_folder();
    void on_remove();
    void on_name_changed();
    void on_readonly_toggled();
    void on_visible_toggled();
};
