/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.cpp                                        */
/*    DATE: June 16th, 2026                             */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "Odometry.h"
#include <cmath>

using namespace std;

//---------------------------------------------------------
// Constructor()

Odometry::Odometry()
{
  m_first_reading = true;
  m_current_x = 0.0;
  m_current_y = 0.0;
  m_previous_x = 0.0;
  m_previous_y = 0.0;
  m_total_distance = 0.0;
}

//---------------------------------------------------------
// Destructor

Odometry::~Odometry()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool Odometry::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

if (key == "NAV_X" && msg.IsDouble()) {
    m_nav_x_list.push_back(msg.GetDouble());
  }
  else if (key == "NAV_Y" && msg.IsDouble()) {
      m_nav_y_list.push_back(msg.GetDouble());
  }
    else if(key != "APPCAST_REQ") // handled by AppCastingMOOSApp
        reportRunWarning("Unhandled Mail: " + key);
}
	
   return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool Odometry::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool Odometry::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Draining the queues to beat the 10Hz vs 4Hz gotcha
  while (!m_nav_x_list.empty() && !m_nav_y_list.empty()) {
      m_current_x = m_nav_x_list.front();
      m_nav_x_list.pop_front();
      
      m_current_y = m_nav_y_list.front();
      m_nav_y_list.pop_front();

      if (m_first_reading) {
          m_previous_x = m_current_x;
          m_previous_y = m_current_y;
          m_first_reading = false;
      } else {
          double leg_dist = std::sqrt((m_current_x - m_previous_x) * (m_current_x - m_previous_x) +
                                  (m_current_y - m_previous_y) * (m_current_y - m_previous_y));
          // Add this new line to publish the individual leg distance
          Notify("LEG_DIST", leg_dist);
          // Print the intermediate leg distance to the terminal
          cout << "LEG_DIST: " << leg_dist << endl;
          
          m_total_distance += leg_dist;
          
          cout << "ODOMETRY_DIST: " << m_total_distance << endl; // Added line
          
          m_previous_x = m_current_x;
          m_previous_y = m_current_y;
      }
  }

  // Publish the updated distance to the MOOSDB
  Notify("ODOMETRY_DIST", m_total_distance);

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Odometry::OnStartUp()
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

    bool handled = false;
    if(param == "foo") {
      handled = true;
    }
    else if(param == "bar") {
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);

  }
  
  registerVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void Odometry::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  // Register("FOOBAR", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "File: Odometry.cpp                          " << endl;
  m_msgs << "============================================" << endl;
  m_msgs << "Total Distance Traveled: " << m_total_distance << " meters" << endl;

  // ACTable actab(4);
  // actab << "Alpha | Bravo | Charlie | Delta";
  // actab.addHeaderLines();
  // actab << "one" << "two" << "three" << "four";
  // m_msgs << actab.getFormattedString();

  return(true);
}
