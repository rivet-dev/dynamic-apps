#include "all.h"

void action_all_startup(void)
{
    action_execute_startup();
    action_debug_startup();
    action_showmenu_startup();
    action_showdesktop_startup();
    action_reconfigure_startup();
    action_exit_startup();
    action_restart_startup();
    action_cyclewindows_startup();
    action_breakchroot_startup();
    action_close_startup();
    action_move_startup();
    action_focus_startup();
    action_raise_startup();
    action_lower_startup();
    action_raiselower_startup();
    action_unfocus_startup();
    action_iconify_startup();
    action_fullscreen_startup();
    action_maximize_startup();
    action_moveresizeto_startup();
    action_moverelative_startup();
    action_shade_startup();
    action_kill_startup();
    action_omnipresent_startup();
    action_directionalwindows_startup();
    action_resize_startup();
    action_decorations_startup();
    action_desktop_startup();
    action_dock_startup();
    action_resizerelative_startup();
    action_addremovedesktop_startup();
    action_dockautohide_startup();
    action_layer_startup();
    action_movetoedge_startup();
    action_growtoedge_startup();
    action_if_startup();
    action_focustobottom_startup();
    /* 3.4-compatibility */
    action_shadelowerraise_startup();
}
