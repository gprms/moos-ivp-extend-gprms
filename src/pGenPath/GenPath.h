/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/
// Add these includes at the top

#ifndef GenPath_HEADER
#define GenPath_HEADER

#include "XYPoint.h"
#include "XYSegList.h"
#include <vector>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class GenPath : public AppCastingMOOSApp
{
 public:
   GenPath();
   ~GenPath();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void registerVariables();

   // Add these to your class definition under 'protected:'
  protected: 
    std::vector<XYPoint> m_nav_points;
    bool m_receive_complete;
    
 private: // Configuration variables

 private: // State variables
};

#endif 
