/************************************************************/
/*    NAME: Mike Benjamin (baseline)                        */
/*    EDIT: 2.680 Lab 9 - Assignment 4 (dynamic planner)    */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.h                                     */
/*    DATE: April 18th, 2022 / Lab 9 rev June 2026          */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include <map>
#include <set>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "XYPolygon.h"

class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {};

 protected:
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();
  void RegisterVariables();

 protected:
  bool handleMailNewSwimmer(std::string);
  bool handleMailFoundSwimmer(std::string);
  bool handleMailNodeReport(std::string);   // adversary contact (sec 5.2)
  void updatePlan();        // recompute path over known swimmers
  void computeConcessions();// pick swimmers to yield to the adversary (sec 5.2)
  void postCurrentPath();   // (re)post stored path: SURVEY_UPDATE + VIEW_SEGLIST
  void postNullPath();      // return home when no swimmers remain
  void setReturn(bool);     // post RETURN=true/false on change only

  // Path heuristics (Lab 12 sec 5.1)
  XYSegList twoVertexLookaheadPath(const XYSegList&, double osx, double osy);

 private: // Config variables
  std::string m_vname;
  // "greedy" (one-vertex nearest-neighbor, default) or "lookahead2"
  // (two-vertex-look-ahead, biases the tour toward swimmer clusters).
  std::string m_heuristic;

  // Adversary conceding (Lab 12 sec 5.2). When enabled, swimmers that the
  // adversary is likely to reach first are dropped from OUR planning set so
  // we do not waste a tour darting toward swimmers we will not win.
  bool         m_concede_enable;       // master on/off (default off)
  double       m_concede_radius;       // yield swimmers within this dist of
                                       // the adversary (<=0 disables rule)
  double       m_concede_ahead_range;  // heading rule: consider swimmers
                                       // within this dist (<=0 disables)
  double       m_concede_ahead_angle;  // ...and within this half-cone (deg)
                                       // of the adversary's heading
  unsigned int m_concede_max;          // cap on number yielded (0 = no cap)
  double       m_concede_replan_dist;  // replan once adversary moves this far

 private: // State variables
  // Known swimmers not yet rescued: id -> location
  std::map<std::string, XYPoint> m_swimmers;
  // Every id we have ever ingested via SWIMMER_ALERT (for dup detection)
  std::set<std::string>          m_known_ids;
  // Ids reported rescued via FOUND_SWIMMER
  std::set<std::string>          m_rescued_ids;
  // Ids conceded to the adversary on the most recent plan (for the appcast)
  std::set<std::string>          m_conceded_ids;

  // Latest known adversary state, from NODE_REPORT (sec 5.2)
  std::string m_contact_name;
  double      m_contact_x;
  double      m_contact_y;
  double      m_contact_hdg;
  bool        m_contact_set;
  // Adversary position used at the last replan (movement-triggered replan)
  double      m_plan_contact_x;
  double      m_plan_contact_y;
  bool        m_plan_contact_set;

  XYSegList  m_path;
  bool       m_plan_pending;   // a new/changed swimmer set needs a replan
  bool       m_returning;      // true once we have commanded RETURN=true

  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;

  // Bookkeeping for the appcast report
  unsigned int m_alerts_total;
  unsigned int m_alerts_dups;
  unsigned int m_replans;
};

#endif
