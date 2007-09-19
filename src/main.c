/* $Id: main.c 4425 2005-09-16 08:09:49Z hedberg $ */
/*-
 * Copyright (c) 2004 os-cillation e.K.
 * maemo specific changes: Copyright (c) 2005 Nokia Corporation
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 * maemo specific changes by Johan Hedberg <johan.hedberg@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libintl.h>
#include <locale.h>
#define _(String) gettext(String)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
/*#include <dbus/dbus-glib-lowlevel.h> */

#include <libosso.h>

#include "terminal-app-wrapper.h"

static gint osso_xterm_incoming(const gchar *interface,
    const gchar *method,
    GArray *arguments,
    gpointer data,
    osso_rpc_t *retval)
{
  gchar *command = NULL;

  if (strcmp(method, "run_command")) {
    retval->type = DBUS_TYPE_STRING;
    retval->value.s = g_strdup("Meh");
    return OSSO_ERROR;
  }

  if (arguments->len && 
      g_array_index(arguments, osso_rpc_t, 0).type == DBUS_TYPE_STRING) {
    command = g_array_index(arguments, osso_rpc_t, 0).value.s;
  }

  retval->value.b = terminal_app_wrapper_launch(TERMINAL_APP_WRAPPER(data),
      command,
      NULL);
  retval->type = DBUS_TYPE_BOOLEAN;

  return OSSO_OK;
}

int
main (int argc, char **argv)
{
  gpointer         app_wrapper;
  GError          *error = NULL;
  osso_context_t  *osso_context;
  const gchar     *command = NULL;
  DBusConnection  *system_bus = NULL;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

#ifdef DEBUG
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);
#endif

  g_set_application_name (_("X Terminal"));

  gtk_init (&argc, &argv);

  if (argc > 2 && !strcmp(argv[1], "-e")) {
    command = argv[2];
  } else if (argc > 1) {
    command = argv[1];
  }

  system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
  if (system_bus && dbus_bus_name_has_owner(system_bus,
	"com.nokia.xterm",
	NULL)) {
    DBusConnection *session_bus = dbus_bus_get(DBUS_BUS_SESSION, NULL);
    if (!session_bus) {
      exit(EXIT_FAILURE);
    }
    DBusMessage *msg = dbus_message_new_method_call("com.nokia.xterm",
	"/com/nokia/xterm",
	"com.nokia.xterm",
	"run_command");
    if (!msg) {
      exit(EXIT_FAILURE);
    }
    if (command) {
      dbus_message_append_args(msg,
	  DBUS_TYPE_STRING, &command,
	  DBUS_TYPE_INVALID);
    }
    dbus_message_set_no_reply(msg, TRUE);
    dbus_connection_send(session_bus, msg, NULL);
    dbus_connection_flush(session_bus);
    exit(EXIT_SUCCESS);
  }

  app_wrapper = terminal_app_wrapper_new();
  g_object_add_weak_pointer(G_OBJECT(app_wrapper), &app_wrapper);

  osso_context = osso_initialize("xterm", VERSION, FALSE, NULL);

  if (osso_context == NULL) {
    g_printerr("osso_initialize() failed!\n");
    exit(EXIT_FAILURE);
  }

  g_object_set_data(G_OBJECT(app_wrapper), "osso", osso_context);
  if (!terminal_app_wrapper_launch (TERMINAL_APP_WRAPPER(app_wrapper), command, &error))
    {
      g_printerr (_("Unable to launch terminal: %s\n"), error->message);
      g_error_free(error);
      exit(EXIT_FAILURE);
    }

  osso_rpc_set_default_cb_f(osso_context,
      osso_xterm_incoming,
      app_wrapper);

  gtk_main ();

  if (app_wrapper != NULL)
    {
      g_object_unref(G_OBJECT(app_wrapper));
    }

  osso_deinitialize(osso_context);

  return EXIT_SUCCESS;
}
