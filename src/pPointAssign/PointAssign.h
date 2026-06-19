/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.h                                          */
/*    DATE: June 18th, 2026                             */
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include <vector>
#include <string>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class PointAssign : public AppCastingMOOSApp
{
 public:
   PointAssign();
   ~PointAssign();

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

 protected: // State variables
  bool m_assign_to_henry;
 
  protected: // State variables
  std::vector<std::string> m_vnames;
  bool m_assign_by_region;
  unsigned int m_vname_idx;
};

#endif 
