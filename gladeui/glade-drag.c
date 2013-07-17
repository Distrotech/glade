/*
 * glade-drag.c
 *
 * Copyright (C) 2013  Juan Pablo Ugarte
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 *
 */

#include "glade-drag.h"

G_DEFINE_INTERFACE (GladeDrag, glade_drag, G_TYPE_OBJECT);

static void
glade_drag_default_init (GladeDragInterface *iface)
{
}

gboolean
glade_drag_can_drag (GladeDrag *source)
{
  GladeDragInterface *iface;

  g_return_val_if_fail (GLADE_IS_DRAG (source), FALSE);
  iface = GLADE_DRAG_GET_INTERFACE (source);
  
  if (iface->can_drag)
    return iface->can_drag (source);
  else
    return FALSE;
}

gboolean
glade_drag_can_drop (GladeDrag *dest, GObject *data)
{
  GladeDragInterface *iface;

  g_return_val_if_fail (GLADE_IS_DRAG (dest), FALSE);
  iface = GLADE_DRAG_GET_INTERFACE (dest);

  if (iface->can_drop)
    return iface->can_drop (dest, data);
  else
    return FALSE;
}

gboolean
glade_drag_drop (GladeDrag *dest, GObject *data)
{
  GladeDragInterface *iface;

  g_return_val_if_fail (GLADE_IS_DRAG (dest), FALSE);
  iface = GLADE_DRAG_GET_INTERFACE (dest);

  if (iface->drop)
    return iface->drop (dest, data);
  else
    return FALSE;
}

void
glade_drag_highlight (GladeDrag *dest, gboolean highlight)
{
  GladeDragInterface *iface;

  g_return_if_fail (GLADE_IS_DRAG (dest));
  iface = GLADE_DRAG_GET_INTERFACE (dest);

  if (iface->highlight)
    iface->highlight (dest, !!highlight);
}
