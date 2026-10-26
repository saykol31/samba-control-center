#pragma once

#include <gtkmm.h>
#include "shared_status.hpp"
class HomePage : public Gtk::Box {
public:
    explicit HomePage(SharedStatus& shared_status);

private:
    SharedStatus& shared_status_;
};
