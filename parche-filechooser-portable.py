#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Parche: portabilidad gtkmm — FileDialog -> FileChooserNative.

Hallazgo (compilación en la MX, 2026-11-01): Gtk::FileDialog
nació en gtkmm 4.10 y la MX trae gtkmm 4.8 (Debian 12), así
que la página FILE no compilaba allí. En la Surface (4.18)
ni se notaba.

Gtk::FileChooserNative existe desde gtkmm 4.0: el mismo
código compila en 4.8, 4.10 y 4.18 sin #ifdefs. El
comportamiento no cambia: diálogo nativo para elegir
carpeta, ACCEPT añade la carpeta, cancelar no hace nada.

Verificado por Muse: compila limpio con gtkmm 4.10.

2 archivos, 1 par cada uno, verificados antes de escribir.
Respaldo .bak-<fecha-hora>. Si un par no coincide, no
escribe nada.

Uso:  python3 parche-filechooser-portable.py  (raíz del repo)
Luego: cmake --build build --parallel 2
"""

import os
import sys
from datetime import datetime

ROOT = os.path.dirname(os.path.abspath(__file__))
STAMP = datetime.now().strftime("%Y%m%d-%H%M%S")

FILES = {'include/file_page.hpp': [('    Glib::RefPtr<Gtk::FileDialog> folder_dialog_;', '    // FileChooserNative (gtkmm 4.0+) en vez de FileDialog\n    // (gtkmm 4.10+): el mismo código compila en la MX\n    // (gtkmm 4.8, Debian 12) y en la Surface (4.18).\n    Glib::RefPtr<Gtk::FileChooserNative> folder_dialog_;')], 'src/file_page.cpp': [('    folder_dialog_ = Gtk::FileDialog::create();\n    folder_dialog_->set_title("Select a folder to share");\n\n    folder_dialog_->select_folder(\n        *window,\n        [this](Glib::RefPtr<Gio::AsyncResult>& result) {\n            try\n            {\n                auto file =\n                    folder_dialog_->select_folder_finish(result);\n\n                if (file)\n                    add_folder(file->get_path());\n            }\n            catch (const Glib::Error&)\n            {\n                // Cancelado por el usuario: nada que hacer.\n            }\n\n            folder_dialog_.reset();\n        });\n}', '    folder_dialog_ = Gtk::FileChooserNative::create(\n        "Select a folder to share",\n        *window,\n        Gtk::FileChooser::Action::SELECT_FOLDER);\n\n    folder_dialog_->signal_response().connect(\n        [this](int response) {\n            if (response == Gtk::ResponseType::ACCEPT)\n            {\n                auto file = folder_dialog_->get_file();\n\n                if (file)\n                    add_folder(file->get_path());\n            }\n\n            // Cancelado o aceptado: el diálogo ya cumplió.\n            folder_dialog_.reset();\n        });\n\n    folder_dialog_->show();\n}')]}

def fail(message):
    print("ERROR: " + message)
    print("No se escribió nada.")
    sys.exit(1)


def main():
    contents = {}

    for relpath, pairs in FILES.items():
        path = os.path.join(ROOT, relpath)
        if not os.path.exists(path):
            fail("no se encuentra " + relpath)
        with open(path, encoding="utf-8") as handle:
            contents[relpath] = handle.read()
        for index, (old, _new) in enumerate(pairs, 1):
            count = contents[relpath].count(old)
            if count != 1:
                fail("%s par %d: %d apariciones (se esperaba 1)"
                     % (relpath, index, count))

    for relpath, pairs in FILES.items():
        path = os.path.join(ROOT, relpath)
        updated = contents[relpath]
        for old, new in pairs:
            updated = updated.replace(old, new, 1)
        backup = path + ".bak-" + STAMP
        with open(backup, "w", encoding="utf-8") as handle:
            handle.write(contents[relpath])
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(updated)
        print("corregido: %s (respaldo %s)"
              % (relpath, os.path.basename(backup)))

    print()
    print("Compilar:  cmake --build build --parallel 2")


if __name__ == "__main__":
    main()
