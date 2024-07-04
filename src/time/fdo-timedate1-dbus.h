/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
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
 */


#ifndef __FDO_TIMEDATE1_DBUS__
#define __FDO_TIMEDATE1_DBUS__

G_BEGIN_DECLS

#include <glib.h>
#include <gio/gio.h>

GDBusProxy *get_timedate1_proxy (GDBusConnection *connection);
gboolean fdo_timedate1_set_time (GDBusProxy *proxy, gint64 t);
gboolean fdo_timedate1_set_ntp (GDBusProxy *proxy, gboolean enabled);
gboolean fdo_timedate1_get_ntp (GDBusConnection *connection);
const gchar *fdo_timedate1_get_timezone (GDBusConnection *connection);
void fdo_timedate1_set_timezone (GDBusProxy *proxy, const gchar *timezone);


#endif /* __FDO_TIMEDATE1_DBUS__ */
