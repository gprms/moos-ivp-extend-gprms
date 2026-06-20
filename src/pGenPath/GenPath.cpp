/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.cpp                                     */
/*    DATE: June 18th, 2026                                 */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include <cmath>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPath.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenPath::GenPath()
{
  // Configuration defaults
  m_update_var   = "WPT_UPDATES";
  m_visit_radius = 3.0;

  // State
  m_first_received   = false;
  m_last_received    = false;
  m_tour_generated   = false;
  m_nav_received     = false;
  m_nav_x            = 0;
  m_nav_y            = 0;
  m_total_received   = 0;
  m_invalid_received = 0;
  m_route_length     = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();

    if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_received = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_received = true;
    }
    else if(key == "VISIT_POINT") {
      string sval = msg.GetString();

      if(sval == "firstpoint") {
        // Start of a fresh set: reset accumulated state.
        m_first_received = true;
        m_last_received  = false;
        m_tour_generated = false;
        m_points.clear();
        m_visited.clear();
      }
      else if(sval == "lastpoint") {
        m_last_received = true;   // complete set received -> trigger tour
      }
      else {
        // Parse "x=8, y=9, id=1"
        string x_str = tokStringParse(sval, "x", ',', '=');
        string y_str = tokStringParse(sval, "y", ',', '=');
        string id_str = tokStringParse(sval, "id", ',', '=');
        if((x_str == "") || (y_str == "")) {
          m_invalid_received++;
        }
        else {
          XYPoint pt(atof(x_str.c_str()), atof(y_str.c_str()));
          if(id_str != "")
            pt.set_label(id_str);
          m_points.push_back(pt);
          m_visited.push_back(false);
          m_total_received++;
        }
      }
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenPath::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Readiness handshake (Lab 7 sec 3.6.1): until the point set starts
  // arriving, advertise that this vehicle's pGenPath is up and ready to
  // receive points. The shoreside pPointAssign holds the timer-script burst
  // until every configured vehicle has reported ready, so the burst is not
  // generated before the uFldShoreBroker VISIT_POINT_$V bridges exist.
  if(!m_first_received)
    Notify("PGENPATH_READY", m_host_community);

  // Generate the tour exactly once, after the full set has been received
  // and we know where the vehicle currently is.
  if(m_last_received && !m_tour_generated && m_nav_received && !m_points.empty())
    generateTour();

  // Keep a running tally of which points have been physically visited.
  if(m_tour_generated)
    updateVisited();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: generateTour()
//   Greedy nearest-neighbor TSP. Imaginary starting vertex is the
//   vehicle's present (NAV_X, NAV_Y). Repeatedly hop to the closest
//   not-yet-chosen point. Publish the ordered list to the waypoint
//   behavior via the configured updates variable.

void GenPath::generateTour()
{
  unsigned int n = m_points.size();
  vector<bool> used(n, false);

  XYSegList seglist;
  double cx = m_nav_x;
  double cy = m_nav_y;
  m_route_length = 0;

  vector<XYPoint> ordered;
  for(unsigned int step=0; step<n; step++) {
    int    best = -1;
    double best_d = 0;
    for(unsigned int i=0; i<n; i++) {
      if(used[i])
        continue;
      double dx = m_points[i].x() - cx;
      double dy = m_points[i].y() - cy;
      double d  = sqrt(dx*dx + dy*dy);
      if((best < 0) || (d < best_d)) {
        best   = (int)i;
        best_d = d;
      }
    }
    if(best < 0)
      break;
    used[best] = true;
    m_route_length += best_d;
    cx = m_points[best].x();
    cy = m_points[best].y();
    seglist.add_vertex(cx, cy);
    ordered.push_back(m_points[best]);
  }

  // Re-order internal bookkeeping to match the tour order so the
  // visited/unvisited counters track sensibly.
  m_points  = ordered;
  m_visited.assign(n, false);

  string update_str = "points = ";
  update_str += seglist.get_spec();
  Notify(m_update_var, update_str);

  // Also render the planned route for the viewer.
  seglist.set_label("tour_" + m_host_community);
  Notify("VIEW_SEGLIST", seglist.get_spec());

  m_tour_generated = true;
}

//---------------------------------------------------------
// Procedure: updateVisited()

void GenPath::updateVisited()
{
  for(unsigned int i=0; i<m_points.size(); i++) {
    if(m_visited[i])
      continue;
    double dx = m_points[i].x() - m_nav_x;
    double dy = m_points[i].y() - m_nav_y;
    if(sqrt(dx*dx + dy*dy) <= m_visit_radius)
      m_visited[i] = true;
  }
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenPath::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = tolower(biteStringX(line, '='));
    string value = stripBlankEnds(line);

    bool handled = false;
    if(param == "update_var") {
      m_update_var = value;
      handled = true;
    }
    else if(param == "visit_radius") {
      m_visit_radius = atof(value.c_str());
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool GenPath::buildReport()
{
  unsigned int visited = 0;
  for(unsigned int i=0; i<m_visited.size(); i++)
    if(m_visited[i]) visited++;
  unsigned int unvisited = m_points.size() - visited;

  m_msgs << "Visit Radius:          " << doubleToStringX(m_visit_radius,1) << endl;
  m_msgs << "Total Points Received: " << uintToString(m_total_received)    << endl;
  m_msgs << "Invalid Pts Received:  " << uintToString(m_invalid_received)  << endl;
  m_msgs << "First Point Received:  " << boolToString(m_first_received)    << endl;
  m_msgs << "Last Point Received:   " << boolToString(m_last_received)     << endl;
  m_msgs << "NAV_X/Y Received:      " << boolToString(m_nav_received)      << endl;
  m_msgs << "Update Variable:       " << m_update_var                      << endl;
  m_msgs << "Tour Generated:        " << boolToString(m_tour_generated)    << endl;
  m_msgs << "Route Length (m):      " << doubleToStringX(m_route_length,1) << endl;
  m_msgs << endl;
  m_msgs << "Tour Status"                                                  << endl;
  m_msgs << "------------------------"                                     << endl;
  m_msgs << "   Points Visited:   " << uintToString(visited)               << endl;
  m_msgs << "   Points Unvisited: " << uintToString(unvisited)             << endl;
  m_msgs << "   Nav Position:     ("  << doubleToStringX(m_nav_x,1) << ", "
         << doubleToStringX(m_nav_y,1) << ")"                              << endl;

  return(true);
}
