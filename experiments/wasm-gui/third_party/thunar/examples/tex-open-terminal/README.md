This is an example for a Thunar Extension that provides additional context menu items.

It adds an "Open Terminal Here" action to the folder context menu and to the file context menu of directories, and thereby allows you to open a new Terminal instance in the given folder.

The example is mainly provided for developers to get an idea about how to write an extension that implements the ThunarxMenuProvider interface.

The extension is not installed by default, as every installed extension increases the resources required to run Thunar. If you want to install this extension, you can use the command `make install-extensionsLTLIBRARIES` in this directory. If you want to uninstall it, you can use the command `make uninstall-extensionsLTLIBRARIES` in this directory.

Please note that this extension is not meant for daily use, but should just serve as an example of how to write a simple Thunar extension. If you want to be able to open a terminal in a specific folder, you should consider installing the thunar-uca extension (see thunar-uca/README.md) and adding an "Open Terminal Here" action there (there's already such an action available by default).
