/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
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

#include <glib/gi18n.h>

#include "gst.h"
#include "network-tool.h"
#include "callbacks.h"
#include "hosts.h"

extern GstTool *tool;


static void
do_popup_menu (GtkWidget *table, GstTablePopup *table_popup, GdkEventButton *event)
{
  gint button, event_time;

  if (!table_popup)
    return;

  if (event)
    {
      button     = event->button;
      event_time = event->time;
    }
  else
    {
      button     = 0;
      event_time = gtk_get_current_event_time ();
    }

  if (table_popup->setup)
    (table_popup->setup) (table);

  gtk_menu_popup (GTK_MENU (table_popup->popup), NULL, NULL, NULL, NULL,
		  button, event_time);
}

gboolean
on_table_button_press (GtkWidget *table, GdkEventButton *event, gpointer data)
{
  GtkTreePath      *path;
  GstTablePopup    *table_popup;
  GtkTreeSelection *selection;

  table_popup = (GstTablePopup *) data;
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (table));

  if (event->button == 3)
    {
      gtk_widget_grab_focus (table);

      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (table),
					 event->x, event->y,
					 &path, NULL, NULL, NULL))
        {
	  gtk_tree_selection_unselect_all (selection);
	  gtk_tree_selection_select_path (selection, path);

	  do_popup_menu (table, table_popup, event);
	}

      return TRUE;
    }

  if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
    {
      if (table_popup->properties)
	(table_popup->properties) (NULL, NULL);

      return TRUE;
    }

  return FALSE;
}

gboolean
on_table_popup_menu (GtkWidget *widget, gpointer data)
{
  GstTablePopup *table_popup = (GstTablePopup *) data;

  do_popup_menu (widget, table_popup, NULL);
  return TRUE;
}

void
on_host_aliases_add_clicked (GtkWidget *widget, gpointer data)
{
  host_aliases_run_dialog (GST_NETWORK_TOOL (tool), NULL);
}

void
on_host_aliases_properties_clicked (GtkWidget *widget, gpointer data)
{
  GtkTreeSelection *selection;
  GtkTreeView      *list;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  list  = GST_NETWORK_TOOL (tool)->host_aliases_list;
  selection = gtk_tree_view_get_selection (list);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    host_aliases_run_dialog (GST_NETWORK_TOOL (tool), &iter);
}

void
on_host_aliases_delete_clicked (GtkWidget *widget, gpointer data)
{
  GtkTreeSelection *selection;
  GtkTreeView *list;
  GtkTreeModel *model;
  GtkTreeIter iter;
  OobsList *hosts_list;
  OobsListIter *list_iter;

  list  = GST_NETWORK_TOOL (tool)->host_aliases_list;
  selection = gtk_tree_view_get_selection (list);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, COL_HOST_ITER, &list_iter, -1);
      hosts_list = oobs_hosts_config_get_static_hosts (GST_NETWORK_TOOL (tool)->hosts_config);
      oobs_list_remove (hosts_list, list_iter);
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      oobs_list_iter_free (list_iter);
      gst_tool_commit (tool, OOBS_OBJECT (GST_NETWORK_TOOL (tool)->hosts_config));
    }
}

void
on_host_aliases_dialog_changed (GtkWidget *widget, gpointer data)
{
  host_aliases_check_fields ();
}

void
on_entry_changed (GtkWidget *widget, gpointer data)
{
  g_object_set_data (G_OBJECT (widget), "content-changed", GINT_TO_POINTER (TRUE));
}

gboolean
on_hostname_focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
  GstNetworkTool *tool;
  gboolean changed;
  const gchar *hostname;

  tool = GST_NETWORK_TOOL (gst_dialog_get_tool (GST_DIALOG (data)));
  changed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "content-changed"));
  hostname = gtk_entry_get_text (GTK_ENTRY (widget));

  if (changed && hostname && *hostname)
    {
      GtkWidget *dialog;
      gint res;

      dialog = gtk_message_dialog_new (GTK_WINDOW (GST_TOOL (tool)->main_dialog),
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_NONE,
				       _("The host name has changed"),
				       NULL);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						_("This will prevent you "
						  "from launching new applications, and so you will "
						  "have to log in again. Continue anyway?"),
						NULL);
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			      _("Change _Host name"), GTK_RESPONSE_ACCEPT,
			      NULL);

      res = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (res == GTK_RESPONSE_ACCEPT)
	{
	  oobs_hosts_config_set_hostname (tool->hosts_config, hostname);
	  gst_tool_commit (GST_TOOL (tool), OOBS_OBJECT (tool->hosts_config));
	}
      else
	{
	  gtk_entry_set_text (GTK_ENTRY (widget),
			      oobs_hosts_config_get_hostname (tool->hosts_config));
	}
    }

  g_object_set_data (G_OBJECT (widget), "content-changed", GINT_TO_POINTER (FALSE));

  return FALSE;
}

gboolean
on_domain_focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
  GstNetworkTool *tool;
  gboolean changed;
  const gchar *domain;

  tool = GST_NETWORK_TOOL (gst_dialog_get_tool (GST_DIALOG (data)));
  changed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "content-changed"));
  domain = gtk_entry_get_text (GTK_ENTRY (widget));

  if (changed)
    {
      oobs_hosts_config_set_domainname (tool->hosts_config, domain);
      gst_tool_commit (GST_TOOL (tool), OOBS_OBJECT (tool->hosts_config));
    }

  g_object_set_data (G_OBJECT (widget), "content-changed", GINT_TO_POINTER (FALSE));

  return FALSE;
}
