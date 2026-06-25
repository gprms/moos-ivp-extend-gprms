/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: BHV_ZigLeg.h                                    */
/*    DATE: June 25th, 2026 (2.680 Lab 13, Assignment 3)    */
/*                                                          */
/*  An extension of BHV_Pulse. Instead of posting a visual  */
/*  pulse, it briefly produces an IvP objective function    */
/*  over heading, offset by a fixed angle from the heading  */
/*  latched when a sister waypoint behavior reports a new    */
/*  waypoint index -- producing a partial zig-zag leg.      */
/************************************************************/

#ifndef BHV_ZIGLEG_HEADER
#define BHV_ZIGLEG_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_ZigLeg : public IvPBehavior {
public:
  BHV_ZigLeg(IvPDomain);
  ~BHV_ZigLeg() {};

  bool         setParam(std::string, std::string);
  IvPFunction* onRunState();

protected: // Local Utility functions
  bool         updateInfoIn();      // pull heading, time, WPT_INDEX
  IvPFunction* buildHeadingFunction(); // ZAIC over the latched heading

protected: // Configuration parameters
  double  m_zig_angle;      // heading offset from latched heading (deg)
  double  m_zig_duration;   // seconds to hold the heading preference
  double  m_zig_delay;      // seconds after waypoint change before zig

protected: // State variables
  double  m_curr_heading;   // ownship heading from the info buffer
  double  m_curr_time;      // current time from the info buffer

  double  m_wpt_index;      // last waypoint index seen
  bool    m_wpt_index_set;  // have we seen any waypoint index yet?

  bool    m_zig_pending;    // a waypoint change is waiting to start a zig
  double  m_mark_time;      // time the pending waypoint change occurred

  bool    m_zig_active;     // currently producing a heading objective
  double  m_zig_start;      // time the active zig began
  double  m_zig_heading;    // the latched, offset heading to steer toward
};

#ifdef WIN32
   // Windows needs to explicitly specify functions to export from a dll
   #define IVP_EXPORT_FUNCTION __declspec(dllexport)
#else
   #define IVP_EXPORT_FUNCTION
#endif

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_ZigLeg(domain);}
}
#endif
