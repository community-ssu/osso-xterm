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

#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <libintl.h>
#include <locale.h>
#define _(String) gettext(String)

#include <gconf/gconf-client.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <vte/vte.h>

#include "terminal-gconf.h"
#include "terminal-widget.h"


enum
{
  PROP_0,
  PROP_CUSTOM_TITLE,
  PROP_ENCODING,
  PROP_TITLE,
};

enum
{
  CONTEXT_MENU,
  SELECTION_CHANGED,
  LAST_SIGNAL,
};


static void     terminal_widget_finalize                      (GObject          *object);
static void     terminal_widget_get_property                  (GObject          *object,
                                                               guint             prop_id,
                                                               GValue           *value,
                                                               GParamSpec       *pspec);
static void     terminal_widget_set_property                  (GObject          *object,
                                                               guint             prop_id,
                                                               const GValue     *value,
                                                               GParamSpec       *pspec);
static gboolean terminal_widget_get_child_command             (TerminalWidget   *widget,
                                                               gchar           **command,
                                                               gchar          ***argv,
                                                               GError          **error);
static gchar  **terminal_widget_get_child_environment         (TerminalWidget   *widget);
#if 0
static void terminal_widget_update_background                 (TerminalWidget   *widget);
#endif
static void terminal_widget_update_binding_backspace          (TerminalWidget   *widget);
static void terminal_widget_update_binding_delete             (TerminalWidget   *widget);
static void terminal_widget_update_colors                     (TerminalWidget   *widget);
static void terminal_widget_update_misc_bell                  (TerminalWidget   *widget);
static void terminal_widget_update_misc_cursor_blinks         (TerminalWidget   *widget);
static void terminal_widget_update_scrolling_lines            (TerminalWidget   *widget);
static void terminal_widget_update_scrolling_on_output        (TerminalWidget   *widget);
static void terminal_widget_update_scrolling_on_keystroke     (TerminalWidget   *widget);
#if 0
static void terminal_widget_update_title                      (TerminalWidget   *widget);
#endif
static void terminal_widget_update_word_chars                 (TerminalWidget   *widget);
static void terminal_widget_vte_child_exited                  (VteTerminal      *terminal,
                                                               TerminalWidget   *widget);
static void terminal_widget_vte_drag_data_received            (VteTerminal      *terminal,
                                                               GdkDragContext   *context,
                                                               gint              x,
                                                               gint              y,
                                                               GtkSelectionData *selection_data,
                                                               guint             info,
                                                               guint             time,
                                                               TerminalWidget   *widget);
static void     terminal_widget_vte_encoding_changed          (VteTerminal    *terminal,
                                                               TerminalWidget *widget);
static void     terminal_widget_vte_eof                       (VteTerminal    *terminal,
                                                               TerminalWidget *widget);
static gboolean terminal_widget_vte_button_press_event        (VteTerminal    *terminal,
                                                               GdkEventButton *event,
                                                               TerminalWidget *widget);
static gboolean terminal_widget_vte_key_press_event           (VteTerminal    *terminal,
                                                               GdkEventKey    *event,
                                                               TerminalWidget *widget);
static void     terminal_widget_vte_realize                   (VteTerminal    *terminal,
                                                               TerminalWidget *widget);
static void     terminal_widget_vte_selection_changed         (VteTerminal    *terminal,
                                                               TerminalWidget *widget);
static void     terminal_widget_vte_window_title_changed      (VteTerminal    *terminal,
                                                               TerminalWidget *widget);
static gboolean terminal_widget_timer_background              (gpointer        user_data);
static void     terminal_widget_gconf_scrollbar               (GConfClient    *client,
                                                               guint           conn_id,
                                                               GConfEntry     *entry,
                                                               TerminalWidget *widget);
static void     terminal_widget_gconf_reverse                 (GConfClient    *client,
                                                               guint           conn_id,
                                                               GConfEntry     *entry,
                                                               TerminalWidget *widget);
static void     terminal_widget_gconf_font_size               (GConfClient    *client,
                                                               guint           conn_id,
                                                               GConfEntry     *entry,
                                                               TerminalWidget *widget);
static void     terminal_widget_update_font_size              (TerminalWidget *widget,
                                                               gint size);
static void     terminal_widget_update_scrolling_bar          (TerminalWidget *widget,
                                                               gboolean show);
static void     terminal_widget_update_reverse                (TerminalWidget *widget,
                                                               gboolean reverse);
#if 0
static void     terminal_widget_timer_background_destroy      (gpointer        user_data);
#endif


static GObjectClass *parent_class;
static guint widget_signals[LAST_SIGNAL];

enum
{
  TARGET_URI_LIST,
  TARGET_UTF8_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_STRING,
  TARGET_TEXT_PLAIN,
  TARGET_MOZ_URL,
};

static GtkTargetEntry target_table[] =
{
  { (gchar*)"text/uri-list", 0, TARGET_URI_LIST },
  { (gchar*)"text/x-moz-url", 0, TARGET_MOZ_URL },
  { (gchar*)"UTF8_STRING", 0, TARGET_UTF8_STRING },
  { (gchar*)"TEXT", 0, TARGET_TEXT },
  { (gchar*)"COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
  { (gchar*)"STRING", 0, TARGET_STRING },
  { (gchar*)"text/plain", 0, TARGET_TEXT_PLAIN },
};


G_DEFINE_TYPE (TerminalWidget, terminal_widget, GTK_TYPE_HBOX);


static void
terminal_widget_class_init (TerminalWidgetClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = terminal_widget_finalize;
  gobject_class->get_property = terminal_widget_get_property;
  gobject_class->set_property = terminal_widget_set_property;

  /**
   * TerminalWidget:custom-title:
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CUSTOM_TITLE,
                                   g_param_spec_string ("custom-title",
                                                        _("Custom title"),
                                                        _("Custom title"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * TerminalWidget:encoding:
   *
   * The encoding the terminal will expect data from the child to be encoded
   * with. For certain terminal types, applications executing in the terminal
   * can change the encoding. The default encoding is defined by the application's
   * locale settings.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ENCODING,
                                   g_param_spec_string ("encoding",
                                                        _("Encoding"),
                                                        _("Terminal encoding"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * TerminalWidget:title:
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        _("Title"),
                                                        _("Title"),
                                                        NULL,
                                                        G_PARAM_READABLE));

  /**
   * TerminalWidget::context-menu
   **/
  widget_signals[CONTEXT_MENU] =
    g_signal_new ("context-menu",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalWidgetClass, context_menu),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * TerminalWidget::selection-changed
   **/
  widget_signals[SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalWidgetClass, selection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
terminal_widget_vte_focus_in_event (VteTerminal    *terminal,
                                    GdkEventFocus *event,
                                    TerminalWidget *widget)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  gtk_im_context_focus_in (widget->im_context);
}


static void
terminal_widget_vte_focus_out_event (VteTerminal    *terminal,
                                    GdkEventFocus *event,
                                    TerminalWidget *widget)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  gtk_im_context_focus_out (widget->im_context);
}

static void
terminal_widget_init (TerminalWidget *widget)
{
  GError *err = NULL;
  gint font_size;
  gboolean scrollbar, reverse;
  GConfValue *gconf_value;

  widget->working_directory = g_get_current_dir ();
  widget->custom_title = g_strdup ("");

  widget->gconf_client = gconf_client_get_default ();

  gconf_client_add_dir(widget->gconf_client,
          OSSO_XTERM_GCONF_PATH,
          GCONF_CLIENT_PRELOAD_NONE,
          &err); /* err */
  if (err != NULL) {
      g_printerr("gconf_client_add_dir(): %s\n", err->message);
      g_clear_error(&err);
  }

  widget->scrollbar_conid = gconf_client_notify_add(widget->gconf_client,
          OSSO_XTERM_GCONF_SCROLLBAR,
          (GConfClientNotifyFunc)terminal_widget_gconf_scrollbar,
          widget,
          NULL, &err);
  if (err != NULL) {
      g_printerr("scrollbar notify add failed: %s\n", err->message);
      g_clear_error(&err);
  }

  widget->font_size_conid = gconf_client_notify_add(widget->gconf_client,
          OSSO_XTERM_GCONF_FONT_SIZE,
          (GConfClientNotifyFunc)terminal_widget_gconf_font_size,
          widget,
          NULL, &err);
  if (err != NULL) {
      g_printerr("font_size notify add failed: %s\n", err->message);
      g_clear_error(&err);
  }

  widget->reverse_conid = gconf_client_notify_add(widget->gconf_client,
          OSSO_XTERM_GCONF_REVERSE,
          (GConfClientNotifyFunc)terminal_widget_gconf_reverse,
          widget,
          NULL, &err);
  if (err != NULL) {
      g_printerr("reverse notify add failed: %s\n", err->message);
      g_clear_error(&err);
  }

  widget->im_context = gtk_im_multicontext_new ();
  widget->terminal = vte_terminal_new ();

  g_signal_connect (G_OBJECT (widget->terminal), "focus-in-event",
                    G_CALLBACK (terminal_widget_vte_focus_in_event), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "focus-out-event",
                    G_CALLBACK (terminal_widget_vte_focus_out_event), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "child-exited",
                    G_CALLBACK (terminal_widget_vte_child_exited), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "encoding-changed",
                    G_CALLBACK (terminal_widget_vte_encoding_changed), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "eof",
                    G_CALLBACK (terminal_widget_vte_eof), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "button-press-event",
                    G_CALLBACK (terminal_widget_vte_button_press_event), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "key-press-event",
                    G_CALLBACK (terminal_widget_vte_key_press_event), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "selection-changed",
                    G_CALLBACK (terminal_widget_vte_selection_changed), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "realize",
                    G_CALLBACK (terminal_widget_vte_realize), widget);
  g_signal_connect (G_OBJECT (widget->terminal), "window-title-changed",
                    G_CALLBACK (terminal_widget_vte_window_title_changed), widget);
  gtk_box_pack_start (GTK_BOX (widget), widget->terminal, TRUE, TRUE, 0);
  gtk_widget_show (widget->terminal);

  /* setup Drag'n'Drop support */
  g_signal_connect (G_OBJECT (widget->terminal), "drag-data-received",
                    G_CALLBACK (terminal_widget_vte_drag_data_received), widget);
  gtk_drag_dest_set (GTK_WIDGET (widget->terminal),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     target_table, G_N_ELEMENTS (target_table),
                     GDK_ACTION_COPY);

  gtk_widget_grab_focus(widget->terminal);

  widget->scrollbar = gtk_vscrollbar_new (VTE_TERMINAL (widget->terminal)->adjustment);
  gtk_box_pack_start (GTK_BOX (widget), widget->scrollbar, FALSE, FALSE, 0);

  /* apply current settings */
  font_size = gconf_client_get_int(widget->gconf_client,
                                   OSSO_XTERM_GCONF_FONT_SIZE,
                                   &err);
  if (err != NULL) {
      g_printerr("Unable to get font size from gconf: %s\n",
                 err->message);
      g_clear_error(&err);
  }

  if (font_size == 0) {
      font_size = OSSO_XTERM_DEFAULT_FONT_SIZE;
  }

  gconf_value = gconf_client_get(widget->gconf_client,
                                 OSSO_XTERM_GCONF_SCROLLBAR,
                                 &err);
  if (err != NULL) {
      g_printerr("Unable to get scrollbar setting from gconf: %s\n",
                 err->message);
      g_clear_error(&err);
  }
  scrollbar = OSSO_XTERM_DEFAULT_SCROLLBAR;
  if (gconf_value) {
          if (gconf_value->type == GCONF_VALUE_BOOL)
                  scrollbar = gconf_value_get_bool(gconf_value);
          gconf_value_free(gconf_value);
  }

  gconf_value = gconf_client_get(widget->gconf_client,
                                 OSSO_XTERM_GCONF_REVERSE,
                                 &err);
  if (err != NULL) {
      g_printerr("Unable to get reverse setting from gconf: %s\n",
                 err->message);
      g_clear_error(&err);
  }
  reverse = OSSO_XTERM_DEFAULT_REVERSE;
  if (gconf_value) {
          if (gconf_value->type == GCONF_VALUE_BOOL)
                  reverse = gconf_value_get_bool(gconf_value);
          gconf_value_free(gconf_value);
  }

  terminal_widget_update_scrolling_bar(TERMINAL_WIDGET(widget), scrollbar);
  terminal_widget_update_font_size (TERMINAL_WIDGET(widget), font_size);
  terminal_widget_update_reverse (TERMINAL_WIDGET(widget), reverse);

  terminal_widget_update_binding_backspace (widget);
  terminal_widget_update_binding_delete (widget);
  terminal_widget_update_misc_bell (widget);
  terminal_widget_update_misc_cursor_blinks (widget);
  terminal_widget_update_scrolling_lines (widget);
  terminal_widget_update_scrolling_on_output (widget);
  terminal_widget_update_scrolling_on_keystroke (widget);
  terminal_widget_update_word_chars (widget);
}


static void
terminal_widget_finalize (GObject *object)
{
  TerminalWidget *widget = TERMINAL_WIDGET (object);

  g_free (widget->working_directory);
  g_strfreev (widget->custom_command);
  g_free (widget->custom_title);

  gconf_client_notify_remove(widget->gconf_client,
                             widget->scrollbar_conid);
  gconf_client_notify_remove(widget->gconf_client,
                             widget->font_size_conid);

  g_object_unref(G_OBJECT(widget->gconf_client));

  g_object_unref(widget->im_context);

  parent_class->finalize (object);
}


static void
terminal_widget_get_property (GObject          *object,
                              guint             prop_id,
                              GValue           *value,
                              GParamSpec       *pspec)
{
  TerminalWidget *widget = TERMINAL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CUSTOM_TITLE:
      g_value_set_string (value, widget->custom_title);
      break;

    case PROP_ENCODING:
      g_value_set_string (value, vte_terminal_get_encoding (VTE_TERMINAL (widget->terminal)));
      break;

    case PROP_TITLE:
      g_value_take_string (value, terminal_widget_get_title (widget));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
terminal_widget_set_property (GObject          *object,
                              guint             prop_id,
                              const GValue     *value,
                              GParamSpec       *pspec)
{
  TerminalWidget *widget = TERMINAL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CUSTOM_TITLE:
      terminal_widget_set_custom_title (widget, g_value_get_string (value));
      break;

    case PROP_ENCODING:
      vte_terminal_set_encoding (VTE_TERMINAL (widget->terminal), g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
terminal_widget_get_child_command (TerminalWidget   *widget,
                                   gchar           **command,
                                   gchar          ***argv,
                                   GError          **error)
{
  struct passwd *pw;
  const gchar   *shell_name;

  if (widget->custom_command != NULL)
    {
      *command = g_strdup (widget->custom_command[0]);
      *argv    = g_strdupv (widget->custom_command);
    }
  else
    {
      pw = getpwuid (getuid ());
      if (G_UNLIKELY (pw == NULL))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Current user unknown"));
          return FALSE;
        }

      shell_name = strrchr (pw->pw_shell, '/');
      if (shell_name != NULL)
        ++shell_name;
      else
        shell_name = pw->pw_shell;
      *command = g_strdup (pw->pw_shell);

      *argv = g_new (gchar *, 2);
      (*argv)[0] = g_strdup (shell_name);
      (*argv)[1] = NULL;
    }

  return TRUE;
}


static gchar**
terminal_widget_get_child_environment (TerminalWidget *widget)
{
  extern gchar    **environ;
  gchar           **result;
  gchar           **p;
  guint             n;

  /* count env vars that are set */
  for (p = environ; *p != NULL; ++p);

  n = p - environ;
  result = g_new (gchar *, n + 1 + 4);

  for (n = 0, p = environ; *p != NULL; ++p)
    {
      if ((strncmp (*p, "COLUMNS=", 8) == 0)
          || (strncmp (*p, "LINES=", 6) == 0)
          || (strncmp (*p, "WINDOWID=", 9) == 0)
          || (strncmp (*p, "TERM=", 5) == 0)
          || (strncmp (*p, "GNOME_DESKTOP_ICON=", 19) == 0)
          || (strncmp (*p, "COLORTERM=", 10) == 0)
          || (strncmp ( *p, "DISPLAY=", 8) == 0))
        {
          /* nothing: do not copy */
        }
      else
        {
          result[n] = g_strdup (*p);
          ++n;
        }
    }

  result[n++] = g_strdup ("COLORTERM=Terminal");

  if (GTK_WIDGET_REALIZED (widget->terminal))
    {
      result[n++] = g_strdup_printf ("WINDOWID=%ld", (glong) GDK_WINDOW_XWINDOW (widget->terminal->window));
      result[n++] = g_strdup_printf ("DISPLAY=%s", gdk_display_get_name (gtk_widget_get_display (widget->terminal)));
    }

  result[n] = NULL;

  return result;
}


#if 0
static void
terminal_widget_update_background (TerminalWidget *widget)
{
  if (G_UNLIKELY (widget->background_timer_id != 0))
    g_source_remove (widget->background_timer_id);

  widget->background_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 250, terminal_widget_timer_background,
                                                    widget, terminal_widget_timer_background_destroy);
}
#endif


static void
terminal_widget_update_binding_backspace (TerminalWidget *widget)
{
    vte_terminal_set_delete_binding (VTE_TERMINAL (widget->terminal), VTE_ERASE_ASCII_BACKSPACE);
}


static void
terminal_widget_update_binding_delete (TerminalWidget *widget)
{
    vte_terminal_set_delete_binding (VTE_TERMINAL (widget->terminal), VTE_ERASE_DELETE_SEQUENCE);
}


static void
terminal_widget_update_colors (TerminalWidget *widget)
{
    /*vte_terminal_set_default_colors(VTE_TERMINAL(widget->terminal));*/
}


static void
terminal_widget_update_misc_bell (TerminalWidget *widget)
{

}


static void
terminal_widget_update_misc_cursor_blinks (TerminalWidget *widget)
{
}


static void
terminal_widget_update_scrolling_lines (TerminalWidget *widget)
{
    gint lines = 200;
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (widget->terminal), lines);
}


static void
terminal_widget_update_scrolling_on_output (TerminalWidget *widget)
{
    vte_terminal_set_scroll_on_output (VTE_TERMINAL (widget->terminal), TRUE);
}


static void
terminal_widget_update_scrolling_on_keystroke (TerminalWidget *widget)
{
}


#if 0
static void
terminal_widget_update_title (TerminalWidget *widget)
{
  g_object_notify (G_OBJECT (widget), "title");
}
#endif


static void
terminal_widget_update_word_chars (TerminalWidget *widget)
{
}

static void
terminal_widget_vte_child_exited (VteTerminal    *terminal,
                                  TerminalWidget *widget)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  gtk_widget_destroy (GTK_WIDGET (widget));
}


static void
terminal_widget_vte_drag_data_received (VteTerminal      *terminal,
                                        GdkDragContext   *context,
                                        gint              x,
                                        gint              y,
                                        GtkSelectionData *selection_data,
                                        guint             info,
                                        guint             time,
                                        TerminalWidget   *widget)
{
  const guint16 *ucs;
  GString       *str;
  gchar        **uris;
  gchar         *filename;
  gchar         *text;
  gint           n;

  switch (info)
    {
    case TARGET_STRING:
    case TARGET_UTF8_STRING:
    case TARGET_COMPOUND_TEXT:
    case TARGET_TEXT:
      text = gtk_selection_data_get_text (selection_data);
      if (G_LIKELY (text != NULL))
        {
          if (G_LIKELY (*text != '\0'))
            vte_terminal_feed_child (VTE_TERMINAL (terminal), text, strlen (text));
          g_free (text);
        }
      break;

    case TARGET_TEXT_PLAIN:
      if (selection_data->format != 8 || selection_data->length == 0)
        {
          g_printerr (_("Unable to drop selection of type text/plain to terminal: Wrong format (%d) or length (%d)\n"),
                      selection_data->format, selection_data->length);
        }
      else
        {
          vte_terminal_feed_child (VTE_TERMINAL (terminal),
                                   selection_data->data,
                                   selection_data->length);
        }
      break;

    case TARGET_MOZ_URL:
      if (selection_data->format != 8
          || selection_data->length == 0
          || (selection_data->length % 2) != 0)
        {
          g_printerr (_("Unable to drop Mozilla URL on terminal: Wrong format (%d) or length (%d)\n"),
                      selection_data->format, selection_data->length);
        }
      else
        {
          str = g_string_new (NULL);
          ucs = (const guint16 *) selection_data->data;
          for (n = 0; n < selection_data->length / 2 && ucs[n] != '\n'; ++n)
            g_string_append_unichar (str, (gunichar) ucs[n]);
          filename = g_filename_from_uri (str->str, NULL, NULL);
          if (filename != NULL)
            {
              vte_terminal_feed_child (VTE_TERMINAL (widget->terminal), filename, strlen (filename));
              g_free (filename);
            }
          else
            {
              vte_terminal_feed_child (VTE_TERMINAL (widget->terminal), str->str, str->len);
            }
          g_string_free (str, TRUE);
        }
      break;

    case TARGET_URI_LIST:
      if (selection_data->format != 8 || selection_data->length == 0)
        {
          g_printerr (_("Unable to drop URI list on terminal: Wrong format (%d) or length (%d)\n"),
                      selection_data->format, selection_data->length);
        }
      else
        {
          text = g_strndup (selection_data->data, selection_data->length);
          uris = g_strsplit (text, "\r\n", 0);
          g_free (text);

          for (n = 0; uris != NULL && uris[n] != NULL; ++n)
            {
              filename = g_filename_from_uri (uris[n], NULL, NULL);
              if (G_LIKELY (filename != NULL))
                {
                  g_free (uris[n]);
                  uris[n] = filename;
                }
            }

          if (uris != NULL)
            {
              text = g_strjoinv (" ", uris);
              vte_terminal_feed_child (VTE_TERMINAL (widget->terminal), text, strlen (text));
              g_strfreev (uris);
            }
        }
      break;
    }
}


static void
terminal_widget_vte_encoding_changed (VteTerminal     *terminal,
                                      TerminalWidget  *widget)
{
  g_object_notify (G_OBJECT (widget), "encoding");
}


static void
terminal_widget_vte_eof (VteTerminal    *terminal,
                         TerminalWidget *widget)
{
  gtk_widget_destroy (GTK_WIDGET (widget));
}


static gboolean
terminal_widget_vte_button_press_event (VteTerminal    *terminal,
                                        GdkEventButton *event,
                                        TerminalWidget *widget)
{
  if (hildon_gtk_im_context_filter_event (widget->im_context, (GdkEvent*)event))
    {
      return TRUE;
    }

  if (event->button == 3)
    {
      g_signal_emit (G_OBJECT (widget), widget_signals[CONTEXT_MENU], 0, event);
      return TRUE;
    }
  else
    {
      hildon_gtk_im_context_show(widget->im_context);
    }

  return FALSE;
}

static gboolean
terminal_widget_vte_key_press_event (VteTerminal    *terminal,
                                     GdkEventKey    *event,
                                     TerminalWidget *widget)
{
  if (event->state == 0 && event->keyval == GDK_Menu)
    {
      g_signal_emit (G_OBJECT (widget), widget_signals[CONTEXT_MENU], 0, event);
      return TRUE;
    }

  return FALSE;
}


static void
terminal_widget_vte_realize (VteTerminal    *terminal,
                             TerminalWidget *widget)
{
  vte_terminal_set_allow_bold (terminal, TRUE);
  terminal_widget_timer_background (TERMINAL_WIDGET (widget));
  terminal_widget_update_colors (TERMINAL_WIDGET (widget));

  gtk_im_context_set_client_window (widget->im_context, widget->terminal->window);
}


static void
terminal_widget_vte_selection_changed (VteTerminal    *terminal,
                                       TerminalWidget *widget)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  g_signal_emit (G_OBJECT (widget), widget_signals[SELECTION_CHANGED], 0);
}


static void
terminal_widget_vte_window_title_changed (VteTerminal    *terminal,
                                          TerminalWidget *widget)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  g_object_notify (G_OBJECT (widget), "title");
}


static gboolean
terminal_widget_timer_background (gpointer user_data)
{
  return FALSE;
}


static void
terminal_widget_update_scrolling_bar (TerminalWidget *widget, gboolean show)
{
    if (show) {
        gtk_widget_show (widget->scrollbar);
    }
    else {
        gtk_widget_hide (widget->scrollbar);
    }
}


static void
terminal_widget_update_reverse (TerminalWidget *widget, gboolean reverse)
{
    if (reverse) {
        GdkColor fg, bg;

        gdk_color_parse("black", &fg);
        gdk_color_parse("white", &bg);

        vte_terminal_set_colors(VTE_TERMINAL(widget->terminal),
                                &fg, &bg,
                                NULL, 0);
    }
    else {
        vte_terminal_set_default_colors(VTE_TERMINAL(widget->terminal));
    }
}


static void
terminal_widget_update_font_size (TerminalWidget *widget, gint size)
{
    gchar *font_name;
    font_name = g_strdup_printf("NewCourier %d", size);
    vte_terminal_set_font_from_string (VTE_TERMINAL (widget->terminal), font_name);
    g_free(font_name);
}


static void
terminal_widget_gconf_scrollbar(GConfClient    *client,
                                guint           conn_id,
                                GConfEntry     *entry,
                                TerminalWidget *widget) {
    GConfValue *value;
    gboolean scrollbar;

    value = gconf_entry_get_value(entry);
    scrollbar = gconf_value_get_bool(value);
    terminal_widget_update_scrolling_bar(widget, scrollbar);
}


static void
terminal_widget_gconf_reverse(GConfClient    *client,
                              guint           conn_id,
                              GConfEntry     *entry,
                              TerminalWidget *widget) {
    GConfValue *value;
    gboolean reverse;

    value = gconf_entry_get_value(entry);
    reverse = gconf_value_get_bool(value);
    terminal_widget_update_reverse(widget, reverse);
}


static void
terminal_widget_gconf_font_size(GConfClient    *client,
                                guint           conn_id,
                                GConfEntry     *entry,
                                TerminalWidget *widget) {
    GConfValue *value;
    gint font_size;

    value = gconf_entry_get_value(entry);
    font_size = gconf_value_get_int(value);
    terminal_widget_update_font_size(widget, font_size);
}

#if 0
static void
terminal_widget_timer_background_destroy (gpointer user_data)
{
  TERMINAL_WIDGET (user_data)->background_timer_id = 0;
}
#endif


/**
 * terminal_widget_new:
 *
 * Return value :
 **/
GtkWidget*
terminal_widget_new ()
{
  return g_object_new(TERMINAL_TYPE_WIDGET, NULL);
}



/**
 * terminal_widget_launch_child:
 * @widget  : A #TerminalWidget.
 *
 * Starts the terminal child process.
 **/
void
terminal_widget_launch_child (TerminalWidget *widget)
{
  GError  *error = NULL;
  gchar   *command;
  gchar  **argv;
  gchar  **env;

  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  if (!terminal_widget_get_child_command (widget, &command, &argv, &error))
    {
      g_error_free (error);
      return;
    }

  env = terminal_widget_get_child_environment (widget);

  widget->pid = vte_terminal_fork_command (VTE_TERMINAL (widget->terminal),
                                           command, argv, env,
                                           widget->working_directory,
                                           TRUE, TRUE, TRUE);

  g_strfreev (argv);
  g_strfreev (env);
  g_free (command);
}


/**
 * terminal_widget_set_custom_command:
 * @widget  : A #TerminalWidget.
 * @command :
 **/
void
terminal_widget_set_custom_command (TerminalWidget *widget,
                                    gchar         **command)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  if (G_UNLIKELY (widget->custom_command != NULL))
    g_strfreev (widget->custom_command);

  if (G_LIKELY (command != NULL && *command != NULL))
    widget->custom_command = g_strdupv (command);
  else
    widget->custom_command = NULL;
}


/**
 * terminal_widget_set_custom_title:
 * @widget  : A #TerminalWidget.
 * @title   : Title string.
 **/
void
terminal_widget_set_custom_title (TerminalWidget *widget,
                                  const gchar    *title)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));

  if (g_ascii_strcasecmp(widget->custom_title, title))
    {
      g_free (widget->custom_title);
      widget->custom_title = g_strdup (title != NULL ? title : "");
      g_object_notify (G_OBJECT (widget), "custom-title");
      g_object_notify (G_OBJECT (widget), "title");
    }
}


/**
 **/
void
terminal_widget_get_size (TerminalWidget *widget,
                          gint           *width_chars,
                          gint           *height_chars)
{
  *width_chars = VTE_TERMINAL (widget->terminal)->column_count;
  *height_chars = VTE_TERMINAL (widget->terminal)->row_count;
}


/**
 **/
void
terminal_widget_set_size (TerminalWidget *widget,
                          gint            width_chars,
                          gint            height_chars)
{
  vte_terminal_set_size (VTE_TERMINAL (widget->terminal), width_chars, height_chars);
}


/**
 * terminal_widget_force_resize_window:
 *
 * I don't like this way, but its required to work-around a Gtk+
 * bug (maybe also caused by a Vte bug, not sure).
 **/
void
terminal_widget_force_resize_window (TerminalWidget *widget,
                                     GtkWindow      *window,
                                     gint            force_columns,
                                     gint            force_rows)
{
  GtkRequisition terminal_requisition;
  GtkRequisition window_requisition;
  gint           width;
  gint           height;
  gint           columns;
  gint           rows;
  gint           xpad;
  gint           ypad;

  gtk_widget_set_size_request (widget->terminal, 2000, 2000);
  gtk_widget_size_request (GTK_WIDGET (window), &window_requisition);
  gtk_widget_size_request (widget->terminal, &terminal_requisition);

  width = window_requisition.width - terminal_requisition.width;
  height = window_requisition.height - terminal_requisition.height;

  if (force_columns < 0)
    columns = VTE_TERMINAL (widget->terminal)->column_count;
  else
    columns = force_columns;

  if (force_rows < 0)
    rows = VTE_TERMINAL (widget->terminal)->row_count;
  else
    rows = force_rows;

  vte_terminal_get_padding (VTE_TERMINAL (widget->terminal), &xpad, &ypad);

  width += xpad + VTE_TERMINAL (widget->terminal)->char_width * columns;
  height += ypad + VTE_TERMINAL (widget->terminal)->char_height * rows;

  if (width < 0 || height < 0) {
      g_printerr("Invalid values: width=%d, height=%d, rows=%d, cols=%d\n",
              width, height, rows, columns);
      return;
  }

  if (GTK_WIDGET_MAPPED (window))
    gtk_window_resize (window, width, height);
  else
    gtk_window_set_default_size (window, width, height);
}


/**
 * terminal_widget_set_window_geometry_hints:
 *
 * I don't like this way, but its required to work-around a Gtk+
 * bug (maybe also caused by a Vte bug, not sure).
 **/
void
terminal_widget_set_window_geometry_hints (TerminalWidget *widget,
                                           GtkWindow      *window)
{
  GdkGeometry hints;
  gint        xpad;
  gint        ypad;

  vte_terminal_get_padding (VTE_TERMINAL (widget->terminal), &xpad, &ypad);

  hints.base_width = xpad;
  hints.base_height = ypad;
  hints.width_inc = VTE_TERMINAL (widget->terminal)->char_width;
  hints.height_inc = VTE_TERMINAL (widget->terminal)->char_height;
  hints.min_width = hints.base_width + hints.width_inc * 4;
  hints.min_height = hints.base_height + hints.height_inc * 2;

  gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                 widget->terminal,
                                 &hints,
                                 GDK_HINT_RESIZE_INC
                                 | GDK_HINT_MIN_SIZE
                                 | GDK_HINT_BASE_SIZE);
}


/**
 * terminal_widget_get_title:
 * @widget      : A #TerminalWidget.
 *
 * Return value :
 **/
gchar*
terminal_widget_get_title (TerminalWidget *widget)
{
  const gchar  *window_title;
  gchar *title;

  g_return_val_if_fail (TERMINAL_IS_WIDGET (widget), NULL);

  if (G_UNLIKELY (*widget->custom_title != '\0'))
    return g_strdup (widget->custom_title);

  window_title = vte_terminal_get_window_title (VTE_TERMINAL (widget->terminal));

  if (window_title != NULL)
      title = g_strdup (window_title);
  else
      title = g_strdup (_("Untitled"));

  return title;
}


/**
 * terminal_widget_get_working_directory:
 * @widget      : A #TerminalWidget.
 *
 * Determinies the working directory using various OS-specific mechanisms.
 *
 * Return value : The current working directory of @widget.
 **/
const gchar*
terminal_widget_get_working_directory (TerminalWidget *widget)
{
  gchar  buffer[4096 + 1];
  gchar *file;
  gchar *cwd;
  gint   length;

  g_return_val_if_fail (TERMINAL_IS_WIDGET (widget), NULL);

  if (widget->pid >= 0)
    {
      file = g_strdup_printf ("/proc/%d/cwd", widget->pid);
      length = readlink (file, buffer, sizeof (buffer));

      if (length > 0 && *buffer == '/')
        {
          buffer[length] = '\0';
          g_free (widget->working_directory);
          widget->working_directory = g_strdup (buffer);
        }
      else if (length == 0)
        {
          cwd = g_get_current_dir ();
          if (G_LIKELY (cwd != NULL))
            {
              if (chdir (file) == 0)
                {
                  g_free (widget->working_directory);
                  widget->working_directory = g_get_current_dir ();
                  chdir (cwd);
                }

              g_free (cwd);
            }
        }

      g_free (file);
    }

  return widget->working_directory;
}


/**
 * terminal_widget_set_working_directory:
 * @widget    : A #TerminalWidget.
 * @directory :
 **/
void
terminal_widget_set_working_directory (TerminalWidget *widget,
                                       const gchar    *directory)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));
  g_return_if_fail (directory != NULL);

  g_free (widget->working_directory);
  widget->working_directory = g_strdup (directory);
}


/**
 * terminal_widget_has_selection:
 * @widget      : A #TerminalWidget.
 *
 * Checks if the terminal currently contains selected text. Note that this is different from
 * determining if the terminal is the owner of any GtkClipboard items.
 *
 * Return value : %TRUE if part of the text in the terminal is selected.
 **/
gboolean
terminal_widget_has_selection (TerminalWidget *widget)
{
  g_return_val_if_fail (TERMINAL_IS_WIDGET (widget), FALSE);
  return vte_terminal_get_has_selection (VTE_TERMINAL (widget->terminal));
}


/**
 * terminal_widget_copy_clipboard:
 * @widget  : A #TerminalWidget.
 *
 * Places the selected text in the terminal in the #GDK_SELECTIN_CLIPBOARD selection.
 **/
void
terminal_widget_copy_clipboard (TerminalWidget *widget)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));
  vte_terminal_copy_clipboard (VTE_TERMINAL (widget->terminal));
}


/**
 * terminal_widget_paste_clipboard:
 * @widget  : A #TerminalWidget.
 *
 * Sends the contents of the #GDK_SELECTION_CLIPBOARD selection to the terminal's
 * child. If neccessary, the data is converted from UTF-8 to the terminal's current
 * encoding.
 **/
void
terminal_widget_paste_clipboard (TerminalWidget *widget)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));
  vte_terminal_paste_clipboard (VTE_TERMINAL (widget->terminal));
}


/**
 * terminal_widget_reset:
 * @widget  : A #TerminalWidget.
 * @clear   : %TRUE to also clear the terminal screen.
 *
 * Resets the terminal.
 **/
void
terminal_widget_reset (TerminalWidget *widget,
                       gboolean        clear)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));
  vte_terminal_reset (VTE_TERMINAL (widget->terminal), TRUE, clear);
}

/**
 * terminal_widget_im_append_menuitems:
 * @widget    : A #TerminalWidget.
 * @menushell : A #GtkMenuShell.
 *
 * Appends menu items for various input methods to the given @menushell.
 * The user can select one of these items to modify the input method
 * used by the terminal.
 **/
void
terminal_widget_im_append_menuitems (TerminalWidget *widget,
                                     GtkMenuShell   *menushell)
{
  g_return_if_fail (TERMINAL_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_MENU_SHELL (menushell));

  vte_terminal_im_append_menuitems (VTE_TERMINAL (widget->terminal), menushell);
}
