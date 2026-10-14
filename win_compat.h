#pragma once

// Constants missing from older Windows SDK headers. The real headers come first so the
// fallbacks only kick in when the SDK lacks them (several are enum values, not macros).

#include <windows.h>

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#ifndef LVS_EX_NOHSCROLL
#define LVS_EX_NOHSCROLL 0x10000000
#endif
#ifndef LVS_EX_INFOTIP
#define LVS_EX_INFOTIP 0x00000400
#endif

#ifndef DWMWA_TRANSITIONS_FORCEDISABLED
#define DWMWA_TRANSITIONS_FORCEDISABLED 3
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_DEFAULT 0
#define DWMWCP_DONOTROUND 1
#define DWMWCP_ROUND 2
#define DWMWCP_ROUNDSMALL 3
#endif

#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080
#endif

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
