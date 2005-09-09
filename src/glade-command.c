/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Joaqu�n Cuenca Abela
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
 * Authors:
 *   Joaqu�n Cuenca Abela <e98cuenc@yahoo.com>
 *   Archit Baweja <bighead@users.sourceforge.net>
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-project.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-palette.h"
#include "glade-command.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-signal.h"
#include "glade-app.h"
#include "glade-fixed-manager.h"

typedef struct {
	GladeWidget      *widget;
	GladeWidget      *parent;
	GladePlaceholder *placeholder;
	GList            *pack_props;
} CommandData;

static GObjectClass* parent_class = NULL;

#define MAKE_TYPE(func, type, parent)			\
GType							\
func ## _get_type (void)				\
{							\
	static GType cmd_type = 0;			\
							\
	if (!cmd_type)					\
	{						\
		static const GTypeInfo info =		\
		{					\
			sizeof (type ## Class),		\
			(GBaseInitFunc) NULL,		\
			(GBaseFinalizeFunc) NULL,	\
			(GClassInitFunc) func ## _class_init,	\
			(GClassFinalizeFunc) NULL,	\
			NULL,				\
			sizeof (type),			\
			0,				\
			(GInstanceInitFunc) NULL	\
		};					\
							\
		cmd_type = g_type_register_static (parent, #type, &info, 0);	\
	}						\
							\
	return cmd_type;				\
}							\

static void
glade_command_finalize (GObject *obj)
{
        GladeCommand *cmd = (GladeCommand *) obj;
        g_return_if_fail (cmd != NULL);

	if (cmd->description)
		g_free (cmd->description);

        /* Call the base class dtor */
	(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

static gboolean
glade_command_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_class_init (GladeCommandClass *klass)
{
	GObjectClass* object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = glade_command_finalize;

	klass->undo_cmd = NULL;
	klass->execute_cmd = NULL;
	klass->unifies = glade_command_unifies;
	klass->collapse = glade_command_collapse;
}

/* compose the _get_type function for GladeCommand */
MAKE_TYPE (glade_command, GladeCommand, G_TYPE_OBJECT)


#define GLADE_MAKE_COMMAND(type, func)					\
static gboolean								\
func ## _undo (GladeCommand *me);					\
static gboolean								\
func ## _execute (GladeCommand *me);					\
static void								\
func ## _finalize (GObject *object);					\
static gboolean								\
func ## _unifies (GladeCommand *this, GladeCommand *other);		\
static void								\
func ## _collapse (GladeCommand *this, GladeCommand *other);		\
static void								\
func ## _class_init (gpointer parent_tmp, gpointer notused)		\
{									\
	GladeCommandClass *parent = parent_tmp;				\
	GObjectClass* object_class;					\
	object_class = G_OBJECT_CLASS (parent);				\
	parent->undo_cmd =  func ## _undo;				\
	parent->execute_cmd =  func ## _execute;			\
	parent->unifies =  func ## _unifies;				\
	parent->collapse =  func ## _collapse;				\
	object_class->finalize = func ## _finalize;			\
}									\
typedef struct {							\
	GladeCommandClass cmd;						\
} type ## Class;							\
static MAKE_TYPE(func, type, GLADE_TYPE_COMMAND)

/**************************************************/

/**
 * glade_command_undo:
 * @project: a #GladeProject
 *
 * Undoes the last command performed in @project.
 */
void
glade_command_undo (GladeProject *project)
{
	GList* undo_stack;
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	undo_stack = project->undo_stack;
	if (undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
		return;

	cmd = GLADE_COMMAND (prev_redo_item->data);
	class = GLADE_COMMAND_GET_CLASS (cmd);
	class->undo_cmd (cmd);

	project->prev_redo_item = prev_redo_item->prev;
}

/**
 * glade_command_redo:
 * @project: a #GladeProject
 *
 * Redoes the last undone command in @project.
 */
void
glade_command_redo (GladeProject *project)
{
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (project->undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
	{
		cmd = GLADE_COMMAND (project->undo_stack->data);
	}
	else
	{
		if (prev_redo_item->next == NULL)
			return;
		
		cmd = GLADE_COMMAND (g_list_next (prev_redo_item)->data);
	}

	g_assert (cmd != NULL);

	class = GLADE_COMMAND_GET_CLASS (cmd);
	class->execute_cmd (cmd);

	project->prev_redo_item = prev_redo_item ? prev_redo_item->next : project->undo_stack;
}

static void
glade_command_push_undo (GladeProject *project, GladeCommand *cmd)
{
	GList* tmp_redo_item;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_COMMAND (cmd));

	/* If there are no "redo" items, and the last "undo" item unifies with
	   us, then we collapse the two items in one and we're done */
	if (project->prev_redo_item != NULL && project->prev_redo_item->next == NULL)
	{
		GladeCommand* cmd1 = project->prev_redo_item->data;
		GladeCommandClass* klass = GLADE_COMMAND_GET_CLASS (cmd1);
		
		if (klass->unifies (cmd1, cmd))
		{
			klass->collapse (cmd1, cmd);
			g_object_unref (cmd);
			return;
		}
	}

	/* We should now free all the "redo" items */
	tmp_redo_item = g_list_next (project->prev_redo_item);
	while (tmp_redo_item)
	{
		g_assert (tmp_redo_item->data);
		g_object_unref (G_OBJECT (tmp_redo_item->data));
		tmp_redo_item = g_list_next (tmp_redo_item);
	}

	if (project->prev_redo_item)
	{
		g_list_free (g_list_next (project->prev_redo_item));
		project->prev_redo_item->next = NULL;
	}
	else
	{
		g_list_free (project->undo_stack);
		project->undo_stack = NULL;
	}

	/* and then push the new undo item */
	project->undo_stack = g_list_append (project->undo_stack, cmd);

	if (project->prev_redo_item == NULL)
		project->prev_redo_item = project->undo_stack;
	else
		project->prev_redo_item = g_list_next (project->prev_redo_item);

	glade_default_app_update_ui ();
}

/**************************************************/
/*******     GLADE_COMMAND_SET_PROPERTY     *******/
/**************************************************/

/* create a new GladeCommandSetProperty class.  Objects of this class will
 * encapsulate a "set property" operation */

typedef struct {
	GladeCommand parent;
	GList *sdata;
} GladeCommandSetProperty;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandSetProperty, glade_command_set_property);
#define GLADE_COMMAND_SET_PROPERTY_TYPE		(glade_command_set_property_get_type ())
#define GLADE_COMMAND_SET_PROPERTY(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetProperty))
#define GLADE_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetPropertyClass))
#define GLADE_IS_COMMAND_SET_PROPERTY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_PROPERTY_TYPE))
#define GLADE_IS_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_PROPERTY_TYPE))

/* Undo the last "set property command" */
static gboolean
glade_command_set_property_undo (GladeCommand *cmd)
{
	return glade_command_set_property_execute (cmd);
}

/**
 * Execute the set property command and revert it. IE, after the execution of 
 * this function cmd will point to the undo action
 */
static gboolean
glade_command_set_property_execute (GladeCommand *cmd)
{
	GladeCommandSetProperty* me = (GladeCommandSetProperty*) cmd;
	GList *l;

	g_return_val_if_fail (me != NULL, FALSE);

	for (l = me->sdata; l; l = l->next)
	{
		GValue new_value = { 0, };
		GCSetPropData *sdata = l->data;

		g_value_init (&new_value, G_VALUE_TYPE (sdata->new_value));
		g_value_copy (sdata->new_value, &new_value);
		
		/* store the current value for undo */
		if (sdata->old_value)
		{
			g_value_copy (sdata->old_value, sdata->new_value);
			g_value_unset (sdata->old_value);
			sdata->old_value = (g_free (sdata->old_value), NULL);
		}
		else
			g_value_copy (sdata->property->value, sdata->new_value);

		glade_property_set_value (sdata->property, &new_value);
		g_value_unset (&new_value);


	}
	return TRUE;
}

static void
glade_command_set_property_finalize (GObject *obj)
{
	GladeCommandSetProperty *me;
	GList *l;

	me = GLADE_COMMAND_SET_PROPERTY (obj);

	for (l = me->sdata; l; l = l->next)
	{
		GCSetPropData *sdata = l->data;
		
		if (sdata->property)
			g_object_unref (G_OBJECT (sdata->property));

		if (sdata->old_value)
		{
			if (G_VALUE_TYPE (sdata->old_value) != 0)
				g_value_unset (sdata->old_value);
			g_free (sdata->old_value);
		}
		if (G_VALUE_TYPE (sdata->new_value) != 0)
			g_value_unset (sdata->new_value);
		g_free (sdata->new_value);

	}
	glade_command_finalize (obj);
}

static gboolean
glade_command_set_property_unifies (GladeCommand *this, GladeCommand *other)
{

       /* XXX FIXME: This doesn't make sence with the reset dialog, when resetting properties
	* they should not unify, also; setting multiple properties should unify as well as long
	* as the same group is changing (this will help with drag/resize widget control).
	*
	*/
       GladeCommandSetProperty *cmd1;
       GladeCommandSetProperty *cmd2;

       if (GLADE_IS_COMMAND_SET_PROPERTY (this) && GLADE_IS_COMMAND_SET_PROPERTY (other))
       {
               cmd1 = (GladeCommandSetProperty*) this;
               cmd2 = (GladeCommandSetProperty*) other;
	       
               return (g_list_length (cmd1->sdata) == 1 &&
                      g_list_length (cmd2->sdata) == 1 &&
                       ((GCSetPropData *)cmd1->sdata->data)->property == 
                       ((GCSetPropData *)cmd2->sdata->data)->property);
       }
       return FALSE;
}

static void
glade_command_set_property_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (this) &&
			  GLADE_IS_COMMAND_SET_PROPERTY (other));

	g_free (this->description);
	this->description = other->description;
	other->description = NULL;

	glade_default_app_update_ui ();
}


#define MAX_UNDO_MENU_ITEM_VALUE_LEN	10
static gchar *
glade_command_set_property_description (GladeCommandSetProperty *me)
{
	GCSetPropData   *sdata;
	gchar *description = NULL;
	gchar *value_name;

	g_assert (me->sdata);

	if (g_list_length (me->sdata) > 1)
		description = g_strdup_printf (_("Setting multiple properties"));
	else 
	{
		sdata = me->sdata->data;
		value_name = glade_property_class_make_string_from_gvalue (sdata->property->class, 
									   sdata->new_value);
		if (!value_name || strlen (value_name) > MAX_UNDO_MENU_ITEM_VALUE_LEN
		    || strchr (value_name, '_')) {
			description = g_strdup_printf (_("Setting %s of %s"),
						       sdata->property->class->name,
						       sdata->property->widget->name);
		} else {
			description = g_strdup_printf (_("Setting %s of %s to %s"),
						       sdata->property->class->name,
						       sdata->property->widget->name, value_name);
		}
		g_free (value_name);
	}

	return description;
}


void
glade_command_set_properties_list (GladeProject *project, GList *props)
{
	GladeCommandSetProperty *me;
	GladeCommand  *cmd;
	GCSetPropData *sdata;
	GList         *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (props);

	me = (GladeCommandSetProperty*) g_object_new (GLADE_COMMAND_SET_PROPERTY_TYPE, NULL);
	cmd = GLADE_COMMAND (me);

	/* Ref all props */
	for (list = props; list; list = list->next)
	{
		sdata = list->data;
		g_object_ref (G_OBJECT (sdata->property));
	}

	me->sdata        = props;
	cmd->description = glade_command_set_property_description (me);
	g_assert (cmd->description);

	/* Push onto undo stack only if it executes successfully. */
	if (glade_command_set_property_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (GLADE_PROJECT (project),
					 GLADE_COMMAND (me));
	else
		/* No leaks on my shift! */
		g_object_unref (G_OBJECT (me));
}


void
glade_command_set_properties (GladeProperty *property, const GValue *old_value, const GValue *new_value, ...)
{
	GCSetPropData *sdata;
	GladeProperty *prop;
	GValue        *ovalue, *nvalue;
	GList         *list = NULL;
	va_list        vl;

	g_return_if_fail (GLADE_IS_PROPERTY (property));

	/* Add first set */
	sdata = g_new (GCSetPropData, 1);
	sdata->property = property;
	sdata->old_value = g_new0 (GValue, 1);
	sdata->new_value = g_new0 (GValue, 1);
	g_value_init (sdata->old_value, G_VALUE_TYPE (old_value));
	g_value_init (sdata->new_value, G_VALUE_TYPE (new_value));
	g_value_copy (old_value, sdata->old_value);
	g_value_copy (new_value, sdata->new_value);
	list = g_list_prepend (list, sdata);

	va_start (vl, new_value);
	while ((prop = va_arg (vl, GladeProperty *)) != NULL)
	{
		ovalue = va_arg (vl, GValue *);
		nvalue = va_arg (vl, GValue *);
		
		g_assert (GLADE_IS_PROPERTY (prop));
		g_assert (G_IS_VALUE (ovalue));
		g_assert (G_IS_VALUE (nvalue));

		sdata = g_new (GCSetPropData, 1);
		sdata->property = g_object_ref (G_OBJECT (prop));
		sdata->old_value = g_new0 (GValue, 1);
		sdata->new_value = g_new0 (GValue, 1);
		g_value_init (sdata->old_value, G_VALUE_TYPE (ovalue));
		g_value_init (sdata->new_value, G_VALUE_TYPE (nvalue));
		g_value_copy (ovalue, sdata->old_value);
		g_value_copy (nvalue, sdata->new_value);
		list = g_list_prepend (list, sdata);
	}	
	va_end (vl);

	glade_command_set_properties_list (property->widget->project, list);
}

void
glade_command_set_property (GladeProperty *property, const GValue* pvalue)
{
	glade_command_set_properties (property, property->value, pvalue, NULL);
}

/**************************************************/
/*******       GLADE_COMMAND_SET_NAME       *******/
/**************************************************/

/* create a new GladeCommandSetName class.  Objects of this class will
 * encapsulate a "set name" operation */
typedef struct {
	GladeCommand parent;

	GladeWidget *widget;
	gchar *old_name;
	gchar *name;
} GladeCommandSetName;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandSetName, glade_command_set_name);
#define GLADE_COMMAND_SET_NAME_TYPE		(glade_command_set_name_get_type ())
#define GLADE_COMMAND_SET_NAME(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_NAME_TYPE, GladeCommandSetName))
#define GLADE_COMMAND_SET_NAME_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_NAME_TYPE, GladeCommandSetNameClass))
#define GLADE_IS_COMMAND_SET_NAME(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_NAME_TYPE))
#define GLADE_IS_COMMAND_SET_NAME_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_NAME_TYPE))

/* Undo the last "set name command" */
static gboolean
glade_command_set_name_undo (GladeCommand *cmd)
{
	return glade_command_set_name_execute (cmd);
}

/**
 * Execute the set name command and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_set_name_execute (GladeCommand *cmd)
{
	GladeCommandSetName* me = GLADE_COMMAND_SET_NAME (cmd);
	char* tmp;

	g_return_val_if_fail (me != NULL, TRUE);
	g_return_val_if_fail (me->widget != NULL, TRUE);
	g_return_val_if_fail (me->name != NULL, TRUE);

	glade_widget_set_name (me->widget, me->name);
	
	tmp = me->old_name;
	me->old_name = me->name;
	me->name = tmp;

	return TRUE;
}

static void
glade_command_set_name_finalize (GObject *obj)
{
	GladeCommandSetName* me;

	g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (obj));

	me = GLADE_COMMAND_SET_NAME (obj);

	g_free (me->old_name);
	g_free (me->name);

	glade_command_finalize (obj);
}

static gboolean
glade_command_set_name_unifies (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *cmd1;
	GladeCommandSetName *cmd2;

	if (GLADE_IS_COMMAND_SET_NAME (this) && GLADE_IS_COMMAND_SET_NAME (other))
	{
		cmd1 = GLADE_COMMAND_SET_NAME (this);
		cmd2 = GLADE_COMMAND_SET_NAME (other);

		return (cmd1->widget == cmd2->widget);
	}

	return FALSE;
}

static void
glade_command_set_name_collapse (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *nthis = GLADE_COMMAND_SET_NAME (this);
	GladeCommandSetName *nother = GLADE_COMMAND_SET_NAME (other);

	g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (this) && GLADE_IS_COMMAND_SET_NAME (other));

	g_free (nthis->old_name);
	nthis->old_name = nother->old_name;
	nother->old_name = NULL;

	g_free (this->description);
	this->description = g_strdup_printf (_("Renaming %s to %s"), nthis->name, nthis->old_name);

	glade_default_app_update_ui ();
}

/* this function takes the ownership of name */
void
glade_command_set_name (GladeWidget *widget, const gchar* name)
{
	GladeCommandSetName *me;
	GladeCommand *cmd;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (name != NULL);

	me = g_object_new (GLADE_COMMAND_SET_NAME_TYPE, NULL);
	cmd = GLADE_COMMAND (me);

	me->widget = widget;
	me->name = g_strdup (name);
	me->old_name = g_strdup (widget->name);

	cmd->description = g_strdup_printf (_("Renaming %s to %s"), me->old_name, me->name);
	if (!cmd->description)
		return;

	if (glade_command_set_name_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (GLADE_PROJECT (widget->project), GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

/***************************************************
 * CREATE / DELETE
 **************************************************/

typedef struct {
	GladeCommand      parent;
	
	GList            *widgets;
	gboolean          create;
	gboolean          from_clipboard;
} GladeCommandCreateDelete;

GLADE_MAKE_COMMAND (GladeCommandCreateDelete, glade_command_create_delete);
#define GLADE_COMMAND_CREATE_DELETE_TYPE	(glade_command_create_delete_get_type ())
#define GLADE_COMMAND_CREATE_DELETE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDelete))
#define GLADE_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDeleteClass))
#define GLADE_IS_COMMAND_CREATE_DELETE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CREATE_DELETE_TYPE))
#define GLADE_IS_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_create_delete_undo (GladeCommand *cmd)
{
	return glade_command_create_delete_execute (cmd);
}

static gboolean
glade_command_create_execute (GladeCommandCreateDelete *me)
{
	GladeClipboard   *clipboard = glade_default_app_get_clipboard();
	CommandData      *cdata = NULL;
	GList            *list, *wlist = NULL;

	glade_default_app_selection_clear (FALSE);

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;

		if (cdata->parent)
		{
			if (cdata->placeholder)
			{
				glade_widget_replace
					(cdata->parent, 
					 G_OBJECT (cdata->placeholder), 
					 cdata->widget->object);
			}
			else if (cdata->parent->manager != NULL)
			{
				glade_fixed_manager_add_child
					(cdata->parent->manager, cdata->widget, FALSE);
			}
			else
			{
				glade_widget_class_container_add 
					(cdata->parent->widget_class,
					 cdata->parent->object,
					 cdata->widget->object);
				glade_widget_set_parent (cdata->widget, cdata->parent);
			}
		}
		
		if (me->from_clipboard)
		{
			wlist = g_list_prepend (wlist, cdata->widget);
		}
		else
		{
			glade_project_add_object 
				(GLADE_PROJECT (cdata->widget->project),
				 cdata->widget->object);
			glade_default_app_selection_add 
				(cdata->widget->object, TRUE);
		}
	}
	
	if (wlist) 
	{
		glade_clipboard_add (clipboard, wlist);
		g_list_free (wlist);
	}

	if (cdata)
		glade_widget_show (cdata->widget);
	
	return TRUE;
}

static gboolean
glade_command_delete_execute (GladeCommandCreateDelete *me)
{
	GladeClipboard   *clipboard = glade_default_app_get_clipboard();
	CommandData      *cdata;
	GList            *list, *wlist = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;

		if (cdata->parent)
		{
			if (cdata->placeholder)
			{
				glade_widget_replace
					(cdata->parent, cdata->widget->object, 
					 G_OBJECT (cdata->placeholder));
			}
			else if (cdata->parent->manager != NULL)
				glade_fixed_manager_remove_child
					(cdata->parent->manager, cdata->widget);
			else
				glade_widget_class_container_remove (cdata->parent->widget_class,
								     cdata->parent->object,
								     cdata->widget->object);
		}

		if (me->from_clipboard == TRUE) 
		{
			wlist = g_list_prepend (wlist, cdata->widget);
		}
		else
		{
			glade_project_remove_object 
				(GLADE_PROJECT (cdata->widget->project), cdata->widget->object);
		}

		glade_widget_hide (cdata->widget);
	}

	if (wlist) 
	{
		glade_clipboard_remove (clipboard, wlist);
		g_list_free (wlist);
	}

	return TRUE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_create_delete_execute (GladeCommand *cmd)
{
	GladeCommandCreateDelete *me = (GladeCommandCreateDelete*) cmd;
	gboolean retval;

	if (me->create)
		retval = glade_command_create_execute (me);
	else
		retval = glade_command_delete_execute (me);

	me->create = !me->create;

	return retval;
}

static void
glade_command_create_delete_finalize (GObject *obj)
{
	GladeCommandCreateDelete *cmd;
	CommandData              *cdata;
	GList                    *list;

	g_return_if_fail (GLADE_IS_COMMAND_CREATE_DELETE (obj));

	cmd = GLADE_COMMAND_CREATE_DELETE (obj);

	for (list = cmd->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		
		if (cdata->placeholder)
			g_object_unref (G_OBJECT (cdata->placeholder));

		if (cdata->widget)
			g_object_unref (G_OBJECT (cdata->widget));
	}
	g_list_free (cmd->widgets);
	
	glade_command_finalize (obj);
}

static gboolean
glade_command_create_delete_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_create_delete_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

/**
 * glade_command_delete:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_command_delete (GList *widgets)
{
	GladeClipboard           *clipboard = glade_default_app_get_clipboard();
	GladeCommandCreateDelete *me;
	GladeWidget              *widget = NULL;
	CommandData              *cdata;
	GList                    *list;

	g_return_if_fail (widgets != NULL);

 	me = g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
	me->create         = FALSE;
	me->from_clipboard = 
		(g_list_find (clipboard->selection, widgets->data) != NULL);


	/* internal children cannot be deleted. Notify the user. */
	for (list = widgets; list && list->data; list = list->next)
	{
		widget = list->data;
		if (widget->internal)
		{
			glade_util_ui_warn (glade_default_app_get_window(),
					    _("You cannot delete a widget internal to a composite widget."));
			return;
		}
	}

	for (list = widgets; list && list->data; list = list->next)
	{
		widget         = list->data;

		cdata          = g_new0 (CommandData, 1);
		cdata->widget  = g_object_ref (G_OBJECT (widget));
		cdata->parent  = glade_widget_get_parent (widget);

		/* !manager here */
		if (cdata->parent != NULL &&
		    cdata->parent->manager == NULL &&
		    glade_util_gtkcontainer_relation 
		    (cdata->parent, cdata->widget))
		{
			cdata->placeholder = 
				GLADE_PLACEHOLDER (glade_placeholder_new ());
			g_object_ref (G_OBJECT (cdata->placeholder));
			gtk_object_sink (GTK_OBJECT (cdata->placeholder));
		}
		me->widgets = g_list_prepend (me->widgets, cdata);
	}

	if (g_list_length (widgets) == 1)
		GLADE_COMMAND (me)->description =
			g_strdup_printf (_("Delete %s"), 
					 GLADE_WIDGET (widgets->data)->name);
	else
		GLADE_COMMAND (me)->description =
			g_strdup_printf (_("Delete multiple"));

	g_assert (widget);
	if (glade_command_create_delete_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (GLADE_PROJECT (widget->project), 
					 GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_create:
 * @class:		the class of the widget (GtkWindow or GtkButton)
 * @placeholder:	the placeholder which will be substituted by the widget
 * @project:            the project his widget belongs to.
 *
 * Creates a new widget of @class and put in place of the @placeholder
 * in the @project
 *
 * Returns the newly created widget.
 */
GladeWidget *
glade_command_create (GladeWidgetClass *class,
		      GladeWidget      *parent,
		      GladePlaceholder *placeholder,
		      GladeProject     *project)
{
	GladeCommandCreateDelete *me;
	CommandData              *cdata;
	GladeWidget              *widget;

	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	if (g_type_is_a (class->type, GTK_TYPE_WINDOW) == FALSE)
		g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);

 	me            = g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
	me->create    = TRUE;

	cdata         = g_new0 (CommandData, 1);
	cdata->parent = parent;
	if ((cdata->placeholder = placeholder) != NULL)
		g_object_ref (G_OBJECT (placeholder));
	
	me->widgets = g_list_append (me->widgets, cdata);

	if (parent && parent->manager != NULL)
		widget = glade_fixed_manager_create_child (parent->manager, class);
	else
		widget = glade_widget_new (parent, class, project);

	/* widget may be null, e.g. the user clicked cancel on a query */
	if ((cdata->widget = widget) == NULL)
	{
		g_object_unref (G_OBJECT (me));
		return NULL;
	}

	GLADE_COMMAND (me)->description  =
		g_strdup_printf (_("Create %s"), 
				 cdata->widget->name);

	if (glade_command_create_delete_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (project, GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));

	return widget;
}

typedef enum {
	GLADE_CUT,
	GLADE_COPY,
	GLADE_PASTE
} GladeCutCopyPasteType;

/**
 * Cut/Copy/Paste
 *
 * Following is the code to extend the GladeCommand Undo/Redo system to 
 * Clipboard functions.
 **/
typedef struct {
	GladeCommand           parent;
	GladeProject          *project;
	GList                 *widgets;
	GladeCutCopyPasteType  type;
	gboolean               from_clipboard;
	gboolean               initial_paste;
} GladeCommandCutCopyPaste;


GLADE_MAKE_COMMAND (GladeCommandCutCopyPaste, glade_command_cut_copy_paste);
#define GLADE_COMMAND_CUT_COPY_PASTE_TYPE		(glade_command_cut_copy_paste_get_type ())
#define GLADE_COMMAND_CUT_COPY_PASTE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CUT_COPY_PASTE_TYPE, GladeCommandCutCopyPaste))
#define GLADE_COMMAND_CUT_COPY_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CUT_COPY_PASTE_TYPE, GladeCommandCutCopyPasteClass))
#define GLADE_IS_COMMAND_CUT_COPY_PASTE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CUT_COPY_PASTE_TYPE))
#define GLADE_IS_COMMAND_CUT_COPY_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_paste_execute (GladeCommandCutCopyPaste *me)
{
	GladeProject       *project = glade_default_app_get_active_project ();
	CommandData        *cdata;
	GList              *list, *remove = NULL, *l;

	if (project && me->widgets)
	{
		glade_default_app_selection_clear (FALSE);

		for (list = me->widgets; list && list->data; list = list->next)
		{
			cdata  = list->data;
			remove = g_list_prepend (remove, cdata->widget);

			if (cdata->parent != NULL)
			{
				/* glade_command_paste ganauntees that if 
				 * there we are pasting to a placeholder, 
				 * there is only one widget.
				 */
				if (cdata->placeholder)
					glade_widget_replace
						(cdata->parent,
						 G_OBJECT (cdata->placeholder),
						 cdata->widget->object);
				else if (cdata->parent->manager != NULL) 
				{
					/* Paste at mouse position only once */
					glade_fixed_manager_add_child (cdata->parent->manager, cdata->widget,
								       me->initial_paste == FALSE);
				}
				else
				{
					glade_widget_class_container_add
						(cdata->parent->widget_class,
						 cdata->parent->object,
						 cdata->widget->object);
					glade_widget_set_parent (cdata->widget, 
								 cdata->parent);
				}

				/* Now that we've added, apply any packing props if nescisary. */
				for (l = cdata->pack_props; l; l = l->next)
				{
					GValue         value = { 0, };
					GladeProperty *saved_prop = l->data;
					GladeProperty *widget_prop = 
						glade_widget_get_pack_property (cdata->widget,
										saved_prop->class->id);

					glade_property_get_value (saved_prop, &value);
					glade_property_set_value (widget_prop, &value);
					g_value_unset (&value);
				}

				
				if (me->initial_paste == FALSE) 
				{
					// Save the packing properties after the initial paste.
					g_assert (cdata->pack_props == NULL);
					for (l = cdata->widget->packing_properties; l; l = l->next)
						cdata->pack_props = 
							g_list_prepend (cdata->pack_props,
									glade_property_dup (GLADE_PROPERTY (l->data),
											    cdata->widget));

				}

				me->initial_paste = TRUE; // Mark it initialy pasted
				
			}

			/* Toplevels get pasted to the active project */
			if (me->from_clipboard && 
			    GTK_WIDGET_TOPLEVEL (cdata->widget->object))
				glade_project_add_object 
					(project, cdata->widget->object);
			else
				glade_project_add_object
					(GLADE_PROJECT (me->project), 
					 cdata->widget->object);
			
			glade_default_app_selection_add
				(cdata->widget->object, FALSE);

			glade_widget_show (cdata->widget);
		}
		glade_default_app_selection_changed ();

		if (remove)
		{
			glade_clipboard_remove (glade_default_app_get_clipboard(), remove);
			g_list_free (remove);
		}
	}
	return TRUE;
}

static gboolean
glade_command_cut_execute (GladeCommandCutCopyPaste *me)
{
	CommandData        *cdata;
	GList              *list, *add = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		add   = g_list_prepend (add, cdata->widget);

		if (cdata->parent)
		{
			if (cdata->placeholder)
				glade_widget_replace
					(cdata->parent,
					 cdata->widget->object,
					 G_OBJECT (cdata->placeholder));
			else if (cdata->parent->manager != NULL) 
				glade_fixed_manager_remove_child
					(cdata->parent->manager, cdata->widget);
			else
				glade_widget_class_container_remove
					(cdata->parent->widget_class,
					 cdata->parent->object,
					 cdata->widget->object);
		}
		
		glade_widget_hide (cdata->widget);
		glade_project_remove_object (GLADE_PROJECT (cdata->widget->project),
					     cdata->widget->object);
		
	}

	if (add)
	{
		glade_clipboard_add (glade_default_app_get_clipboard(), add);
		g_list_free (add);
	}
	return TRUE;
}

static gboolean
glade_command_copy_execute (GladeCommandCutCopyPaste *me)
{
	GList              *list, *add = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
		add = g_list_prepend (add, ((CommandData *)list->data)->widget);

	if (add)
	{
		glade_clipboard_add (glade_default_app_get_clipboard(), add);
		g_list_free (add);
	}
	return TRUE;
}

static gboolean
glade_command_copy_undo (GladeCommandCutCopyPaste *me)
{
	GList              *list, *remove = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
		remove = g_list_prepend (remove, ((CommandData *)list->data)->widget);

	if (remove)
	{
		glade_clipboard_remove (glade_default_app_get_clipboard(), remove);
		g_list_free (remove);
	}
	return TRUE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_cut_copy_paste_execute (GladeCommand *cmd)
{
	GladeCommandCutCopyPaste *me = (GladeCommandCutCopyPaste *) cmd;
	gboolean retval = FALSE;

	switch (me->type)
	{
	case GLADE_CUT:
		retval = glade_command_cut_execute (me);
		break;
	case GLADE_COPY:
		retval = glade_command_copy_execute (me);
		break;
	case GLADE_PASTE:
		retval = glade_command_paste_execute (me);
		break;
	}

	return retval;
}

static gboolean
glade_command_cut_copy_paste_undo (GladeCommand *cmd)
{
	GladeCommandCutCopyPaste *me = (GladeCommandCutCopyPaste *) cmd;
	gboolean retval = FALSE;

	switch (me->type)
	{
	case GLADE_CUT:
		retval = glade_command_paste_execute (me);
		break;
	case GLADE_COPY:
		retval = glade_command_copy_undo (me);
		break;
	case GLADE_PASTE:
		retval = glade_command_cut_execute (me);
		break;
	}
	return retval;
}

static void
glade_command_cut_copy_paste_finalize (GObject *obj)
{
	GladeCommandCutCopyPaste *me = GLADE_COMMAND_CUT_COPY_PASTE (obj);
	CommandData              *cdata;
	GList                    *list;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		if (cdata->widget)
			g_object_unref (cdata->widget);
		if (cdata->placeholder)
			g_object_unref (cdata->placeholder);
		if (cdata->pack_props)
		{
			g_list_foreach (cdata->pack_props, (GFunc)g_object_unref, NULL);
			g_list_free (cdata->pack_props);
		}
		g_free (cdata);
	}
	g_list_free (me->widgets);
	glade_command_finalize (obj);
}

static gboolean
glade_command_cut_copy_paste_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_cut_copy_paste_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_cut_copy_paste_common (GList                 *widgets,
				     GladeWidget           *parent,
				     GladePlaceholder      *placeholder,
				     GladeCutCopyPasteType  type)
{
	GladeCommandCutCopyPaste *me;
	CommandData              *cdata;
	GladeWidget              *widget = NULL, *child;
	GList                    *l, *list, *children, *placeholders = NULL;
	gchar                    *fmt = NULL;

	g_return_if_fail (widgets && widgets->data);

	/* Some prelimenary error checking here */
	switch (type) {
	case GLADE_CUT:
	case GLADE_COPY:
		for (list = widgets; list && list->data; list = list->next)
		{
			widget = list->data;
			if (widget->internal)
			{
				glade_util_ui_warn 
					(glade_default_app_get_window(), 
					 _("You cannot copy/cut a widget "
					   "internal to a composite widget."));
				return;
			}
		}
		fmt = (type == GLADE_CUT) ? _("Cut %s") : _("Copy %s");
		break;
	case GLADE_PASTE:

		for (list = widgets; 
		     list && list->data; 
		     list = list->next)
		{
			widget = list->data;
			if (parent == NULL &&
			    GTK_WIDGET_TOPLEVEL (widget->object) == FALSE)
			{
				glade_util_ui_warn 
					(glade_default_app_get_window(), 
					 _("Only top-level widgets can be "
					   "pasted without a parent."));
				return;
			}

			if (placeholder &&
			    GTK_WIDGET_TOPLEVEL (widget->object))
			{
				glade_util_ui_warn 
					(glade_default_app_get_window(), 
					 _("You cannot paste a top-level "
					   "to a placeholder."));
				return;
			}
		}
		
		fmt = _("Paste %s");
		break;
	}
	g_assert (fmt);
	
	widget = GLADE_WIDGET (widgets->data);

	/* And now we feel safe enough to go on and create */
	me                 = (GladeCommandCutCopyPaste *) 
		g_object_new (GLADE_COMMAND_CUT_COPY_PASTE_TYPE, NULL);
	me->project        = glade_widget_get_project (widget);
	me->type           = type;
	me->from_clipboard = (type == GLADE_PASTE);
	GLADE_COMMAND (me)->description = 
		g_strdup_printf (fmt, g_list_length (widgets) == 1 ? 
				 widget->name : _("multiple"));

	for (list = widgets; list && list->data; list = list->next)
	{
		widget             = list->data;

		cdata              = g_new0 (CommandData, 1);

		/* Widget */
		if (type == GLADE_COPY)
			cdata->widget = glade_widget_dup (widget);
		else
			cdata->widget = g_object_ref (G_OBJECT (widget));

		/* Parent */
		if (parent == NULL)
			cdata->parent = glade_widget_get_parent (widget);
		else if (type == GLADE_PASTE && placeholder)
			cdata->parent = glade_placeholder_get_parent (placeholder);
		else 
			cdata->parent = parent;

		/* Placeholder */
		if (type == GLADE_CUT)
		{
			if (cdata->parent && cdata->parent->manager == NULL &&
			    glade_util_gtkcontainer_relation
			    (cdata->parent, cdata->widget))
			{
				cdata->placeholder = GLADE_PLACEHOLDER 
					(glade_placeholder_new ());
				g_object_ref (G_OBJECT (cdata->placeholder));
				gtk_object_sink (GTK_OBJECT (cdata->placeholder));
			}
			else if (placeholder != NULL)
				cdata->placeholder = g_object_ref (placeholder);
		}
		else if (type == GLADE_PASTE && placeholder != NULL &&
			 g_list_length (widgets) == 1)
		{
			cdata->placeholder = g_object_ref (placeholder);
		}
		else if (type == GLADE_PASTE && cdata->parent &&
			 cdata->parent->manager == NULL &&
			 glade_util_gtkcontainer_relation (cdata->parent, widget))
		{
			if ((children = glade_widget_class_container_get_children
			     (cdata->parent->widget_class, cdata->parent->object)) != NULL)
			{
				for (l = children; l && l->data; l = l->next)
				{
					child = l->data;

					/* Find a placeholder for this child */
					if (GLADE_IS_PLACEHOLDER (child) &&
					    g_list_find (placeholders, child) == NULL)
					{
						placeholders = g_list_append (placeholders, child);
						cdata->placeholder = g_object_ref (child);
						break;
					}
				}
				g_list_free (children);
			}
		}

		/* Save a copy of packing properties on cut so we can
		 * re-apply them at undo time.
		 */
		if (type == GLADE_CUT)
		{
			for (l = cdata->widget->packing_properties; l; l = l->next)
				cdata->pack_props = 
					g_list_prepend (cdata->pack_props,
							glade_property_dup (GLADE_PROPERTY (l->data),
									    cdata->widget));
		}

		me->widgets = g_list_prepend (me->widgets, cdata);
	}

	if (type == GLADE_CUT)
		me->initial_paste = TRUE;
	else
		me->initial_paste = FALSE;

	/*
	 * Push it onto the undo stack only on success
	 */
	if (glade_command_cut_copy_paste_execute (GLADE_COMMAND (me)))
		glade_command_push_undo 
			(glade_default_app_get_active_project(),
			 GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));

	if (placeholders) 
		g_list_free (placeholders);
}

/**
 * glade_command_paste:
 * @widgets: a #GList
 * @parent: a #GladeWidget
 * @placeholder: a #GladePlaceholder
 *
 * TODO: write me
 */
void
glade_command_paste (GList *widgets, GladeWidget *parent, GladePlaceholder *placeholder)
{
	glade_command_cut_copy_paste_common (widgets, parent, placeholder, GLADE_PASTE);
}

/**
 * glade_command_cut:
 * @widgets: a #GList
 *
 * TODO: write me
 */
void
glade_command_cut (GList *widgets)
{
	glade_command_cut_copy_paste_common (widgets, NULL, NULL, GLADE_CUT);
}

/**
 * glade_command_copy:
 * @widgets: a #GList
 *
 * TODO: write me
 */
void
glade_command_copy (GList *widgets)
{
	glade_command_cut_copy_paste_common (widgets, NULL, NULL, GLADE_COPY);
}

/*********************************************************/
/*******     GLADE_COMMAND_ADD_SIGNAL     *******/
/*********************************************************/

/* create a new GladeCommandAddRemoveChangeSignal class.  Objects of this class will
 * encapsulate an "add or remove signal handler" operation */
typedef enum {
	GLADE_ADD,
	GLADE_REMOVE,
	GLADE_CHANGE
} GladeAddType;

typedef struct {
	GladeCommand   parent;

	GladeWidget   *widget;

	GladeSignal   *signal;
	GladeSignal   *new_signal;

	GladeAddType   type;
} GladeCommandAddSignal;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandAddSignal, glade_command_add_signal);
#define GLADE_COMMAND_ADD_SIGNAL_TYPE		(glade_command_add_signal_get_type ())
#define GLADE_COMMAND_ADD_SIGNAL(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_ADD_SIGNAL_TYPE, GladeCommandAddSignal))
#define GLADE_COMMAND_ADD_SIGNAL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_ADD_SIGNAL_TYPE, GladeCommandAddSignalClass))
#define GLADE_IS_COMMAND_ADD_SIGNAL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_ADD_SIGNAL_TYPE))
#define GLADE_IS_COMMAND_ADD_SIGNAL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_ADD_SIGNAL_TYPE))

static void
glade_command_add_signal_finalize (GObject *obj)
{
	GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (obj);
	glade_signal_free (cmd->signal);

	g_object_unref (cmd->widget);

	if (cmd->signal)
		glade_signal_free (cmd->signal);
	if (cmd->new_signal)
		glade_signal_free (cmd->new_signal);

	glade_command_finalize (obj);
}

static gboolean
glade_command_add_signal_undo (GladeCommand *this)
{
	return glade_command_add_signal_execute (this);
}

static gboolean
glade_command_add_signal_execute (GladeCommand *this)
{
	GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (this);
	GladeSignal           *temp;

	switch (cmd->type)
 	{
	case GLADE_ADD:
		glade_widget_add_signal_handler (cmd->widget, cmd->signal);
		cmd->type = GLADE_REMOVE;
		break;
	case GLADE_REMOVE:
		glade_widget_remove_signal_handler (cmd->widget, cmd->signal);
		cmd->type = GLADE_ADD;
		break;
	case GLADE_CHANGE:
		glade_widget_change_signal_handler (cmd->widget, 
						    cmd->signal, 
						    cmd->new_signal);
		temp            = cmd->signal;
		cmd->signal     = cmd->new_signal;
		cmd->new_signal = temp;
		break;
	default:
		break;
	}
	return TRUE;
}

static gboolean
glade_command_add_signal_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_add_signal_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_add_remove_change_signal (GladeWidget       *glade_widget,
					const GladeSignal *signal,
					const GladeSignal *new_signal,
					GladeAddType       type)
{
	GladeCommandAddSignal *me = GLADE_COMMAND_ADD_SIGNAL
		(g_object_new (GLADE_COMMAND_ADD_SIGNAL_TYPE, NULL));
	GladeCommand          *cmd = GLADE_COMMAND (me);

	/* we can only add/remove a signal to a widget that has been wrapped by a GladeWidget */
	g_assert (glade_widget != NULL);
	g_assert (glade_widget->project != NULL);

	me->widget       = g_object_ref(glade_widget);
	me->type         = type;
	me->signal       = glade_signal_clone (signal);
	me->new_signal   = new_signal ? 
		glade_signal_clone (new_signal) : NULL;
	
	cmd->description =
		g_strdup_printf (type == GLADE_ADD ? _("Add signal handler %s") :
				 type == GLADE_REMOVE ? _("Remove signal handler %s") :
				 _("Change signal handler %s"), 
				 signal->handler);
	
	if (glade_command_add_signal_execute (cmd))
		glade_command_push_undo (GLADE_PROJECT (glade_widget->project), cmd);
	else
		g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_add_signal:
 * @glade_widget: a #GladeWidget
 * @signal: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_add_signal (GladeWidget *glade_widget, const GladeSignal *signal)
{
	glade_command_add_remove_change_signal
		(glade_widget, signal, NULL, GLADE_ADD);
}

/**
 * glade_command_remove_signal:
 * @glade_widget: a #GladeWidget
 * @signal: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_remove_signal (GladeWidget *glade_widget, const GladeSignal *signal)
{
	glade_command_add_remove_change_signal
		(glade_widget, signal, NULL, GLADE_REMOVE);
}

/**
 * glade_command_change_signal:
 * @glade_widget: a #GladeWidget
 * @old: a #GladeSignal
 * @new: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_change_signal	(GladeWidget *glade_widget, 
				 const GladeSignal *old, 
				 const GladeSignal *new)
{
	glade_command_add_remove_change_signal 
		(glade_widget, old, new, GLADE_CHANGE);
}
