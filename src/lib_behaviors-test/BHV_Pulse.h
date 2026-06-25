/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: BHV_Pulse.h                                     */
/*    DATE: June 25th, 2026 (2.680 Lab 13, Assignment 1/2)  */
/*                                                          */
/*  A first behavior. It produces NO IvP function (never    */
/*  influences the vehicle); it merely posts a VIEW_RANGE_  */
/*  PULSE visual artifact a few seconds after a sister      */
/*  waypoint behavior reports a new waypoint index.         */
/************************************************************/

#ifndef BHV_PULSE_HEADER
#define BHV_PULSE_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_Pulse : public IvPBehavior {
public:
  BHV_Pulse(IvPDomain);
  ~BHV_Pulse() {};

  bool         setParam(std::string, std::string);
  IvPFunction* onRunState();

protected: // Local Utility functions
  bool         updateInfoIn();   // pull NAV_X/Y, time, WPT_INDEX from buffer
  void         postRangePulse(); // build + post the VIEW_RANGE_PULSE

protected: // Configuration parameters
  double  m_pulse_range;     // radius the pulse expands to (m)
  double  m_pulse_duration;  // seconds the pulse takes to expand
  double  m_pulse_delay;     // seconds after waypoint change before firing

protected: // State variables
  double  m_osx;             // ownship x from the info buffer
  double  m_osy;             // ownship y from the info buffer
  double  m_curr_time;       // current time from the info buffer

  double  m_wpt_index;       // last waypoint index seen
  bool    m_wpt_index_set;   // have we seen any waypoint index yet?

  bool    m_pulse_pending;   // a waypoint change is waiting to pulse
  double  m_mark_time;       // time the pending waypoint change occurred
};

#ifdef WIN32
   // Windows needs to explicitly specify functions to export from a dll
   #define IVP_EXPORT_FUNCTION __declspec(dllexport)
#else
   #define IVP_EXPORT_FUNCTION
#endif

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_Pulse(domain);}
}
#endif
