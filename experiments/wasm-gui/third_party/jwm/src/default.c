/**
 * @file default.c
 * @author Joe Wingbermuehle
 * @date 2017
 *
 * @brief Default configuration.
 *
 */

#include "default.h"

const char * const BASE_CONFIG =
   "<JWM>"
      "<Key key=\"Up\">up</Key>"
      "<Key key=\"Down\">down</Key>"
      "<Key key=\"Right\">right</Key>"
      "<Key key=\"Left\">left</Key>"
      "<Key key=\"Return\">select</Key>"
      "<Key key=\"Escape\">escape</Key>"
      "<Mouse context=\"title\" button=\"1\">move</Mouse>"
      "<Mouse context=\"title\" button=\"2\">move</Mouse>"
      "<Mouse context=\"title\" button=\"3\">window</Mouse>"
      "<Mouse context=\"title\" button=\"11\">maximize</Mouse>"
      "<Mouse context=\"icon\" button=\"1\">window</Mouse>"
      "<Mouse context=\"icon\" button=\"2\">move</Mouse>"
      "<Mouse context=\"icon\" button=\"3\">window</Mouse>"
      "<Mouse context=\"border\" button=\"1\">resize</Mouse>"
      "<Mouse context=\"border\" button=\"2\">move</Mouse>"
      "<Mouse context=\"border\" button=\"3\">window</Mouse>"
      "<Mouse context=\"close\" button=\"-1\">close</Mouse>"
      "<Mouse context=\"close\" button=\"2\">move</Mouse>"
      "<Mouse context=\"close\" button=\"-3\">close</Mouse>"
      "<Mouse context=\"maximize\" button=\"-1\">maximize</Mouse>"
      "<Mouse context=\"maximize\" button=\"-2\">maxv</Mouse>"
      "<Mouse context=\"maximize\" button=\"-3\">maxh</Mouse>"
      "<Mouse context=\"minimize\" button=\"-1\">minimize</Mouse>"
      "<Mouse context=\"minimize\" button=\"2\">move</Mouse>"
      "<Mouse context=\"minimize\" button=\"-3\">shade</Mouse>"
   "</JWM>"
;

const char * const DEFAULT_CONFIG =
   "<JWM>"
      "<RootMenu onroot=\"1\">"
         "<Program>xterm</Program>"
         "<Restart/>"
         "<Exit/>"
      "</RootMenu>"
      "<Tray x=\"0\" y=\"-1\">"
         "<Pager/><TaskList/><Dock/><Clock/>"
      "</Tray>"
   "</JWM>"
;
