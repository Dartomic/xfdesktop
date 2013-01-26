/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *  Copyright (c) 2004-2007 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"

#ifndef O_BINARY
#define O_BINARY  0
#endif

static GList *
list_files_in_dir(const gchar *path)
{
    GDir *dir;
    gboolean needs_slash = TRUE;
    const gchar *file;
    GList *files = NULL;

    dir = g_dir_open(path, 0, 0);
    if(!dir)
        return NULL;

    if(path[strlen(path)-1] == '/')
        needs_slash = FALSE;

    while((file = g_dir_read_name(dir))) {
        gchar *current_file = g_strdup_printf(needs_slash ? "%s/%s" : "%s%s",
                                              path, file);

        files = g_list_insert_sorted(files, current_file, (GCompareFunc)g_strcmp0);
    }

    g_dir_close(dir);

    return files;
}


gchar *
xfdesktop_backdrop_choose_next(const gchar *filename)
{
    GList *files, *current_file, *start_file;
    gchar *file = NULL;

    g_return_val_if_fail(filename, NULL);

    files = list_files_in_dir(g_path_get_dirname(filename));
    if(!files)
        return NULL;

    /* Get the our current background in the list */
    current_file = g_list_find_custom(files, filename, (GCompareFunc)g_strcmp0);

    /* if somehow we don't have a valid file, grab the first one available */
    if(current_file == NULL)
        current_file = g_list_first(files);

    start_file = current_file;

    /* We want the next valid image file in the dir while making sure
     * we don't loop on ourselves */
    do {
        current_file = g_list_next(current_file);

        /* we hit the end of the list */
        if(current_file == NULL)
            current_file = g_list_first(files);

        /* We went through every item in the list */
        if(g_strcmp0(start_file->data, current_file->data) == 0)
            break;

    } while(!xfdesktop_image_file_is_valid(current_file->data));

    file = g_strdup(current_file->data);
    g_list_free_full(files, g_free);

    return file;
}

gchar *
xfdesktop_backdrop_choose_random(const gchar *filename)
{
    static gboolean __initialized = FALSE;
    static gint previndex = -1;
    GList *files;
    gchar *file = NULL;
    gint n_items = 0, cur_file, tries = 0;

    g_return_val_if_fail(filename, NULL);

    files = list_files_in_dir(g_path_get_dirname(filename));
    if(!files)
        return NULL;

    n_items = g_list_length(files);

    if(1 == n_items) {
        file = g_strdup(g_list_first(files)->data);
        g_list_free_full(files, g_free);
        return file;
    }

    /* NOTE: 4.3BSD random()/srandom() are a) stronger and b) faster than
    * ANSI-C rand()/srand(). So we use random() if available */
    if(G_UNLIKELY(!__initialized))    {
        guint seed = time(NULL) ^ (getpid() + (getpid() << 15));
#ifdef HAVE_SRANDOM
        srandom(seed);
#else
        srand(seed);
#endif
        __initialized = TRUE;
    }

    do {
        if(tries++ == n_items) {
            /* this isn't precise, but if we've failed to get a good
             * image after all this time, let's just give up */
            g_warning("Unable to find good image from list; giving up");
            g_list_free_full(files, g_free);
            return NULL;
        }

        do {
#ifdef HAVE_SRANDOM
            cur_file = random() % n_items;
#else
            cur_file = rand() % n_items;
#endif
        } while(cur_file == previndex && G_LIKELY(previndex != -1));

    } while(!xfdesktop_image_file_is_valid(g_list_nth(files, cur_file)->data));

    previndex = cur_file;

    file = g_strdup(g_list_nth(files, cur_file)->data);
    g_list_free_full(files, g_free);

    return file;
}

gboolean
xfdesktop_image_file_is_valid(const gchar *filename)
{
    g_return_val_if_fail(filename, FALSE);

    /* if gdk can get pixbuf info from the file then it's an image file */
    return (gdk_pixbuf_get_file_info(filename, NULL, NULL) == NULL ? FALSE : TRUE);
}

gboolean
xfdesktop_check_is_running(Window *xid)
{
    const gchar *display = g_getenv("DISPLAY");
    gchar *p;
    gint xscreen = -1;
    gchar selection_name[100];
    Atom selection_atom;

    if(display) {
        if((p=g_strrstr(display, ".")))
            xscreen = atoi(p);
    }
    if(xscreen == -1)
        xscreen = 0;

    g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
    selection_atom = XInternAtom(gdk_x11_get_default_xdisplay(), selection_name, False);

    if((*xid = XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom)))
        return TRUE;

    return FALSE;
}

void
xfdesktop_send_client_message(Window xid, const gchar *msg)
{
    GdkEventClient gev;
    GtkWidget *win;

    win = gtk_invisible_new();
    gtk_widget_realize(win);

    gev.type = GDK_CLIENT_EVENT;
    gev.window = gtk_widget_get_window(win);
    gev.send_event = TRUE;
    gev.message_type = gdk_atom_intern("STRING", FALSE);
    gev.data_format = 8;
    strcpy(gev.data.b, msg);

    gdk_event_send_client_message((GdkEvent *)&gev, (GdkNativeWindow)xid);
    gdk_flush();

    gtk_widget_destroy(win);
}

/* Code taken from xfwm4/src/menu.c:grab_available().  This should fix the case
 * where binding 'xfdesktop -menu' to a keyboard shortcut sometimes works and
 * sometimes doesn't.  Credit for this one goes to Olivier.
 */
gboolean
xfdesktop_popup_grab_available (GdkWindow *win, guint32 timestamp)
{
    GdkEventMask mask =
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
        GDK_POINTER_MOTION_MASK;
    GdkGrabStatus g1;
    GdkGrabStatus g2;
    gboolean grab_failed = FALSE;
    gint i = 0;

    TRACE ("entering grab_available");

    g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
    g2 = gdk_keyboard_grab (win, TRUE, timestamp);

    while ((i++ < 2500) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS)
                || (g2 != GDK_GRAB_SUCCESS))))
    {
        TRACE ("grab not available yet, mouse reason: %d, keyboard reason: %d, waiting... (%i)", g1, g2, i);
        if(g1 == GDK_GRAB_INVALID_TIME || g2 == GDK_GRAB_INVALID_TIME)
            break;

        g_usleep (100);
        if (g1 != GDK_GRAB_SUCCESS)
        {
            g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
        }
        if (g2 != GDK_GRAB_SUCCESS)
        {
            g2 = gdk_keyboard_grab (win, TRUE, timestamp);
        }
    }

    if (g1 == GDK_GRAB_SUCCESS)
    {
        gdk_pointer_ungrab (timestamp);
    }
    if (g2 == GDK_GRAB_SUCCESS)
    {
        gdk_keyboard_ungrab (timestamp);
    }

    return (!grab_failed);
}
