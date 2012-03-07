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
 *      Android-specific header defines.
 *
 *      See readme.txt for copyright information.
 */


#ifndef ALLEGRO_ANDROID
   #error bad include
#endif

#include <fcntl.h>

#ifdef __cplusplus
extern "C" int main();
#else
extern int main();
#endif

#include "allegro5/platform/aintuthr.h"

struct ALLEGRO_EVENT_SOURCE_REAL;
void _android_check_mutex(struct ALLEGRO_EVENT_SOURCE_REAL *es);

/* Nothing left */