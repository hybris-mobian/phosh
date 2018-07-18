/*
 * Copyright (C) 2018 Purism SPC
 * SPDX-License-Identifier: GPL-3.0+
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "phosh-wayland"

#include "config.h"
#include "phosh-wayland.h"

#include <gdk/gdkwayland.h>

/**
 * SECTION:phosh-wayland
 * @short_description: A wayland registry listener
 * @Title: PhoshWayland
 *
 * The #PhoshWayland singleton is responsible for listening to wayland
 * registry events registering the objects that show up there to make
 * them available to Phosh's other classes.
 */

typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct phosh_private *phosh_private;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct org_kde_kwin_idle *idle_manager;
  struct zwlr_input_inhibit_manager_v1 *input_inhibit_manager;
  struct gamma_control_manager *gamma_control_manager;
  struct wl_seat *wl_seat;
  struct xdg_wm_base *xdg_wm_base;
  GPtrArray *wl_outputs;
} PhoshWaylandPrivate;


typedef struct _PhoshWayland {
  GObject parent;
} PhoshWayland;


G_DEFINE_TYPE_WITH_PRIVATE (PhoshWayland, phosh_wayland, G_TYPE_OBJECT)


static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
  PhoshWayland *self = data;
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  struct wl_output *output;

  if (!strcmp (interface, "phosh_private")) {
      priv->phosh_private = wl_registry_bind (
        registry,
        name,
        &phosh_private_interface,
        1);
  } else  if (!strcmp (interface, zwlr_layer_shell_v1_interface.name)) {
      priv->layer_shell = wl_registry_bind (
        registry,
        name,
        &zwlr_layer_shell_v1_interface,
        1);
  } else if (!strcmp (interface, "wl_output")) {
    output = wl_registry_bind (
      registry,
      name,
      &wl_output_interface, 2);
    g_ptr_array_add (priv->wl_outputs, output);
  } else if (!strcmp (interface, "org_kde_kwin_idle")) {
    priv->idle_manager = wl_registry_bind (
      registry,
      name,
      &org_kde_kwin_idle_interface,
      1);
  } else if (!strcmp(interface, "wl_seat")) {
    priv->wl_seat = wl_registry_bind(
      registry, name, &wl_seat_interface,
      1);
  } else if (!strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name)) {
    priv->input_inhibit_manager = wl_registry_bind(
      registry,
      name,
      &zwlr_input_inhibit_manager_v1_interface,
      1);
  } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
    priv->xdg_wm_base = wl_registry_bind(
      registry,
      name,
      &xdg_wm_base_interface,
      1);
  } else if (!strcmp(interface, gamma_control_manager_interface.name)) {
    priv->gamma_control_manager = wl_registry_bind(
      registry,
      name,
      &gamma_control_manager_interface,
      1);
  }
}


static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
  // TODO
}


static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove
};


static void
phosh_wayland_constructed (GObject *object)
{
  PhoshWayland *self = PHOSH_WAYLAND (object);
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  guint num_outputs;
  GdkDisplay *gdk_display;

  G_OBJECT_CLASS (phosh_wayland_parent_class)->constructed (object);

  gdk_set_allowed_backends ("wayland");
  gdk_display = gdk_display_get_default ();
  priv->display = gdk_wayland_display_get_wl_display (gdk_display);

  if (priv->display == NULL) {
      g_error ("Failed to get display: %m\n");
  }

  priv->registry = wl_display_get_registry (priv->display);
  wl_registry_add_listener (priv->registry, &registry_listener, self);

  /* Wait until we have been notified about the wayland globals we require */
  num_outputs = priv->wl_outputs->len;
  if (!num_outputs || !priv->layer_shell || !priv->idle_manager ||
      !priv->input_inhibit_manager || !priv->phosh_private || !priv->xdg_wm_base)
    wl_display_roundtrip (priv->display);
  num_outputs = priv->wl_outputs->len;
  if (!num_outputs || !priv->layer_shell || !priv->idle_manager ||
      !priv->input_inhibit_manager || !priv->xdg_wm_base) {
    g_error ("Could not find needed globals\n"
             "outputs: %d, layer_shell: %p, seat: %p, "
             "inhibit: %p, xdg_wm: %p\n",
             num_outputs, priv->layer_shell, priv->idle_manager,
             priv->input_inhibit_manager, priv->xdg_wm_base);
  }
  if (!priv->phosh_private) {
    g_info ("Could not find phosh private interface, disabling some features\n");
  }
}


static void
phosh_wayland_dispose (GObject *object)
{
  PhoshWayland *self = PHOSH_WAYLAND (object);
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);

  g_clear_pointer (&priv->wl_outputs, g_ptr_array_unref);
  G_OBJECT_CLASS (phosh_wayland_parent_class)->dispose (object);
}


static void
phosh_wayland_class_init (PhoshWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = phosh_wayland_constructed;
  object_class->dispose = phosh_wayland_dispose;
}


static void
phosh_wayland_init (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  priv->wl_outputs = g_ptr_array_new ();
}


PhoshWayland *
phosh_wayland_get_default ()
{
  static PhoshWayland *instance;

  if (instance == NULL) {
    instance = g_object_new (PHOSH_TYPE_WAYLAND, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
  }
  return instance;
}


struct zwlr_layer_shell_v1 *
phosh_wayland_get_zwlr_layer_shell_v1 (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->layer_shell;
}


struct gamma_control_manager*
phosh_wayland_get_gamma_control_manager (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->gamma_control_manager;
}


struct wl_seat*
phosh_wayland_get_wl_seat (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->wl_seat;
}


struct xdg_wm_base*
phosh_wayland_get_xdg_wm_base (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->xdg_wm_base;
}


struct zwlr_input_inhibit_manager_v1*
phosh_wayland_get_zwlr_input_inhibit_manager_v1 (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->input_inhibit_manager;
}


struct org_kde_kwin_idle*
phosh_wayland_get_org_kde_kwin_idle (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->idle_manager;
}


struct phosh_private*
phosh_wayland_get_phosh_private (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->phosh_private;
}


GPtrArray*
phosh_wayland_get_wl_outputs (PhoshWayland *self)
{
  PhoshWaylandPrivate *priv = phosh_wayland_get_instance_private (self);
  return priv->wl_outputs;
}
