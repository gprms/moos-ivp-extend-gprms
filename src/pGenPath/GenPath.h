/************************************************************/
/*    NAME: gprms                                           */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.h                                       */
/*    DATE: June 18th, 2026                                 */
/*    DESC: Vehicle-side app. Ingests the VISIT_POINT set   */
/*          sent from the shoreside, waits for the complete */
/*          set, builds a greedy nearest-neighbor tour from  */
/*          the vehicle's present position, and publishes it */
/*          to the waypoint behavior as a dynamic update.    */
/************************************************************/

#ifndef GenPath_HEADER
#define GenPath_HEADER

#include <string>
#include <vector>
#include "XYPoint.h"
#include "XYSegList.h"
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class GenPath : public AppCastingMOOSApp
{
 public:
   GenPath();
   ~GenPath() {}

 protected: // Standard MOOSApp functions to overload
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload
   bool buildReport();

 protected:
   void registerVariables();
   void generateTour();
   void updateVisited();

 private: // Configuration variables
   std::string m_update_var;   // MOOS var the waypoint behavior reads (updates=)
   double      m_visit_radius; // a point counts visited within this many meters

 private: // State variables
   std::vector<XYPoint> m_points;     // all received tour points
   std::vector<bool>    m_visited;    // parallel to m_points

   bool   m_first_received;
   bool   m_last_received;
   bool   m_tour_generated;
   bool   m_nav_received;

   double m_nav_x;
   double m_nav_y;

   unsigned int m_total_received;
   unsigned int m_invalid_received;
   double       m_route_length;
};

#endif
