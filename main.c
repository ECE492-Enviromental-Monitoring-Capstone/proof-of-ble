#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <stdio.h>
#include <unistd.h>

#include "bluez_advertisement.h"
#include "bluez_characteristic.h"
#include "bluez_hci.h"
#include "bluez_service.h"

#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_HCI_OBJ_PATH "/org/bluez/hci0"
#define BLUEZ_HCI_ADV_IFACE "org.bluez.LEAdvertisingManager1"

#define APP_BUS_NAME "edu.aemi"
#define APP_OBJ_PATH "/edu/aemi"
#define SERVICE_OBJ_PATH APP_OBJ_PATH "/service0"
#define ADV_OBJ_PATH APP_OBJ_PATH "/advertisement"

#define AEMI_SERVICE_UUID "2d2a7a9b-9b68-491d-b507-1f27244b5b82"

/*
How the following code is supposed to work:

1. Connect to the system dbus
2. Create a

*/
GDBusConnection *sys_bus = NULL;
guint own_name_id = 0;
GDBusObjectManagerServer *obj_server = NULL;
BluezAdvertisementOrgBluezLEAdvertisement1 *adv_iface = NULL;
const gchar *const uuids[2] = {AEMI_SERVICE_UUID, NULL};

void callback_advertisement_registered(GObject *source_object, GAsyncResult *res,
                                       gpointer user_data)
{
    GError *dbus_error = NULL;
    GVariant *dbus_result = g_dbus_connection_call_finish(sys_bus, res, &dbus_error);
    g_printf("Advertisement Registered!\n");
    if (dbus_result != NULL)
    {
        g_printf("Result info:\n");
        g_variant_print(dbus_result, TRUE);
        g_printf("\n");
    }
    if (dbus_error != NULL)
    {
        g_printf("Error: %s\n", dbus_error->message);
        g_error_free(dbus_error);
    }
}

void callback_bus_name_lost(GDBusConnection *connection, const gchar *name,
                            gpointer user_data)
{
    g_printf("Bus name lost for some reason?\n");
}

void callback_bus_name_aquired(GDBusConnection *connection, const gchar *name,
                               gpointer user_data)
{
    g_printf("Name \"%s\" aquired on system bus!\n", name);
    g_printf("Registering Advertisement!\n");

    g_dbus_connection_call(sys_bus, BLUEZ_BUS_NAME, BLUEZ_HCI_OBJ_PATH,
                           BLUEZ_HCI_ADV_IFACE, "RegisterAdvertisement",
                           g_variant_new("(oa{sv})", ADV_OBJ_PATH, NULL), NULL,
                           G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL,
                           (GAsyncReadyCallback)callback_advertisement_registered, NULL);
}

gboolean handle_authorize_method(GDBusObjectSkeleton *self,
                                 GDBusInterfaceSkeleton *interface,
                                 GDBusMethodInvocation *invocation, gpointer user_data)
{
    g_printf("A Method has been called on %s!\n", (gchar *)user_data);
    return TRUE;
}

void callback_sys_bus_ready(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    sys_bus = g_bus_get_finish(res, NULL);
    if (sys_bus != NULL)
    {
        g_printf("Connected to System Bus!\n");
    }
    else
    {
        g_printf("System Bus Failed!\n");
        return;
    }

    obj_server = g_dbus_object_manager_server_new(APP_OBJ_PATH);

    adv_iface = bluez_advertisement_org_bluez_leadvertisement1_skeleton_new();

    // Set properties
    bluez_advertisement_org_bluez_leadvertisement1_set_type_(adv_iface, "peripheral");
    bluez_advertisement_org_bluez_leadvertisement1_set_local_name(adv_iface, "AEMI");
    bluez_advertisement_org_bluez_leadvertisement1_set_discoverable(adv_iface, TRUE);
    bluez_advertisement_org_bluez_leadvertisement1_set_service_uuids(adv_iface, uuids);

    /*
    g_signal_connect(adv_obj, "authorize-method", G_CALLBACK(handle_authorize_method),
                     "Advertisement!");
    */

    g_dbus_object_manager_server_set_connection(obj_server, sys_bus);

    if (g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(adv_iface), sys_bus,
                                         ADV_OBJ_PATH, NULL))
    {
        g_printf("Advertisement Exported\n");
    }
    else
    {
        g_printf("Advertisement Failed to Export!\n");
    }

    own_name_id = g_bus_own_name_on_connection(
        sys_bus, APP_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, callback_bus_name_aquired,
        callback_bus_name_lost, NULL, NULL);
}

int main()
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    g_bus_get(G_BUS_TYPE_SYSTEM, NULL, callback_sys_bus_ready, NULL);

    g_main_loop_run(loop);

    if (sys_bus != NULL)
        g_object_unref(sys_bus);
    if (own_name_id != 0)
        g_bus_unown_name(own_name_id);

    return 0;
}

/* Old test code
gboolean stdin_callback(GIOChannel *source, GIOCondition condition,
                        gpointer data) {
  gchar *in_str;
  gsize strlen;
  gsize terminator_pos;
  GError *errs;

  if (g_io_channel_read_line(source, &in_str, &strlen, &terminator_pos, NULL)
!= G_IO_STATUS_NORMAL) return FALSE;

  g_printf("SIGNAL: STDIN READ %zu bytes\n", strlen);
  g_free(in_str);
  return TRUE;
}


int main() {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  GIOChannel *stdin_channel = g_io_channel_unix_new(STDIN_FILENO);

  g_io_add_watch(stdin_channel, G_IO_IN, stdin_callback, NULL);

  g_main_loop_run(loop);
  return 0;
}
*/