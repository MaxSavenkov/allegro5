#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"
#include "allegro5/internal/aintern_opengl.h"
#include "allegro5/internal/aintern_vector.h"
#include "allegro5/internal/aintern_mir.h"
#include "allegro5/debug.h"

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <mirclient/mir_toolkit/mir_client_library.h>

ALLEGRO_DEBUG_CHANNEL("display")

static ALLEGRO_DISPLAY_INTERFACE *vt;

static EGLDisplay egl_display;
static EGLSurface egl_window;
static EGLContext egl_context;

typedef struct MIR_STATE
{
    MirSurface *surface;
} MIR_STATE;

static MIR_STATE mir_state;

static bool cursor_added = false;

struct ALLEGRO_DISPLAY_MIR_EXTRA {
};

static int pot(int n)
{
   int i = 1;
   while (i < n) {
      i = i * 2;
   }
   return i;
}

/* Helper to set up GL state as we want it. */
static void setup_gl(ALLEGRO_DISPLAY *d)
{
    ALLEGRO_OGL_EXTRAS *ogl = d->ogl_extras;

    if (ogl->backbuffer)
        _al_ogl_resize_backbuffer(ogl->backbuffer, d->w, d->h);
    else
        ogl->backbuffer = _al_ogl_create_backbuffer(d);
}

static const MirDisplayOutput *find_active_output(
    const MirDisplayConfiguration *conf)
{
   const MirDisplayOutput *output = NULL;
   int d;

   for (d = 0; d < (int)conf->num_outputs; d++)
   {
      const MirDisplayOutput *out = conf->outputs + d;

      if (out->used &&
          out->connected &&
          out->num_modes &&
          out->current_mode < out->num_modes)
      {
          output = out;
          break;
      }
   }

   return output;
}

static void print_mir_connection_error( MirConnection *connection, const char *details )
{
   const char *error = "Unknown error";
   if (connection != NULL)
       error = mir_connection_get_error_message(connection);
   ALLEGRO_ERROR( "Mir connection error. %s: %s\n", details, error);
}

static void print_mir_surface_error( const char *details )
{
   const char *error = "Unknown error";
   if (mir_state.surface != NULL)
       error = mir_surface_get_error_message(mir_state.surface);
   ALLEGRO_ERROR( "Mir surface error. %s: %s\n", details, error);
}

static void print_egl_error( const char *details )
{
   int e = eglGetError();
   const char *egl_error = "";

#define CASE(x) case x: egl_error = #x; break;

   switch(e)
   {
      CASE( EGL_SUCCESS );
      CASE( EGL_NOT_INITIALIZED );
      CASE( EGL_BAD_ACCESS );
      CASE( EGL_BAD_ALLOC  );
      CASE( EGL_BAD_ATTRIBUTE );
      CASE( EGL_BAD_CONTEXT );
      CASE( EGL_BAD_CONFIG );
      CASE( EGL_BAD_CURRENT_SURFACE );
      CASE( EGL_BAD_DISPLAY );
      CASE( EGL_BAD_SURFACE );
      CASE( EGL_BAD_MATCH );
      CASE( EGL_BAD_PARAMETER );
      CASE( EGL_BAD_NATIVE_PIXMAP );
      CASE( EGL_BAD_NATIVE_WINDOW );
      CASE( EGL_CONTEXT_LOST );
   }
#undef CASE

   ALLEGRO_ERROR( "EGL error. %s: %s\n", details, egl_error);
}


static void handle_mir_lifecycle_event(MirConnection *connection, MirLifecycleState state, void *context)
{
   ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)context;

   _al_event_source_lock(&display->es);
   if ( !_al_event_source_needs_to_generate_event(&display->es))
   {
      _al_event_source_unlock(&display->es);
      return;
   }

   switch(state)
   {
      case mir_lifecycle_state_will_suspend:
      {
         ALLEGRO_EVENT event;
         event.display.type = ALLEGRO_EVENT_DISPLAY_HALT_DRAWING;
         event.display.timestamp = al_current_time();
         _al_event_source_emit_event(&display->es, &event);
      }
      break;

      case mir_lifecycle_state_resumed:
      {
         ALLEGRO_EVENT event;
         event.display.type = ALLEGRO_EVENT_DISPLAY_RESUME_DRAWING;
         event.display.timestamp = al_current_time();
         _al_event_source_emit_event(&display->es, &event);
      }
      break;

      case mir_lifecycle_connection_lost:
      {
         ALLEGRO_EVENT event;
         event.display.type = ALLEGRO_EVENT_DISPLAY_DISCONNECTED;
         event.display.timestamp = al_current_time();
         _al_event_source_emit_event(&display->es, &event);
      }
      break;
   }

   _al_event_source_unlock(&display->es);
}

void _al_handle_mir_resize_event(int new_w, int new_h, void *context)
{
   ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)context;

   _al_event_source_lock(&display->es);
   if ( !_al_event_source_needs_to_generate_event(&display->es))
   {
      _al_event_source_unlock(&display->es);
      return;
   }

   ALLEGRO_EVENT event;
   event.display.type = ALLEGRO_EVENT_DISPLAY_RESIZE;
   event.display.width = new_w;
   event.display.height = new_h;
   event.display.timestamp = al_current_time();
   _al_event_source_emit_event(&display->es, &event);
   _al_event_source_unlock(&display->es);
}

static int translate_mir_orientation(MirOrientation orientation)
{
   switch(orientation)
   {
      case mir_orientation_normal: return ALLEGRO_DISPLAY_ORIENTATION_0_DEGREES;
      case mir_orientation_inverted: return ALLEGRO_DISPLAY_ORIENTATION_180_DEGREES;
      case mir_orientation_right: return ALLEGRO_DISPLAY_ORIENTATION_90_DEGREES;
      case mir_orientation_left: return ALLEGRO_DISPLAY_ORIENTATION_270_DEGREES;
   }

   return ALLEGRO_DISPLAY_ORIENTATION_UNKNOWN;
}

void _al_handle_mir_orientation_event(MirOrientation new_orientation, void *context)
{
   ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)context;

   _al_event_source_lock(&display->es);
   if ( !_al_event_source_needs_to_generate_event(&display->es))
   {
      _al_event_source_unlock(&display->es);
      return;
   }

   ALLEGRO_EVENT event;
   event.display.type = ALLEGRO_EVENT_DISPLAY_ORIENTATION;
   event.display.orientation = translate_mir_orientation( new_orientation );
   event.display.timestamp = al_current_time();
   _al_event_source_emit_event(&display->es, &event);
   _al_event_source_unlock(&display->es);
}

/*
   This Mir routines are designed to work on Ubuntu Touch devices.
   Which mostly means I do not care about multi-display cases, or
   even creating non-fullscreen windows. If you want to, you can
   extend this work :)
*/
static bool _mir_create_display(ALLEGRO_DISPLAY *display)
{
   ALLEGRO_SYSTEM_MIR *s = (ALLEGRO_SYSTEM_MIR *)al_get_system_driver();
   ALLEGRO_DISPLAY_MIR *d = (void *)display;
   ALLEGRO_EXTRA_DISPLAY_SETTINGS *eds = _al_get_new_display_settings();

   MirConnection *connection = s->connection;

   mir_state.surface = 0;

   egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (egl_display == EGL_NO_DISPLAY) {
      print_egl_error("Unable to get EGL display");
      return false;
   }

   int major, minor;
   if (!eglInitialize(egl_display, &major, &minor)) {
      print_egl_error("Unable to initialize EGL display");
      return false;
   }

   static EGLint attrib_list[] =
   {
      EGL_DEPTH_SIZE, 0,
      EGL_STENCIL_SIZE, 0,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // Require GLES2 mode - important!
      EGL_NONE
   };

   attrib_list[1] = eds->settings[ALLEGRO_DEPTH_SIZE];
   attrib_list[3] = eds->settings[ALLEGRO_STENCIL_SIZE];

   if (eds->settings[ALLEGRO_RED_SIZE] || eds->settings[ALLEGRO_GREEN_SIZE] ||
         eds->settings[ALLEGRO_BLUE_SIZE] ||
         eds->settings[ALLEGRO_ALPHA_SIZE]) {
      attrib_list[5] = eds->settings[ALLEGRO_RED_SIZE];
      attrib_list[7] = eds->settings[ALLEGRO_GREEN_SIZE];
      attrib_list[9] = eds->settings[ALLEGRO_BLUE_SIZE];
      attrib_list[11] = eds->settings[ALLEGRO_ALPHA_SIZE];
   }
   else if (eds->settings[ALLEGRO_COLOR_SIZE] == 16) {
      attrib_list[5] = 5;
      attrib_list[7] = 6;
      attrib_list[9] = 5;
      attrib_list[11] = 0;
   }

   EGLConfig config;
   int num_configs;

   if (!eglChooseConfig(egl_display, attrib_list, &config, 1, &num_configs)) {
      print_egl_error("Unable to choose EGL config");
      return false;
   }

   eglBindAPI(EGL_OPENGL_ES_API);

   int es_ver = (display->flags & ALLEGRO_PROGRAMMABLE_PIPELINE) ?
      2 : 1;

   static EGLint ctxattr[3] = {
      EGL_CONTEXT_CLIENT_VERSION, 0xDEADBEEF,
      EGL_NONE
   };

   ctxattr[1] = es_ver;

   egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, ctxattr);
   if (egl_context == EGL_NO_CONTEXT) {
      print_egl_error("Unable to create EGL context");
      return false;
   }

   // Get the first available pixel format
   // TODO: Maybe try to select the most appropriate? At least, honor requested color depth?
   MirPixelFormat pixel_format;
   unsigned int valid_formats;
   mir_connection_get_available_surface_formats(connection, &pixel_format, 1, &valid_formats);

   MirSurfaceSpec *spec = mir_connection_create_spec_for_normal_surface(connection, d->display.w, d->display.h, pixel_format);
   if ( spec == NULL)
   {
      print_mir_connection_error( connection, "Failed to create display spec" );
      return false;
   }

   if ( display->flags & ALLEGRO_FULLSCREEN )
      mir_surface_spec_set_state( spec, mir_surface_state_fullscreen );
   else if ( display->flags & ALLEGRO_MAXIMIZED )
      mir_surface_spec_set_state( spec, mir_surface_state_maximized );
   else if ( display->flags & ALLEGRO_MINIMIZED )
      mir_surface_spec_set_state( spec, mir_surface_state_minimized );

   mir_surface_spec_set_buffer_usage( spec, mir_buffer_usage_hardware );
   mir_surface_spec_set_shell_chrome(spec, mir_shell_chrome_low);

   mir_surface_spec_set_name(spec, __PRETTY_FUNCTION__);

   mir_state.surface = mir_surface_create_sync( spec );
   if( mir_state.surface == NULL )
   {
      print_mir_connection_error( connection, "Failed to create surface" );
      return false;
   }

   if( !mir_surface_is_valid(mir_state.surface) || mir_surface_get_error_message(mir_state.surface)[0] != '\0' )
   {
      print_mir_surface_error( "Failed to create surface" );
      return false;
   }

   egl_window = eglCreateWindowSurface(
      egl_display, config, mir_buffer_stream_get_egl_native_window(mir_surface_get_buffer_stream(mir_state.surface)), NULL);
   if (egl_window == EGL_NO_SURFACE) {
      print_egl_error("Failed to create window surface");
      return false;
   }

   if (!eglMakeCurrent(egl_display, egl_window, egl_window, egl_context)) {
      print_egl_error("Failed make surface current");
      return false;
   }

   mir_connection_set_lifecycle_event_callback( connection, handle_mir_lifecycle_event, display );
   mir_surface_set_event_handler(mir_state.surface, al_mir_handle_event, display );

   return true;
}

static ALLEGRO_DISPLAY *mir_create_display(int w, int h)
{
   ALLEGRO_DISPLAY_MIR *d = al_calloc(1, sizeof *d);
   ALLEGRO_DISPLAY *display = (void*)d;
   ALLEGRO_OGL_EXTRAS *ogl = al_calloc(1, sizeof *ogl);
   display->ogl_extras = ogl;
   display->vt = _al_get_mir_display_driver();
   display->flags = al_get_new_display_flags();

   ALLEGRO_SYSTEM_MIR *system = (void *)al_get_system_driver();

   /* Add ourself to the list of displays. */
   ALLEGRO_DISPLAY_MIR **add;
   add = _al_vector_alloc_back(&system->system.displays);
   *add = d;

   /* Each display is an event source. */
   _al_event_source_init(&display->es);

   display->extra_settings.settings[ALLEGRO_COMPATIBLE_DISPLAY] = 1;

   // On Ubuntu Touch, we need to always create fullscreen display,
   // but with desktop Mir, let user decide.
#ifdef ALLEGRO_UBUNTU_TOUCH
   if ( true )
#else
   if ( display->flags & ALLEGRO_FULLSCREEN_WINDOW )
#endif
   {
      ALLEGRO_SYSTEM_MIR *smir = (ALLEGRO_SYSTEM_MIR *)al_get_system_driver();
      if ( !smir->display_config )
         return NULL;

      int adapter = al_get_new_display_adapter();
      if ( adapter < 0 || adapter >= smir->display_config->num_outputs )
         adapter = 0;

      MirDisplayOutput *output = &smir->display_config->outputs[adapter];
      if ( output->current_mode >= output->num_modes )
         return NULL;

      MirDisplayMode *mode = &output->modes[output->current_mode];
      int importancy = ALLEGRO_REQUIRE;
      int supported_orientations = al_get_new_display_option( ALLEGRO_SUPPORTED_ORIENTATIONS, &importancy );

      w = mode->horizontal_resolution;
      h = mode->vertical_resolution;

      if ( supported_orientations & ALLEGRO_DISPLAY_ORIENTATION_LANDSCAPE )
      {
         if ( w < h )
         {
            int tmp = w;
            w = h; h = tmp;
         }
      }
      else if ( supported_orientations & ALLEGRO_DISPLAY_ORIENTATION_PORTRAIT )
      {
         if ( w > h )
         {
            int tmp = w;
            w = h; h = tmp;
         }
      }
   }

   display->w = w;
   display->h = h;

   if (!_mir_create_display(display)) {
      // FIXME: cleanup
      return NULL;
   }

   //al_grab_mouse(display);

   _al_ogl_manage_extensions(display);
   _al_ogl_set_extensions(ogl->extension_api);

   setup_gl(display);

   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);

   display->flags |= ALLEGRO_OPENGL;

   //if (al_is_mouse_installed() && !getenv("DISPLAY")) {
   //   _al_evdev_set_mouse_range(0, 0, display->w-1, display->h-1);
   //}

   //set_cursor_data(d, default_cursor, DEFAULT_CURSOR_WIDTH, DEFAULT_CURSOR_HEIGHT);

   return display;
}

static void mir_destroy_display(ALLEGRO_DISPLAY *d)
{
   ALLEGRO_DISPLAY_MIR *mirdisplay = (ALLEGRO_DISPLAY_MIR *)d;

   //hide_cursor(pidisplay);
   //delete_cursor_data(pidisplay);

   _al_set_current_display_only(d);

   while (d->bitmaps._size > 0) {
      ALLEGRO_BITMAP **bptr = (ALLEGRO_BITMAP **)_al_vector_ref_back(&d->bitmaps);
      ALLEGRO_BITMAP *b = *bptr;
      _al_convert_to_memory_bitmap(b);
   }

   _al_event_source_free(&d->es);

   ALLEGRO_SYSTEM_MIR *system = (void *)al_get_system_driver();
   _al_vector_find_and_delete(&system->system.displays, &d);

   eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(egl_display, egl_window);
   eglDestroyContext(egl_display, egl_context);
   eglTerminate(egl_display);

   //_al_mutex_lock(&system->lock);
   if ( mir_state.surface )
      mir_surface_release_sync(mir_state.surface);
//   if ( mir_state.connection )
//      mir_connection_release(mir_state.connection);

   mir_state.surface = 0;
   //_al_mutex_unlock(&system->lock);

//   if (system->mouse_grab_display == d) {
//      system->mouse_grab_display = NULL;
//   }
}

static bool mir_set_current_display(ALLEGRO_DISPLAY *d)
{
   (void)d;
   _al_ogl_update_render_state(d);
   return true;
}

static int mir_get_orientation(ALLEGRO_DISPLAY *d)
{   
   (void)d;

   if ( !mir_state.surface )
      return ALLEGRO_DISPLAY_ORIENTATION_0_DEGREES;

   MirOrientation orientation = mir_surface_get_orientation(mir_state.surface);

   return( translate_mir_orientation(orientation) );
}


/* We support only one window and only one OpenGL context, so all bitmaps
 * are compatible.
 */
static bool mir_is_compatible_bitmap(
   ALLEGRO_DISPLAY *display,
   ALLEGRO_BITMAP *bitmap
) {
    (void)display;
    (void)bitmap;
    return true;
}

/* Resizing is not possible. */
static bool mir_resize_display(ALLEGRO_DISPLAY *d, int w, int h)
{
    (void)d;
    (void)w;
    (void)h;
    return false;
}

/* The icon cannot be changed at runtime. */
static void mir_set_icons(ALLEGRO_DISPLAY *d, int num_icons, ALLEGRO_BITMAP *bitmaps[])
{
    (void)d;
    (void)num_icons;
    (void)bitmaps;
}

/* No windows title on mobile */
static void mir_set_window_title(ALLEGRO_DISPLAY *display, char const *title)
{
    (void)display;
    (void)title;
}

/* The window always spans the entire screen right now. */
static void mir_set_window_position(ALLEGRO_DISPLAY *display, int x, int y)
{
    (void)display;
    (void)x;
    (void)y;
}

/* The window cannot be constrained. */
static bool mir_set_window_constraints(ALLEGRO_DISPLAY *display,
   int min_w, int min_h, int max_w, int max_h)
{
   (void)display;
   (void)min_w;
   (void)min_h;
   (void)max_w;
   (void)max_h;
   return false;
}

/* Always fullscreen. */
static bool mir_set_display_flag(ALLEGRO_DISPLAY *display,
   int flag, bool onoff)
{
   (void)display;
   (void)flag;
   (void)onoff;
   return false;
}

static void mir_get_window_position(ALLEGRO_DISPLAY *display, int *x, int *y)
{
    (void)display;
    *x = 0;
    *y = 0;
}

/* The window cannot be constrained. */
static bool mir_get_window_constraints(ALLEGRO_DISPLAY *display,
   int *min_w, int *min_h, int *max_w, int *max_h)
{
   (void)display;
   (void)min_w;
   (void)min_h;
   (void)max_w;
   (void)max_h;
   return false;
}

static bool mir_wait_for_vsync(ALLEGRO_DISPLAY *display)
{
    (void)display;
    return false;
}

void mir_flip_display(ALLEGRO_DISPLAY *disp)
{
   (void)disp;
   eglSwapBuffers(egl_display, egl_window);
}

static void mir_update_display_region(ALLEGRO_DISPLAY *d, int x, int y,
                                       int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    mir_flip_display(d);
}

static bool mir_acknowledge_resize(ALLEGRO_DISPLAY *d)
{
   setup_gl(d);
   return true;
}

static bool mir_show_mouse_cursor(ALLEGRO_DISPLAY *display)
{
   (void)display;
   //ALLEGRO_DISPLAY_MIR *d = (void *)display;
   //hide_cursor(d);
   //show_cursor(d);
   return true;
}

static bool mir_hide_mouse_cursor(ALLEGRO_DISPLAY *display)
{
   (void)display;
   //ALLEGRO_DISPLAY_MIR *d = (void *)display;
   //hide_cursor(d);
   return true;
}

static bool mir_set_mouse_cursor(ALLEGRO_DISPLAY *display, ALLEGRO_MOUSE_CURSOR *cursor)
{
   (void)display;
   (void)cursor;
   return true;
}

static bool mir_set_system_mouse_cursor(ALLEGRO_DISPLAY *display, ALLEGRO_SYSTEM_MOUSE_CURSOR cursor_id)
{
   (void)display;
   (void)cursor_id;
   return true;
}

/* Obtain a reference to this driver. */
ALLEGRO_DISPLAY_INTERFACE *_al_get_mir_display_driver(void)
{
    if (vt)
        return vt;

    vt = al_calloc(1, sizeof *vt);

    vt->create_display = mir_create_display;
    vt->destroy_display = mir_destroy_display;
    vt->set_current_display = mir_set_current_display;
    vt->flip_display = mir_flip_display;
    vt->update_display_region = mir_update_display_region;
    vt->acknowledge_resize = mir_acknowledge_resize;
    vt->create_bitmap = _al_ogl_create_bitmap;
    vt->get_backbuffer = _al_ogl_get_backbuffer;
    vt->set_target_bitmap = _al_ogl_set_target_bitmap;

    vt->get_orientation = mir_get_orientation;

    vt->is_compatible_bitmap = mir_is_compatible_bitmap;
    vt->resize_display = mir_resize_display;
    vt->set_icons = mir_set_icons;
    vt->set_window_title = mir_set_window_title;
    vt->set_window_position = mir_set_window_position;
    vt->get_window_position = mir_get_window_position;
    vt->set_window_constraints = mir_set_window_constraints;
    vt->get_window_constraints = mir_get_window_constraints;
    vt->set_display_flag = mir_set_display_flag;
    vt->wait_for_vsync = mir_wait_for_vsync;

    vt->update_render_state = _al_ogl_update_render_state;

    _al_ogl_add_drawing_functions(vt);

    vt->set_mouse_cursor = mir_set_mouse_cursor;
    vt->set_system_mouse_cursor = mir_set_system_mouse_cursor;
    vt->show_mouse_cursor = mir_show_mouse_cursor;
    vt->hide_mouse_cursor = mir_hide_mouse_cursor;

    return vt;
}
