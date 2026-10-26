#include <gtkmm.h>
#include "main_window.hpp"

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(
        "com.saykol.sambacontrolcenter.v2");

    return app->make_window_and_run<MainWindow>(argc, argv);
}
