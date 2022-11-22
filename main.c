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
#define BLUEZ_HCI_GATT_MANAGER_IFACE "org.bluez.GattManager1"

#define APP_BUS_NAME "edu.aemi"
#define APP_OBJ_PATH "/edu/aemi"
#define SERVICE_OBJ_PATH APP_OBJ_PATH "/service0"
#define CHAR_OBJ_PATH SERVICE_OBJ_PATH "/char0"
#define ADV_OBJ_PATH APP_OBJ_PATH "/advertisement"

#define AEMI_SERVICE_UUID "2d2a7a9b-9b68-491d-b507-1f27244b5b82"
#define AEMI_CHAR_UUID "2d2a7a9b-9b68-491d-b507-1f27244b5b83" // One Higher

#define BLE_DEFAULT_MTU 23 // Minimum Required!

/*
How the following code is supposed to work:

1. Connect to the system dbus
2. Create a

*/
GDBusConnection *sys_bus = NULL;
guint own_name_id = 0;
GDBusObjectManagerServer *obj_server = NULL;
BluezAdvertisementOrgBluezLEAdvertisement1 *adv_iface = NULL;
const gchar *const uuids[] = {AEMI_SERVICE_UUID, NULL};

void callback_advertisement_registered(GObject *source_object, GAsyncResult *res,
                                       gpointer user_data)
{
    GError *dbus_error = NULL;
    GVariant *dbus_result = g_dbus_connection_call_finish(sys_bus, res, &dbus_error);
    if (dbus_result != NULL)
    {
        g_printf("Advertisement Registered!\n");
    }
    if (dbus_error != NULL)
    {
        g_printf("Error: %s\n", dbus_error->message);
        g_error_free(dbus_error);
    }
}

void callback_gatt_app_registered(GObject *source_object, GAsyncResult *res,
                                  gpointer user_data)
{
    GError *dbus_error = NULL;
    GVariant *dbus_result = g_dbus_connection_call_finish(sys_bus, res, &dbus_error);
    if (dbus_result != NULL)
    {
        g_printf("Application Registered!\n");
        g_printf("Registering Advertisement!\n");

        g_dbus_connection_call(
            sys_bus, BLUEZ_BUS_NAME, BLUEZ_HCI_OBJ_PATH, BLUEZ_HCI_ADV_IFACE,
            "RegisterAdvertisement", g_variant_new("(oa{sv})", ADV_OBJ_PATH, NULL), NULL,
            G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL,
            (GAsyncReadyCallback)callback_advertisement_registered, NULL);
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

    g_printf("Registering Service!\n");
    g_dbus_connection_call(sys_bus, BLUEZ_BUS_NAME, BLUEZ_HCI_OBJ_PATH,
                           BLUEZ_HCI_GATT_MANAGER_IFACE, "RegisterApplication",
                           g_variant_new("(oa{sv})", APP_OBJ_PATH, NULL), NULL,
                           G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL,
                           (GAsyncReadyCallback)callback_gatt_app_registered, NULL);
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

    BluezServiceOrgBluezGattService1 *service_iface =
        bluez_service_org_bluez_gatt_service1_skeleton_new();
    BluezServiceObjectSkeleton *service_object =
        bluez_service_object_skeleton_new(SERVICE_OBJ_PATH);
    bluez_service_object_skeleton_set_org_bluez_gatt_service1(service_object,
                                                              service_iface);

    BluezCharacteristicOrgBluezGattCharacteristic1 *char_iface =
        bluez_characteristic_org_bluez_gatt_characteristic1_skeleton_new();
    BluezCharacteristicObjectSkeleton *char_object =
        bluez_characteristic_object_skeleton_new(CHAR_OBJ_PATH);
    bluez_characteristic_object_skeleton_set_org_bluez_gatt_characteristic1(char_object,
                                                                            char_iface);

    // Set properties.
    // For Service
    gchar const *service_includes[] = {NULL}; // Strange not useful feature!
    bluez_service_org_bluez_gatt_service1_set_uuid(service_iface, AEMI_SERVICE_UUID);
    bluez_service_org_bluez_gatt_service1_set_primary(service_iface, TRUE);
    bluez_service_org_bluez_gatt_service1_set_includes(service_iface, service_includes);
    bluez_service_org_bluez_gatt_service1_set_handle(service_iface, 0);

    // For Characteristic
    gchar const *char_flags[] = {NULL};
    bluez_characteristic_org_bluez_gatt_characteristic1_set_uuid(char_iface,
                                                                 AEMI_CHAR_UUID);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_service(char_iface,
                                                                    SERVICE_OBJ_PATH);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_value(char_iface, "");
    bluez_characteristic_org_bluez_gatt_characteristic1_set_write_acquired(char_iface,
                                                                           FALSE);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_notify_acquired(char_iface,
                                                                            FALSE);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_notifying(char_iface, FALSE);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_flags(char_iface, char_flags);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_handle(char_iface, 0);
    bluez_characteristic_org_bluez_gatt_characteristic1_set_mtu(char_iface,
                                                                BLE_DEFAULT_MTU);

    // TODO: Handlers for the different methods!

    // Export Service and Characteristic (one each).
    g_dbus_object_manager_server_export(obj_server,
                                        G_DBUS_OBJECT_SKELETON(service_object));
    // You can in principle unrefrence them.
    g_object_unref(service_iface);
    g_object_unref(service_object);

    // Don't export, test.

    // g_dbus_object_manager_server_export(obj_server,
    // G_DBUS_OBJECT_SKELETON(char_object));
    // Unreference them again
    g_object_unref(char_iface);
    g_object_unref(char_object);

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