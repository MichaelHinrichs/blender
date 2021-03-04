/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WintabWin32 class.
 */

#pragma once

#include <memory>
#include <vector>
#include <wtypes.h>

#include "GHOST_Types.h"

#include <wintab.h>
// PACKETDATA and PACKETMODE modify structs in pktdef.h, so make sure they come first
#define PACKETDATA \
  (PK_BUTTONS | PK_NORMAL_PRESSURE | PK_ORIENTATION | PK_CURSOR | PK_X | PK_Y | PK_TIME)
#define PACKETMODE 0
#include <pktdef.h>

/* typedefs for Wintab functions to allow dynamic loading. */
typedef UINT(API *GHOST_WIN32_WTInfo)(UINT, UINT, LPVOID);
typedef BOOL(API *GHOST_WIN32_WTGet)(HCTX, LPLOGCONTEXTA);
typedef BOOL(API *GHOST_WIN32_WTSet)(HCTX, LPLOGCONTEXTA);
typedef HCTX(API *GHOST_WIN32_WTOpen)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL(API *GHOST_WIN32_WTClose)(HCTX);
typedef int(API *GHOST_WIN32_WTPacketsGet)(HCTX, int, LPVOID);
typedef int(API *GHOST_WIN32_WTQueueSizeGet)(HCTX);
typedef BOOL(API *GHOST_WIN32_WTQueueSizeSet)(HCTX, int);
typedef BOOL(API *GHOST_WIN32_WTEnable)(HCTX, BOOL);
typedef BOOL(API *GHOST_WIN32_WTOverlap)(HCTX, BOOL);

/* typedefs for Wintab and Windows resource management. */
typedef std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&::FreeLibrary)> unique_hmodule;
typedef std::unique_ptr<std::remove_pointer_t<HCTX>, GHOST_WIN32_WTClose> unique_hctx;

struct GHOST_WintabInfoWin32 {
  GHOST_TInt32 x, y;
  GHOST_TEventType type;
  GHOST_TButtonMask button;
  GHOST_TUns64 time;
  GHOST_TabletData tabletData;
};

class GHOST_Wintab {
 public:
  static GHOST_Wintab *loadWintab(HWND hwnd);

  /**
   * Enables Wintab context and brings it to the top of the overlap order.
   */
  void enable();

  /**
   * Puts Wintab context at bottom of overlap order and disables it.
   */
  void disable();

  /**
   * Brings Wintab context to the top of the overlap order.
   */
  void gainFocus();

  /**
   * Puts Wintab context at bottom of overlap order.
   */
  void loseFocus();

  /**
   * TODO
   */
  void leaveRange();

  /**
   * Handle Wintab coordinate changes when DisplayChange events occur.
   */
  void remapCoordinates();

  /*
   * TODO
   */
  void mapWintabToSysCoordinates(LONG x_in, LONG y_in, int &x_out, int &y_out);

  /**
   * Updates cached Wintab properties for current cursor.
   */
  void updateCursorInfo();

  /**
   * Handle Wintab info changes such as change in number of connected tablets.
   * \param lParam: LPARAM of the event.
   */
  void processInfoChange(LPARAM lParam);

  /**
   * TODO
   */
  bool devicesPresent();

  /**
   * Translate Wintab packets into GHOST_WintabInfoWin32 structs.
   * \param outWintabInfo: Storage to return resulting GHOST_WintabInfoWin32 structs.
   */
  void getInput(std::vector<GHOST_WintabInfoWin32> &outWintabInfo);

  /**
   * TODO
   */
  bool trustCoordinates();

  /**
   * TODO
   * Note: Only test coordiantes on button press, not release. This prevents issues when async
   * mismatch causes mouse movement to replay, which is only an issue while drawing.
   */
  bool testCoordinates(int sysX, int sysY, int wtX, int wtY);

  /**
  * TODO
  */
  GHOST_TabletData getLastTabletData();

 private:
  /** Wintab dll handle. */
  unique_hmodule m_handle;
  /** API functions */
  GHOST_WIN32_WTInfo m_fpInfo = nullptr;
  GHOST_WIN32_WTGet m_fpGet = nullptr;
  GHOST_WIN32_WTSet m_fpSet = nullptr;
  GHOST_WIN32_WTPacketsGet m_fpPacketsGet = nullptr;
  GHOST_WIN32_WTEnable m_fpEnable = nullptr;
  GHOST_WIN32_WTOverlap m_fpOverlap = nullptr;

  /** Stores the Tablet context if detected Tablet features using WinTab.dll. */
  unique_hctx m_context;
  /** Pressed button map. */
  GHOST_TUns8 m_buttons = 0;
  /** TODO */
  bool m_coordTrusted = false;

  /** Corodinate space defined by origin and extent. */
  struct Coord {
    /* Origin of coordinate space. */
    LONG org[2] = {0, 0};
    /** Extent of coordiante space. */
    int ext[2] = {1, 1};
  };

  /** Tablet input range. */
  Coord m_tabletCoord = {};
  /** System output range. */
  Coord m_systemCoord = {};

  LONG m_maxPressure = 0;
  LONG m_maxAzimuth = 0;
  LONG m_maxAltitude = 0;

  UINT m_numDevices = 0;
  /** Reusable buffer to read in Wintab packets. */
  std::vector<PACKET> m_pkts;
  GHOST_TabletData m_lastTabletData = GHOST_TABLET_DATA_NONE;

  GHOST_Wintab(HWND hwnd,
               unique_hmodule handle,
               GHOST_WIN32_WTInfo info,
               GHOST_WIN32_WTGet get,
               GHOST_WIN32_WTSet set,
               GHOST_WIN32_WTPacketsGet packetsGet,
               GHOST_WIN32_WTEnable enable,
               GHOST_WIN32_WTOverlap overlap,
               unique_hctx hctx,
               Coord tablet,
               Coord system,
               int queueSize);

  /**
   * Convert Wintab system mapped (mouse) buttons into Ghost button mask.
   * \param cursor: The Wintab cursor associated to the button.
   * \param physicalButton: The physical button ID to inspect.
   * \return The system mapped button.
   */
  GHOST_TButtonMask mapWintabToGhostButton(UINT cursor, WORD physicalButton);

  /**
   * TODO
   */
  static void modifyContext(LOGCONTEXT &lc);

  /**
   * TODO
   */
  static void extractCoordinates(LOGCONTEXT &lc, Coord &tablet, Coord &system);
};
