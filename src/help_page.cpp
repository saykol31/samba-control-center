#include "help_page.hpp"

#include <string>
#include <utility>
#include <vector>

/*
 * ================================================================
 * HELP — índice de preguntas y respuestas
 * ================================================================
 *
 * El contenido vive aquí, en una tabla: pregunta y su
 * respuesta. Para corregir un texto se toca solo la tabla.
 */

namespace {

const std::vector<std::pair<std::string, std::string>>&
faq_entries()
{
    static const std::vector<std::pair<std::string, std::string>>
        entries = {
    {
        "What is this application for?",
        "Samba Control Center sets up and manages Samba on "
        "this computer, so folders can be shared with Windows "
        "computers on your local network — and so this "
        "computer can open folders shared by Windows, too.\n"
        "Everything the program does could also be done by "
        "hand with commands; the program does it with "
        "buttons, explains each step in the CONTROL log, and "
        "keeps the system files valid while doing it."
    },
    {
        "What needs to be installed first?",
        "The program drives a handful of system tools "
        "(Samba itself, a network scanner, a firewall "
        "manager and others). Open SETTINGS and look at the "
        "DEPENDENCIES table: every tool is listed with its "
        "state.\n"
        "If one is missing, the table offers to install it. "
        "All the installs travel together and ask for your "
        "administrator password once."
    },
    {
        "How do I share a folder for the first time?",
        "Four steps, in this order:\n"
        "1. FILE: press ADD FOLDER and pick the folder (or "
        "CREATE to make a new one).\n"
        "2. USER, SERVER column: make sure your user has a "
        "Samba password (ADD or SET PASSWORD).\n"
        "3. CONTROL: press CONFIGURE SAMBA and enter the "
        "administrator password when asked.\n"
        "4. From Windows, open \\\\COMPUTER\\ShareName in "
        "Explorer (this computer's name or IP, and the "
        "share's name as shown in FILE).\n"
        "HOME shows the state of services at all times, so "
        "you can check the result there."
    },
    {
        "Can I share more than one folder?",
        "Yes. The FILE tab is a list: add every folder you "
        "want to share, and CONFIGURE SAMBA writes one "
        "section per folder into the Samba configuration. "
        "Each folder keeps its own settings (read only, "
        "visible)."
    },
    {
        "How do I make a folder read only?",
        "In FILE, click the folder in the list and tick "
        "READ ONLY in the editor below. Then run CONFIGURE "
        "SAMBA in CONTROL so the change reaches the server.\n"
        "A read only share can be opened and copied from, "
        "but nobody can change or delete its contents over "
        "the network."
    },
    {
        "What is the workgroup, and how do I change it?",
        "The workgroup is the name of the little club your "
        "computers belong to: Windows shows network "
        "computers grouped by it, so the Linux and Windows "
        "machines should share the same workgroup name.\n"
        "Set it in SETTINGS (WORKGROUP field) and press "
        "SAVE VALUES — then run CONFIGURE SAMBA in CONTROL "
        "to apply it. SETTINGS only saves values; CONTROL "
        "is what applies them. Without a saved value, the "
        "classic WORKGROUP name is used."
    },
    {
        "How do I create a Samba user and set its password?",
        "Go to USER, SERVER column. The list shows the "
        "existing Samba users.\n"
        "ADD: creates a Samba user for a Linux user that "
        "already exists on this machine, and asks for its "
        "Samba password. The Samba password is separate "
        "from the Linux login password — it is the one "
        "Windows will ask for.\n"
        "SET PASSWORD, ENABLE, DISABLE and DELETE work on "
        "the selected user. Every action asks for the "
        "administrator password once and then really "
        "happens — there is no separate save step."
    },
    {
        "How do I start, restart or stop Samba?",
        "From the HOME tab, with the three power buttons at "
        "the bottom: BOOT starts the services, RESET "
        "restarts them, SHUTDOWN stops them. The lamps at "
        "the top show whether smbd and nmbd are running.\n"
        "Note that CONFIGURE SAMBA already restarts the "
        "services as part of its work, so after configuring "
        "you do not need RESET."
    },
    {
        "Windows does not see my folders. What do I check?",
        "In this order:\n"
        "1. HOME: smbd and nmbd lamps are running. If not, "
        "press BOOT.\n"
        "2. You ran CONFIGURE SAMBA after your last change "
        "in FILE or SETTINGS — nothing applies itself.\n"
        "3. CONTROL, FIREWALL: the rule allows Samba. The "
        "button also opens SSH, and turns the firewall on.\n"
        "4. Both computers are in the same workgroup (see "
        "the workgroup question), and the folder still "
        "exists on disk — a missing folder is skipped by "
        "design (see the question about FILE below).\n"
        "5. The user and password you type in Windows exist "
        "in USER, SERVER column, and the account is not "
        "disabled.\n"
        "CONTROL also has network tests (PING WINDOWS, TCP "
        "445, SHOW PORTS) to see whether the Windows "
        "computer answers at all."
    },
    {
        "How do I open a Windows shared folder from Linux?",
        "Go to USER, CLIENT column (the right one):\n"
        "1. SCAN finds the computers on your networks.\n"
        "2. Click one and press LIST SHARES; enter the "
        "Windows user and password when asked.\n"
        "3. Pick a share and press CONNECT: it opens in "
        "your file manager. DISCONNECT releases it.\n"
        "Connections can be saved so the server, share and "
        "user are remembered for next time — the password "
        "is never saved."
    },
    {
        "Does the application save my passwords?",
        "No. No password is ever written to the "
        "program's configuration file.\n"
        "Samba passwords live inside Samba's own user "
        "database, managed in the USER tab. Windows "
        "passwords are typed when you connect and forgotten "
        "when the connection closes. The configuration file "
        "(~/.config/samba-control-center/config) holds only "
        "folders, the workgroup, the Windows IP and saved "
        "connections."
    },
    {
        "What exactly does CONFIGURE SAMBA do? What if it "
        "goes wrong?",
        "It builds a new /etc/samba/smb.conf from your FILE "
        "folders and your SETTINGS values, and then, in one "
        "administrator authorization:\n"
        "1. Validates the new file with testparm. If it is "
        "not valid, nothing is touched.\n"
        "2. Copies the current smb.conf to a dated backup "
        "(smb.conf.backup-YYYYMMDD-HHMMSS, in /etc/samba).\n"
        "3. Installs the new file, restarts the services "
        "and applies the firewall rule.\n"
        "If something goes wrong after a change, the way "
        "back is to copy one of those backups over "
        "/etc/samba/smb.conf (as administrator) and restart "
        "the services from HOME."
    },
    {
        "Why is a folder I added in FILE not being shared?",
        "Because sharing only happens for folders that "
        "exist. If a folder in the FILE list was deleted or "
        "its disk is not mounted, FILE marks it and "
        "CONFIGURE SAMBA leaves it out of smb.conf — on "
        "purpose, so one missing folder cannot break the "
        "rest.\n"
        "Fix the path or create the folder from FILE, then "
        "run CONFIGURE SAMBA again."
    },
    {
        "Do I need an internet connection?",
        "No. Everything in this program happens on your "
        "local network, between your own computers.\n"
        "The only downloads that can ever occur are the "
        "system packages, if you choose to install a "
        "missing dependency from SETTINGS."
    },
        };

    return entries;
}

} // namespace


HelpPage::HelpPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    set_margin(10);

    paned_.set_hexpand(true);
    paned_.set_vexpand(true);
    paned_.set_position(330);

    // Índice: una fila por pregunta.
    index_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    index_.signal_row_selected().connect(
        sigc::mem_fun(*this, &HelpPage::on_question_selected));

    for (const auto& entry : faq_entries())
    {
        auto* label = Gtk::make_managed<Gtk::Label>(entry.first);
        label->set_halign(Gtk::Align::START);
        label->set_wrap(true);
        label->set_margin(6);

        index_.append(*label);
    }

    index_scroll_.set_child(index_);
    index_scroll_.set_policy(
        Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    paned_.set_start_child(index_scroll_);

    // Texto completo.
    buffer_ = Gtk::TextBuffer::create();
    text_view_.set_buffer(buffer_);
    text_view_.set_editable(false);
    text_view_.set_cursor_visible(false);
    text_view_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    text_view_.set_left_margin(12);
    text_view_.set_right_margin(12);
    text_view_.set_top_margin(8);

    build_text();

    text_scroll_.set_child(text_view_);
    text_scroll_.set_policy(
        Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    paned_.set_end_child(text_scroll_);

    append(paned_);
}


void HelpPage::build_text()
{
    auto heading_tag = buffer_->create_tag("help_heading");
    heading_tag->property_weight().set_value(700);
    heading_tag->property_scale().set_value(1.15);

    auto end = buffer_->end();

    buffer_->insert(
        end,
        "Samba Control Center — Help\n\n"
        "Pick a question on the left and the text jumps to "
        "its answer. The questions are the ones a new user "
        "asks, in the order they usually come up.\n\n");

    int number = 0;

    for (const auto& entry : faq_entries())
    {
        ++number;

        auto mark = buffer_->create_mark(
            "help_q" + std::to_string(number),
            buffer_->end(),
            false);

        marks_.push_back(mark);

        end = buffer_->end();

        buffer_->insert_with_tag(
            end,
            entry.first + "\n",
            heading_tag);

        end = buffer_->end();

        buffer_->insert(end, entry.second + "\n\n");
    }
}


void HelpPage::on_question_selected(Gtk::ListBoxRow* row)
{
    if (!row)
        return;

    const int index = row->get_index();

    if (index < 0 ||
        index >= static_cast<int>(marks_.size()))
        return;

    auto iter = buffer_->get_iter_at_mark(marks_[index]);

    // Llevar el cursor a la respuesta: la vista se
    // desplaza hasta él, y la selección subraya el
    // encabezado de la respuesta.
    auto line_end = iter;
    line_end.forward_to_line_end();

    buffer_->place_cursor(iter);
    buffer_->select_range(iter, line_end);

    // Ajuste fino: el encabezado cerca del borde
    // superior, como en la ayuda clásica.
    Gdk::Rectangle location;
    text_view_.get_iter_location(iter, location);

    auto adjustment = text_scroll_.get_vadjustment();

    if (adjustment)
    {
        double target = location.get_y() - 12.0;

        const double max_value =
            adjustment->get_upper() -
            adjustment->get_page_size();

        if (target < 0.0)
            target = 0.0;

        if (target > max_value)
            target = max_value;

        adjustment->set_value(target);
    }

    text_view_.grab_focus();
}
