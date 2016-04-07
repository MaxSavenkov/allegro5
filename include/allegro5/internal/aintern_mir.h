#ifndef _ALLEGRO_INTERNAL_MIR_SYSTEM
#define _ALLEGRO_INTERNAL_MIR_SYSTEM

#include "allegro5/allegro.h"
#include <allegro5/internal/aintern_system.h>
#include <allegro5/internal/aintern_display.h>
#include <allegro5/internal/aintern_touch_input.h>

typedef struct MirSurface MirSurface;
typedef struct MirConnection MirConnection;
typedef struct MirDisplayConfiguration MirDisplayConfiguration;
typedef union MirEvent MirEvent;

typedef struct ALLEGRO_SYSTEM_MIR {
   ALLEGRO_SYSTEM system;

   MirConnection *connection;
   MirDisplayConfiguration *display_config;
} ALLEGRO_SYSTEM_MIR;

typedef struct ALLEGRO_DISPLAY_MIR_EXTRA
               ALLEGRO_DISPLAY_MIR_EXTRA;

typedef struct ALLEGRO_DISPLAY_MIR {
   ALLEGRO_DISPLAY display;
   ALLEGRO_DISPLAY_MIR_EXTRA *extra;
} ALLEGRO_DISPLAY_MIR;

ALLEGRO_SYSTEM_INTERFACE *_al_system_mir_driver(void);
ALLEGRO_DISPLAY_INTERFACE *_al_get_mir_display_driver(void);
ALLEGRO_KEYBOARD_DRIVER *_al_get_mir_keyboard_driver(void);
ALLEGRO_MOUSE_DRIVER * _al_get_mir_mouse_driver(void);
ALLEGRO_TOUCH_INPUT_DRIVER *_al_get_mir_touch_input_driver(void);

void al_mir_handle_event( MirSurface *surface, MirEvent const *event, void *context );

#endif
