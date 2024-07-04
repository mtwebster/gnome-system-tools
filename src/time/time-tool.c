/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2005 Carlos Garnacho.
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
 * Authors: Carlos Garnacho Parro <carlosg@gnome.org>
 */

#include <glib.h>
#include <glib/gi18n.h>
#include "time-tool.h"
#include "gst.h"
#include "fdo-timedate1-dbus.h"

#define GST_TIME_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GST_TYPE_TIME_TOOL, GstTimeToolPrivate))
#define APPLY_CONFIG_TIMEOUT 2000

#define SCREENSAVER_SERVICE "org.gnome.ScreenSaver"
#define SCREENSAVER_PATH "/org/gnome/ScreenSaver"
#define SCREENSAVER_INTERFACE "org.gnome.ScreenSaver"

typedef struct _GstTimeToolPrivate GstTimeToolPrivate;

struct _GstTimeToolPrivate {
	guint clock_timeout;
	guint apply_timeout;

	guint configuration_changed_id;

	GDBusConnection *connection;
    GDBusProxy *proxy;
};

enum {
	CONFIGURATION_AUTOMATIC,
	CONFIGURATION_MANUAL
};

enum {
	COL_TEXT,
	COL_WIDGET,
	COL_LAST
};

static void  gst_time_tool_class_init     (GstTimeToolClass *class);
static void  gst_time_tool_init           (GstTimeTool      *tool);
static void  gst_time_tool_finalize       (GObject          *object);

static GObject *gst_time_tool_constructor (GType                  type,
					   guint                  n_construct_properties,
					   GObjectConstructParam *construct_params);
static void  gst_time_tool_update_gui     (GstTool *tool);
static void  gst_time_tool_update_config  (GstTool *tool);
static void  gst_time_tool_close          (GstTool *tool);

static void  on_option_configuration_changed (GtkWidget   *widget,
					      GstTimeTool *time_tool);


G_DEFINE_TYPE (GstTimeTool, gst_time_tool, GST_TYPE_TOOL);

static struct tm *
get_current_time (void)
{
    time_t tt;
    tzset();
    tt=time(NULL);

    return localtime(&tt);
}

static void
gst_time_tool_class_init (GstTimeToolClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GstToolClass *tool_class = GST_TOOL_CLASS (class);
	
	object_class->constructor = gst_time_tool_constructor;
	object_class->finalize = gst_time_tool_finalize;
	tool_class->update_gui = gst_time_tool_update_gui;
	tool_class->update_config = gst_time_tool_update_config;
	tool_class->close = gst_time_tool_close;

	g_type_class_add_private (object_class,
				  sizeof (GstTimeToolPrivate));
}

static void
gst_time_tool_init (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

    GError *error = NULL;

    priv->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (priv->connection == NULL)
    {
        g_critical ("Could not get system bus: %s", error->message);
    }

    priv->proxy = get_timedate1_proxy (priv->connection);
}

static gboolean
on_apply_timeout (GstTimeTool *tool)
{
    GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);
	gint year, month, day;
    guint hour, minute, second;

	gtk_calendar_get_date (GTK_CALENDAR (tool->calendar), &year, &month, &day);
	hour   = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->hours));
	minute = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->minutes));
	second = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->seconds));

    GDateTime *dt = g_date_time_new_local ((guint) year, (guint) month - 1, (guint) day, hour, minute, second);
    fdo_timedate1_set_time (priv->proxy, g_date_time_to_unix (dt));
    g_date_time_unref (dt);

	gst_time_tool_start_clock (tool);

    priv->apply_timeout = 0;

	return FALSE;
}

static void
update_apply_timeout (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	gst_time_tool_stop_clock (tool);

	if (priv->apply_timeout > 0) {
		g_source_remove (priv->apply_timeout);
		priv->apply_timeout = 0;
	}

	priv->apply_timeout = g_timeout_add (APPLY_CONFIG_TIMEOUT, (GSourceFunc) on_apply_timeout, tool);
}

static void
on_value_changed (GtkWidget *widget, gpointer data)
{
	gint value;
	gchar *str;

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
	str = g_strdup_printf ("%02d", (gint) value);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	gtk_entry_set_text (GTK_ENTRY (widget), str);
	g_free (str);
}

static void
on_editable_changed (GtkWidget *widget, gpointer data)
{
	update_apply_timeout (GST_TIME_TOOL (data));
}

#define is_leap_year(yyy) ((((yyy % 4) == 0) && ((yyy % 100) != 0)) || ((yyy % 400) == 0));

static void
change_calendar (GtkWidget *calendar, gint increment)
{
	gint day, month, year;
	gint days_in_month;
	gboolean leap_year;

	static const gint month_length[2][13] = {
		{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

	gtk_calendar_get_date (GTK_CALENDAR (calendar),
			       (guint*) &year, (guint*) &month, (guint*) &day);

	leap_year = is_leap_year (year);
	days_in_month = month_length [leap_year][month+1];

	if (increment != 0) {
		day += increment;

		if (day < 1) {
			day = month_length [leap_year][month] + day;
			month--;
		} else if (day > days_in_month) {
			day -= days_in_month;
			month++;
		}

		if (month < 0) {
			year--;
			leap_year = is_leap_year (year);
			month = 11;
			day = month_length [leap_year][month+1];
		} else if (month > 11) {
			year++;
			leap_year = is_leap_year (year);
			month = 0;
			day = 1;
		}

		gtk_calendar_select_month (GTK_CALENDAR (calendar),
					   month, year);
		gtk_calendar_select_day (GTK_CALENDAR (calendar),
					 day);
	}
}

static void
on_spin_button_wrapped (GtkWidget *widget, gpointer data)
{
	GstTimeTool *tool = GST_TIME_TOOL (data);
	gint value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));

	if (widget == tool->seconds)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (tool->minutes),
				      (value == 0) ? GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 1);
	else if (widget == tool->minutes)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (tool->hours),
				      (value == 0) ? GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 1);
	else if (widget == tool->hours)
		change_calendar (tool->calendar, (value == 0) ? 1 : -1);
}

static void
on_calendar_day_selected (GtkWidget *widget, gpointer data)
{
	update_apply_timeout (GST_TIME_TOOL (data));
}

static GtkWidget*
prepare_spin_button (GstTool *tool, const gchar *widget_name)
{
	GtkWidget *widget;

	widget = gst_dialog_get_widget (tool->main_dialog, widget_name);

	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (on_editable_changed), tool);
	g_signal_connect (G_OBJECT (widget), "wrapped",
			  G_CALLBACK (on_spin_button_wrapped), tool);
	/*
	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (on_value_changed), tool);
	*/

	return widget;
}

static void
init_timezone (GstTimeTool *time_tool)
{
	GstTool *tool = GST_TOOL (time_tool);
	GtkWidget *w;
	GPtrArray *locs;
	guint i;

	time_tool->tzmap = e_tz_map_new (tool);
	g_return_if_fail (time_tool->tzmap != NULL);
	
	w = gst_dialog_get_widget (tool->main_dialog, "map_window");
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (time_tool->tzmap->map));
	gtk_widget_show (GTK_WIDGET (time_tool->tzmap->map));

	w = gst_dialog_get_widget (tool->main_dialog, "location_combo");
	locs = tz_get_locations (e_tz_map_get_tz_db (time_tool->tzmap));

	for (i = 0; i < locs->len; i++)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (w),
		                                tz_location_get_zone (g_ptr_array_index (locs, i)));

	time_tool->timezone_dialog = gst_dialog_get_widget (tool->main_dialog, "time_zone_window");
}

static void
on_option_configuration_changed (GtkWidget *widget,
				 GstTimeTool *time_tool)
{
    GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (time_tool);

	gint option;
	gboolean active;

	option = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	active = (option == CONFIGURATION_AUTOMATIC);

    fdo_timedate1_set_ntp (priv->proxy, active);
}

static void
on_option_changed (GtkWidget   *combo,
		   GstTimeTool *time_tool)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget, *container;
	gint option;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
		return;

	option = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	gtk_tree_model_get (model, &iter,
			    COL_WIDGET, &widget,
			    -1);

	container = gst_dialog_get_widget (GST_TOOL (time_tool)->main_dialog, "configuration_container");

	/* remove the child */
	if (gtk_bin_get_child (GTK_BIN (container)))
		gtk_container_remove (GTK_CONTAINER (container), gtk_bin_get_child (GTK_BIN (container)));

	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (container);
}

static void
add_option (GstTool      *tool,
	    GtkListStore *store,
	    const gchar  *text,
	    const gchar  *widget)
{
	GtkTreeIter iter;
	GtkWidget *w, *toplevel;

	w = gst_dialog_get_widget (tool->main_dialog, widget);
	toplevel = gtk_widget_get_toplevel (w);

	g_object_ref (w);
	gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (w)), w);
	gtk_widget_destroy (toplevel);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_TEXT, text,
			    COL_WIDGET, w,
			    -1);
}

static void
set_cell_layout (GtkCellLayout *combo)
{
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new ();

	gtk_cell_layout_clear (combo);
	gtk_cell_layout_pack_start (combo, renderer, TRUE);
	gtk_cell_layout_set_attributes (combo, renderer,
					"text", COL_TEXT,
					NULL);
}

static void
add_options_combo (GstTimeTool *time_tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (time_tool);
	GstTool *tool = GST_TOOL (time_tool);
	GtkWidget *combo;
	GtkListStore *store;

	combo = gst_dialog_get_widget (tool->main_dialog, "configuration_options");
	store = gtk_list_store_new (COL_LAST, G_TYPE_STRING, GTK_TYPE_WIDGET);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
	add_option (tool, store, _("Keep synchronized with Internet servers"), "automatic_configuration");
	add_option (tool, store, _("Manual"), "manual_configuration");
	g_object_unref (store);

	set_cell_layout (GTK_CELL_LAYOUT (combo));
	g_signal_connect_after (combo, "changed",
				G_CALLBACK (on_option_changed), time_tool);

	priv->configuration_changed_id =
		g_signal_connect (combo, "changed",
				  G_CALLBACK (on_option_configuration_changed), time_tool);
}

static GObject*
gst_time_tool_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	GObject *object;
	GstTimeTool *time_tool;

	object = (* G_OBJECT_CLASS (gst_time_tool_parent_class)->constructor) (type,
									       n_construct_properties,
									       construct_params);
	time_tool = GST_TIME_TOOL (object);
	time_tool->map_hover_label = gst_dialog_get_widget (GST_TOOL (time_tool)->main_dialog, "location_label");
	time_tool->hours    = prepare_spin_button (GST_TOOL (time_tool), "hours");
	time_tool->minutes  = prepare_spin_button (GST_TOOL (time_tool), "minutes");
	time_tool->seconds  = prepare_spin_button (GST_TOOL (time_tool), "seconds");

	time_tool->calendar = gst_dialog_get_widget (GST_TOOL (time_tool)->main_dialog, "calendar");

	g_signal_connect (G_OBJECT (time_tool->calendar), "day-selected",
			  G_CALLBACK (on_calendar_day_selected), time_tool);

	init_timezone (time_tool);
	add_options_combo (time_tool);

	gtk_window_set_resizable (GTK_WINDOW (GST_TOOL (time_tool)->main_dialog), FALSE);

    gst_tool_update_gui (GST_TOOL (object));

	return object;
}

static void
gst_time_tool_finalize (GObject *object)
{
	GstTimeTool *tool = GST_TIME_TOOL (object);

	(* G_OBJECT_CLASS (gst_time_tool_parent_class)->finalize) (object);
}

static void
gst_time_tool_update_gui (GstTool *tool)
{
	GstTimeTool *time_tool = GST_TIME_TOOL (tool);
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (time_tool);
	GtkWidget *timezone, *configuration_options, *timeserver_button;
	gint option = CONFIGURATION_MANUAL;

	gst_time_tool_start_clock (GST_TIME_TOOL (tool));
	timezone = gst_dialog_get_widget (tool->main_dialog, "tzlabel");
	configuration_options = gst_dialog_get_widget (tool->main_dialog, "configuration_options");
	timeserver_button = gst_dialog_get_widget (tool->main_dialog, "timeserver_button");

	gtk_label_set_text (GTK_LABEL (timezone), fdo_timedate1_get_timezone (priv->connection));

	g_signal_handler_block (configuration_options, priv->configuration_changed_id);

    option = fdo_timedate1_get_ntp (priv->connection) ? CONFIGURATION_AUTOMATIC : CONFIGURATION_MANUAL;

	gtk_combo_box_set_active (GTK_COMBO_BOX (configuration_options), option);
	g_signal_handler_unblock (configuration_options, priv->configuration_changed_id);
}

static void
gst_time_tool_update_config (GstTool *tool)
{
}

static void
gst_time_tool_close (GstTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	if (priv->apply_timeout > 0) {
		/* disable timeout and apply changes
		 * immediately, the tool is closing! */
		g_source_remove (priv->apply_timeout);
		priv->apply_timeout = 0;
		on_apply_timeout (GST_TIME_TOOL (tool));
	}

	gst_time_tool_stop_clock (GST_TIME_TOOL (tool));

	(* GST_TOOL_CLASS (gst_time_tool_parent_class)->close) (tool);
}

GstTool*
gst_time_tool_new (void)
{
	return g_object_new (GST_TYPE_TIME_TOOL,
			     "name", "time",
			     "title", _("Time and Date Settings"),
			     "icon", "time-admin",
                 "show-lock-button", FALSE,
			     NULL);
}

static void
freeze_clock (GstTimeTool *tool)
{
	g_signal_handlers_block_by_func (tool->hours,   on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->minutes, on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->seconds, on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->calendar, on_calendar_day_selected, tool);
}

static void
thaw_clock (GstTimeTool *tool)
{
	g_signal_handlers_unblock_by_func (tool->hours,   on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->minutes, on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->seconds, on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->calendar, on_calendar_day_selected, tool);
}

static void
gst_time_tool_update_clock (GstTimeTool *tool)
{
    struct tm *t = get_current_time ();

	freeze_clock (tool);

	gtk_calendar_select_month (GTK_CALENDAR (tool->calendar), t->tm_mon, t->tm_year + 1900);
	gtk_calendar_select_day   (GTK_CALENDAR (tool->calendar), t->tm_mday);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->hours),   (gfloat) t->tm_hour);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->minutes), (gfloat) t->tm_min);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->seconds), (gfloat) t->tm_sec);

	thaw_clock (tool);
}

static gboolean
clock_tick (gpointer data)
{
	GstTimeTool *tool = (GstTimeTool *) data;

	gst_time_tool_update_clock (tool);

	return TRUE;
}

void
gst_time_tool_start_clock (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	if (priv->clock_timeout == 0) {
		/* Do a preliminary update, just for showing
		   something with sense in the gui immediatly */
		gst_time_tool_update_clock (tool);

		priv->clock_timeout = g_timeout_add (1000, (GSourceFunc) clock_tick, tool);
	}
}

void
gst_time_tool_stop_clock (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	if (priv->clock_timeout > 0) {
		g_source_remove (priv->clock_timeout);
		priv->clock_timeout = 0;
	}
}

void
gst_time_tool_run_timezone_dialog (GstTimeTool *time_tool)
{
    GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (time_tool);
	GstTool *tool;
	GtkWidget *label;
	const gchar *timezone;
	gchar *tz_name = NULL;
	TzLocation *tz_location;

	tool  = GST_TOOL (time_tool);
	label = gst_dialog_get_widget (tool->main_dialog, "tzlabel");

	timezone = fdo_timedate1_get_timezone (priv->connection);
	e_tz_map_set_tz_from_name (time_tool->tzmap, timezone);

	gtk_window_set_transient_for (GTK_WINDOW (time_tool->timezone_dialog),
				      GTK_WINDOW (tool->main_dialog));

	gst_dialog_add_edit_dialog (tool->main_dialog, time_tool->timezone_dialog);

	gtk_widget_show (time_tool->timezone_dialog);
	gtk_dialog_run (GTK_DIALOG (time_tool->timezone_dialog));
	gtk_widget_hide (time_tool->timezone_dialog);

	gst_dialog_remove_edit_dialog (tool->main_dialog, time_tool->timezone_dialog);

	tz_name     = e_tz_map_get_selected_tz_name (time_tool->tzmap);
	tz_location = e_tz_map_get_location_by_name (time_tool->tzmap, tz_name);

	if (!timezone || strcmp (tz_name, timezone) != 0) {
        fdo_timedate1_set_timezone (priv->proxy, tz_name);
		gtk_label_set_text (GTK_LABEL (label), tz_name);
	}
}
