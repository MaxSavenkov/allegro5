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
 *      Internal header file for the MacOS X Allegro library port.
 *
 *      By Angelo Mottola.
 *
 *      See readme.txt for copyright information.
 */


#ifndef AINTOSX_H
#define AINTOSX_H

#ifdef __OBJC__

#include "allegro/platform/aintunix.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


#ifndef NSAppKitVersionNumber10_1
#define NSAppKitVersionNumber10_1       620
#endif


#define OSX_GFX_NONE                    0
#define OSX_GFX_WINDOW                  1
#define OSX_GFX_FULL                    2

#define BMP_EXTRA(bmp)                  ((BMP_EXTRA_INFO *)((bmp)->extra))

#define HID_MAX_DEVICES                 MAX_JOYSTICKS
#define HID_MOUSE                       0
#define HID_JOYSTICK                    1
#define HID_GAMEPAD                     2

#define HID_MAX_DEVICE_ELEMENTS         ((MAX_JOYSTICK_AXIS * MAX_JOYSTICK_STICKS) + MAX_JOYSTICK_BUTTONS)
#define HID_ELEMENT_BUTTON              0
#define HID_ELEMENT_AXIS                1
#define HID_ELEMENT_AXIS_PRIMARY_X      2
#define HID_ELEMENT_AXIS_PRIMARY_Y      3
#define HID_ELEMENT_STANDALONE_AXIS     4
#define HID_ELEMENT_HAT                 5



@interface AllegroAppDelegate : NSObject
- (BOOL)application: (NSApplication *)theApplication openFile: (NSString *)filename;
- (void)applicationDidFinishLaunching: (NSNotification *)aNotification;
- (void)applicationDidChangeScreenParameters: (NSNotification *)aNotification;
+ (void)app_main: (id)arg;
- (void)app_quit: (id)sender;
@end


@interface AllegroWindow : NSWindow
- (void)display;
- (void)miniaturize: (id)sender;
@end


@interface AllegroWindowDelegate : NSObject
- (BOOL)windowShouldClose: (id)sender;
- (void)windowDidDeminiaturize: (NSNotification *)aNotification;
@end


@interface AllegroView: NSQuickDrawView
- (void)resetCursorRects;
@end


typedef void RETSIGTYPE;


typedef struct BMP_EXTRA_INFO
{
   GrafPtr port;
} BMP_EXTRA_INFO;


typedef struct HID_ELEMENT
{
   int type;
   IOHIDElementCookie cookie;
   int max, min;
   int stick;
   int index;
   char *name;
} HID_ELEMENT;


typedef struct HID_DEVICE
{
   int type;
   char *manufacturer;
   char *product;
   int num_elements;
   HID_ELEMENT element[HID_MAX_DEVICE_ELEMENTS];
   IOHIDDeviceInterface **interface;
} HID_DEVICE;



void osx_event_handler(void);

void setup_direct_shifts(void);
void osx_init_fade_system(void);
void osx_fade_screen(int fade_in, double seconds);
void osx_qz_blit_to_self(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
void osx_qz_created_sub_bitmap(BITMAP *bmp, BITMAP *parent);
BITMAP *osx_qz_create_video_bitmap(int width, int height);
BITMAP *osx_qz_create_system_bitmap(int width, int height);
void osx_qz_destroy_video_bitmap(BITMAP *bmp);
int osx_setup_colorconv_blitter(void);
void osx_update_dirty_lines(void);

unsigned long osx_qz_write_line(BITMAP *bmp, int line);
void osx_qz_unwrite_line(BITMAP *bmp);
void osx_qz_acquire(BITMAP *bmp);
void osx_qz_release(BITMAP *bmp);

void osx_keyboard_handler(int pressed, NSEvent *event);
void osx_keyboard_modifiers(unsigned int new_mods);
void osx_keyboard_focused(int focused, int state);

void osx_mouse_handler(int dx, int dy, int dz, int buttons);

HID_DEVICE *osx_hid_scan(int type, int *num_devices);
void osx_hid_free(HID_DEVICE *devices, int num_devices);


AL_VAR(NSBundle *, osx_bundle);
AL_VAR(pthread_mutex_t, osx_event_mutex);
AL_VAR(pthread_mutex_t, osx_window_mutex);
AL_VAR(int, osx_gfx_mode);
AL_VAR(NSCursor *, osx_cursor);
AL_VAR(AllegroWindow *, osx_window);
AL_ARRAY(char, osx_window_title);
AL_VAR(CGDirectPaletteRef, osx_palette);
AL_VAR(int, osx_palette_dirty);
AL_VAR(int, osx_mouse_warped);
AL_VAR(int, osx_skip_mouse_move);
AL_VAR(int, osx_emulate_mouse_buttons);
AL_VAR(NSTrackingRectTag, osx_mouse_tracking_rect);
extern AL_METHOD(void, osx_window_close_hook, (void));


#endif

#endif
