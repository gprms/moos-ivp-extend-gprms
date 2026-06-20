/************************************************************/
/*    NAME: Mike Benjamin (baseline)                        */
/*    EDIT: 2.680 Lab 9 - Assignment 4 (dynamic planner)    */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: April 18th, 2022 / Lab 9 rev June 2026          */
/*                                                          */
/*  Dynamic rescue path planner.                            */
/*   - Ingests SWIMMER_ALERT messages from the shoreside    */
/*     uFldRescueMgr, storing each swimmer uniquely by ID.  */
/*   - Ignores duplicate alerts for an ID already known     */
/*     (or already rescued).                                */
/*   - Ingests FOUND_SWIMMER messages and drops rescued     */
/*     swimmers from the plan.                              */
/*   - Whenever the swimmer set changes, recomputes a       */
/*     waypoint traversal using a Nearest-Neighbor TSP      */
/*     heuristic (greedyPath) seeded at ownship position,   */
/*     and updates the helm BHV_Waypoint via SURVEY_UPDATE. */
/************************************************************/

#include <cstdlib>
#include <iterator>
#include "GenRescue.h"
#include "MBUtils.h"
#include "ColorParse.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "GeomUtils.h"
#include "PathUtils.h"
#include "ACTable.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // Initialize state variables
  m_plan_pending = false;

  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = false;
  m_nav_y_set = false;

  m_alerts_total = 0;
  m_alerts_dups  = 0;
  m_replans      = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    string sval = msg.GetString();

    bool handled = true;
    if(key == "SWIMMER_ALERT")
      handled = handleMailNewSwimmer(sval);
    else if(key == "FOUND_SWIMMER")
      handled = handleMailFoundSwimmer(sval);
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }
    else if(key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      handled = false;

    if(!handled)
      reportRunWarning("Unhandled Mail: " + key + "=" + sval);
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Recompute the path ONLY when the swimmer set has changed (a new
  // swimmer arrived or one was rescued). We wait until we have an
  // ownship position so the NN traversal can be seeded sensibly.
  // Recomputing on every iteration would re-order the waypoints as
  // ownship moves and continually reset BHV_Waypoint to its first
  // point, stalling the traversal -- so we strictly avoid that.
  if(m_plan_pending && m_nav_x_set && m_nav_y_set) {
    updatePlan();
    m_plan_pending = false;
  }
  // Re-post the (unchanged) stored plan periodically so a helm or
  // behavior that came up after our last post still receives it.
  // The string is identical between posts, so BHV_Waypoint does not
  // reset its waypoint index.
  else if((m_iteration % 20) == 0)
    postCurrentPath();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(biteStringX(sLine, '='));
    string value  = sLine;
    if(param == "vname")
      m_vname = value;
  }

  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void GenRescue::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()
//   Example: SWIMMER_ALERT = x=23, y=54, id=04
//   Store swimmers uniquely by ID; ignore duplicates and any
//   alert for a swimmer already rescued.

bool GenRescue::handleMailNewSwimmer(string str)
{
  double x = 0, y = 0;
  string id;
  bool got_x = false, got_y = false;

  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string field = stripBlankEnds(svector[i]);
    string key   = tolower(biteStringX(field, '='));
    string val   = field;
    if(key == "x")  { x = atof(val.c_str()); got_x = true; }
    else if(key == "y")  { y = atof(val.c_str()); got_y = true; }
    else if(key == "id") { id = val; }
  }

  if((id == "") || !got_x || !got_y) {
    reportRunWarning("Bad SWIMMER_ALERT: " + str);
    return(false);
  }

  m_alerts_total++;

  // Duplicate: same ID already known, or already rescued. Ignore.
  if(m_known_ids.count(id) || m_rescued_ids.count(id)) {
    m_alerts_dups++;
    return(true);
  }

  m_known_ids.insert(id);
  m_swimmers[id] = XYPoint(x, y);

  // A genuinely new swimmer: trigger a replan.
  m_plan_pending = true;
  reportEvent("New swimmer id=" + id + " at (" + doubleToStringX(x,1) +
              "," + doubleToStringX(y,1) + "); known=" +
              uintToString(m_swimmers.size()));
  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()
//   Example: FOUND_SWIMMER = id=01, finder=abe
//   Remove the rescued swimmer from the planning set and replan.

bool GenRescue::handleMailFoundSwimmer(string str)
{
  string id, finder;

  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string field = stripBlankEnds(svector[i]);
    string key   = tolower(biteStringX(field, '='));
    string val   = field;
    if(key == "id")          id = val;
    else if(key == "finder") finder = val;
  }

  if(id == "") {
    reportRunWarning("Bad FOUND_SWIMMER: " + str);
    return(false);
  }

  // Record as rescued so any future alert for this ID is ignored,
  // even if we never had it in our own planning set.
  m_rescued_ids.insert(id);

  // If it was in our active set, drop it and replan.
  if(m_swimmers.count(id)) {
    m_swimmers.erase(id);
    m_plan_pending = true;
    reportEvent("Rescued id=" + id + " (finder=" + finder +
                "); remaining=" + uintToString(m_swimmers.size()));
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: updatePlan()
//   Build a Nearest-Neighbor TSP traversal over all known (not yet
//   rescued) swimmers, seeded at ownship position, and post it.

void GenRescue::updatePlan()
{
  m_replans++;

  // (Re)compute the nearest-neighbor traversal over all known,
  // not-yet-rescued swimmers, seeded at the current ownship position.
  if(m_swimmers.empty()) {
    postNullPath();
    return;
  }

  XYSegList segl;
  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++)
    segl.add_vertex(q->second.x(), q->second.y());

  m_path = greedyPath(segl, m_nav_x, m_nav_y);
  m_path.set_label("rescue_path");

  reportEvent("Replan #" + uintToString(m_replans) + ": " +
              uintToString(m_path.size()) + " swimmers");
  postCurrentPath();
}

//---------------------------------------------------------
// Procedure: postCurrentPath()
//   Push the already-computed traversal to the helm waypoint
//   behavior via SURVEY_UPDATE (and VIEW_SEGLIST for rendering).
//   Does NOT recompute -- so periodic re-posts are byte-identical
//   and never reset BHV_Waypoint's progress.

void GenRescue::postCurrentPath()
{
  if(m_path.size() == 0)
    return;

  Notify("VIEW_SEGLIST", m_path.get_spec());
  Notify("SURVEY_UPDATE", "points=" + m_path.get_spec_pts());
}

//---------------------------------------------------------
// Procedure: postNullPath()
//   No swimmers left to rescue: send the vehicle to hold at its
//   current position (single-vertex waypoint) so it stops chasing
//   a stale path.

void GenRescue::postNullPath()
{
  if(!m_nav_x_set || !m_nav_y_set)
    return;

  m_path = XYSegList();
  m_path.add_vertex(m_nav_x, m_nav_y);
  m_path.set_label("rescue_path");

  Notify("VIEW_SEGLIST", m_path.get_spec());
  Notify("SURVEY_UPDATE", "points=" + m_path.get_spec_pts());
  reportEvent("No swimmers remain; holding at ownship position");
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Vehicle Name: " << m_vname << endl;
  m_msgs << "Ownship Pos:  ";
  if(m_nav_x_set && m_nav_y_set)
    m_msgs << "(" << doubleToStringX(m_nav_x,1) << ","
           << doubleToStringX(m_nav_y,1) << ")" << endl;
  else
    m_msgs << "<unknown>" << endl;
  m_msgs << "=========================================" << endl;
  m_msgs << "Alerts received:    " << m_alerts_total << endl;
  m_msgs << "Duplicate alerts:   " << m_alerts_dups  << endl;
  m_msgs << "Swimmers known:     " << m_swimmers.size() << endl;
  m_msgs << "Swimmers rescued:   " << m_rescued_ids.size() << endl;
  m_msgs << "Replans posted:     " << m_replans << endl;
  m_msgs << "Current path size:  " << m_path.size() << endl;
  m_msgs << "=========================================" << endl;

  ACTable actab(3);
  actab << "SwimmerID | X | Y";
  actab.addHeaderLines();
  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++) {
    actab << q->first
          << doubleToStringX(q->second.x(),1)
          << doubleToStringX(q->second.y(),1);
  }
  m_msgs << actab.getFormattedString();

  return(true);
}
