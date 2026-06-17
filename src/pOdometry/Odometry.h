/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.h                                          */
/*    DATE: June 16th, 2026                             */
/************************************************************/
#ifndef Odometry_HEADER
#define Odometry_HEADER

#include <list> // Include at the top of the file
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class Odometry : public AppCastingMOOSApp
{
 public:
   Odometry();
   ~Odometry();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();
 
 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void registerVariables();

 private: // Configuration variables

 private: // State variables
    bool   m_first_reading;
    double m_current_x;
    double m_current_y;
    double m_previous_x;
    double m_previous_y;
    double m_total_distance;

 protected: // private/protected member variables  
    std::list<double> m_nav_x_list;
    std::list<double> m_nav_y_list;
};

#endif 
