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
 *      Dynamic driver lists shared by Unixy system drivers.
 *
 *      By Peter Wang.
 *
 *      See readme.txt for copyright information.
 */

#include "allegro5/allegro.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_bitmap.h"
#include "allegro5/internal/aintern_system.h"

#if defined ALLEGRO_WITH_XWINDOWS
#ifndef ALLEGRO_RASPBERRYPI
#include "allegro5/platform/aintxglx.h"
#else
#include "allegro5/internal/aintern_raspberrypi.h"
#endif
#elif defined ALLEGRO_MIR
#include "allegro5/platform/aintmir.h"
#endif



/* This is a function each platform must define to register all available
 * system drivers.
 */
void _al_register_system_interfaces(void)
{
   ALLEGRO_SYSTEM_INTERFACE **add;
#if defined ALLEGRO_WITH_XWINDOWS && !defined ALLEGRO_RASPBERRYPI
   add = _al_vector_alloc_back(&_al_system_interfaces);
   *add = _al_system_xglx_driver();
#elif defined ALLEGRO_RASPBERRYPI
   add = _al_vector_alloc_back(&_al_system_interfaces);
   *add = _al_system_raspberrypi_driver();
#elif defined ALLEGRO_MIR
   add = _al_vector_alloc_back(&_al_system_interfaces);
   *add = _al_system_mir_driver();
#endif
}

