/*
 * glade-gtk-overlay.c - GladeWidgetAdaptor for GtkOverlay widget
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
 *
 * Authors:
 *      Juan Pablo Ugarte <juanpablougarte@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <config.h>

#include "glade-gtk.h"
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <string.h>

static void
restore_opacity (GtkWidget *w)
{
  GladeWidget *gw = glade_widget_get_from_gobject (w);

  if (gw)
    {
      GladeProperty *prop = glade_widget_get_property (gw, "opacity");
      glade_property_sync (prop);
    }
  else
    gtk_widget_set_opacity (w, 1);
}

static void
overlay_children_set_visible (GtkWidget *overlay,
                              GtkWidget *widget)
{
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (overlay));
  GList *l, *children;

  children = gtk_container_get_children (GTK_CONTAINER (overlay));

  if (!widget || widget == child)
    restore_opacity (child);
  else
    gtk_widget_set_opacity (child, .24);

  for (l = children; l; l = g_list_next (l))
    {
      GtkWidget *w = l->data;

      if (w == child)
        continue;

      if (!widget)
        {
          gtk_widget_set_visible (w, TRUE);
          gtk_widget_set_opacity (w, .4);
        }
      else
        {
          if (w == widget)
            {
              gtk_widget_set_visible (w, TRUE);
              restore_opacity (w);
            }
          else
            gtk_widget_set_visible (w, FALSE);
        }
    }
  
  g_list_free (children);
}

static GtkWidget *
get_overlay_children (GtkWidget *deep_child, GtkWidget *overlay)
{
  GtkWidget *parent = deep_child;
  
  while (parent)
    {
      if (parent == overlay)
        return deep_child;

      deep_child = parent;
      parent = gtk_widget_get_parent (parent);
    }

  return NULL;
}

static void
on_project_selection_changed (GladeProject *project, GtkWidget *overlay)
{
  GtkWidget *selected = NULL;
  GList *l;
  
  for (l = glade_project_selection_get (project); l; l = g_list_next (l))
    {
      GtkWidget *selection;
      
      if (GTK_IS_WIDGET (l->data) && (selection = GTK_WIDGET (l->data)) &&
          overlay != selection)
        selected = get_overlay_children (selection, overlay);

      if (selected)
        break;
    }

  overlay_children_set_visible (overlay, selected);
}

static void 
on_widget_project_notify (GObject *gobject,
                          GParamSpec *pspec,
                          GladeProject *old_project)
{
  GladeWidget *gwidget = GLADE_WIDGET (gobject);
  GladeProject *project = glade_widget_get_project (gwidget);
  GObject *object = glade_widget_get_object (gwidget);

  if (old_project)
    g_signal_handlers_disconnect_by_func (old_project, on_project_selection_changed, object);

  g_signal_handlers_disconnect_by_func (gwidget, on_widget_project_notify, old_project);
  
  g_signal_connect_object (gwidget, "notify::project",
                           G_CALLBACK (on_widget_project_notify),
                           project, 0);

  if (project)
    g_signal_connect_object (project, "selection-changed",
                             G_CALLBACK (on_project_selection_changed),
                             object, 0);
}

void
glade_gtk_overlay_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *object,
                               GladeCreateReason   reason)
{
  if (reason == GLADE_CREATE_USER)
    gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());

  on_widget_project_notify (G_OBJECT (glade_widget_get_from_gobject (object)),
                            NULL, NULL);
}

gboolean
glade_gtk_overlay_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_WIDGET (child))
    {
      if (user_feedback)
	{
	  GladeWidgetAdaptor *widget_adaptor = 
	    glade_widget_adaptor_get_by_type (GTK_TYPE_WIDGET);

	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 ONLY_THIS_GOES_IN_THAT_MSG,
				 glade_widget_adaptor_get_title (widget_adaptor),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_overlay_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  gchar *special_type = g_object_get_data (child, "special-child-type");
  GtkWidget *bin_child;

  if ((special_type && !strcmp (special_type, "overlay")) ||
      ((bin_child = gtk_bin_get_child (GTK_BIN (object))) &&
       !GLADE_IS_PLACEHOLDER (bin_child)))
    {
      g_object_set_data (child, "special-child-type", "overlay");
      gtk_overlay_add_overlay (GTK_OVERLAY (object), GTK_WIDGET (child));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, object, child);
}

void
glade_gtk_overlay_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gchar *special_type = g_object_get_data (child, "special-child-type");

  if (special_type && !strcmp (special_type, "overlay"))
    {
      g_object_set_data (child, "special-child-type", NULL);
      restore_opacity (GTK_WIDGET (child));
    }

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
  
  if (gtk_bin_get_child (GTK_BIN (object)) == NULL)
    gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
}


typedef struct
{
  GtkWidget *toplevel;
  gint x, y;
  GtkWidget *child;
  gint level;
} FindInContainerData;

static void
find_inside_container (GtkWidget *widget, FindInContainerData *data)
{
  gint x, y, w, h;

  if ((data->child && data->level) || !gtk_widget_get_mapped (widget))
    return;

  gtk_widget_translate_coordinates (data->toplevel, widget, data->x, data->y,
                                    &x, &y);
  
  /* Margins are not part of the widget allocation */
  w = gtk_widget_get_allocated_width (widget) + gtk_widget_get_margin_right (widget);
  h = gtk_widget_get_allocated_height (widget) + gtk_widget_get_margin_bottom (widget);

  if (x >= (0 - gtk_widget_get_margin_left (widget)) && x < w &&
      y >= (0 - gtk_widget_get_margin_top (widget)) && y < h)
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (widget);

      if (GTK_IS_CONTAINER (widget))
        {
          if (!data->level)
            data->child = NULL;

          data->level++;

          if (gwidget)
            data->child = glade_widget_get_child_at_position (gwidget, x, y);
          else
            gtk_container_forall (GTK_CONTAINER (widget),
                                  (GtkCallback) find_inside_container,
                                  data);
          data->level--;
        }

      if (data->level)
        {
          if (!data->child && (GLADE_IS_PLACEHOLDER (widget) || gwidget))
            data->child = widget;
        }
      else if ((!data->child || data->toplevel == gtk_widget_get_parent (data->child)) &&
               (GLADE_IS_PLACEHOLDER (widget) || gwidget))
        data->child = widget;
    }
}

GtkWidget *
glade_gtk_overlay_get_child_at_position (GladeWidgetAdaptor *adaptor,
                                         GtkWidget          *widget,
                                         gint                x,
                                         gint                y)
{
  if (!gtk_widget_get_mapped (widget))
    return NULL;

  if (x >= 0 && x <= gtk_widget_get_allocated_width (widget) &&
      y >= 0 && y <= gtk_widget_get_allocated_height (widget))
    {
      FindInContainerData data = {widget, x, y, NULL, 0};

      gtk_container_forall (GTK_CONTAINER (widget),
                            (GtkCallback) find_inside_container,
                            &data);

      return (data.child) ? data.child : widget;
    }
  
  return NULL;
}
