/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      X-Windows mouse module.
 *
 *      By Peter Wang.
 *
 *      Original by Michael Bukin.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro5/allegro.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_mouse.h"
#include "allegro5/internal/aintern_mir.h"

#include <mirclient/mir_toolkit/mir_client_library.h>

ALLEGRO_DEBUG_CHANNEL("mouse")

typedef struct ALLEGRO_MOUSE_MIR
{
   ALLEGRO_MOUSE parent;
   ALLEGRO_MOUSE_STATE state;
   int min_x, min_y;
   int max_x, max_y;
} ALLEGRO_MOUSE_MIR;

static bool mirmouse_installed = false;

/* the one and only mouse object */
static ALLEGRO_MOUSE_MIR the_mouse;

/* forward declarations */
static bool mirmouse_init(void);
static void mirmouse_exit(void);
static ALLEGRO_MOUSE *mirmouse_get_mouse(void);
static unsigned int mirmouse_get_mouse_num_buttons(void);
static unsigned int mirmouse_get_mouse_num_axes(void);
static bool mirmouse_set_mouse_xy(ALLEGRO_DISPLAY *,int x, int y);
static bool mirmouse_set_mouse_axis(int which, int z);
static void mirmouse_get_state(ALLEGRO_MOUSE_STATE *ret_state);

static void wheel_motion_handler(int x_button, ALLEGRO_DISPLAY *display);
static unsigned int x_button_to_al_button(unsigned int x_button);
static void generate_mouse_event(unsigned int type,
   int x, int y, int z, int w, float pressure,
   int dx, int dy, int dz, int dw,
   unsigned int button,
   ALLEGRO_DISPLAY *display);



/* the driver vtable */
#define MOUSEDRV_MIR AL_ID('M','I','R','R')

static ALLEGRO_MOUSE_DRIVER mousedrv_mir =
{
   MOUSEDRV_MIR,
   "",
   "",
   "Mir mouse",
   mirmouse_init,
   mirmouse_exit,
   mirmouse_get_mouse,
   mirmouse_get_mouse_num_buttons,
   mirmouse_get_mouse_num_axes,
   mirmouse_set_mouse_xy,
   mirmouse_set_mouse_axis,
   mirmouse_get_state
};



ALLEGRO_MOUSE_DRIVER *_al_get_mir_mouse_driver(void)
{
   return &mousedrv_mir;
}


static void scale_xy(int *x, int *y)
{
#ifdef ALLEGRO_RASPBERRYPI
   ALLEGRO_SYSTEM *s = al_get_system_driver();
   if (s && s->displays._size > 0) {
      ALLEGRO_DISPLAY **ref = _al_vector_ref(&s->displays, 0);
      ALLEGRO_DISPLAY *d = *ref;
      if (d) {
         ALLEGRO_DISPLAY_RASPBERRYPI *disp = (void *)d;
         /* Not sure what's a better approach than adding 0.5 to
          * get an accurate last pixel */
         *x = ((*x)+0.5) * d->w / disp->screen_width;
         *y = ((*y)+0.5) * d->h / disp->screen_height;
      }
   }
#else
   (void)x;
   (void)y;
#endif
}


/* xmouse_init:
 *  Initialise the driver.
 */
static bool mirmouse_init(void)
{
   if (mirmouse_installed)
      return false;

   memset(&the_mouse, 0, sizeof the_mouse);
   _al_event_source_init(&the_mouse.parent.es);

   mirmouse_installed = true;

   return true;
}



/* xmouse_exit:
 *  Shut down the mouse driver.
 */
static void mirmouse_exit(void)
{
   if (!mirmouse_installed)
      return;
   mirmouse_installed = false;

   _al_event_source_free(&the_mouse.parent.es);
}



/* xmouse_get_mouse:
 *  Returns the address of a ALLEGRO_MOUSE structure representing the mouse.
 */
static ALLEGRO_MOUSE *mirmouse_get_mouse(void)
{
   ASSERT(mirmouse_installed);

   return (ALLEGRO_MOUSE *)&the_mouse;
}



/* xmouse_get_mouse_num_buttons:
 *  Return the number of buttons on the mouse.
 */
static unsigned int mirmouse_get_mouse_num_buttons(void)
{
   // Not sure how to get number of buttons in Mir
   return 3;
}



/* xmouse_get_mouse_num_axes:
 *  Return the number of axes on the mouse.
 */
static unsigned int mirmouse_get_mouse_num_axes(void)
{
   ASSERT(mirmouse_installed);

   /* TODO: is there a way to detect whether z/w axis actually exist? */
   return 4;
}



/* xmouse_set_mouse_xy:
 *  Set the mouse position.  Return true if successful.
 */
static bool mirmouse_set_mouse_xy(ALLEGRO_DISPLAY *display, int x, int y)
{
   if (!mirmouse_installed)
      return false;

   // Not sure if possible
   return true;
}



/* xmouse_set_mouse_axis:
 *  Set the mouse wheel position.  Return true if successful.
 */
static bool mirmouse_set_mouse_axis(int which, int v)
{
   if (!mirmouse_installed)
      return false;

   // Not sure if possible
   return true;
}




/* xmouse_get_state:
 *  Copy the current mouse state into RET_STATE, with any necessary locking.
 */
static void mirmouse_get_state(ALLEGRO_MOUSE_STATE *ret_state)
{
   ASSERT(mirmouse_installed);

   _al_event_source_lock(&the_mouse.parent.es);
   {
      *ret_state = the_mouse.state;
   }
   _al_event_source_unlock(&the_mouse.parent.es);
}

/*
   Helper function called from _al_handle_mir_mouse_event.
   Assumes the_mouse.parent.es is locked already.

   Since Mir does not report WHICH button changed state,
   might as well iterate over the lot of them.
*/
static void mir_handle_mouse_button_event( const MirPointerEvent* event, ALLEGRO_DISPLAY* display )
{
   int new_buttons = mir_pointer_event_buttons(event);
   int prev_buttons = the_mouse.state.buttons;

   the_mouse.state.pressure = new_buttons ? 1.0 : 0.0; /* TODO */

   int i;
   for( i = 0; i < sizeof(MirPointerButtons) * 8; ++i )
   {
      int mask = 1 << i;
      if ( (prev_buttons & mask) != (new_buttons & mask) )
      {
         generate_mouse_event(
            (new_buttons & mask) ? ALLEGRO_EVENT_MOUSE_BUTTON_DOWN : ALLEGRO_EVENT_MOUSE_BUTTON_UP,
            the_mouse.state.x, the_mouse.state.y, the_mouse.state.z,
            the_mouse.state.w, the_mouse.state.pressure,
            0, 0, 0, 0,
            i, display);
      }
   }

   the_mouse.state.buttons = new_buttons;
}

/*
   Helper function called from _al_handle_mir_mouse_event.
   Assumes the_mouse.parent.es is locked already.
*/
static void mir_handle_mouse_enter_event( const MirPointerEvent* event, ALLEGRO_DISPLAY* display )
{
   generate_mouse_event(
      ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY,
      the_mouse.state.x, the_mouse.state.y, the_mouse.state.z,
      the_mouse.state.w, the_mouse.state.pressure,
      0, 0, 0, 0,
      0, display);
}

/*
   Helper function called from _al_handle_mir_mouse_event.
   Assumes the_mouse.parent.es is locked already.
*/
static void mir_handle_mouse_leave_event( const MirPointerEvent* event, ALLEGRO_DISPLAY* display )
{
   generate_mouse_event(
      ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY,
      the_mouse.state.x, the_mouse.state.y, the_mouse.state.z,
      the_mouse.state.w, the_mouse.state.pressure,
      0, 0, 0, 0,
      0, display);
}

/*
   Helper function called from _al_handle_mir_mouse_event.
   Assumes the_mouse.parent.es is locked already.
*/
static void mir_handle_mouse_move_event( const MirPointerEvent* event, ALLEGRO_DISPLAY* display, int dx, int dy, int dz, int dw )
{
   generate_mouse_event(
      ALLEGRO_EVENT_MOUSE_AXES,
      the_mouse.state.x, the_mouse.state.y, the_mouse.state.z,
      the_mouse.state.w, the_mouse.state.pressure,
      dx, dy, dz, dw,
      0, display);
}

void _al_handle_mir_mouse_event( const MirPointerEvent* event )
{
   ALLEGRO_SYSTEM *system = al_get_system_driver();
   ASSERT(system != NULL);

   ALLEGRO_DISPLAY **dptr = _al_vector_ref(&system->displays, 0);
   ALLEGRO_DISPLAY *display = *dptr;
   ASSERT(display != NULL);

   MirPointerAction action = mir_pointer_event_action(event);   

   _al_event_source_lock(&the_mouse.parent.es);
   {
      int nx = mir_pointer_event_axis_value(event, mir_pointer_axis_x);
      int ny = mir_pointer_event_axis_value(event, mir_pointer_axis_y);
      int nz = mir_pointer_event_axis_value(event, mir_pointer_axis_vscroll);
      int nw = mir_pointer_event_axis_value(event, mir_pointer_axis_hscroll);

      int dx = nx - the_mouse.state.x;
      int dy = ny - the_mouse.state.y;
      int dz = nz - the_mouse.state.z;
      int dw = nw - the_mouse.state.w;

      the_mouse.state.x = nx;
      the_mouse.state.y = ny;
      the_mouse.state.z = nz;
      the_mouse.state.w = nw;

      switch(action)
      {
         case mir_pointer_action_button_up:
         case mir_pointer_action_button_down:
            mir_handle_mouse_button_event(event, display);
            break;
         case mir_pointer_action_enter:
            mir_handle_mouse_enter_event(event, display);
            break;
         case mir_pointer_action_leave:
            mir_handle_mouse_leave_event(event, display);
            break;
         case mir_pointer_action_motion:
            mir_handle_mouse_move_event(event, display, dx, dy, dz, dw);
            break;
      }
   }
   _al_event_source_unlock(&the_mouse.parent.es);
}

static void generate_mouse_event(unsigned int type,
                                 int x, int y, int z, int w, float pressure,
                                 int dx, int dy, int dz, int dw,
                                 unsigned int button,
                                 ALLEGRO_DISPLAY *display)
{
   ALLEGRO_EVENT event;

   if (!_al_event_source_needs_to_generate_event(&the_mouse.parent.es))
      return;

   event.mouse.type = type;
   event.mouse.timestamp = al_get_time();
   event.mouse.display = display;
   event.mouse.x = x;
   event.mouse.y = y;
   event.mouse.z = z;
   event.mouse.w = w;
   event.mouse.dx = dx;
   event.mouse.dy = dy;
   event.mouse.dz = dz;
   event.mouse.dw = dw;
   event.mouse.button = button;
   event.mouse.pressure = pressure;
   _al_event_source_emit_event(&the_mouse.parent.es, &event);
}

/* vim: set sts=3 sw=3 et: */
