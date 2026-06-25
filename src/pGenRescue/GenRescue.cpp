/************************************************************/
/*    NAME: gprms                        */
/*    EDIT: 2.680 Lab 9 - Assignment 4 (dynamic planner)    */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: June 24th, 2026 / Lab 12 rev June 2026          */
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
/*   - Ingests NODE_REPORT for the adversary and concedes   */
/*     swimmers the adversary is likely to reach first      */
/*     (Lab 12 sec 5.2), planning over the remaining set.   */
/************************************************************/

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <utility>
#include <iterator>
#include "GenRescue.h"
#include "MBUtils.h"
#include "ColorParse.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "GeomUtils.h"
#include "AngleUtils.h"
#include "PathUtils.h"
#include "ACTable.h"
#include "NodeRecord.h"
#include "NodeRecordUtils.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // Initialize state variables
  m_plan_pending = false;
  m_returning    = false;
  m_heuristic    = "greedy";

  // Adversary conceding (sec 5.2): off until enabled in the config block.
  m_concede_enable      = false;
  m_concede_radius      = 15.0;
  m_concede_ahead_range = 0.0;
  m_concede_ahead_angle = 30.0;
  m_concede_max         = 0;
  m_concede_replan_dist = 5.0;

  m_contact_x   = 0;
  m_contact_y   = 0;
  m_contact_hdg = 0;
  m_contact_set = false;

  m_plan_contact_x   = 0;
  m_plan_contact_y   = 0;
  m_plan_contact_set = false;

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
    else if((key == "FOUND_SWIMMER") || (key == "RESCUED_SWIMMER"))
      handled = handleMailFoundSwimmer(sval);
    else if(key == "NODE_REPORT")
      handled = handleMailNodeReport(sval);
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
    else if(param == "heuristic_type") {
      string v = tolower(value);
      if((v == "greedy") || (v == "lookahead2"))
        m_heuristic = v;
      else
        reportConfigWarning("Unknown heuristic_type: " + value);
    }
    else if(param == "concede")
      setBooleanOnString(m_concede_enable, value);
    else if(param == "concede_radius")
      m_concede_radius = atof(value.c_str());
    else if(param == "concede_ahead_range")
      m_concede_ahead_range = atof(value.c_str());
    else if(param == "concede_ahead_angle")
      m_concede_ahead_angle = atof(value.c_str());
    else if(param == "concede_max")
      m_concede_max = (unsigned int)(atoi(value.c_str()));
    else if(param == "concede_replan_dist")
      m_concede_replan_dist = atof(value.c_str());
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
  Register("FOUND_SWIMMER", 0);    // deprecated by uFldRescueMgr but still posted
  Register("RESCUED_SWIMMER", 0);  // current rescue-confirmation variable
  Register("NODE_REPORT", 0);      // adversary contact (sec 5.2)
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
// Procedure: handleMailNodeReport()        (Lab 12 sec 5.2)
//   Example: NODE_REPORT = NAME=ben,X=10.2,Y=-5.8,SPD=0,HDG=304.4,...
//   De-serialize the adversary's position and heading. If the adversary
//   has moved appreciably since our last plan, trigger a replan so our
//   concessions track where the adversary actually is.

bool GenRescue::handleMailNodeReport(string str)
{
  NodeRecord record = string2NodeRecord(str);
  string name = record.getName();
  if(name == "") {
    reportRunWarning("Bad NODE_REPORT: " + str);
    return(false);
  }

  // A NODE_REPORT about ourselves is not an adversary; ignore it.
  if(tolower(name) == tolower(m_vname))
    return(true);

  m_contact_name = name;
  m_contact_x    = record.getX();
  m_contact_y    = record.getY();
  m_contact_hdg  = record.getHeading();
  m_contact_set  = true;

  // Only conceding cares about the adversary, so only it triggers replans.
  if(!m_concede_enable)
    return(true);

  if(!m_plan_contact_set) {
    // First adversary fix: replan so concessions are applied at all.
    m_plan_pending = true;
  }
  else {
    double moved = distPointToPoint(m_plan_contact_x, m_plan_contact_y,
                                    m_contact_x, m_contact_y);
    if(moved >= m_concede_replan_dist)
      m_plan_pending = true;
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: computeConcessions()           (Lab 12 sec 5.2)
//   Populate m_conceded_ids with swimmers we yield to the adversary, so
//   updatePlan() can plan a shorter tour over only the swimmers we can
//   realistically reach first. Two complementary rules, unioned:
//     (1) Proximity: any swimmer within m_concede_radius of the adversary.
//     (2) Heading:   any swimmer within m_concede_ahead_range and inside a
//                    +/- m_concede_ahead_angle cone of the adversary heading
//                    (i.e. roughly where the adversary is already pointed).
//   Closest-to-adversary swimmers are conceded first when a cap applies.
//   We never concede the entire field -- at least one swimmer is always
//   kept so we keep working rather than idling.

void GenRescue::computeConcessions()
{
  vector<pair<double, string> > cand;   // (dist-to-adversary, id)

  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++) {
    double sx = q->second.x();
    double sy = q->second.y();
    double d  = distPointToPoint(m_contact_x, m_contact_y, sx, sy);

    bool concede = false;
    if((m_concede_radius > 0) && (d <= m_concede_radius))
      concede = true;
    if(!concede && (m_concede_ahead_range > 0) && (d <= m_concede_ahead_range)) {
      double bearing = relAng(m_contact_x, m_contact_y, sx, sy);
      double diff    = bearing - m_contact_hdg;
      while(diff > 180)  diff -= 360;
      while(diff < -180) diff += 360;
      if(fabs(diff) <= m_concede_ahead_angle)
        concede = true;
    }
    if(concede)
      cand.push_back(make_pair(d, q->first));
  }

  // Concede nearest-to-adversary first, honoring any configured cap.
  sort(cand.begin(), cand.end());
  unsigned int limit = cand.size();
  if((m_concede_max > 0) && (m_concede_max < limit))
    limit = m_concede_max;
  // Never concede every swimmer; always keep at least one to plan toward.
  if(limit >= m_swimmers.size())
    limit = m_swimmers.size() - 1;

  for(unsigned int i=0; i<limit; i++)
    m_conceded_ids.insert(cand[i].second);
}

//---------------------------------------------------------
// Procedure: updatePlan()
//   Build a Nearest-Neighbor TSP traversal over all known (not yet
//   rescued) swimmers, seeded at ownship position, and post it.

void GenRescue::updatePlan()
{
  m_replans++;

  // (Re)compute the traversal over all known, not-yet-rescued swimmers,
  // seeded at the current ownship position.
  if(m_swimmers.empty()) {
    postNullPath();
    return;
  }

  // We have swimmers to visit. If we had previously commanded a return
  // home (set empty earlier, then a new alert arrived), cancel it so the
  // helm leaves RETURNING mode and resumes surveying.
  if(m_returning)
    setReturn(false);

  // Decide which swimmers to yield to the adversary this plan (sec 5.2).
  // Recomputed fresh each replan, so a swimmer the adversary moved away
  // from automatically re-enters our plan on the next replan.
  m_conceded_ids.clear();
  if(m_concede_enable && m_contact_set)
    computeConcessions();

  XYSegList segl;
  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++) {
    if(m_conceded_ids.count(q->first))
      continue;
    segl.add_vertex(q->second.x(), q->second.y());
  }

  // Record the adversary position behind this plan so subsequent reports
  // can tell when the adversary has moved enough to warrant a new plan.
  if(m_contact_set) {
    m_plan_contact_x   = m_contact_x;
    m_plan_contact_y   = m_contact_y;
    m_plan_contact_set = true;
  }

  if(m_heuristic == "lookahead2")
    m_path = twoVertexLookaheadPath(segl, m_nav_x, m_nav_y);
  else
    m_path = greedyPath(segl, m_nav_x, m_nav_y);
  m_path.set_label("rescue_path");

  string conceded_note;
  if(!m_conceded_ids.empty())
    conceded_note = ", conceded " + uintToString(m_conceded_ids.size()) +
                    " to " + m_contact_name;
  reportEvent("Replan #" + uintToString(m_replans) + " (" + m_heuristic +
              "): " + uintToString(m_path.size()) + " swimmers" +
              conceded_note);
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
//   No swimmers left to rescue: transition the helm into its
//   return-home behavior by posting RETURN=true. This satisfies the
//   Lab 12 edge case (sec 3.2/3.3): if the final swimmer is rescued by
//   the adversary before we arrive, we must immediately head home
//   rather than hold station on a stale survey path.

void GenRescue::postNullPath()
{
  // Clear any rendered rescue path and stop periodic re-posts.
  m_path = XYSegList();
  XYSegList empty;
  empty.set_label("rescue_path");
  empty.set_active(false);
  Notify("VIEW_SEGLIST", empty.get_spec());

  setReturn(true);
  reportEvent("No swimmers remain; returning home (RETURN=true)");
}

//---------------------------------------------------------
// Procedure: setReturn()
//   Post RETURN=true/false to the helm, but only on a change, so we do
//   not spam the MOOSDB once we are already in (or out of) RETURNING.

void GenRescue::setReturn(bool val)
{
  if(val == m_returning)
    return;
  m_returning = val;
  Notify("RETURN", val ? "true" : "false");
}

//---------------------------------------------------------
// Procedure: twoVertexLookaheadPath()        (Lab 12 sec 5.1)
//   Greedy tour with a two-vertex look-ahead. Instead of always taking
//   the single nearest swimmer, score each candidate by the distance to
//   reach it PLUS the distance from it to its own nearest remaining
//   neighbor. This biases the tour toward entering swimmer clusters
//   early rather than darting to an isolated-but-near swimmer and then
//   doubling back across the field. O(N^3); trivial for N<30.

XYSegList GenRescue::twoVertexLookaheadPath(const XYSegList& pts,
                                            double osx, double osy)
{
  vector<XYPoint> remaining;
  for(unsigned int i=0; i<pts.size(); i++)
    remaining.push_back(XYPoint(pts.get_vx(i), pts.get_vy(i)));

  XYSegList result;
  double curx = osx, cury = osy;

  while(!remaining.empty()) {
    unsigned int best_i = 0;
    double best_score = -1;
    for(unsigned int i=0; i<remaining.size(); i++) {
      double d1 = distPointToPoint(curx, cury,
                                   remaining[i].x(), remaining[i].y());
      // distance from candidate i to its nearest OTHER remaining vertex
      double d2 = 0;
      bool d2_set = false;
      for(unsigned int j=0; j<remaining.size(); j++) {
        if(j == i)
          continue;
        double d = distPointToPoint(remaining[i].x(), remaining[i].y(),
                                    remaining[j].x(), remaining[j].y());
        if(!d2_set || (d < d2)) {
          d2 = d;
          d2_set = true;
        }
      }
      double score = d1 + d2;
      if((best_score < 0) || (score < best_score)) {
        best_score = score;
        best_i = i;
      }
    }
    result.add_vertex(remaining[best_i].x(), remaining[best_i].y());
    curx = remaining[best_i].x();
    cury = remaining[best_i].y();
    remaining.erase(remaining.begin() + best_i);
  }
  return result;
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Vehicle Name: " << m_vname << endl;
  m_msgs << "Heuristic:    " << m_heuristic << endl;
  m_msgs << "Returning:    " << (m_returning ? "true" : "false") << endl;
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
  m_msgs << "Conceding:    " << (m_concede_enable ? "ENABLED" : "off");
  if(m_concede_enable) {
    m_msgs << "  (radius=" << doubleToStringX(m_concede_radius,1);
    if(m_concede_ahead_range > 0)
      m_msgs << ", ahead=" << doubleToStringX(m_concede_ahead_range,1)
             << "m/" << doubleToStringX(m_concede_ahead_angle,0) << "deg";
    if(m_concede_max > 0)
      m_msgs << ", max=" << m_concede_max;
    m_msgs << ")";
  }
  m_msgs << endl;
  m_msgs << "Adversary:    ";
  if(m_contact_set)
    m_msgs << m_contact_name << " @ (" << doubleToStringX(m_contact_x,1)
           << "," << doubleToStringX(m_contact_y,1) << ") hdg "
           << doubleToStringX(m_contact_hdg,0) << endl;
  else
    m_msgs << "<none seen>" << endl;
  m_msgs << "Conceded now: " << m_conceded_ids.size() << " swimmer(s)" << endl;
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
