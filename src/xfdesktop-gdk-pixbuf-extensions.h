/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __XFDESKTOP_GDK_PIXBUF_EXTENSIONS_H__
#define __XFDESKTOP_GDK_PIXBUF_EXTENSIONS_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS;

GdkPixbuf *xfdesktop_gdk_pixbuf_colorize  (const GdkPixbuf *src,
                                           const GdkColor  *color);

GdkPixbuf *xfdesktop_gdk_pixbuf_lucent    (const GdkPixbuf *src,
                                           guint            percent);

GdkPixbuf *xfdesktop_gdk_pixbuf_spotlight (const GdkPixbuf *src);

G_END_DECLS;

#endif /* !__XFDESKTOP_GDK_PIXBUF_EXTENSIONS_H__ */
