#include <glib.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <unistd.h>


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
