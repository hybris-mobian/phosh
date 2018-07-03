/*
 * Copyright (C) 2018 Purism SPC
 * SPDX-License-Identifier: GPL-3.0+
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * Somewhat based on mutter's src/backends/meta-monitor-manager.c
 */

#define G_LOG_DOMAIN "phosh-monitor-manager"

#include "monitor-manager.h"
#include "monitor/monitor.h"
#include <gdk/gdkwayland.h>

static void phosh_monitor_manager_display_config_init (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfigIface *iface);

typedef struct _PhoshMonitorManager
{
  PhoshDisplayDbusOrgGnomeMutterDisplayConfigSkeleton parent;

  GPtrArray *monitors;   /* Currently known monitors */

  int dbus_name_id;
  int serial;
} PhoshMonitorManager;

G_DEFINE_TYPE_WITH_CODE (PhoshMonitorManager,
                         phosh_monitor_manager,
                         PHOSH_DISPLAY_DBUS_TYPE_ORG_GNOME_MUTTER_DISPLAY_CONFIG_SKELETON,
                         G_IMPLEMENT_INTERFACE (
                           PHOSH_DISPLAY_DBUS_TYPE_ORG_GNOME_MUTTER_DISPLAY_CONFIG,
                           phosh_monitor_manager_display_config_init));

static gboolean
phosh_monitor_manager_handle_get_resources (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation)
{
  PhoshMonitorManager *self = PHOSH_MONITOR_MANAGER (skeleton);
  GVariantBuilder crtc_builder, output_builder, mode_builder;

  g_return_val_if_fail (self->monitors->len, FALSE);
  g_debug ("DBus %s\n", __func__);

  g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uxiiiiiuaua{sv})"));
  g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(uxiausauaua{sv})"));
  g_variant_builder_init (&mode_builder, G_VARIANT_TYPE ("a(uxuudu)"));

  /* CRTCs */
  for (int i = 0; i < self->monitors->len; i++) {
    PhoshMonitor *monitor = g_ptr_array_index (self->monitors, i);
    GVariantBuilder transforms;

    /* TODO: add transforms */
    g_variant_builder_init (&transforms, G_VARIANT_TYPE ("au"));
    g_variant_builder_add (&transforms, "u", 0);

    g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                           i,           /* ID */
                           i,           /* (gint64)crtc->crtc_id, */
                           monitor->x,
                           monitor->y,
                           monitor->width,
                           monitor->height,
                           0,           /* current_mode_index, */
                           0,           /* (guint32)crtc->transform, */
                           &transforms,
                           NULL         /* properties */);
  }

  /* outputs */
  for (int i = 0; i < self->monitors->len; i++) {
    PhoshMonitor *monitor = g_ptr_array_index (self->monitors, i);
    GVariantBuilder crtcs, modes, clones, properties;

    g_variant_builder_init (&crtcs, G_VARIANT_TYPE ("au"));
    g_variant_builder_add (&crtcs, "u", i /* possible_crtc_index */);
    g_variant_builder_init (&modes, G_VARIANT_TYPE ("au"));
    g_variant_builder_add (&modes, "u", 0 /* mode_index */);
    g_variant_builder_init (&clones, G_VARIANT_TYPE ("au"));
    g_variant_builder_add (&clones, "u", -1 /* possible_clone_index */);
    g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (&properties, "{sv}", "vendor",
                           g_variant_new_string (monitor->vendor));
    g_variant_builder_add (&properties, "{sv}", "product",
                           g_variant_new_string (monitor->product));
    g_variant_builder_add (&properties, "{sv}", "width-mm",
                           g_variant_new_int32 (monitor->width_mm));
    g_variant_builder_add (&properties, "{sv}", "height-mm",
                           g_variant_new_int32 (monitor->height_mm));

    g_variant_builder_add (&output_builder, "(uxiausauaua{sv})",
                           i, /* ID */
                           i, /* (gint64)output->winsys_id, */
                           i, /* crtc_index, */
                           &crtcs,
                           g_strdup_printf("DP%d", i), /* output->name */
                           &modes,
                           &clones,
                           &properties);
  }

  /* modes */
  for (int i = 0; i < self->monitors->len; i++) {
    PhoshMonitor *monitor = g_ptr_array_index (self->monitors, i);
    GArray *modes = monitor->modes;

    for (int k = 0; k < modes->len; k++) {
      PhoshMonitorMode *mode = &g_array_index (modes, PhoshMonitorMode, k);
      g_variant_builder_add (&mode_builder, "(uxuudu)",
                             k,            /* ID */
                             k,            /* (gint64)mode->mode_id, */
                             mode->width,  /* (guint32)mode->width, */
                             mode->height, /* (guint32)mode->height, */
                             (double)mode->refresh / 1000.0, /* (double)mode->refresh_rate, */
                             0             /* (guint32)mode->flags); */
      );
    }
  }

  phosh_display_dbus_org_gnome_mutter_display_config_complete_get_resources (
    skeleton,
    invocation,
    self->serial,
    g_variant_builder_end (&crtc_builder),
    g_variant_builder_end (&output_builder),
    g_variant_builder_end (&mode_builder),
    65535,  /* max_screen_width */
    65535   /* max_screen_height */
    );

  return TRUE;
}

static gboolean
phosh_monitor_manager_handle_change_backlight (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation,
  guint                  serial,
  guint                  output_index,
  gint                   value)
{
  g_debug ("Unimplemented DBus call %s\n", __func__);
  return FALSE;
}


static gboolean
phosh_monitor_manager_handle_get_crtc_gamma (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation,
  guint                  serial,
  guint                  crtc_id)
{
  g_debug ("Unimplemented DBus call %s\n", __func__);
  return FALSE;
}


static gboolean
phosh_monitor_manager_handle_set_crtc_gamma (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation,
  guint                  serial,
  guint                  crtc_id,
  GVariant              *red_v,
  GVariant              *green_v,
  GVariant              *blue_v)
{
  g_debug ("Unimplemented DBus call %s\n", __func__);
  return FALSE;
}

#define MODE_FORMAT "(siiddada{sv})"
#define MODES_FORMAT "a" MODE_FORMAT
#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_MONITORS_FORMAT "a" MONITOR_SPEC_FORMAT
#define LOGICAL_MONITOR_FORMAT "(iidub" LOGICAL_MONITOR_MONITORS_FORMAT "a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

static gboolean
phosh_monitor_manager_handle_get_current_state (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation)
{
  PhoshMonitorManager *self = PHOSH_MONITOR_MANAGER (skeleton);
  GVariantBuilder monitors_builder, logical_monitors_builder, properties_builder;

  g_debug ("DBus call %s\n", __func__);
  g_variant_builder_init (&monitors_builder,
                          G_VARIANT_TYPE (MONITORS_FORMAT));
  g_variant_builder_init (&logical_monitors_builder,
                          G_VARIANT_TYPE (LOGICAL_MONITORS_FORMAT));

  for (int i = 0; i < self->monitors->len; i++) {
    double scale;
    PhoshMonitor *monitor = g_ptr_array_index (self->monitors, i);
    GVariantBuilder modes_builder, supported_scales_builder, mode_properties_builder,
      monitor_properties_builder;

    g_variant_builder_init (&modes_builder, G_VARIANT_TYPE (MODES_FORMAT));

    for (int k = 0; k < monitor->modes->len; k++) {
      PhoshMonitorMode *mode = &g_array_index (monitor->modes, PhoshMonitorMode, k);

      g_variant_builder_init (&supported_scales_builder,
                              G_VARIANT_TYPE ("ad"));
      g_variant_builder_add (&supported_scales_builder, "d",
                             (double)monitor->scale);

      g_variant_builder_init (&mode_properties_builder,
                              G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&mode_properties_builder, "{sv}",
                             "is-current",
                             g_variant_new_boolean (TRUE));
      g_variant_builder_add (&mode_properties_builder, "{sv}",
                             "is-preferred",
                             g_variant_new_boolean (i == 0));

      g_variant_builder_add (&modes_builder, MODE_FORMAT,
                             g_strdup_printf ("%dx%d@%.0f", mode->width, mode->height,
                                              mode->refresh / 1000.0), /* mode_id, */
                             mode->width,
                             mode->height,
                             (double) mode->refresh / 1000.0,
                             scale,  /* (double) preferred_scale, */
                             &supported_scales_builder,
                             &mode_properties_builder);
    }

    g_variant_builder_init (&monitor_properties_builder,
                            G_VARIANT_TYPE ("a{sv}"));

    g_variant_builder_add (&monitor_properties_builder, "{sv}",
                           "is-builtin",
                           g_variant_new_boolean (TRUE));

    g_variant_builder_add (&monitors_builder, MONITOR_FORMAT,
                           g_strdup_printf ("DP%d", i),         /* monitor_spec->connector, */
                           monitor->vendor,                     /* monitor_spec->vendor, */
                           monitor->product,                    /* monitor_spec->product, */
                           g_strdup_printf ("00%d", i),         /* monitor_spec->serial, */
                           &modes_builder,
                           &monitor_properties_builder);
  }


  for (int i = 0; i < self->monitors->len; i++) {
    PhoshMonitor *monitor = g_ptr_array_index (self->monitors, i);
    GVariantBuilder logical_monitor_monitors_builder;

    g_variant_builder_init (&logical_monitor_monitors_builder,
                            G_VARIANT_TYPE (LOGICAL_MONITOR_MONITORS_FORMAT));
    g_variant_builder_add (&logical_monitor_monitors_builder,
                           MONITOR_SPEC_FORMAT,
                           g_strdup_printf ("DP%d", i),         /* monitor_spec->connector, */
                           monitor->vendor,                     /* monitor_spec->vendor, */
                           monitor->product,                    /* monitor_spec->product, */
                           g_strdup_printf ("00%d", i)          /* monitor_spec->serial, */
      );

    g_variant_builder_add (&logical_monitors_builder,
                           LOGICAL_MONITOR_FORMAT,
                           monitor->x,           /* logical_monitor->rect.x */
                           monitor->y,          /* logical_monitor->rect.y */
                           /* (double) logical_monitor->scale */
                           (double)monitor->scale,
                           0,                    /* logical_monitor->transform */
                           i == 0,               /* logical_monitor->is_primary */
                           &logical_monitor_monitors_builder,
                           NULL);
  }

  g_variant_builder_init (&properties_builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&properties_builder, "{sv}",
                         "supports-mirroring",
                         g_variant_new_boolean (FALSE));

  g_variant_builder_add (&properties_builder, "{sv}",
                         "layout-mode",
                         g_variant_new_uint32 (0));

  phosh_display_dbus_org_gnome_mutter_display_config_complete_get_current_state (
    skeleton,
    invocation,
    self->serial,
    g_variant_builder_end (&monitors_builder),
    g_variant_builder_end (&logical_monitors_builder),
    g_variant_builder_end (&properties_builder));

  return TRUE;
}


static gboolean
phosh_monitor_manager_handle_apply_monitors_config (
  PhoshDisplayDbusOrgGnomeMutterDisplayConfig *skeleton,
  GDBusMethodInvocation *invocation,
  guint                  serial,
  guint                  method,
  GVariant              *logical_monitor_configs_variant,
  GVariant              *properties_variant)
{
  g_debug ("Stubbed DBus call %s\n", __func__);

  /* Just do nothing for the moment */
  phosh_display_dbus_org_gnome_mutter_display_config_complete_apply_monitors_config (
    skeleton,
    invocation);

  return TRUE;
}


static void
phosh_monitor_manager_display_config_init (PhoshDisplayDbusOrgGnomeMutterDisplayConfigIface *iface)
{
  iface->handle_get_resources = phosh_monitor_manager_handle_get_resources;
  iface->handle_change_backlight = phosh_monitor_manager_handle_change_backlight;
  iface->handle_get_crtc_gamma = phosh_monitor_manager_handle_get_crtc_gamma;
  iface->handle_set_crtc_gamma = phosh_monitor_manager_handle_set_crtc_gamma;
  iface->handle_get_current_state = phosh_monitor_manager_handle_get_current_state;
  iface->handle_apply_monitors_config = phosh_monitor_manager_handle_apply_monitors_config;
}


static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  g_debug ("Acquired name %s\n", name);
}


static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  g_debug ("Lost or failed to acquire name %s\n", name);
}


static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  PhoshMonitorManager *self = user_data;

  /* We need to use Mutter's object path here to make gnome-settings happy */
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self),
                                    connection,
                                    "/org/gnome/Mutter/DisplayConfig",
                                    NULL);
}


static void
phosh_monitor_manager_finalize (GObject *object)
{
  PhoshMonitorManager *self = PHOSH_MONITOR_MANAGER (object);

  g_ptr_array_free (self->monitors, TRUE);
}


static void
phosh_monitor_manager_constructed (GObject *object)
{
  PhoshMonitorManager *self = PHOSH_MONITOR_MANAGER (object);

  self->dbus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                       "org.gnome.Mutter.DisplayConfig",
                                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                       G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                       on_bus_acquired,
                                       on_name_acquired,
                                       on_name_lost,
                                       g_object_ref (self),
                                       g_object_unref);
}


static void
phosh_monitor_manager_class_init (PhoshMonitorManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = phosh_monitor_manager_constructed;
  object_class->finalize = phosh_monitor_manager_finalize;
}


static void
phosh_monitor_manager_init (PhoshMonitorManager *self)
{
  self->monitors = g_ptr_array_new_with_free_func ((GDestroyNotify) (g_object_unref));
  self->serial = 1;
}

PhoshMonitorManager *
phosh_monitor_manager_new (void)
{
  return g_object_new (PHOSH_TYPE_MONITOR_MANAGER, NULL);
}


void
phosh_monitor_manager_add_monitor (PhoshMonitorManager *self, PhoshMonitor *monitor)
{
  g_ptr_array_add (self->monitors, monitor);
}


PhoshMonitor *
phosh_monitor_manager_get_monitor (PhoshMonitorManager *self, guint num)
{
  if (num >= self->monitors->len)
    return NULL;

  return g_ptr_array_index (self->monitors, num);
}


guint
phosh_monitor_manager_get_num_monitors (PhoshMonitorManager *self)
{
  return self->monitors->len;
}


#undef MONITOR_MODE_SPEC_FORMAT
#undef MONITOR_CONFIG_FORMAT
#undef MONITOR_CONFIGS_FORMAT
#undef LOGICAL_MONITOR_CONFIG_FORMAT