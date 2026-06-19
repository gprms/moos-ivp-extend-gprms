/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.cpp                                        */
/*    DATE: June 18th, 2026                             */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "PointAssign.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

PointAssign::PointAssign()
{
  m_assign_to_henry = true;
}

//---------------------------------------------------------
// Destructor

PointAssign::PointAssign()
{
  m_assign_by_region = false;
  m_vname_idx = 0;
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

    if (key == "VISIT_POINT") {
      
      // 1. Handle the bookend flags (send to ALL vehicles)
      if (sval == "firstpoint" || sval == "lastpoint") {
        for (unsigned int i = 0; i < m_vnames.size(); i++) {
          string var_name = "VISIT_POINT_" + m_vnames[i];
          Notify(var_name, sval);
        }
      }
      
      // 2. Handle normal coordinate points
      else {
        string target_vname = "";

        if (m_assign_by_region) {
           // Parse the X coordinate from "x=8, y=9, id=1"
           double x_pos = 0;
           string x_str = tokStringParse(sval, "x", ',', '=');
           if (x_str != "") {
               x_pos = atof(x_str.c_str());
           }

           // Split the region at X = 87.5
           if (x_pos < 87.5 && m_vnames.size() > 0) {
             target_vname = m_vnames; // Send West points to vehicle 1
           } else if (m_vnames.size() > 1) {
             target_vname = m_vnames[2]; // Send East points to vehicle 2
           }
        } 
        else {
           // Alternate the assignment
           if (m_vnames.size() > 0) {
             target_vname = m_vnames[m_vname_idx];
             m_vname_idx = (m_vname_idx + 1) % m_vnames.size();
           }
        }

        // 3. Post the point to the targeted vehicle
        if (target_vname != "") {
          string var_name = "VISIT_POINT_" + target_vname;
          Notify(var_name, sval);
        }
      }
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool PointAssign::OnConnectToServer()
{
  registerVariables();
  
  // Trigger uTimerScript to start firing points
  Notify("UTS_PAUSE", "false"); 
  
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool PointAssign::Iterate()
{
  AppCastingMOOSApp::Iterate();
  // Do your thing here!
  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

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
    string value = line;

    // Read the vehicle names dynamically
    if(param == "vname") {
      m_vnames.push_back(toupper(value));
    }
    // Read the assignment mode
    else if(param == "assign_by_region") {
      m_assign_by_region = (tolower(value) == "true");
    }
  }
  
  registerVariables();  
  return(true);
}


//---------------------------------------------------------
// Procedure: registerVariables()

void PointAssign::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  
  // Register for incoming points from uTimerScript
  Register("VISIT_POINT", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool PointAssign::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "File:                                       " << endl;
  m_msgs << "============================================" << endl;

  ACTable actab(4);
  actab << "Alpha | Bravo | Charlie | Delta";
  actab.addHeaderLines();
  actab << "one" << "two" << "three" << "four";
  m_msgs << actab.getFormattedString();

  return(true);
}




