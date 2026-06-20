/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.cpp                                 */
/*    DATE: June 18th, 2026                                 */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include "MBUtils.h"
#include "ACTable.h"
#include "XYPoint.h"
#include "PointAssign.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

PointAssign::PointAssign()
{
  // Configuration defaults
  m_assign_by_region = false;
  m_region_xmin      = -25;
  m_region_xmax      = 200;

  // State
  m_vname_idx        = 0;
  m_points_received  = 0;
  m_invalid_received = 0;
  m_first_received   = false;
  m_last_received    = false;
  m_distribution_started = false;
  m_settle_ticks     = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool PointAssign::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();
    string sval   = msg.GetString();

    if(key == "PGENPATH_READY") {
      m_ready_vehicles.insert(stripBlankEnds(sval));
    }
    else if(key == "VISIT_POINT") {

      // 1. Handle the bookend flags - forward to ALL vehicles so each
      //    vehicle's pGenPath knows when the complete set has arrived.
      if(sval == "firstpoint") {
        m_first_received = true;
        for(unsigned int i=0; i<m_vnames.size(); i++)
          Notify(pointVar(m_vnames[i]), "firstpoint");
      }
      else if(sval == "lastpoint") {
        m_last_received = true;
        for(unsigned int i=0; i<m_vnames.size(); i++)
          Notify(pointVar(m_vnames[i]), "lastpoint");
      }
      // 2. Handle normal coordinate points
      else {
        handlePoint(sval);
      }
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: handlePoint()
//   Parse a "x=.., y=.., id=.." posting, choose a target vehicle
//   based on the configured assignment mode, forward it, and render
//   a colored VIEW_POINT for the shoreside viewer.

//---------------------------------------------------------
// Procedure: pointVar()
//   Build the per-vehicle publish variable. uFldShoreBroker's $V macro
//   uppercases the node name (e.g. DEPLOY_HENRY, VISIT_POINT_HENRY), so the
//   "bridge = src=VISIT_POINT_$V, alias=VISIT_POINT" route is VISIT_POINT_HENRY.
//   We must therefore publish to the UPPERCASE variable for the share to fire.

string PointAssign::pointVar(const string& vname)
{
  return("VISIT_POINT_" + toupper(vname));
}

//---------------------------------------------------------
// Procedure: handlePoint()

void PointAssign::handlePoint(const string& sval)
{
  if(m_vnames.empty())
    return;

  string x_str = tokStringParse(sval, "x",  ',', '=');
  string y_str = tokStringParse(sval, "y",  ',', '=');
  string id_str = tokStringParse(sval, "id", ',', '=');

  if((x_str == "") || (y_str == "")) {
    m_invalid_received++;
    return;
  }

  double x_pos = atof(x_str.c_str());
  double y_pos = atof(y_str.c_str());
  m_points_received++;

  // Decide the target vehicle index
  unsigned int idx = 0;
  if(m_assign_by_region) {
    // Split the region in half along x (East/West). West -> vehicle[0],
    // East -> vehicle[1]. With more than two vehicles, fall back to even
    // bands across x.
    double mid = (m_region_xmin + m_region_xmax) / 2.0;
    if(m_vnames.size() == 2) {
      idx = (x_pos < mid) ? 0 : 1;
    }
    else {
      double span = (m_region_xmax - m_region_xmin) / (double)m_vnames.size();
      idx = (unsigned int)((x_pos - m_region_xmin) / span);
      if(idx >= m_vnames.size())
        idx = m_vnames.size() - 1;
    }
  }
  else {
    // Alternating assignment: point1->v0, point2->v1, point3->v0, ...
    idx = m_vname_idx;
    m_vname_idx = (m_vname_idx + 1) % m_vnames.size();
  }

  string target = m_vnames[idx];
  Notify(pointVar(target), sval);
  m_vcount[target]++;

  // Visual output: one colored point per vehicle. Label must be unique
  // (use the id) so pMarineViewer does not overwrite points.
  const char* palette[] = {"yellow", "dodger_blue", "orange",
                           "lime_green", "magenta", "white"};
  string color = palette[idx % 6];
  string label = "pt_" + (id_str=="" ? doubleToStringX(m_points_received,0) : id_str);
  postViewPoint(x_pos, y_pos, label, color);
}

//---------------------------------------------------------
// Procedure: postViewPoint()

void PointAssign::postViewPoint(double x, double y, string label, string color)
{
  XYPoint point(x, y);
  point.set_label(label);
  point.set_color("vertex", color);
  point.set_param("vertex_size", "4");
  Notify("VIEW_POINT", point.get_spec());
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool PointAssign::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool PointAssign::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Handshaking (Lab 7 sec 3.6.1): hold the uTimerScript burst until every
  // configured vehicle's pGenPath has reported ready. This guarantees the
  // vehicles are connected and the uFldShoreBroker VISIT_POINT_$V bridges
  // are in place before the one-shot burst of points is generated.
  if(!m_distribution_started && !m_vnames.empty()) {
    bool all_ready = true;
    for(unsigned int i=0; i<m_vnames.size(); i++)
      if(!m_ready_vehicles.count(m_vnames[i]))
        all_ready = false;

    if(all_ready) {
      // Allow a short settle window for the bridges to finish registering.
      if(m_settle_ticks < 8)
        m_settle_ticks++;
      else {
        Notify("UTS_PAUSE", "false");
        m_distribution_started = true;
      }
    }
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool PointAssign::OnStartUp()
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
    string value = stripBlankEnds(line);   // strip surrounding whitespace

    bool handled = false;
    if(param == "vname") {
      // Names are read from config - never hard-coded. Keep the case as
      // given so it matches the vehicle community name used in the readiness
      // handshake; the uFldShoreBroker $V bridge uppercases it (see pointVar).
      m_vnames.push_back(value);
      handled = true;
    }
    else if(param == "assign_by_region") {
      m_assign_by_region = (tolower(value) == "true");
      handled = true;
    }
    else if(param == "region_xmin") {
      m_region_xmin = atof(value.c_str());
      handled = true;
    }
    else if(param == "region_xmax") {
      m_region_xmax = atof(value.c_str());
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  if(m_vnames.empty())
    reportConfigWarning("No vname configured - no points will be distributed");

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void PointAssign::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
  Register("PGENPATH_READY", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool PointAssign::buildReport()
{
  string mode = m_assign_by_region ? "region (East/West)" : "alternating";

  m_msgs << "Assignment Mode:   " << mode                       << endl;
  m_msgs << "Vehicles:          " << uintToString(m_vnames.size()) << endl;
  m_msgs << "Region x-range:    [" << doubleToStringX(m_region_xmin,1)
         << ", " << doubleToStringX(m_region_xmax,1) << "]"      << endl;
  string ready_list;
  for(std::set<string>::iterator q=m_ready_vehicles.begin(); q!=m_ready_vehicles.end(); q++)
    ready_list += (ready_list==""?"":",") + *q;
  m_msgs << "Vehicles Ready:    " << (ready_list==""?"(none)":ready_list) << endl;
  m_msgs << "Distribution Open: " << boolToString(m_distribution_started) << endl;
  m_msgs << "First Point Recvd: " << boolToString(m_first_received) << endl;
  m_msgs << "Last Point Recvd:  " << boolToString(m_last_received)  << endl;
  m_msgs << "Total Pts Recvd:   " << uintToString(m_points_received) << endl;
  m_msgs << "Invalid Recvd:     " << uintToString(m_invalid_received) << endl;
  m_msgs << endl;

  ACTable actab(3);
  actab << "Vehicle | Points Assigned | Publish Var";
  actab.addHeaderLines();
  for(unsigned int i=0; i<m_vnames.size(); i++) {
    string vn = m_vnames[i];
    actab << vn << uintToString(m_vcount[vn]) << pointVar(vn);
  }
  m_msgs << actab.getFormattedString();

  return(true);
}
