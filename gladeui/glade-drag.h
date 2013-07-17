/*
 * glade-drag.h
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

#ifndef _GLADE_DRAG_H_
#define _GLADE_DRAG_H_

#include "glade-widget-adaptor.h"

G_BEGIN_DECLS

#define GLADE_TYPE_DRAG                (glade_drag_get_type ())
#define GLADE_DRAG(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_DRAG, GladeDrag))
#define GLADE_IS_DRAG(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_DRAG))
#define GLADE_DRAG_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GLADE_TYPE_DRAG, GladeDragInterface))

typedef struct _GladeDrag GladeDrag;
typedef struct _GladeDragInterface GladeDragInterface;

struct _GladeDragInterface
{
  GTypeInterface parent_instance;
 
  gboolean (*can_drag)  (GladeDrag *source);
  
  gboolean (*can_drop)  (GladeDrag *dest,
                         GObject   *data);

  gboolean (*drop)      (GladeDrag *dest,
                         GObject   *data);

  void     (*highlight) (GladeDrag *dest,
                         gboolean   highlight);
  
  void (* padding1) (void);
  void (* padding2) (void);
  void (* padding3) (void);
  void (* padding4) (void); 
};

GType glade_drag_get_type (void) G_GNUC_CONST;

gboolean glade_drag_can_drag  (GladeDrag *source);

gboolean glade_drag_can_drop  (GladeDrag *dest,
                               GObject   *data);

gboolean glade_drag_drop      (GladeDrag *dest,
                               GObject   *data);

void     glade_drag_highlight (GladeDrag *dest,
                               gboolean   highlight);

G_END_DECLS

#endif /* _GLADE_DRAG_DEST_H_ */
