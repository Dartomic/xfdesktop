/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                     2004 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxml/parser.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "menu.h"
#include "menu-file.h"
#include "menu-dentry.h"
#include "menu-icon.h"
#include "dummy.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* where to find the current panel icon theme (if any) */
#define CHANNEL "xfce"
#define DEFAULT_ICON_THEME "Curve"

/* max length window list menu items */
#define WLIST_MAXLEN 20

#define EVENTMASK (ButtonPressMask|SubstructureNotifyMask|PropertyChangeMask)

static GtkWidget *create_desktop_menu();

/* ugly */
extern GdkPixbuf *dummy_icon;

/* a bit hackish, but works well enough */
gboolean is_using_system_rc = TRUE;
gboolean use_menu_icons = TRUE;

static GtkWidget *desktop_menu = NULL, *windowlist = NULL;
static GList *MainMenuData;	/* TODO: Free this at some point */

static time_t last_menu_gen = 0;

static McsClient *client = NULL;
gchar icon_theme[128];  /* so menu-icon.c can see this */
static time_t last_theme_change = 0;

GHashTable *menu_entry_hash = NULL;

static NetkScreen *netk_screen = NULL;

static gboolean EditMode = FALSE;

/*******************************************************************************
 *  User menu
 *******************************************************************************
 */

static gboolean menu_check_update(gpointer data)
{
	if(menu_file_need_update() || last_theme_change > last_menu_gen || !desktop_menu)
		desktop_menu = create_desktop_menu();
	
	return TRUE;
}

static void
free_menu_data (GList * menu_data)
{
    MenuItem *mi;
    GList *li;

    TRACE ("dummy");
    for (li = menu_data; li; li = li->next) 
    {
        mi = li->data;

        if(mi)
        {
            if(mi->path)
            {
                g_free(mi->path);
            }
            if(mi->cmd)
            {
                g_free(mi->cmd);
            }
            if (mi->pix_free)
            {
                g_object_unref(mi->pix_free);
            }
			if(mi->icon)
				xmlFree(mi->icon);
            g_free(mi);
        }
    }
    g_list_free (menu_data);
    menu_data = NULL;
}

static void
do_exec (gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    TRACE ("dummy");
	
	switch(fork()) {
		case -1:
			g_error("%s: unable to fork()\n", PACKAGE);
			break;
		
		case 0:
#ifdef HAVE_SETSID
			setsid();
#endif
			if(execlp((char *)callback_data, (char *)callback_data))
				g_error("%s: unable to spawn %s: %s\n", PACKAGE, (char *)callback_data, strerror(errno));
			_exit(0);
			break;
		default:
			break;
	}
}

static void
do_term_exec (gpointer callback_data, guint callback_action,
	      GtkWidget * widget)
{
    TRACE ("dummy");

	switch(fork()) {
		case -1:
			g_error("%s: unable to fork()\n", PACKAGE);
			break;
		
		case 0:
#ifdef HAVE_SETSID
			setsid();
#endif
			if(execlp("xfterm4", "xfterm4", "-e", (char *)callback_data, NULL))
				g_error("%s: unable to spawn %s: %s\n", PACKAGE, (char *)callback_data, strerror(errno));
			_exit(0);
			break;
		default:
			break;
	}
}

static void
do_builtin (gpointer callback_data, guint callback_action, GtkWidget * widget)
{
	char *builtin = (char *) callback_data;

	TRACE ("dummy");
	if(!strcmp (builtin, "edit")) {
		EditMode = (EditMode ? FALSE : TRUE);

		/* Need to rebuild menu, so destroy the current one */
		if(MainMenuData)
			free_menu_data (MainMenuData);
		MainMenuData = NULL;
		
		create_desktop_menu();
	} else if (!strcmp (builtin, "quit"))
		quit ();
}

static void
do_edit (gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    TRACE ("dummy");
    EditMode = FALSE;

    /* Need to rebuild menu, so destroy the current one */
	if(MainMenuData)
		free_menu_data (MainMenuData);
    MainMenuData = NULL;
	
	create_desktop_menu();
}

static GdkFilterReturn
client_event_filter1(GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
	if(mcs_client_process_event (client, (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
mcs_watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
	GdkWindow *gdkwin;

	gdkwin = gdk_window_lookup (window);

	if(is_start)
		gdk_window_add_filter (gdkwin, client_event_filter1, NULL);
	else
		gdk_window_remove_filter (gdkwin, client_event_filter1, NULL);
}


static void
mcs_notify_cb(const gchar *name, const gchar *channel_name, McsAction action,
              McsSetting *setting, void *cb_data)
{
	if(strcasecmp(channel_name, CHANNEL) || !setting)
		return;
			
	if((action==MCS_ACTION_NEW || action==MCS_ACTION_CHANGED) &&
			!strcmp(setting->name, "theme") && setting->type==MCS_TYPE_STRING)
	{
		g_strlcpy(icon_theme, setting->data.v_string, 128);
		last_theme_change = time(NULL);
	}
}

GtkItemFactoryEntry
parse_item (MenuItem * item)
{
    GtkItemFactoryEntry t;

    DBG ("%s (type=%d) (term=%d)\n", item->path, item->type, item->term);

    t.path = item->path;
    t.accelerator = NULL;
    t.callback_action = 1;      /* non-zero ! */
      
	/* if we don't give any pixdata, GtkItemFactory will demote the item from a
	 * GtkImageMenuItem back down to a GtkMenuItem */
	if(use_menu_icons)
		t.extra_data = (gpointer) my_pixbuf;

    if (!EditMode)
    {
        switch (item->type)
        {
            case MI_APP:
                t.callback = (item->term ? do_term_exec : do_exec);
				if(use_menu_icons)
					t.item_type = "<ImageItem>";
				else
					t.item_type = "<Item>";
                break;
            case MI_SEPARATOR:
                t.callback = NULL;
			    item->icon = NULL;
                t.item_type = "<Separator>";
                break;
            case MI_SUBMENU:
                t.callback = NULL;
			    item->icon = NULL;
				t.item_type = "<Branch>";
                break;
            case MI_TITLE:
                t.callback = NULL;
			    item->icon = NULL;
                t.item_type = "<Title>";
                break;
            case MI_BUILTIN:
                t.callback = do_builtin;
                if(use_menu_icons)
					t.item_type = "<ImageItem>";
				else
					t.item_type = "<Item>";
                break;
            default:
                break;
        }
    }
    else
    {
	t.callback = do_edit;

	if (item->type == MI_SUBMENU)
	    t.item_type = "<Branch>";
	else
	    t.item_type = "<Item>";

	if (item->type == MI_SEPARATOR)
	{
	    gchar *parent_menu;

	    parent_menu = g_path_get_dirname (item->path);
	    g_free (item->path);
	    item->path = g_strconcat (parent_menu, "--- separator ---", NULL);
	    t.path = item->path;

	    g_free (parent_menu);
	}
    }

    return t;
}

/* returns the menu widget */
static GtkWidget *
create_desktop_menu (void)
{
	static GtkItemFactory *ifactory = NULL;
    struct stat st;
    static char *filename = NULL;
    GtkWidget *img;
    GtkImageMenuItem *imgitem;
	GtkItemFactoryEntry entry;
	GdkPixbuf *pix;
	MenuItem *item = NULL;
	GList *li, *menu_data = NULL;
	gint i;

    TRACE ("dummy");
    if (!filename || is_using_system_rc)
    {
	if (filename)
	    g_free (filename);

	filename = menu_file_get();
    }

    /* may have been removed */
    if (stat (filename, &st) < 0)
    {
	if (filename)
	{
	    g_free (filename);
	}
	filename = menu_file_get ();
    }

    /* Still no luck? Something got broken! */
    if (stat (filename, &st) < 0)
    {
	if (filename)
	{
	    g_free (filename);
	}
	return NULL;
    }
	
 	if(ifactory)
		g_object_unref(G_OBJECT(ifactory));
	ifactory = gtk_item_factory_new (GTK_TYPE_MENU, "<popup>", NULL);
    
	if (MainMenuData)
		free_menu_data (MainMenuData);
	MainMenuData = NULL;

	if(menu_entry_hash)
		g_hash_table_destroy(menu_entry_hash);
	menu_entry_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, NULL);
	
	/* main source of menu data: the menu file */
	menu_data = menu_file_parse (filename, NULL);
	if(!menu_data) {
		g_error("%s: Error parsing menu file %s\n", PACKAGE, filename);
		return NULL;
	}
        
	g_hash_table_destroy(menu_entry_hash);
	menu_entry_hash = NULL;
		
	for (li = menu_data; li; li = li->next) {
		/* parse current item */
		item = (MenuItem *) li->data;
		
		if(!item) {
			g_warning("%s: found a NULL MenuItem\n", PACKAGE);
			continue;
		}

		entry = parse_item (item);
    
		if(!EditMode) {
			gtk_item_factory_create_item (ifactory, &entry, item->cmd, 1);
			if (use_menu_icons && item->icon) {
				pix = menu_icon_find(item->icon);
				if(pix) {
					imgitem = GTK_IMAGE_MENU_ITEM (gtk_item_factory_get_item (ifactory, item->path));
					if (imgitem) {
						img = gtk_image_new_from_pixbuf (pix);
						gtk_widget_show (img);
						gtk_image_menu_item_set_image (imgitem, img);
					}
					if(pix != dummy_icon)
						item->pix_free = pix;
				}
			}
		} else {
			gtk_item_factory_create_item (ifactory, &entry, item, 1);
		}
	}
    
	/* save menu data for later */
	MainMenuData = menu_data;
	/* mark off when we did all this */
	last_menu_gen = time(NULL);
	
    return gtk_item_factory_get_widget (ifactory, "<popup>");
}


/*******************************************************************************  
 *  Window list menu
 *******************************************************************************
 */

static void
activate_window (GtkWidget * item, NetkWindow * win)
{
    TRACE ("dummy");
    netk_window_activate (win);
}

static void
set_num_screens (gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    int n;

    TRACE ("dummy");
    if (!xa_NET_NUMBER_OF_DESKTOPS)
    {
	xa_NET_NUMBER_OF_DESKTOPS =
	    XInternAtom (GDK_DISPLAY (), "_NET_NUMBER_OF_DESKTOPS", False);
    }

    n = GPOINTER_TO_INT (num);

    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY ();
    sev.format = 32;
    sev.window = GDK_ROOT_WINDOW ();
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;

    gdk_error_trap_push ();

    XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), False,
		SubstructureNotifyMask | SubstructureRedirectMask,
		(XEvent *) & sev);

    gdk_flush ();
    gdk_error_trap_pop ();
}

static GtkWidget *
create_window_list_item (NetkWindow * win)
{
    const char *name = NULL;
    GString *label;
    GtkWidget *mi;

    TRACE ("dummy");
    if (netk_window_is_skip_pager (win) || netk_window_is_skip_tasklist (win))
	return NULL;

    if (!name)
	name = netk_window_get_name (win);

    label = g_string_new (name);

    if (label->len >= WLIST_MAXLEN)
    {
	g_string_truncate (label, WLIST_MAXLEN);
	g_string_append (label, " ...");
    }

    if (netk_window_is_minimized (win))
    {
	g_string_prepend (label, "[");
	g_string_append (label, "]");
    }

    mi = gtk_menu_item_new_with_label (label->str);

    g_string_free (label, TRUE);

    return mi;
}

static GtkWidget *
create_windowlist_menu (void)
{
    int i, n;
    GList *windows, *li;
    GtkWidget *menu3, *mi, *label;
    NetkWindow *win;
    NetkWorkspace *ws, *aws;
    GtkStyle *style;

    TRACE ("dummy");
    menu3 = gtk_menu_new ();
    style = gtk_widget_get_style (menu3);

/*      mi = gtk_menu_item_new_with_label(_("Window list"));
    gtk_widget_set_sensitive(mi, FALSE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
*/
    windows = netk_screen_get_windows_stacked (netk_screen);
    n = netk_screen_get_workspace_count (netk_screen);
    aws = netk_screen_get_active_workspace (netk_screen);

    for (i = 0; i < n; i++)
    {
	char *ws_name;
	const char *realname;
	gboolean active;

	ws = netk_screen_get_workspace (netk_screen, i);
	realname = netk_workspace_get_name (ws);

	active = (ws == aws);

	if (realname)
	{
	    ws_name = g_strdup_printf ("<i>%s</i>", realname);
	}
	else
	{
	    ws_name = g_strdup_printf ("<i>%d</i>", i + 1);
	}

	mi = gtk_menu_item_new_with_label (ws_name);
	g_free (ws_name);

	label = gtk_bin_get_child (GTK_BIN (mi));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	if (active)
	    gtk_widget_set_sensitive (mi, FALSE);

	g_signal_connect_swapped (mi, "activate",
				  G_CALLBACK (netk_workspace_activate), ws);

	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	for (li = windows; li; li = li->next)
	{
	    win = li->data;

	    /* sticky windows don;t match the workspace
	     * only show them on the active workspace */
	    if (netk_window_get_workspace (win) != ws &&
		!(active && netk_window_is_sticky (win)))
	    {
		continue;
	    }

	    mi = create_window_list_item (win);

	    if (!mi)
		continue;

	    gtk_widget_show (mi);
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	    if (!active)
	    {
		gtk_widget_modify_fg (gtk_bin_get_child (GTK_BIN (mi)),
				      GTK_STATE_NORMAL,
				      &(style->fg[GTK_STATE_INSENSITIVE]));
	    }

	    g_signal_connect (mi, "activate", G_CALLBACK (activate_window),
			      win);
	}

	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    }

    mi = gtk_menu_item_new_with_label (_("Add workspace"));
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    g_signal_connect_swapped (mi, "activate",
			      G_CALLBACK (set_num_screens),
			      GINT_TO_POINTER (n + 1));

    mi = gtk_menu_item_new_with_label (_("Delete workspace"));
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    g_signal_connect_swapped (mi, "activate",
			      G_CALLBACK (set_num_screens),
			      GINT_TO_POINTER (n - 1));

    return menu3;
}

/* Popup menu / windowlist
 * -----------------------
*/
void
popup_menu (int button, guint32 time)
{
	if(menu_file_need_update() || last_theme_change > last_menu_gen || !desktop_menu)
		desktop_menu = create_desktop_menu();

    if (desktop_menu)
    {
	gtk_menu_popup (GTK_MENU (desktop_menu), NULL, NULL, NULL, NULL,
			button, time);
    }
}

void
popup_windowlist (int button, guint32 time)
{
    static GtkWidget *menu = NULL;

    if (menu)
    {
	gtk_widget_destroy (menu);
    }

    windowlist = menu = create_windowlist_menu ();

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
}

static gboolean
button_press (GtkWidget * w, GdkEventButton * bevent)
{
    int button = bevent->button;
    int state = bevent->state;
    gboolean handled = FALSE;

    DBG ("button press");

    if (button == 2 || (button == 1 && state & GDK_SHIFT_MASK &&
			state & GDK_CONTROL_MASK))
    {
	popup_windowlist (button, bevent->time);
	handled = TRUE;
    }
    else if (button == 3 || (button == 1 && state & GDK_SHIFT_MASK))
    {
	popup_menu (button, bevent->time);
	handled = TRUE;
    }

    return handled;
}

static gboolean
button_scroll (GtkWidget * w, GdkEventScroll * sevent)
{
    GdkScrollDirection direction = sevent->direction;
    NetkWorkspace *ws = NULL;
    gint n, active;

    DBG ("scroll");

    n = netk_screen_get_workspace_count (netk_screen);

    if (n <= 1)
	return FALSE;

    ws = netk_screen_get_active_workspace (netk_screen);
    active = netk_workspace_get_number (ws);

    if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_LEFT)
    {
	ws = (active > 0) ?
	    netk_screen_get_workspace (netk_screen, active - 1) :
	    netk_screen_get_workspace (netk_screen, n - 1);
    }
    else
    {
	ws = (active < n - 1) ?
	    netk_screen_get_workspace (netk_screen, active + 1) :
	    netk_screen_get_workspace (netk_screen, 0);
    }

    netk_workspace_activate (ws);
    return TRUE;
}

/*  Initialization 
 *  --------------
*/
void
menu_init (XfceDesktop * xfdesktop)
{
    TRACE ("dummy");
    netk_screen = xfdesktop->netk_screen;

    DBG ("connecting callbacks");

#ifdef HAVE_SIGNAL_H
	signal(SIGCHLD, SIG_IGN);
#endif
	
	/* track icon theme changes (from the panel) */
    if(!client) {
        Display *dpy = GDK_DISPLAY();
        int screen = XDefaultScreen(dpy);
        
        if(!mcs_client_check_manager(dpy, screen, "xfce-mcs-manager"))
            g_warning("%s: mcs manager not running\n", PACKAGE);
        client = mcs_client_new(dpy, screen, mcs_notify_cb, mcs_watch_cb, NULL);
        if(client)
            mcs_client_add_channel(client, CHANNEL);
        else
            g_strlcpy(icon_theme, DEFAULT_ICON_THEME, 128);
    }
	
	/* create the menu */
	desktop_menu = create_desktop_menu();

	/* initialise menu file modification time check */
	menu_file_need_update();
	gtk_timeout_add(10000, menu_check_update, NULL);
	
    g_signal_connect (xfdesktop->fullscreen, "button-press-event",
		      G_CALLBACK (button_press), NULL);

    g_signal_connect (xfdesktop->fullscreen, "scroll-event",
		      G_CALLBACK (button_scroll), NULL);
}

void
menu_load_settings (XfceDesktop * xfdesktop)
{
    TRACE ("dummy");
}
