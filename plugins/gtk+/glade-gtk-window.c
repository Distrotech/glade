/*
 * glade-gtk-window.c - GladeWidgetAdaptor for GtkWindow
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-window-editor.h"
#include "glade-about-dialog-editor.h"
#include "glade-gtk.h"

#define GLADE_TAG_ACCEL_GROUPS "accel-groups"
#define GLADE_TAG_ACCEL_GROUP  "group"

static void
glade_gtk_window_read_accel_groups (GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *groups_node;
  GladeProperty *property;
  gchar *string = NULL;

  if ((groups_node =
       glade_xml_search_child (node, GLADE_TAG_ACCEL_GROUPS)) != NULL)
    {
      GladeXmlNode *node;

      for (node = glade_xml_node_get_children (groups_node);
           node; node = glade_xml_node_next (node))
        {
          gchar *group_name, *tmp;

          if (!glade_xml_node_verify (node, GLADE_TAG_ACCEL_GROUP))
            continue;

          group_name = glade_xml_get_property_string_required
              (node, GLADE_TAG_NAME, NULL);

          if (string == NULL)
            string = group_name;
          else if (group_name != NULL)
            {
              tmp =
                  g_strdup_printf ("%s%s%s", string, GPC_OBJECT_DELIMITER,
                                   group_name);
              string = (g_free (string), tmp);
              g_free (group_name);
            }
        }
    }

  if (string)
    {
      property = glade_widget_get_property (widget, "accel-groups");
      g_assert (property);

      /* we must synchronize this directly after loading this project
       * (i.e. lookup the actual objects after they've been parsed and
       * are present).
       */
      g_object_set_data_full (G_OBJECT (property),
                              "glade-loaded-object", string, g_free);
    }
}

void
glade_gtk_window_read_widget (GladeWidgetAdaptor * adaptor,
                              GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  /* Sync the icon mode */
  if (glade_widget_property_original_default (widget, "icon") == FALSE)
    glade_widget_property_set (widget, "glade-window-icon-name", FALSE);
  else
    glade_widget_property_set (widget, "glade-window-icon-name", TRUE);

  glade_gtk_window_read_accel_groups (widget, node);
}

static void
glade_gtk_window_write_accel_groups (GladeWidget * widget,
                                     GladeXmlContext * context,
                                     GladeXmlNode * node)
{
  GladeXmlNode *groups_node, *group_node;
  GList *groups = NULL, *list;
  GladeWidget *agroup;

  groups_node = glade_xml_node_new (context, GLADE_TAG_ACCEL_GROUPS);

  if (glade_widget_property_get (widget, "accel-groups", &groups))
    {
      for (list = groups; list; list = list->next)
        {
          agroup = glade_widget_get_from_gobject (list->data);
          group_node = glade_xml_node_new (context, GLADE_TAG_ACCEL_GROUP);
          glade_xml_node_append_child (groups_node, group_node);
          glade_xml_node_set_property_string (group_node, GLADE_TAG_NAME,
                                              glade_widget_get_name (agroup));
        }
    }

  if (!glade_xml_node_get_children (groups_node))
    glade_xml_node_delete (groups_node);
  else
    glade_xml_node_append_child (node, groups_node);

}


void
glade_gtk_window_write_widget (GladeWidgetAdaptor * adaptor,
                               GladeWidget * widget,
                               GladeXmlContext * context, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context,
                                                 node);

  glade_gtk_window_write_accel_groups (widget, context, node);
}

GladeEditable *
glade_gtk_window_create_editable (GladeWidgetAdaptor * adaptor,
				  GladeEditorPageType type)
{
  GladeEditable *editable;

  if (type == GLADE_PAGE_GENERAL &&
      /* Don't show all the GtkWindow properties for offscreen windows.
       *
       * We spoof the offscreen window type instead of using the real GType,
       * so don't check it by GType but use strcmp() instead.
       */
      strcmp (glade_widget_adaptor_get_name (adaptor), "GtkOffscreenWindow") != 0)
    {
      GType window_type = glade_widget_adaptor_get_object_type (adaptor);

      if (g_type_is_a (window_type, GTK_TYPE_ABOUT_DIALOG))
	editable = (GladeEditable *) glade_about_dialog_editor_new ();
      else
	editable = (GladeEditable *) glade_window_editor_new ();
    }
  else
    editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

  return editable;
}

void
glade_gtk_window_set_property (GladeWidgetAdaptor * adaptor,
			       GObject * object,
			       const gchar * id, const GValue * value)
{
  if (!strcmp (id, "glade-window-icon-name"))
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);

      glade_widget_property_set_sensitive (gwidget, "icon", FALSE, NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "icon-name", FALSE, NOT_SELECTED_MSG);

      if (g_value_get_boolean (value))
	glade_widget_property_set_sensitive (gwidget, "icon-name", TRUE, NULL);
      else
	glade_widget_property_set_sensitive (gwidget, "icon", TRUE, NULL);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}
