//#include <sys/time.h>

#include "allegro5/allegro.h"
#include "allegro5/platform/aintmir.h"
#include "allegro5/internal/aintern_mir.h"
#include "allegro5/platform/aintunix.h"
#include "allegro5/platform/aintlnx.h"

#include <mirclient/mir_toolkit/mir_client_library.h>

//#include <signal.h>

ALLEGRO_DEBUG_CHANNEL("system")

static ALLEGRO_SYSTEM_INTERFACE *mir_vt;

static void print_mir_connection_error( MirConnection *connection, const char *details )
{
   const char *error = "Unknown error";
   if (connection != NULL)
       error = mir_connection_get_error_message(connection);
   ALLEGRO_ERROR( "Mir connection error. %s: %s\n", details, error);
}

static ALLEGRO_SYSTEM *mir_initialize(int flags)
{
   (void)flags;

   ALLEGRO_SYSTEM_MIR *s;

   s = al_calloc(1, sizeof *s);

   s->connection = mir_connect_sync(NULL, __PRETTY_FUNCTION__);
   if (s->connection == NULL || !mir_connection_is_valid(s->connection))
   {
       print_mir_connection_error( s->connection, "Failed to connect to default server" );
       return false;
   }

   s->display_config = mir_connection_create_display_config(s->connection);

   _al_vector_init(&s->system.displays, sizeof (ALLEGRO_DISPLAY_MIR *));

   _al_unix_init_time();

   //s->inhibit_screensaver = false;

   s->system.vt = mir_vt;

   return &s->system;
}

static void mir_shutdown_system(void)
{
   ALLEGRO_SYSTEM *s = al_get_system_driver();
   ALLEGRO_SYSTEM_MIR *smir = (void *)s;

   ALLEGRO_INFO("shutting down.\n");

   /* Close all open displays. */
   while (_al_vector_size(&s->displays) > 0) {
      ALLEGRO_DISPLAY **dptr = _al_vector_ref(&s->displays, 0);
      ALLEGRO_DISPLAY *d = *dptr;
      al_destroy_display(d);
   }
   _al_vector_free(&s->displays);

   mir_display_config_destroy(smir->display_config);
   mir_connection_release(smir->connection);

//   if (getenv("DISPLAY")) {
//      _al_thread_join(&spi->thread);
//      XCloseDisplay(spi->x11display);
//   }

//   bcm_host_deinit();

//   raise(SIGINT);

   al_free(smir);
}

static ALLEGRO_JOYSTICK_DRIVER *mir_get_joystick_driver(void)
{
    return 0;
}

static int mir_get_num_video_adapters(void)
{     
   ALLEGRO_SYSTEM *sys = al_get_system_driver();
   if ( !sys )
      return false;
   ALLEGRO_SYSTEM_MIR *smir = (ALLEGRO_SYSTEM_MIR *)sys;
   if ( !smir->display_config )
      return false;

   return smir->display_config->num_outputs;
}

static bool mir_get_monitor_info(int adapter, ALLEGRO_MONITOR_INFO *info)
{
   ALLEGRO_SYSTEM *sys = al_get_system_driver();
   if ( !sys )
      return false;
   ALLEGRO_SYSTEM_MIR *smir = (ALLEGRO_SYSTEM_MIR *)sys;
   if ( !smir->display_config )
      return false;
   if ( adapter >= smir->display_config->num_outputs )
      return false;

   MirDisplayOutput *output = &smir->display_config->outputs[adapter];
   if ( output->current_mode >= output->num_modes )
      return false;

   info->x1 = output->position_x;
   info->y1 = output->position_y;
   info->x2 = output->position_x + output->modes[ output->current_mode ].horizontal_resolution;
   info->y2 = output->position_x + output->modes[ output->current_mode ].vertical_resolution;

   return true;
}

static bool mir_get_cursor_position(int *ret_x, int *ret_y)
{
   // FIXME:
   //(void)ret_x;
   //(void)ret_y;
   return false;
}

static bool mir_inhibit_screensaver(bool inhibit)
{
   //ALLEGRO_SYSTEM_MIR *system = (void *)al_get_system_driver();
   return true;
}

static int mir_get_num_display_modes(void)
{
   ALLEGRO_SYSTEM *sys = al_get_system_driver();
   if ( !sys )
      return false;
   ALLEGRO_SYSTEM_MIR *smir = (ALLEGRO_SYSTEM_MIR *)sys;
   if ( !smir->display_config )
      return false;

   int adapter = al_get_new_display_adapter();
   if ( adapter < 0 || adapter >= smir->display_config->num_outputs )
      adapter = 0;

   MirDisplayOutput *output = &smir->display_config->outputs[adapter];

   return output->num_modes;
}

int translate_mir_format(MirPixelFormat fmt)
{
   switch(fmt)
   {
      case mir_pixel_format_invalid: return ALLEGRO_NUM_PIXEL_FORMATS;
      case mir_pixel_format_abgr_8888: return ALLEGRO_PIXEL_FORMAT_ABGR_8888;
      case mir_pixel_format_xbgr_8888: return ALLEGRO_PIXEL_FORMAT_XBGR_8888;
      case mir_pixel_format_argb_8888: return ALLEGRO_PIXEL_FORMAT_ARGB_8888;
      case mir_pixel_format_xrgb_8888: return ALLEGRO_PIXEL_FORMAT_XRGB_8888;
      case mir_pixel_format_bgr_888: return ALLEGRO_PIXEL_FORMAT_BGR_888;
      case mir_pixel_format_rgb_888: return ALLEGRO_PIXEL_FORMAT_RGB_888;
      case mir_pixel_format_rgb_565: return ALLEGRO_PIXEL_FORMAT_RGB_565;
      case mir_pixel_format_rgba_5551: return ALLEGRO_PIXEL_FORMAT_RGBA_5551;
      case mir_pixel_format_rgba_4444: return ALLEGRO_PIXEL_FORMAT_RGBA_4444;
      // TODO: Add support for big-endian pixel formats when Mir supports them
   }

   return ALLEGRO_NUM_PIXEL_FORMATS;
}

static ALLEGRO_DISPLAY_MODE *mir_get_display_mode(int mode, ALLEGRO_DISPLAY_MODE *dm)
{
   ALLEGRO_SYSTEM *sys = al_get_system_driver();
   if ( !sys )
      return false;
   ALLEGRO_SYSTEM_MIR *smir = (ALLEGRO_SYSTEM_MIR *)sys;
   if ( !smir->display_config )
      return false;

   int adapter = al_get_new_display_adapter();
   if ( adapter < 0 || adapter >= smir->display_config->num_outputs )
      adapter = 0;

   MirDisplayOutput *output = &smir->display_config->outputs[adapter];
   MirDisplayMode *out_mode = &output->modes[mode];

   dm->width = out_mode->horizontal_resolution;
   dm->height = out_mode->vertical_resolution;
   dm->format = translate_mir_format( output->current_format );
   dm->refresh_rate = out_mode->refresh_rate;

   return dm;
}

static ALLEGRO_MOUSE_CURSOR *mir_create_mouse_cursor(ALLEGRO_BITMAP *bmp, int focus_x_ignored, int focus_y_ignored)
{
   return 0;
}

static void mir_destroy_mouse_cursor(ALLEGRO_MOUSE_CURSOR *cursor)
{
   //ALLEGRO_MOUSE_CURSOR_RASPBERRYPI *pi_cursor = (void *)cursor;
   //al_destroy_bitmap(pi_cursor->bitmap);
   //al_free(pi_cursor);
}

/* Internal function to get a reference to this driver. */
ALLEGRO_SYSTEM_INTERFACE *_al_system_mir_driver(void)
{
   if (mir_vt)
      return mir_vt;

   mir_vt = al_calloc(1, sizeof *mir_vt);

   mir_vt->initialize = mir_initialize;
   mir_vt->get_display_driver = _al_get_mir_display_driver;
   mir_vt->get_keyboard_driver = _al_get_mir_keyboard_driver;
   mir_vt->get_mouse_driver = _al_get_mir_mouse_driver;
   mir_vt->get_touch_input_driver = _al_get_mir_touch_input_driver;
   mir_vt->get_joystick_driver = mir_get_joystick_driver;
   mir_vt->get_num_display_modes = mir_get_num_display_modes;
   mir_vt->get_display_mode = mir_get_display_mode;
   mir_vt->shutdown_system = mir_shutdown_system;
   mir_vt->get_num_video_adapters = mir_get_num_video_adapters;
   mir_vt->get_monitor_info = mir_get_monitor_info;
   mir_vt->create_mouse_cursor = mir_create_mouse_cursor;
   mir_vt->destroy_mouse_cursor = mir_destroy_mouse_cursor;
   mir_vt->get_cursor_position = mir_get_cursor_position;
   mir_vt->get_path = _al_unix_get_path;
   mir_vt->inhibit_screensaver = mir_inhibit_screensaver;

   return mir_vt;
}


/* vim: set sts=3 sw=3 et: */
