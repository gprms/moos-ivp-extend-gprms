/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.h                                   */
/*    DATE: June 18th, 2026                                 */
/*    DESC: Shoreside app - distributes VISIT_POINT postings*/
/*          to a configurable set of vehicles, either in an */
/*          alternating fashion or split East/West by region*/
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include <vector>
#include <string>
#include <map>
#include <set>
#include "XYPoint.h"
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class PointAssign : public AppCastingMOOSApp
{
 public:
   PointAssign();
   ~PointAssign() {}

 protected: // Standard MOOSApp functions to overload
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload
   bool buildReport();

 protected:
   void registerVariables();
   std::string pointVar(const std::string& vname);
   void handlePoint(const std::string& sval);
   void postViewPoint(double x, double y, std::string label, std::string color);

 private: // Configuration variables
   std::vector<std::string> m_vnames;   // vehicle names (no hard-coding)
   bool                     m_assign_by_region;
   double                   m_region_xmin;
   double                   m_region_xmax;

 private: // State variables
   unsigned int                m_vname_idx;       // round-robin index (alternating)
   unsigned int                m_points_received; // total real points seen
   unsigned int                m_invalid_received;
   bool                        m_first_received;
   bool                        m_last_received;
   std::map<std::string, unsigned int> m_vcount;  // points sent per vehicle

   std::set<std::string>       m_ready_vehicles;  // vehicles whose pGenPath is up
   bool                        m_distribution_started;
   unsigned int                m_settle_ticks;    // grace ticks after all-ready
};

#endif
