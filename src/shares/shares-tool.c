/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Authors: Carlos Garnacho Parro <carlosg@gnome.org>.
 */

#include <glib-object.h>
#include "shares-tool.h"
#include "users-table.h"
#include <glib/gi18n.h>
#include "gst.h"
#include "table.h"
#include "share-settings.h"

static void gst_shares_tool_class_init (GstSharesToolClass *class);
static void gst_shares_tool_init       (GstSharesTool      *tool);
static void gst_shares_tool_finalize   (GObject            *object);

static GObject * gst_shares_tool_constructor (GType                  type,
					      guint                  n_construct_properties,
					      GObjectConstructParam *construct_params);

static void gst_shares_tool_update_gui    (GstTool         *tool);
static void gst_shares_tool_update_config (GstTool         *tool);

static void gst_shares_tool_update_services_availability (GstSharesTool *tool);

G_DEFINE_TYPE (GstSharesTool, gst_shares_tool, GST_TYPE_TOOL);

static void
gst_shares_tool_class_init (GstSharesToolClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GstToolClass *tool_class = GST_TOOL_CLASS (class);

	object_class->constructor = gst_shares_tool_constructor;
	object_class->finalize = gst_shares_tool_finalize;
	tool_class->update_gui = gst_shares_tool_update_gui;
	tool_class->update_config = gst_shares_tool_update_config;
}

static void
gst_shares_tool_init (GstSharesTool *tool)
{
	GstTool *gst_tool = GST_TOOL (tool);

	tool->nfs_config = oobs_nfs_config_get ();
	gst_tool_add_configuration_object (gst_tool, tool->nfs_config, TRUE);

	tool->smb_config = oobs_smb_config_get ();
	gst_tool_add_configuration_object (gst_tool, tool->smb_config, TRUE);

	tool->hosts_config = oobs_hosts_config_get ();
	gst_tool_add_configuration_object (gst_tool, tool->hosts_config, TRUE);

	tool->users_config = oobs_users_config_get ();
	gst_tool_add_configuration_object (gst_tool, tool->users_config, TRUE);
}

static GObject *
gst_shares_tool_constructor (GType                  type,
			     guint                  n_construct_properties,
			     GObjectConstructParam *construct_params)
{
	GObject *object;

	object = (* G_OBJECT_CLASS (gst_shares_tool_parent_class)->constructor) (type,
										 n_construct_properties,
										 construct_params);
	users_table_create (GST_TOOL (object));

	return object;
}

static void
gst_shares_tool_finalize (GObject *object)
{
	GstSharesTool *tool = GST_SHARES_TOOL (object);

	if (tool->nfs_config)
		g_object_unref (tool->nfs_config);

	(* G_OBJECT_CLASS (gst_shares_tool_parent_class)->finalize) (object);
}

static void
add_shares (OobsList *list)
{
	OobsListIter iter;
	OobsShare *share;
	gboolean valid;

	valid = oobs_list_get_iter_first (list, &iter);

	while (valid) {
		share = OOBS_SHARE (oobs_list_get (list, &iter));

		table_add_share (share, &iter);
		g_object_unref (share);
		valid = oobs_list_iter_next (list, &iter);
	}
}

static void
update_global_smb_config (GstTool       *tool,
			  OobsSMBConfig *config)
{
	GtkWidget *widget;
	const gchar *str;
	gboolean is_wins_server;

	str = oobs_smb_config_get_workgroup (config);
	widget = gst_dialog_get_widget (tool->main_dialog, "smb_workgroup");
	gtk_entry_set_text (GTK_ENTRY (widget), (str) ? str : "");

	is_wins_server = oobs_smb_config_get_is_wins_server (config);
	widget = gst_dialog_get_widget (tool->main_dialog, "smb_is_wins");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), is_wins_server);
		
	str = oobs_smb_config_get_wins_server (config);
	widget = gst_dialog_get_widget (tool->main_dialog, "smb_wins_server");
	gtk_entry_set_text (GTK_ENTRY (widget), (str) ? str : "");
}

static gboolean
install_packages (GstTool *tool, gboolean install_nfs, gboolean install_smb)
{
	gchar **packages;
	gboolean retval;
	gint i = 0;

	if (!install_nfs && !install_smb)
		return FALSE;

	packages = g_new0 (gchar*, 3);

	if (install_nfs)
		packages[i++] = g_strdup ("nfs-kernel-server");

	if (install_smb)
		packages[i++] = g_strdup ("samba");

	retval = gst_packages_install (GTK_WINDOW (tool->main_dialog), packages);
	g_strfreev (packages);

	return retval;
}

static gboolean
check_servers (GstSharesTool *tool)
{
	GtkWidget *dialog;
	GtkWidget *nfs_check, *smb_check;
	gint response;
	gboolean retval = FALSE;

	if (tool->smb_available || tool->nfs_available)
		return TRUE;

	dialog = gtk_message_dialog_new (GTK_WINDOW (GST_TOOL (tool)->main_dialog),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("Sharing services are not installed"));
	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
						    _("You need to install at least either Samba or NFS "
						      "in order to share your folders."));
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install services"), GTK_RESPONSE_OK);

	/* add NFS checkbutton */
	nfs_check = gtk_check_button_new_with_mnemonic (_("Install Unix networks support (_NFS)"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (nfs_check), TRUE);
	gtk_widget_show (nfs_check);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), nfs_check);

	/* add SMB checkbutton */
	smb_check = gtk_check_button_new_with_mnemonic (_("Install Windows networks support (_SMB)"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (smb_check), TRUE);
	gtk_widget_show (smb_check);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), smb_check);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_OK) {
		if (install_packages (GST_TOOL (tool),
				      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (nfs_check)),
				      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (smb_check)))) {
			gst_tool_update_config (GST_TOOL (tool));
			gst_tool_update_gui (GST_TOOL (tool));

			retval = TRUE;
		}
	}

	gtk_widget_destroy (dialog);
	return retval;
}

static void
gst_shares_tool_update_gui (GstTool *tool)
{
	GstSharesTool *shares_tool;
	GtkWidget *dialog_notebook;
	OobsList *list;

	shares_tool = GST_SHARES_TOOL (tool);
	dialog_notebook = gst_dialog_get_widget (tool->main_dialog, "shares_admin");

	if (check_servers (shares_tool)) {
		table_clear ();

		list  = oobs_nfs_config_get_shares (OOBS_NFS_CONFIG (shares_tool->nfs_config));
		add_shares (list);

		list = oobs_smb_config_get_shares (OOBS_SMB_CONFIG (shares_tool->smb_config));
		add_shares (list);

		update_global_smb_config (tool, OOBS_SMB_CONFIG (shares_tool->smb_config));
		gtk_widget_set_sensitive (dialog_notebook, TRUE);
	} else {
		/* disable the tool UI, there's no way to add shares */
		gtk_widget_set_sensitive (dialog_notebook, FALSE);
	}

	users_table_set_config (shares_tool);

	if (shares_tool->path) {
		gst_tool_authenticate (tool, GST_SHARES_TOOL (tool)->smb_config);
		gst_tool_authenticate (tool, GST_SHARES_TOOL (tool)->nfs_config);
		share_settings_dialog_run (shares_tool->path, TRUE);
	}
}

static void
gst_shares_tool_update_config (GstTool *tool)
{
	GstSharesTool *shares_tool;

	shares_tool = GST_SHARES_TOOL (tool);
	gst_shares_tool_update_services_availability (shares_tool);
}

/* Services detection is very rough:
 * since Upstart jobs are not supported (yet?), we fall back on checking
 * that files are present, which means packages are installed.
 * Anyway, the only right way to do this would be checking that
 * the package is present, e.g. using PackageKit. */
static void
gst_shares_tool_update_services_availability (GstSharesTool *tool)
{
        if (g_file_test ("/usr/sbin/smbd", G_FILE_TEST_EXISTS))
        	tool->smb_available = TRUE;

        if (g_file_test ("/usr/sbin/rpc.nfsd", G_FILE_TEST_EXISTS))
		tool->nfs_available = TRUE;
}

GstSharesTool*
gst_shares_tool_new (void)
{
	return g_object_new (GST_TYPE_SHARES_TOOL,
			     "name", "shares",
			     "title", _("Shared Folders"),
			     "icon", "folder-remote",
			     NULL);
}
