#include <mirclient/mir_toolkit/mir_client_library.h>

/* TODO: Currently, this code only handles single-window case, i.e.
   If a more complex cases are needed (for desktop, or maybe later table apps),
   a surface -> ALLEGRO_DISPLAY search must be performed, and corresponding
   display passed into all handler functions.
*/

void al_mir_handle_event( MirSurface *surface, MirEvent const *event, void *context )
{
   (void)surface;

   MirEventType type = mir_event_get_type(event);
   switch(type)
   {
      case mir_event_type_input:
      {
         const MirInputEvent *input_event = mir_event_get_input_event(event);
         MirInputEventType input_type = mir_input_event_get_type(input_event);
         switch(input_type)
         {
            case mir_input_event_type_key:
               _al_handle_mir_keyboard_event( mir_input_event_get_keyboard_event(input_event) );
               break;
            case mir_input_event_type_touch:
               _al_handle_mir_touch_event( mir_input_event_get_touch_event(input_event) );
               break;
            case mir_input_event_type_pointer:
               _al_handle_mir_mouse_event( mir_input_event_get_pointer_event(input_event) );
               break;
         }
      }
      break;

      case mir_event_type_resize:
      {
         const MirResizeEvent *resize_event = mir_event_get_resize_event(event);
         const int new_w = mir_resize_event_get_width(resize_event);
         const int new_h = mir_resize_event_get_height(resize_event);

         _al_handle_mir_resize_event(new_w, new_h, context);
      }
      break;

      case mir_event_type_orientation:
      {
         const MirOrientationEvent *orientation_event = mir_event_get_orientation_event(event);
         MirOrientation new_direction = mir_orientation_event_get_direction(orientation_event);

         _al_handle_mir_orientation_event(new_direction, context);
      }
      break;

      default:
         return;
   }
}
