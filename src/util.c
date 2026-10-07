/* gimp-raw-heic -- small helpers
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <gdk/gdk.h>
#endif

#include "rawheic.h"

gchar *
rawheic_file_path (GFile *file)
{
  return file ? g_file_get_path (file) : NULL;
}

/* Is Shift held right now?  Used to invert the "show dialog" choice. */
gboolean
rawheic_shift_pressed (void)
{
#ifdef G_OS_WIN32
  return (GetAsyncKeyState (VK_SHIFT) & 0x8000) != 0;
#else
  GdkDisplay *display = gdk_display_get_default ();

  if (! display)
    return FALSE;

  return (gdk_keymap_get_modifier_state (gdk_keymap_get_for_display (display)) &
          GDK_SHIFT_MASK) != 0;
#endif
}
