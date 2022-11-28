#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "bluez_advertisement.h"
#include "bluez_characteristic.h"
#include "bluez_service.h"

#define DBUS_PROP_IFACE "org.freedesktop.DBus.Properties"

#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_HCI_OBJ_PATH "/org/bluez/hci0"
#define BLUEZ_HCI_ADV_IFACE "org.bluez.LEAdvertisingManager1"
#define BLUEZ_HCI_GATT_MANAGER_IFACE "org.bluez.GattManager1"
#define BLUEZ_HCI_ADAPT_IFACE "org.bluez.Adapter1"

#define APP_FULL_BLUETOOTH_ALIAS "Acoustic and Environmental Monitoring Instrument"
#define APP_BUS_NAME "edu.aemi"
#define APP_OBJ_PATH "/edu/aemi"
#define SERVICE_OBJ_PATH APP_OBJ_PATH "/service0"
#define CHAR_OBJ_PATH SERVICE_OBJ_PATH "/char0"
#define ADV_OBJ_PATH APP_OBJ_PATH "/advertisement"

#define AEMI_SERVICE_UUID "2d2a7a9b-9b68-491d-b507-1f27244b5b82"
#define AEMI_CHAR_UUID "2d2a7a9b-9b68-491d-b507-1f27244b5b83" // One Higher

#define BLE_DEFAULT_MTU 23                // Minimum Required!
#define NOTIFY_REP_TIME_MS 3000           // Send message every 3 seconds
#define NOTIFY_REP_MSG "BLE Notify On!\n" // Send this message!

#define MAX_SEND_QUEUE_BYTES 512 // At most 1/2 KiB can be queued up to send.

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

gboolean notify_on = FALSE;

/*
These are functions to handle connected devices.
The implementation is currently behaviourly simplistic.

* For each connected device -> Every 10 seconds send it a "Hi!\n" notifcation

*/

// This struct contains data for each connected BLE device!
typedef struct ConnectedDevice
{
    guint32 counter;     // Just a random counter.
    gchar *obj;          // Object path for the remote device.
    GSocket *write_sock; // This is the socket which recieves data written to the
                         // characteristic.
    GSocket *notif_sock; // This is the socket which sends notifications out.

    GByteArray *notif_send_queue; // Bytes to be sent!

    GSource *
        notify_sending_source; // This is a source which tries to send out the send queue.

    GSource
        *notify_timeout_source; // This is a timeout source which periodically sends data,
                                // it holds a reference to us so must be deleted first.
} ConnectedDevice;

// Dictionary for HCI objects (connected devices) -> ConnectedDevice .
// (strings (really object paths) -> ConnectedDevice*)
GHashTable *connected_dict;

// Helper function for deleting a connected device.
void free_connected(ConnectedDevice *dev)
{
    // Callback needs to be deleted.
    if (dev->notify_timeout_source != NULL)
    {
        g_source_destroy(dev->notify_timeout_source);
        g_source_unref(dev->notify_timeout_source);
    }

    if (dev->notify_sending_source != NULL)
    {
        g_source_destroy(dev->notify_sending_source);
        g_source_unref(dev->notify_sending_source);
    }

    if (dev->write_sock != NULL)
        g_object_unref(dev->write_sock);
    if (dev->notif_sock != NULL)
        g_object_unref(dev->notif_sock);

    g_byte_array_unref(dev->notif_send_queue);

    g_free(dev->obj); // Delete the string we allocated for it as a key.
    g_free(dev);      // Delete the entire struct.
}

// Register a new connection and create a ConnectedDevice for it.
ConnectedDevice *new_connected(gchar const *obj)
{
    if (connected_dict == NULL)
    {
        // Lazy init!
        connected_dict = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                               (GDestroyNotify)free_connected);
    }

    static guint32 count_connected = 0;

    // Allocate new device and store in list
    ConnectedDevice *new_device = g_malloc0(sizeof(ConnectedDevice));

    new_device->counter = count_connected++;
    new_device->obj = g_strdup(obj); // Note: The key is also copied.
    new_device->write_sock = NULL;
    new_device->notif_sock = NULL;

    new_device->notif_send_queue = g_byte_array_sized_new(MAX_SEND_QUEUE_BYTES);

    /* Old code: Don't launch on startup, make on demand.
    new_device->write_sock = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_DEFAULT, NULL);
    new_device->notif_sock = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_DEFAULT, NULL);
    */

    g_hash_table_insert(connected_dict, new_device->obj, new_device);
    return new_device;
}

// Get a ConnectedDevice* for a connected device.
// If it doesn't exist then create one for it.
ConnectedDevice *get_connected(gchar const *obj)
{
    ConnectedDevice *res = NULL;

    // Try looking for it
    if (connected_dict != NULL)
    {
        res = g_hash_table_lookup(connected_dict, obj);
    }
    // If it doesn't work just call it quits.
    if (res == NULL)
    {
        res = new_connected(obj);
    }
    return res;
}

void remove_connected(gchar const *obj)
{
    g_hash_table_remove(connected_dict, obj);
}

/*
GSources for Notifications
*/
gboolean notify_write_callback(GSocket *socket, GIOCondition condition,
                               gpointer user_data)
{
    ConnectedDevice *dev = (ConnectedDevice *)user_data;

    g_assert(!(condition &
               (G_IO_IN | G_IO_PRI | G_IO_NVAL | G_IO_ERR))); // This should never happen.

    if (condition & G_IO_HUP & 0) // Its closed on the other side!
    {
        g_printf("Notify pipe for \"%s\" is broken, emptying send queue.\n", dev->obj);

        dev->notif_send_queue->len = 0; // Flush send queue
        // Remove our source
        g_source_unref(dev->notify_sending_source);
        dev->notify_sending_source = NULL;

        // Delete the dead socket.
        g_assert(dev->notif_sock != NULL);
        g_object_unref(dev->notif_sock);
        dev->notif_sock = NULL;

        return G_SOURCE_REMOVE; // Do not continue this callback.
    }

    GError *err = NULL;

    g_assert(condition & G_IO_OUT);
    g_assert(!g_socket_get_blocking(socket));
    g_assert(socket == dev->notif_sock);

    if (dev->notif_send_queue->len < 1)
    {
        g_printf("warn: notify callback called for no data.\n");
        // Remove our source
        g_source_unref(dev->notify_sending_source);
        dev->notify_sending_source = NULL;
        return G_SOURCE_REMOVE;
    }

    gssize sent_len = g_socket_send(socket, dev->notif_send_queue->data,
                                    dev->notif_send_queue->len, NULL, &err);

    if (err == NULL)
    {
        g_assert(sent_len > 0);
        g_assert(sent_len <= dev->notif_send_queue->len); // More bytes impossible.

        if (sent_len == dev->notif_send_queue->len)
        {
            // Everything is sent, can delete our source and complete.
            dev->notif_send_queue->len = 0;
            g_source_unref(dev->notify_sending_source);
            dev->notify_sending_source = NULL;
            return G_SOURCE_REMOVE;
        }
        else
        {
            // Some bytes leftover.
            g_byte_array_remove_range(dev->notif_send_queue, 0, sent_len);
            return G_SOURCE_CONTINUE;
        }
    }
    else
    {
        if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        {
            g_printf("Info: G_IO_OUT was ready but socket is blocking.\n");
            g_error_free(err);
            // Nothing to do, just continue.
            return G_SOURCE_CONTINUE;
        }

        // Other errors are bugs.
        g_printf("Error: notif callback had an error: \"%s\"\n", err->message);
        g_error_free(err);

        // Don't attempt to recover.
        dev->notif_send_queue->len = 0; // Flush send queue
        // Remove our source
        g_source_unref(dev->notify_sending_source);
        dev->notify_sending_source = NULL;

        // Delete the dead socket.
        g_assert(dev->notif_sock != NULL);
        g_object_unref(dev->notif_sock);
        dev->notif_sock = NULL;

        return G_SOURCE_REMOVE; // Do not continue this callback.
    }

    g_assert(FALSE); // Shouldn't get here.
    return G_SOURCE_REMOVE;
}

guint queued_notif_send(ConnectedDevice *dev, guint8 const *data, guint len)
{
    g_assert(dev->notif_send_queue != NULL);
    g_assert(dev->notif_send_queue->len <= MAX_SEND_QUEUE_BYTES);

    guint to_send = len;

    if (dev->notif_send_queue->len + len > MAX_SEND_QUEUE_BYTES)
    {
        to_send = MAX_SEND_QUEUE_BYTES - dev->notif_send_queue->len;
        g_printf("Warning: queue to send to \"%s\" was too full!\n sending only %u/%u "
                 "bytes.\n",
                 dev->obj, to_send, len);
    }
    g_byte_array_append(dev->notif_send_queue, data, to_send);

    if (dev->notif_sock == NULL || g_socket_is_closed(dev->notif_sock))
    {
        g_printf("Warning: trying to send to broken notify socket connection, likely a "
                 "bug.\n");
        return to_send;
    }

    if (dev->notify_sending_source == NULL)
    {
        // No GSource set up to send data.
        // Set one up to start dishing things out.
        dev->notify_sending_source =
            g_socket_create_source(dev->notif_sock, G_IO_OUT, NULL);
        g_source_set_callback(dev->notify_sending_source,
                              G_SOURCE_FUNC(notify_write_callback), (gpointer)dev, NULL);
        g_source_attach(dev->notify_sending_source, NULL);
    }

    return to_send;
}

gboolean notif_spam_callback(gpointer user_data)
{
    ConnectedDevice *dev = (ConnectedDevice *)user_data;

    if (!notify_on)
    {
        // Delete this source!
        g_source_unref(dev->notify_timeout_source);
        dev->notify_timeout_source = NULL;
        return G_SOURCE_REMOVE;
    }

    queued_notif_send(dev, NOTIFY_REP_MSG, strlen(NOTIFY_REP_MSG));
    g_printf("Sending out BLE Notification to \"%s\"!\n", dev->obj);
    return G_SOURCE_CONTINUE;
}

/*
Signal Handlers for the Characteristic!
*/
gboolean handle_acquire_notify(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                               GDBusMethodInvocation *invocation, GUnixFDList *fd_list,
                               GVariant *arg_options, gpointer user_data)
{
    gchar *var_string = g_variant_print(arg_options, TRUE);
    g_printf("Acquire Notify Called! Options:%s\n", var_string);
    g_free(var_string);

    // What is going on with this?
    // No idea why it passes in a null pointer?
    g_assert(fd_list == NULL);

    gchar *obj = NULL;
    guint16 mtu = 0;

    g_assert(g_variant_lookup(arg_options, "device", "&o", &obj));
    g_assert(g_variant_lookup(arg_options, "mtu", "q", &mtu));

    ConnectedDevice *dev = get_connected(obj);

    // If it already has a working socket we just return that.
    // Otherwise needs a new one.
    int socket_fds[2];

    // Create a unix socketpair.
    g_assert(
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | O_CLOEXEC, 0, socket_fds) == 0);

    // Delete any existing socket, not needed.
    if (dev->notif_sock != NULL)
    {
        g_printf("warning: overwriting old socket!\n");
        g_object_unref(dev->notif_sock);
    }

    // We eat the first one for ourself.
    dev->notif_sock = g_socket_new_from_fd(socket_fds[0], NULL);
    g_socket_set_blocking(dev->notif_sock, FALSE); // Non blocking.

    // Place the other socket into a fdlist.
    GUnixFDList *fd_list2 = g_unix_fd_list_new_from_array(&socket_fds[1], 1);

    // Send over the socket and mtu.
    bluez_characteristic_org_bluez_gatt_characteristic1_complete_acquire_notify(
        object, invocation, fd_list2, g_variant_new_handle(0), mtu);

    // freeing instantly closes the list?
    // g_object_unref(fd_list2);

    // Turn on Notifications
    notify_on = TRUE;

    // Add a timeout to constantly poll device.
    // Delete existing if any.
    if (dev->notify_timeout_source != NULL)
    {
        g_source_destroy(dev->notify_timeout_source);
        g_source_unref(dev->notify_timeout_source);
    }
    // Create a source and attach to main.
    // It is passed a pointer to the device to notify.
    dev->notify_timeout_source = g_timeout_source_new(NOTIFY_REP_TIME_MS);
    g_source_set_callback(dev->notify_timeout_source, G_SOURCE_FUNC(notif_spam_callback),
                          (gpointer)dev, NULL);
    g_source_attach(dev->notify_timeout_source, NULL);

    return TRUE;
};

gboolean handle_acquire_write(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                              GDBusMethodInvocation *invocation, GUnixFDList *fd_list,
                              GVariant *arg_options, gpointer user_data)
{
    gchar *var_string = g_variant_print(arg_options, TRUE);
    g_printf("Acquire Write Called! Options:%s\n", var_string);
    g_free(var_string);
    return FALSE;
};

gboolean handle_confirm(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
    g_printf("Confirmation Recievied\n");
    bluez_characteristic_org_bluez_gatt_characteristic1_complete_confirm(object,
                                                                         invocation);
    return TRUE;
};

gboolean handle_read_value(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                           GDBusMethodInvocation *invocation, GVariant *arg_options,
                           gpointer user_data)
{
    return FALSE; // Not Handled
};

gboolean handle_start_notify(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                             GDBusMethodInvocation *invocation, gpointer user_data)
{
    g_printf("Start Notify Called!\n");
    bluez_characteristic_org_bluez_gatt_characteristic1_complete_start_notify(object,
                                                                              invocation);
    notify_on = TRUE;
    return TRUE;
};

gboolean handle_stop_notify(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                            GDBusMethodInvocation *invocation, gpointer user_data)
{
    g_printf("Stop Notify Called!\n");
    bluez_characteristic_org_bluez_gatt_characteristic1_complete_stop_notify(object,
                                                                             invocation);
    notify_on = FALSE;
    return TRUE;
};

gboolean handle_write_value(BluezCharacteristicOrgBluezGattCharacteristic1 *object,
                            GDBusMethodInvocation *invocation, const gchar *arg_value,
                            GVariant *arg_options, gpointer user_data)
{
    return FALSE; // Not Handled
};

/*
 Callbacks and main code!
*/
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
    g_printf("Setting up BlueTooth Adaptor\n");

    // Sets device "alias" to AEMI in long form. Not really important, can be removed
    // later. No callbacks, don't care what happens.
    g_dbus_connection_call(sys_bus, BLUEZ_BUS_NAME, BLUEZ_HCI_OBJ_PATH, DBUS_PROP_IFACE,
                           "Set",
                           g_variant_new("(ssv)", BLUEZ_HCI_ADAPT_IFACE, "Alias",
                                         g_variant_new_string(APP_FULL_BLUETOOTH_ALIAS)),
                           NULL, G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL, NULL, NULL);

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
    gchar const *char_flags[] = {"write-without-response", "notify", NULL};
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

    // Handlers for the different methods!
    g_signal_connect(char_iface, "handle-acquire-notify",
                     G_CALLBACK(handle_acquire_notify), NULL);
    g_signal_connect(char_iface, "handle-acquire-write", G_CALLBACK(handle_acquire_write),
                     NULL);
    g_signal_connect(char_iface, "handle-confirm", G_CALLBACK(handle_confirm), NULL);
    g_signal_connect(char_iface, "handle-start-notify", G_CALLBACK(handle_start_notify),
                     NULL);
    g_signal_connect(char_iface, "handle-stop-notify", G_CALLBACK(handle_stop_notify),
                     NULL);

    // Export Service and Characteristic (one each).
    g_dbus_object_manager_server_export(obj_server,
                                        G_DBUS_OBJECT_SKELETON(service_object));
    // You can in principle unrefrence them.
    g_object_unref(service_iface);
    g_object_unref(service_object);

    g_dbus_object_manager_server_export(obj_server, G_DBUS_OBJECT_SKELETON(char_object));
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