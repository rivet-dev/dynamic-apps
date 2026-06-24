
/* Generated data (by glib-mkenums) */

#include <glib-object.h>
#include "netstatus-enums.h"



/* enumerations from "./netstatus-util.h" */
#include "./netstatus-util.h"

static const GEnumValue _netstatus_state_values[] = {
  { NETSTATUS_STATE_DISCONNECTED, "NETSTATUS_STATE_DISCONNECTED", "disconnected" },
  { NETSTATUS_STATE_IDLE, "NETSTATUS_STATE_IDLE", "idle" },
  { NETSTATUS_STATE_TX, "NETSTATUS_STATE_TX", "tx" },
  { NETSTATUS_STATE_RX, "NETSTATUS_STATE_RX", "rx" },
  { NETSTATUS_STATE_TX_RX, "NETSTATUS_STATE_TX_RX", "tx-rx" },
  { NETSTATUS_STATE_ERROR, "NETSTATUS_STATE_ERROR", "error" },
  { NETSTATUS_STATE_LAST, "NETSTATUS_STATE_LAST", "last" },
  { 0, NULL, NULL }
};

GType
netstatus_state_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("NetstatusState", _netstatus_state_values);

  return type;
}


static const GEnumValue _netstatus_error_values[] = {
  { NETSTATUS_ERROR_NONE, "NETSTATUS_ERROR_NONE", "none" },
  { NETSTATUS_ERROR_ICONS, "NETSTATUS_ERROR_ICONS", "icons" },
  { NETSTATUS_ERROR_SOCKET, "NETSTATUS_ERROR_SOCKET", "socket" },
  { NETSTATUS_ERROR_STATISTICS, "NETSTATUS_ERROR_STATISTICS", "statistics" },
  { NETSTATUS_ERROR_IOCTL_IFFLAGS, "NETSTATUS_ERROR_IOCTL_IFFLAGS", "ioctl-ifflags" },
  { NETSTATUS_ERROR_IOCTL_IFCONF, "NETSTATUS_ERROR_IOCTL_IFCONF", "ioctl-ifconf" },
  { NETSTATUS_ERROR_NO_INTERFACES, "NETSTATUS_ERROR_NO_INTERFACES", "no-interfaces" },
  { NETSTATUS_ERROR_WIRELESS_DETAILS, "NETSTATUS_ERROR_WIRELESS_DETAILS", "wireless-details" },
  { 0, NULL, NULL }
};

GType
netstatus_error_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("NetstatusError", _netstatus_error_values);

  return type;
}


static const GEnumValue _netstatus_debug_flags_values[] = {
  { NETSTATUS_DEBUG_NONE, "NETSTATUS_DEBUG_NONE", "none" },
  { NETSTATUS_DEBUG_POLLING, "NETSTATUS_DEBUG_POLLING", "polling" },
  { 0, NULL, NULL }
};

GType
netstatus_debug_flags_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("NetstatusDebugFlags", _netstatus_debug_flags_values);

  return type;
}



/* Generated data ends here */

