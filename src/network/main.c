/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* Copyright (C) 2004 Carlos Garnacho
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
 * Authors: Carlos Garnacho Parro  <carlosg@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <glib/gi18n.h>

#include "gst.h"
#include "network-tool.h"
#include "callbacks.h"

GstTool *tool;

static GstDialogSignal signals[] = {
  /* general tab */
  { "domain",                       "focus-out-event", G_CALLBACK (on_domain_focus_out) },
  { "hostname",                     "changed", G_CALLBACK (on_entry_changed) },
  { "domain",                       "changed", G_CALLBACK (on_entry_changed) },
  /* host aliases tab */
  { "host_aliases_add",             "clicked", G_CALLBACK (on_host_aliases_add_clicked) },
  { "host_aliases_properties",      "clicked", G_CALLBACK (on_host_aliases_properties_clicked) },
  { "host_aliases_delete",          "clicked", G_CALLBACK (on_host_aliases_delete_clicked) },
  /* host aliases dialog */
  { "host_alias_address",           "changed", G_CALLBACK (on_host_aliases_dialog_changed) },
  { NULL }
};

static GstDialogSignal signals_after[] = {
  { "hostname",                     "focus-out-event", G_CALLBACK (on_hostname_focus_out) },
  { NULL }
};

static const gchar *policy_widgets [] = {
	"hostname",
	"domain",
	"dns_list",
	"dns_list_add",
	"dns_list_delete",
	"search_domain_list",
	"search_domain_add",
	"search_domain_delete",
	"host_aliases_list",
	"host_aliases_add",
	"host_aliases_properties",
	"host_aliases_delete",
	NULL
};

static void
init_filters (void)
{
  gst_filter_init (GTK_ENTRY (gst_dialog_get_widget (tool->main_dialog, "host_alias_address")), GST_FILTER_IP);
}

static void
set_text_buffers_callback (void)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;

  textview = gst_dialog_get_widget (tool->main_dialog, "host_alias_list");
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

  g_signal_connect (G_OBJECT (buffer), "changed",
		    G_CALLBACK (on_host_aliases_dialog_changed), NULL);
}

int
main (int argc, gchar *argv[])
{
  gst_init_tool ("network-admin", argc, argv, NULL);
  tool = gst_network_tool_new ();

  gst_dialog_require_authentication_for_widgets (tool->main_dialog, policy_widgets);
  gst_dialog_connect_signals (tool->main_dialog, signals);
  gst_dialog_connect_signals_after (tool->main_dialog, signals_after);
  set_text_buffers_callback ();
  init_filters ();

  gtk_widget_show (GTK_WIDGET (tool->main_dialog));

  gtk_main ();
  return 0;
}
