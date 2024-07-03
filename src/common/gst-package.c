/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/*
 * Copyright (C) 2004 Vincent Untz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:   Vincent Untz <vincent@vuntz.net>
 *            Guillaume Desmottes <cass@skynet.be>
 *            Carlos Garnacho <carlosg@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gst-package.h"

static void
show_error_dialog (GtkWindow   *window,
		   const gchar *secondary_text)
{
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (window,
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       _("Could not install package"));

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_text);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
}

static gboolean
gst_packages_packagekit(GtkWindow   *window,
                        const gchar *packages[])
{
  GDBusProxy *proxy = NULL;
  GError *error = NULL;
  guint32 xid = 0;
  GVariant *retval = NULL;
  gboolean success = FALSE;

  /* get a session bus proxy */
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE, NULL,
                                         "org.freedesktop.PackageKit",
                                         "/org/freedesktop/PackageKit",
                                         "org.freedesktop.PackageKit.Modify",
                                         NULL, &error);
  if (proxy == NULL) {
    g_warning ("failed to reach Packagekit proxy: %s",
               error ? error->message : "(unknown)");
    show_error_dialog (window, error ? error->message : _("Unknown error"));
    g_error_free (error);
    goto out;
  }

  /* get the window ID, or use 0 for non-modal */
  xid = GDK_WINDOW_XID ( gtk_widget_get_window ( GTK_WIDGET (window)));

  /* issue the sync request */
  retval = g_dbus_proxy_call_sync (proxy,
                                   "InstallPackageNames",
                                   g_variant_new ("(u^a&ss)",
                                                  xid,
                                                  packages,
                                                  "hide-finished"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* cancellable */
                                   &error);
  if (retval == NULL) {
    g_warning ("failed to install package: %s",
               error ? error->message : "(unknown)");
    show_error_dialog (window, error ? error->message : _("Unknown error"));
    g_error_free (error);
    goto out;
  }

  success = TRUE;

out:
  if (proxy != NULL)
    g_object_unref (proxy);
  if (retval != NULL)
    g_variant_unref (retval);

  return success;
}

gboolean
gst_packages_install (GtkWindow   *window,
		      const gchar *packages[])
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return gst_packages_packagekit(window, packages);
}
