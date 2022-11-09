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

#define APP_BUS_NAME "edu.aemi"
#define APP_OBJ_PATH "/edu/aemi"
#define SERVICE_OBJ_PATH (APP_OBJ_PATH "/service0")

/*
How the following code is supposed to work:

1. Connect to the system dbus
2. Create a



*/
GDBusConnection *sys_bus = NULL;
guint own_name_id = 0;

void callback_bus_name_aquired(GDBusConnection *connection, const gchar *name,
                               gpointer user_data) {
  g_printf("Name \"%s\" aquired on system bus!", name);
}

void callback_sys_bus_ready(GObject *source_object, GAsyncResult *res,
                            gpointer user_data) {
  sys_bus = g_bus_get_finish(res, NULL);
  if (sys_bus != NULL) {
    g_printf("Connected to System Bus!\n");
  } else {
    g_printf("System Bus Failed!\n");
    return;
  }

  own_name_id = g_bus_own_name_on_connection(
      sys_bus, APP_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE,
      callback_bus_name_aquired, NULL, NULL, NULL);
}

int main() {
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