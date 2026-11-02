#pragma once

#include <gtkmm.h>
#include <vector>

/*
 * HELP: el manual dentro de la aplicación (modelo de la
 * ayuda de Windows XP que pidió Esequiel, 2026-11-01):
 * un índice de preguntas frecuentes a la izquierda y el
 * texto completo a la derecha. Tocar una pregunta lleva
 * el texto hasta su párrafo de respuesta. Sin internet,
 * sin salir del programa.
 */
class HelpPage : public Gtk::Box
{
public:
    HelpPage();

private:
    Gtk::Paned paned_{Gtk::Orientation::HORIZONTAL};

    Gtk::ScrolledWindow index_scroll_;
    Gtk::ListBox index_;

    Gtk::ScrolledWindow text_scroll_;
    Gtk::TextView text_view_;
    Glib::RefPtr<Gtk::TextBuffer> buffer_;

    // Una marca por respuesta, en el orden del índice.
    std::vector<Glib::RefPtr<Gtk::TextBuffer::Mark>> marks_;

    void build_text();
    void on_question_selected(Gtk::ListBoxRow* row);
};
