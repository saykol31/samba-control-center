#pragma once

#include <gtkmm.h>
#include <string>

class SharedStatus : public Gtk::Box {
public:
    SharedStatus();

    Gtk::Label& samba() { return samba_status_; }
    Gtk::Label& tcpdump() { return tcpdump_status_; }
    Gtk::Label& folder() { return folder_status_; }
    Gtk::Label& smbd() { return smbd_status_; }
    Gtk::Label& nmbd() { return nmbd_status_; }
    Gtk::Label& firewall() { return firewall_status_; }
    Gtk::Label& ports() { return ports_status_; }
    Gtk::Label& network() { return network_status_; }

private:
    Gtk::Label overall_dot_;
    Gtk::Label overall_text_;

    Gtk::Label samba_status_, tcpdump_status_, folder_status_;
    Gtk::Label smbd_status_, nmbd_status_, firewall_status_;
    Gtk::Label ports_status_, network_status_;

    void add_row(Gtk::Box& column, const char* name,
                 Gtk::Label& value, int row_number);
    void refresh_overall_status();
    static std::string normalized_status(const Gtk::Label& label);
};
