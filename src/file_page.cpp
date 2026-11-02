#include "file_page.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/*
 * ================================================================
 * PÁGINA FILE — implementación (ver file_page.hpp)
 * ================================================================
 */


/*
 * ------------------------------------------------------------
 * UTILIDADES (mismo criterio que el resto de la app)
 * ------------------------------------------------------------
 */

static std::string trim(const std::string& input)
{
    const char* whitespace = " \t\n\r\f\v";

    const auto begin = input.find_first_not_of(whitespace);

    if (begin == std::string::npos)
        return "";

    const auto end = input.find_last_not_of(whitespace);

    return input.substr(begin, end - begin + 1);
}


/*
 * El mismo archivo de configuración que usa el panel
 * heredado para windows_ip:
 * $XDG_CONFIG_HOME/samba-control-center/config
 */
static fs::path config_file_path()
{
    const char* config_dir = g_get_user_config_dir();

    if (config_dir && *config_dir)
    {
        return fs::path(config_dir)
            / "samba-control-center"
            / "config";
    }

    return fs::path(Glib::get_home_dir())
        / ".config"
        / "samba-control-center"
        / "config";
}


static std::string share_display_name(
    const std::string& folder,
    const std::string& name)
{
    if (!name.empty())
        return name;

    const std::string base =
        fs::path(folder).filename().string();

    return base.empty() ? folder : base;
}


/*
 * ------------------------------------------------------------
 * FÁBRICAS DE COLUMNAS
 * ------------------------------------------------------------
 */

// Son métodos de FilePage porque ShareRow es un tipo
// privado de la página.
Glib::RefPtr<Gtk::SignalListItemFactory>
FilePage::make_text_factory(
    const std::function<Glib::ustring(const ShareRow&)>& getter)
{
    auto factory = Gtk::SignalListItemFactory::create();

    factory->signal_setup().connect(
        [](const Glib::RefPtr<Glib::ObjectBase>& item) {
            auto list_item =
                std::dynamic_pointer_cast<Gtk::ListItem>(item);

            if (!list_item)
                return;

            auto* label = Gtk::make_managed<Gtk::Label>();
            label->set_halign(Gtk::Align::START);
            label->set_ellipsize(Pango::EllipsizeMode::END);
            list_item->set_child(*label);
        });

    factory->signal_bind().connect(
        [getter](const Glib::RefPtr<Glib::ObjectBase>& item) {
            auto list_item =
                std::dynamic_pointer_cast<Gtk::ListItem>(item);

            if (!list_item)
                return;

            auto row =
                std::dynamic_pointer_cast<ShareRow>(
                    list_item->get_item());

            auto* label =
                dynamic_cast<Gtk::Label*>(list_item->get_child());

            if (label && row)
                label->set_text(getter(*row));
        });

    return factory;
}


Glib::RefPtr<Gtk::SignalListItemFactory>
FilePage::make_state_factory()
{
    auto factory = Gtk::SignalListItemFactory::create();

    factory->signal_setup().connect(
        [](const Glib::RefPtr<Glib::ObjectBase>& item) {
            auto list_item =
                std::dynamic_pointer_cast<Gtk::ListItem>(item);

            if (!list_item)
                return;

            auto* label = Gtk::make_managed<Gtk::Label>();
            label->set_halign(Gtk::Align::START);
            list_item->set_child(*label);
        });

    factory->signal_bind().connect(
        [](const Glib::RefPtr<Glib::ObjectBase>& item) {
            auto list_item =
                std::dynamic_pointer_cast<Gtk::ListItem>(item);

            if (!list_item)
                return;

            auto row =
                std::dynamic_pointer_cast<ShareRow>(
                    list_item->get_item());

            auto* label =
                dynamic_cast<Gtk::Label*>(list_item->get_child());

            if (!label || !row)
                return;

            std::error_code error;

            if (fs::is_directory(row->folder, error))
            {
                label->set_markup(
                    "<span foreground='green' "
                    "weight='bold'>✓ OK</span>");
            }
            else
            {
                label->set_markup(
                    "<span foreground='orange' "
                    "weight='bold'>[FALTA]</span>");
            }
        });

    return factory;
}


/*
 * ------------------------------------------------------------
 * PÁGINA FILE
 * ------------------------------------------------------------
 */

FilePage::FilePage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10)
{
    set_margin_start(16);
    set_margin_end(16);
    set_margin_top(16);
    set_margin_bottom(16);
    set_hexpand(true);
    set_vexpand(true);

    store_ = Gio::ListStore<ShareRow>::create();
    selection_ = Gtk::SingleSelection::create(store_);
    column_view_.set_model(selection_);
    column_view_.set_show_column_separators(true);

    column_view_.append_column(
        Gtk::ColumnViewColumn::create(
            "Folder",
            make_text_factory(
                [](const ShareRow& row) {
                    return Glib::ustring(row.folder);
                })));

    column_view_.append_column(
        Gtk::ColumnViewColumn::create(
            "Share",
            make_text_factory(
                [](const ShareRow& row) {
                    return Glib::ustring(
                        share_display_name(row.folder, row.name));
                })));

    column_view_.append_column(
        Gtk::ColumnViewColumn::create(
            "Permissions",
            make_text_factory(
                [](const ShareRow& row) {
                    return Glib::ustring(
                        row.read_only
                            ? "Read only"
                            : "Read / Write");
                })));

    column_view_.append_column(
        Gtk::ColumnViewColumn::create(
            "Visible",
            make_text_factory(
                [](const ShareRow& row) {
                    return Glib::ustring(row.visible ? "Yes" : "No");
                })));

    column_view_.append_column(
        Gtk::ColumnViewColumn::create(
            "State",
            make_state_factory()));

    auto* title = Gtk::make_managed<Gtk::Label>(
        "FILE — Shared folders");
    title->set_halign(Gtk::Align::START);
    append(*title);

    auto* note = Gtk::make_managed<Gtk::Label>(
        "Choose the folders this machine shares. A folder "
        "marked [FALTA] no longer exists: it is left out "
        "when configuring, and everything else keeps working.");
    note->set_wrap(true);
    note->set_halign(Gtk::Align::START);
    append(*note);

    auto* scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(
        Gtk::PolicyType::AUTOMATIC,
        Gtk::PolicyType::AUTOMATIC);
    scrolled->set_vexpand(true);
    scrolled->set_child(column_view_);
    append(*scrolled);

    auto* editors = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    auto* name_label = Gtk::make_managed<Gtk::Label>(
        "Share name:");
    editors->append(*name_label);

    name_entry_.set_hexpand(true);
    editors->append(name_entry_);
    editors->append(readonly_check_);
    editors->append(visible_check_);
    append(*editors);

    auto* buttons = Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    buttons->append(btn_add_folder_);

    auto* new_label = Gtk::make_managed<Gtk::Label>(
        "New folder:");
    buttons->append(*new_label);

    new_folder_entry_.set_placeholder_text(
        "Name or path — created inside your home if relative");
    new_folder_entry_.set_hexpand(true);
    buttons->append(new_folder_entry_);
    buttons->append(btn_create_);
    buttons->append(btn_remove_);
    buttons->append(btn_refresh_);
    append(*buttons);

    btn_add_folder_.signal_clicked().connect(
        sigc::mem_fun(*this, &FilePage::on_add_folder));
    btn_create_.signal_clicked().connect(
        sigc::mem_fun(*this, &FilePage::on_create_folder));
    btn_remove_.signal_clicked().connect(
        sigc::mem_fun(*this, &FilePage::on_remove));
    btn_refresh_.signal_clicked().connect(
        [this]() {
            refresh_all_rows();
            signal_refresh_requested.emit();
        });

    name_entry_.signal_changed().connect(
        sigc::mem_fun(*this, &FilePage::on_name_changed));
    readonly_check_.signal_toggled().connect(
        sigc::mem_fun(*this, &FilePage::on_readonly_toggled));
    visible_check_.signal_toggled().connect(
        sigc::mem_fun(*this, &FilePage::on_visible_toggled));

    selection_->signal_selection_changed().connect(
        [this](guint, guint) { sync_editors(); });

    load_shares();
    sync_editors();
}


/*
 * ------------------------------------------------------------
 * PERSISTENCIA
 * ------------------------------------------------------------
 */

void FilePage::load_shares()
{
    std::ifstream input(config_file_path());
    std::string line;

    while (input && std::getline(input, line))
    {
        const std::string clean = trim(line);

        if (clean.empty() || clean[0] == '#')
            continue;

        if (clean.rfind("share=", 0) != 0)
            continue;

        const std::string rest = clean.substr(6);

        // Campos: nombre|ruta|rw|visible — la ruta es el
        // segundo campo y puede contener '|' en teoría; se
        // parte por el primer y los dos últimos separadores.
        const auto first = rest.find('|');
        const auto last = rest.rfind('|');

        if (first == std::string::npos ||
            last == std::string::npos || last == first)
            continue;

        const auto middle = rest.rfind('|', last - 1);

        if (middle == std::string::npos || middle == first)
            continue;

        const std::string name =
            trim(rest.substr(0, first));
        const std::string folder =
            trim(rest.substr(first + 1, middle - first - 1));
        const std::string perms =
            trim(rest.substr(middle + 1, last - middle - 1));
        const std::string visible =
            trim(rest.substr(last + 1));

        if (folder.empty())
            continue;

        store_->append(
            ShareRow::create(
                folder,
                name,
                perms == "ro",
                visible != "0"));
    }

    if (store_->get_n_items() == 0)
    {
        // Compatibilidad: sin lista guardada, la carpeta
        // compartida es la de siempre, ~/Red.
        const std::string home = Glib::get_home_dir();

        store_->append(
            ShareRow::create(
                home + "/Red",
                "Red",
                false,
                true));
    }
}


void FilePage::save_shares() const
{
    const fs::path config_path = config_file_path();

    std::error_code error;
    fs::create_directories(config_path.parent_path(), error);

    // Se conservan las demás claves (windows_ip y lo que
    // venga); solo se reescriben las líneas share=.
    std::vector<std::string> kept_lines;

    {
        std::ifstream input(config_path);
        std::string line;

        while (input && std::getline(input, line))
        {
            if (trim(line).rfind("share=", 0) == 0)
                continue;

            kept_lines.push_back(line);
        }
    }

    const fs::path temp_path =
        fs::path(config_path.string() + ".tmp");

    {
        std::ofstream output(temp_path);

        if (!output)
            return;

        for (const auto& line : kept_lines)
            output << line << '\n';

        for (guint i = 0; i < store_->get_n_items(); ++i)
        {
            auto row = store_->get_item(i);

            output
                << "share="
                << share_display_name(row->folder, row->name)
                << '|' << row->folder
                << '|' << (row->read_only ? "ro" : "rw")
                << '|' << (row->visible ? '1' : '0')
                << '\n';
        }
    }

    fs::rename(temp_path, config_path, error);
}


/*
 * ------------------------------------------------------------
 * CONSULTAS (para CONFIGURAR, rebanada siguiente)
 * ------------------------------------------------------------
 */

std::vector<FilePage::ShareDef> FilePage::shares() const
{
    std::vector<ShareDef> result;

    for (guint i = 0; i < store_->get_n_items(); ++i)
    {
        auto row = store_->get_item(i);

        result.push_back(
            {row->folder,
             share_display_name(row->folder, row->name),
             row->read_only,
             row->visible});
    }

    return result;
}


std::vector<FilePage::ShareDef> FilePage::active_shares() const
{
    std::vector<ShareDef> result;

    for (const auto& share : shares())
    {
        std::error_code error;

        if (fs::is_directory(share.folder, error))
            result.push_back(share);
    }

    return result;
}


/*
 * ------------------------------------------------------------
 * EDICIÓN
 * ------------------------------------------------------------
 */

void FilePage::add_folder(const std::string& path)
{
    if (path.empty())
        return;

    for (guint i = 0; i < store_->get_n_items(); ++i)
    {
        if (store_->get_item(i)->folder == path)
        {
            selection_->set_selected(i);
            return;
        }
    }

    store_->append(
        ShareRow::create(
            path,
            fs::path(path).filename().string(),
            false,
            true));

    selection_->set_selected(store_->get_n_items() - 1);
    save_shares();
}


Glib::RefPtr<FilePage::ShareRow> FilePage::selected_row() const
{
    auto item = selection_->get_selected_item();

    return std::dynamic_pointer_cast<ShareRow>(item);
}


void FilePage::sync_editors()
{
    auto row = selected_row();

    syncing_ = true;

    name_entry_.set_text(row ? row->name : "");
    name_entry_.set_sensitive(static_cast<bool>(row));
    readonly_check_.set_active(row && row->read_only);
    readonly_check_.set_sensitive(static_cast<bool>(row));
    visible_check_.set_active(row && row->visible);
    visible_check_.set_sensitive(static_cast<bool>(row));
    btn_remove_.set_sensitive(static_cast<bool>(row));

    syncing_ = false;
}


void FilePage::touch_row(const Glib::RefPtr<ShareRow>& row)
{
    // Gio::ListStore no avisa de cambios internos del
    // objeto: se reinsertar la fila para que la vista
    // vuelva a leerla.
    for (guint i = 0; i < store_->get_n_items(); ++i)
    {
        if (store_->get_item(i) == row)
        {
            store_->splice(i, 1, {row});
            return;
        }
    }
}


void FilePage::refresh_all_rows()
{
    /*
     * REFRESH de verdad: relee el config y reconstruye
     * las filas con objetos nuevos. La versión anterior
     * reinsertaba LOS MISMOS objetos con splice() y la
     * vista reutilizaba los widgets de cada posición
     * (el ítem no cambiaba): el bind —que es quien
     * comprueba si la carpeta existe— nunca volvía a
     * correr, y el estado solo se corregía cerrando y
     * reabriendo la app. Visto en campo y reproducido
     * en laboratorio el 2026-11-02.
     */
    std::string selected_folder;

    if (auto row = selected_row())
        selected_folder = row->folder;

    store_->remove_all();
    load_shares();

    if (!selected_folder.empty())
    {
        for (guint i = 0; i < store_->get_n_items(); ++i)
        {
            auto item = store_->get_item(i);

            if (item && item->folder == selected_folder)
            {
                selection_->set_selected(i);
                break;
            }
        }
    }

    sync_editors();
}


void FilePage::on_name_changed()
{
    if (syncing_)
        return;

    auto row = selected_row();

    if (!row)
        return;

    row->name = trim(name_entry_.get_text().raw());
    touch_row(row);
    save_shares();
}


void FilePage::on_readonly_toggled()
{
    if (syncing_)
        return;

    auto row = selected_row();

    if (!row)
        return;

    row->read_only = readonly_check_.get_active();
    touch_row(row);
    save_shares();
}


void FilePage::on_visible_toggled()
{
    if (syncing_)
        return;

    auto row = selected_row();

    if (!row)
        return;

    row->visible = visible_check_.get_active();
    touch_row(row);
    save_shares();
}


void FilePage::on_remove()
{
    auto row = selected_row();

    if (!row)
        return;

    for (guint i = 0; i < store_->get_n_items(); ++i)
    {
        if (store_->get_item(i) == row)
        {
            store_->remove(i);
            break;
        }
    }

    save_shares();
    sync_editors();
}


void FilePage::on_create_folder()
{
    const std::string text =
        trim(new_folder_entry_.get_text().raw());

    if (text.empty())
        return;

    fs::path path(text);

    if (path.is_relative())
        path = fs::path(Glib::get_home_dir()) / path;

    std::error_code error;
    fs::create_directories(path, error);

    if (error)
        return;

    new_folder_entry_.set_text("");
    add_folder(path.string());
}


void FilePage::on_add_folder()
{
    auto* root = get_root();
    auto* window = dynamic_cast<Gtk::Window*>(root);

    if (!window)
        return;

    folder_dialog_ = Gtk::FileChooserNative::create(
        "Select a folder to share",
        *window,
        Gtk::FileChooser::Action::SELECT_FOLDER);

    folder_dialog_->signal_response().connect(
        [this](int response) {
            if (response == Gtk::ResponseType::ACCEPT)
            {
                auto file = folder_dialog_->get_file();

                if (file)
                    add_folder(file->get_path());
            }

            // Cancelado o aceptado: el diálogo ya cumplió.
            folder_dialog_.reset();
        });

    folder_dialog_->show();
}
