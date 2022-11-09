#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include <stdio.h>
#include <unistd.h>

#define APP_NAME "edu.aemi"
#define APP_OBJ_PATH "/edu/aemi"

#define SERVICE_OBJ_PATH (APP_OBJ_PATH "/service0")

GDBusConnection *sys_bus = NULL;
GDBusObjectManagerServer *app = NULL;

void cb_bus_ready(GObject *source_object, GAsyncResult *res,
                  gpointer user_data) {
  sys_bus = g_bus_get_finish(res, NULL);
  if (sys_bus != NULL) {
    g_printf("Bus Ready!\n");
  } else {
    g_printf("System Bus Failed!\n");
    return;
  }

/*
  app = g_dbus_object_manager_server_new(APP_OBJ_PATH);

  AEMIOrgFreedesktopDBusProperties *service_prop_interface =
      aemi_org_freedesktop_dbus_properties_skeleton_new();
  AEMIOrgBluezGattService1 *service_service_interface =
      aemi_org_bluez_gatt_service1_skeleton_new();

  AEMIObjectSkeleton *service = aemi_object_skeleton_new(SERVICE_OBJ_PATH);

  aemi_object_skeleton_set_org_freedesktop_dbus_properties(
      service, service_prop_interface);
  aemi_object_skeleton_set_org_bluez_gatt_service1(service,
                                                   service_service_interface);
  g_object_unref(service_prop_interface);
  g_object_unref(service_service_interface);
*/

  g_printf("Made Service Object!\n");
}

int main() {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  g_bus_get(G_BUS_TYPE_SYSTEM, NULL, cb_bus_ready, NULL);

  g_main_loop_run(loop);
  return 0;
}

/* Old test code
gboolean stdin_callback(GIOChannel *source, GIOCondition condition,
                        gpointer data) {
  gchar *in_str;
  gsize strlen;
  gsize terminator_pos;
  GError *errs;

  if (g_io_channel_read_line(source, &in_str, &strlen, &terminator_pos, NULL) !=
      G_IO_STATUS_NORMAL)
    return FALSE;

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