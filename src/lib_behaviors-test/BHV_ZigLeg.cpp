/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: BHV_ZigLeg.cpp                                  */
/*    DATE: June 25th, 2026 (2.680 Lab 13, Assignment 3)    */
/************************************************************/

#include <cstdlib>
#include "BHV_ZigLeg.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "BuildUtils.h"
#include "ZAIC_PEAK.h"

using namespace std;

//-----------------------------------------------------------
// Procedure: Constructor

BHV_ZigLeg::BHV_ZigLeg(IvPDomain gdomain) :
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "zigleg");

  // This behavior influences heading only, so restrict its domain to
  // course. (The vehicle's speed is left to the waypoint behavior.)
  m_domain = subDomain(m_domain, "course");

  // Default values for configuration parameters
  m_zig_angle    = 45;
  m_zig_duration = 10;
  m_zig_delay    = 5;   // begin the zig five seconds after the waypoint

  // Default values for state variables
  m_curr_heading  = 0;
  m_curr_time     = 0;
  m_wpt_index     = 0;
  m_wpt_index_set = false;
  m_zig_pending   = false;
  m_mark_time     = 0;
  m_zig_active    = false;
  m_zig_start     = 0;
  m_zig_heading   = 0;

  // We need ownship heading and the waypoint index posted by the sister
  // waypoint behavior.
  addInfoVars("NAV_HEADING");
  // WPT_INDEX is legitimately absent until the sister waypoint behavior
  // makes its first post; "no_warning" suppresses the benign startup
  // BHV_WARNING the helm would otherwise raise for a missing buffer var.
  addInfoVars("WPT_INDEX", "no_warning");
}

//---------------------------------------------------------------
// Procedure: setParam - handle behavior configuration parameters

bool BHV_ZigLeg::setParam(string param, string val)
{
  param = tolower(param);
  double double_val = atof(val.c_str());

  if((param == "zig_angle") && isNumber(val)) {
    m_zig_angle = double_val;
    return(true);
  }
  else if((param == "zig_duration") && (double_val > 0) && isNumber(val)) {
    m_zig_duration = double_val;
    return(true);
  }
  else if((param == "zig_delay") && (double_val >= 0) && isNumber(val)) {
    m_zig_delay = double_val;
    return(true);
  }
  return(false);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()
//   Pull ownship heading, current time, and the sister waypoint
//   behavior's waypoint index from the info buffer. Detect a change in
//   the waypoint index and mark the time so the zig can begin
//   m_zig_delay seconds later.

bool BHV_ZigLeg::updateInfoIn()
{
  bool ok1;
  m_curr_heading = getBufferDoubleVal("NAV_HEADING", ok1);
  if(!ok1) {
    postWMessage("No ownship NAV_HEADING in info_buffer.");
    return(false);
  }

  m_curr_time = getBufferCurrTime();

  // WPT_INDEX may be absent on the first iterations; not an error.
  bool ok_ix;
  double wpt_index = getBufferDoubleVal("WPT_INDEX", ok_ix);
  if(!ok_ix)
    return(true);

  if(!m_wpt_index_set) {
    m_wpt_index     = wpt_index;
    m_wpt_index_set = true;
    return(true);
  }

  if(wpt_index != m_wpt_index) {
    m_wpt_index   = wpt_index;
    m_zig_pending = true;
    m_mark_time   = m_curr_time;
  }
  return(true);
}

//-----------------------------------------------------------
// Procedure: buildHeadingFunction()
//   Build an IvP objective function peaked at the latched, offset
//   heading using the ZAIC_PEAK tool.

IvPFunction *BHV_ZigLeg::buildHeadingFunction()
{
  ZAIC_PEAK crs_zaic(m_domain, "course");
  crs_zaic.setSummit(m_zig_heading);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180.0);
  crs_zaic.setSummitDelta(0);
  crs_zaic.setValueWrap(true);
  if(crs_zaic.stateOK() == false) {
    postWMessage("Course ZAIC problems " + crs_zaic.getWarnings());
    return(0);
  }

  return(crs_zaic.extractIvPFunction());
}

//-----------------------------------------------------------
// Procedure: onRunState()
//   Five seconds after each waypoint change, begin producing a heading
//   objective function offset from the heading at that moment, and keep
//   producing it for m_zig_duration seconds. The offset heading is
//   latched ONCE at the start of the zig -- recomputing it every
//   iteration off the present heading would make the vehicle spiral
//   rather than make a clean zig leg.

IvPFunction *BHV_ZigLeg::onRunState()
{
  if(!updateInfoIn())
    return(0);

  // Time to begin a pending zig? Latch the offset heading now.
  if(m_zig_pending && ((m_curr_time - m_mark_time) >= m_zig_delay)) {
    m_zig_heading = angle360(m_curr_heading + m_zig_angle);
    m_zig_active  = true;
    m_zig_start   = m_curr_time;
    m_zig_pending = false;
  }

  if(!m_zig_active)
    return(0);

  // The zig has run its course: stop influencing the vehicle.
  if((m_curr_time - m_zig_start) >= m_zig_duration) {
    m_zig_active = false;
    return(0);
  }

  IvPFunction *ipf = buildHeadingFunction();
  if(ipf == 0)
    postWMessage("Problem Creating the IvP Function");

  if(ipf)
    ipf->setPWT(m_priority_wt);

  return(ipf);
}
