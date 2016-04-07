#include "allegro5/allegro.h"
#include "allegro5/internal/aintern_mir.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_keyboard.h"

#include <mirclient/mir_toolkit/mir_client_library.h>

#include <stdio.h>

ALLEGRO_DEBUG_CHANNEL("keyboard")

static ALLEGRO_KEYBOARD the_keyboard;
static ALLEGRO_KEYBOARD_STATE the_state;

static bool mir_init_keyboard(void)
{
    memset(&the_keyboard, 0, sizeof the_keyboard);
    _al_event_source_init(&the_keyboard.es);
    return true;
}

static void mir_exit_keyboard(void)
{
    _al_event_source_free(&the_keyboard.es);
}


static ALLEGRO_KEYBOARD *mir_get_keyboard(void)
{
    return &the_keyboard;
}

static bool mir_set_keyboard_leds(int leds)
{
    (void)leds;
    return false;
}

static char const *mir_keycode_to_name(int keycode)
{
   /*
      TODO: Mir uses X11 keycodes, so we should re-use code
      from xkeyboard.c probably?
   */

   static bool created = false;
   static char names[ALLEGRO_KEY_MAX][5];

   ASSERT(keycode >= 0 && keycode < ALLEGRO_KEY_MAX);

   if (!created) {
      int i;
      created = true;
      for (i = 0; i < ALLEGRO_KEY_MAX; i++) {
         snprintf(names[i], 5, "%d", i);
      }
   }

   return names[keycode];
}

static void mir_get_keyboard_state(ALLEGRO_KEYBOARD_STATE *ret_state)
{
   _al_event_source_lock(&the_keyboard.es);
   {
      *ret_state = the_state;
   }
   _al_event_source_unlock(&the_keyboard.es);
}

static ALLEGRO_KEYBOARD_DRIVER mir_keyboard_driver = {
    AL_ID('M','I','R','R'),
    "",
    "",
    "mir keyboard",
    mir_init_keyboard,
    mir_exit_keyboard,
    mir_get_keyboard,
    mir_set_keyboard_leds,
    mir_keycode_to_name,
    mir_get_keyboard_state
};

ALLEGRO_KEYBOARD_DRIVER *_al_get_mir_keyboard_driver(void)
{
    return &mir_keyboard_driver;
}

static void mir_keyboard_handle_event(ALLEGRO_DISPLAY *display,
   int scancode, int unichar, int modifiers, ALLEGRO_EVENT_TYPE event_type)
{
   ALLEGRO_EVENT event;

   ASSERT(display != NULL);
   ASSERT(scancode > 0);

   if (event_type == ALLEGRO_EVENT_KEY_UP) {
      _AL_KEYBOARD_STATE_CLEAR_KEY_DOWN(the_state, scancode);
   }
   else {
      _AL_KEYBOARD_STATE_SET_KEY_DOWN(the_state, scancode);
   }

   _al_event_source_lock(&the_keyboard.es);

   if (_al_event_source_needs_to_generate_event(&the_keyboard.es)) {

      event.keyboard.type = event_type;
      event.keyboard.timestamp = al_get_time();
      event.keyboard.display = display;
      event.keyboard.keycode = scancode;
      event.keyboard.unichar = unichar;
      event.keyboard.modifiers = modifiers;
      event.keyboard.repeat = event_type == ALLEGRO_EVENT_KEY_CHAR;

      _al_event_source_emit_event(&the_keyboard.es, &event);
   }

   _al_event_source_unlock(&the_keyboard.es);
}

void _al_handle_mir_keyboard_event( const MirKeyboardEvent* event )
{
   ALLEGRO_SYSTEM *system = al_get_system_driver();
   ASSERT(system != NULL);

   ALLEGRO_DISPLAY **dptr = _al_vector_ref(&system->displays, 0);
   ALLEGRO_DISPLAY *display = *dptr;
   ASSERT(display != NULL);

   MirKeyboardAction action = mir_keyboard_event_action(event);
   int keycode = mir_keyboard_event_key_code(event);
   int scancode = mir_keyboard_event_scan_code(event);
   MirInputEventModifiers modifiers = mir_keyboard_event_modifiers(event);

   int al_modifiers = 0;
   if ( modifiers & mir_input_event_modifier_alt ) al_modifiers |= ALLEGRO_KEYMOD_ALT;
   if ( modifiers & mir_input_event_modifier_shift ) al_modifiers |= ALLEGRO_KEYMOD_SHIFT;
   if ( modifiers & mir_input_event_modifier_ctrl ) al_modifiers |= ALLEGRO_KEYMOD_CTRL;
   if ( modifiers & mir_input_event_modifier_caps_lock ) al_modifiers |= ALLEGRO_KEYMOD_CAPSLOCK;
   if ( modifiers & mir_input_event_modifier_scroll_lock ) al_modifiers |= ALLEGRO_KEYMOD_SCROLLLOCK;
   if ( modifiers & mir_input_event_modifier_num_lock ) al_modifiers |= ALLEGRO_KEYMOD_NUMLOCK;
   // TODO: Other Mir modifiers?

   switch( action )
   {
      case mir_keyboard_action_down: mir_keyboard_handle_event( display, scancode, keycode, al_modifiers, ALLEGRO_EVENT_KEY_DOWN ); break;
      case mir_keyboard_action_up: mir_keyboard_handle_event( display, scancode, keycode, al_modifiers, ALLEGRO_EVENT_KEY_UP ); break;
      case mir_keyboard_action_repeat: mir_keyboard_handle_event( display, scancode, keycode, al_modifiers, ALLEGRO_EVENT_KEY_CHAR ); break;
   }
}

/* vim: set sts=3 sw=3 et: */
