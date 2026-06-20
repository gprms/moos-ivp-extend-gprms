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
  void updatePlan();        // recompute NN-TSP path over known swimmers
  void postCurrentPath();   // (re)post stored path: SURVEY_UPDATE + VIEW_SEGLIST
  void postNullPath();      // hold station when no swimmers remain

 private: // Config variables
  std::string m_vname;

 private: // State variables
  // Known swimmers not yet rescued: id -> location
  std::map<std::string, XYPoint> m_swimmers;
  // Every id we have ever ingested via SWIMMER_ALERT (for dup detection)
  std::set<std::string>          m_known_ids;
  // Ids reported rescued via FOUND_SWIMMER
  std::set<std::string>          m_rescued_ids;

  XYSegList  m_path;
  bool       m_plan_pending;   // a new/changed swimmer set needs a replan

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
