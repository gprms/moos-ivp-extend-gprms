/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: BHV_Pulse.cpp                                   */
/*    DATE: June 25th, 2026 (2.680 Lab 13, Assignment 1/2)  */
/************************************************************/

#include <cstdlib>
#include "BHV_Pulse.h"
#include "MBUtils.h"
#include "XYRangePulse.h"

using namespace std;

//-----------------------------------------------------------
// Procedure: Constructor

BHV_Pulse::BHV_Pulse(IvPDomain gdomain) :
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "pulse");

  // Default values for configuration parameters
  m_pulse_range    = 20;
  m_pulse_duration = 4;
  m_pulse_delay    = 5;   // "five seconds after" hitting a waypoint

  // Default values for state variables
  m_osx            = 0;
  m_osy            = 0;
  m_curr_time      = 0;
  m_wpt_index      = 0;
  m_wpt_index_set  = false;
  m_pulse_pending  = false;
  m_mark_time      = 0;

  // We need ownship position and the waypoint index posted by the sister
  // waypoint behavior. Declaring them here ensures the helm subscribes
  // and keeps them in the info buffer for us.
  addInfoVars("NAV_X, NAV_Y");
  // WPT_INDEX is legitimately absent until the sister waypoint behavior
  // makes its first post; "no_warning" suppresses the benign startup
  // BHV_WARNING the helm would otherwise raise for a missing buffer var.
  addInfoVars("WPT_INDEX", "no_warning");
}

//---------------------------------------------------------------
// Procedure: setParam - handle behavior configuration parameters

bool BHV_Pulse::setParam(string param, string val)
{
  param = tolower(param);
  double double_val = atof(val.c_str());

  if((param == "pulse_range") && (double_val > 0) && isNumber(val)) {
    m_pulse_range = double_val;
    return(true);
  }
  else if((param == "pulse_duration") && (double_val > 0) && isNumber(val)) {
    m_pulse_duration = double_val;
    return(true);
  }
  else if((param == "pulse_delay") && (double_val >= 0) && isNumber(val)) {
    m_pulse_delay = double_val;
    return(true);
  }
  return(false);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()
//   Pull the three pieces of information this behavior needs from the
//   info buffer: ownship position, the current time, and the waypoint
//   index reported by the sister waypoint behavior. Returns false (and
//   posts a warning) if anything essential is missing.

bool BHV_Pulse::updateInfoIn()
{
  bool ok1, ok2;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);
  if(!ok1 || !ok2) {
    postWMessage("No ownship NAV_X/NAV_Y in info_buffer.");
    return(false);
  }

  m_curr_time = getBufferCurrTime();

  // WPT_INDEX may legitimately be absent on the very first iterations,
  // before the waypoint behavior has posted anything. That is not an
  // error; we simply have no waypoint event to react to yet.
  bool ok_ix;
  double wpt_index = getBufferDoubleVal("WPT_INDEX", ok_ix);
  if(!ok_ix)
    return(true);

  // First index we ever see: latch it, do not treat it as a "change".
  if(!m_wpt_index_set) {
    m_wpt_index     = wpt_index;
    m_wpt_index_set = true;
    return(true);
  }

  // A genuine change in the waypoint index: mark the time so we can fire
  // the pulse m_pulse_delay seconds later.
  if(wpt_index != m_wpt_index) {
    m_wpt_index     = wpt_index;
    m_pulse_pending = true;
    m_mark_time     = m_curr_time;
  }
  return(true);
}

//-----------------------------------------------------------
// Procedure: postRangePulse()
//   Build a VIEW_RANGE_PULSE centered at ownship's current position and
//   post it for pMarineViewer to render.

void BHV_Pulse::postRangePulse()
{
  XYRangePulse pulse;
  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label("bhv_pulse");
  pulse.set_rad(m_pulse_range);
  pulse.set_time(m_curr_time);
  pulse.set_color("edge", "yellow");
  pulse.set_color("fill", "yellow");
  pulse.set_duration(m_pulse_duration);

  string spec = pulse.get_spec();
  postMessage("VIEW_RANGE_PULSE", spec);
}

//-----------------------------------------------------------
// Procedure: onRunState()
//   This behavior never returns an IvP function -- it does not influence
//   the vehicle. It simply watches the waypoint index and, m_pulse_delay
//   seconds after each change, posts a range pulse.

IvPFunction *BHV_Pulse::onRunState()
{
  if(!updateInfoIn())
    return(0);

  if(m_pulse_pending && ((m_curr_time - m_mark_time) >= m_pulse_delay)) {
    postRangePulse();
    m_pulse_pending = false;
  }

  return(0);
}
