Thunar User Customizable Actions (thunar-uca)
=============================================

Thunar-uca is an extension to Thunar, which enables sophisticated users to add additional actions to the file managers context menus, without having to write their own extensions for each and every action they want to use.

By default the thunar-uca extension will be installed, but as with every extension, it will slightly increase the resource usage of Thunar (this shouldn't be a real problem unless you're targeting an embedded system), and so can you pass `--disable-uca-plugin` to configure and the plugin won't be built and installed. Since it is an extension, you can also easily uninstall it
afterwards by removing the thunar-uca.so file from the lib/thunarx-2/ directory of your installation (be sure to quit Thunar before removing files though).

To actually manage the actions open the "Edit" menu in the menu bar of a Thunar window and select "Configure Custom Actions...". A dialog will appear which lists the currently configured actions.

List of valid command parameter variables
=========================================

|   Variable | Usage |
| :------------ | :------------ |
| %f | The path of the first selected file. |
| %F | The paths to all selected files. |
|%u |The URI of the first selected file (using the file:-URI scheme). |
|%U |The URIs of all selected files (using the file:-URI scheme). |
| %d |The directory of the first selected file. |
| %D |The directories to all selected files. |
| %n |The name of the first selected file (without the path). |
|%N |The names of all selected files (without the paths). |