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

#include <glib.h>
#include <gio/gio.h>

#include "fdo-timedate1-dbus.h"

GDBusProxy *
get_timedate1_proxy (GDBusConnection *connection)
{
    GError *error = NULL;
    GDBusProxy *proxy;

    proxy = g_dbus_proxy_new_sync (connection,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                   "org.freedesktop.timedate1",
                                   "/org/freedesktop/timedate1",
                                   "org.freedesktop.timedate1",
                                   NULL,
                                   &error);
    if(proxy == NULL)
    {
        g_critical ("Failed to connect to org.freedesktop.timedate1: %s", error->message);
        g_error_free(error);
    }

    return proxy;
}

gboolean
fdo_timedate1_set_time (GDBusProxy *proxy, gint64 t)
{
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_sync (proxy,
                                 "SetTime",
                                  g_variant_new ("(xbb)", t * 1000 * 1000,0,0),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);

    if(ret == NULL)
    {
        g_critical ("Set time failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    return TRUE;
}

gboolean
fdo_timedate1_set_ntp (GDBusProxy *proxy, gboolean enabled)
{
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_sync (proxy,
                                 "SetNTP",
                                  g_variant_new ("(bb)", enabled, enabled),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);

    if (ret == NULL)
    {
        g_critical ("Set ntp failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    return TRUE;
}

gboolean
fdo_timedate1_get_ntp (GDBusConnection *connection)
{
    GDBusProxy *proxy = NULL;
    GError     *error = NULL;
    GVariant   *ret;
    GVariant   *ntp;

    proxy = g_dbus_proxy_new_sync (connection,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                  "org.freedesktop.timedate1",
                                  "/org/freedesktop/timedate1",
                                  "org.freedesktop.DBus.Properties",
                                   NULL,
                                   &error);
    if (proxy == NULL)
    {
        g_critical ("Could not get NTP state: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    ret = g_dbus_proxy_call_sync (proxy,
                                 "Get",
                                  g_variant_new ("(ss)",
                                 "org.freedesktop.timedate1",
                                 "NTP"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                  NULL,
                                  &error);
    if (ret == NULL)
    {
        g_critical ("Could not get NTP state: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    g_variant_get (ret, "(v)", &ntp);
    return g_variant_get_boolean (ntp);
}

const gchar *
fdo_timedate1_get_timezone (GDBusConnection *connection)
{
    GDBusProxy *proxy = NULL;
    GError     *error = NULL;
    GVariant   *ret;
    GVariant   *timezone;

    proxy = g_dbus_proxy_new_sync (connection,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                  "org.freedesktop.timedate1",
                                  "/org/freedesktop/timedate1",
                                  "org.freedesktop.DBus.Properties",
                                   NULL,
                                   &error);
    if (proxy == NULL)
    {
        g_critical ("Could not get timezone: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    ret = g_dbus_proxy_call_sync (proxy,
                                 "Get",
                                  g_variant_new ("(ss)",
                                 "org.freedesktop.timedate1",
                                 "Timezone"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                  NULL,
                                  &error);

    if (ret == NULL)
    {
        g_critical ("Could not get timezone: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    g_variant_get (ret, "(v)", &timezone);
    return g_variant_get_string (timezone,0);
}

void
fdo_timedate1_set_timezone (GDBusProxy *proxy, const gchar *timezone)
{
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_sync (proxy,
                                 "SetTimezone",
                                  g_variant_new ("(sb)", timezone, 1),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);

    if (ret == NULL)
    {
        g_critical ("Could not set timezone: %s", error->message);
        g_error_free (error);
    }
}
