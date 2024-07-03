/* -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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
 * Authors: Carlos Garnacho Parro <carlosg@gnome.org>
 * Authors: Vincent Untz <vincent@vuntz.net>
 * Authors: Guillaume Desmottes <cass@skynet.be>
 */

#ifndef __GST_PACKAGES_H__
#define __GST_PACKAGES_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean gst_packages_install (GtkWindow *window, const char **packages);

G_END_DECLS

#endif /* __GST_PACKAGES_H__ */
