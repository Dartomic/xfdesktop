/*   add_dialog.h */

/*  Copyright (C)  Jean-Fran�ois Wauthy under GNU GPL
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

#ifndef __HAVE_ADDDIALOG_HEADER
#define __HAVE_ADDDIALOG_HEADER

/* Structure */
struct _controls_add{
  enum {LAUNCHER, SUBMENU, SEPARATOR, TITLE, INCLUDE, QUIT} entry_type;
  GtkWidget *label_name;
  GtkWidget *entry_name;
  GtkWidget *label_command;
  GtkWidget *entry_command;
  GtkWidget *browse_button;
  GtkWidget *label_icon;
  GtkWidget *entry_icon;
  GtkWidget *browse_button2;
};

/* Prototype */
void add_entry_cb(GtkWidget *widget, gpointer data);

#endif
