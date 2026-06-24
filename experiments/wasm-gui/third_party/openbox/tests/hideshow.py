#!/usr/bin/env python

import pygtk
import gtk
import gobject
pygtk.require('2.0')

class FolderSelector(gtk.Window):
    def __init__(self, jules):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        print "init folder selector", self, jules
        self.set_title("Select Folder")
        self.jules = jules

        self.set_size_request(140, 200)

        self.list_model = gtk.ListStore(gobject.TYPE_STRING)
        self.tree = gtk.TreeView(self.list_model)
        self.folder_column = gtk.TreeViewColumn('Folder')
        self.tree.append_column(self.folder_column)

        self.folder_cell = gtk.CellRendererText()
        self.folder_column.pack_start(self.folder_cell, True)
        self.folder_column.add_attribute(self.folder_cell, 'text', 0)

        self.tree.set_search_column(0)

        self.icon_theme = gtk.icon_theme_get_default()

        self.add(self.tree)
        self.show_all()
        self.tree.columns_autosize()
        print "done init"

class Jules(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.set_title("Jules")
        self.set_size_request(150, 320)
        self.connect("delete_event", self.on_delete_event)
        self.connect("destroy", self.on_destroy)
        self.scroll = gtk.ScrolledWindow()

        self.tree_model = gtk.TreeStore(gobject.TYPE_STRING,
                                        gobject.TYPE_STRING)
        self.tree = gtk.TreeView(self.tree_model)
        self.file_column = gtk.TreeViewColumn('name', gtk.CellRendererText(),
                                              markup=0)
        self.file_column.set_sort_indicator(True)
        self.file_column.set_clickable(True)
        self.file_column.set_sort_column_id(1)
        self.tree.append_column(self.file_column)
        self.tree.set_headers_clickable(True)
        self.tree.set_search_column(0)

        self.scroll.add(self.tree)
        self.add(self.scroll)
        self.show_all()

        self.project_selector = FolderSelector(self)
        self.project_selector.hide()
        self.project_selector.hide()

        self.project_selector.show()

    def on_delete_event(self, widget, event):
        return False

    def on_destroy(self, widget):
        gtk.main_quit()

    def run(self):
        gtk.main()


if __name__ == "__main__":
    jules = Jules()
    jules.run()
