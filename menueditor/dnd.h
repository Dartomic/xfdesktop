/*   dnd.h */

/*  Copyright (C) 2005 Jean-François Wauthy under GNU GPL
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __HAVE_DND_HEADER
#define __HAVE_DND_HEADER

/* Prototype */
void treeview_drag_data_rcv_cb (GtkWidget * widget, GdkDragContext * dc, guint x, guint y, GtkSelectionData * sd, guint info, guint t, gpointer user_data);
gboolean treeview_drag_drop_cb (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, MenuEditor *me);

#endif
