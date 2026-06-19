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
  
  m_depth_thresh   = 0.0;
  m_dist_at_depth  = 0.0;
  m_current_depth  = 0.0;
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
        // Add this new block to handle incoming depth
        else if(key == "NAV_DEPTH" && msg.IsDouble()) {
          m_current_depth = msg.GetDouble();
        }
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

            // Accumulate distance traveled while below depth threshold
            if (m_current_depth > m_depth_thresh) {
              m_dist_at_depth += leg_dist;
            }
          
          m_previous_x = m_current_x;
          m_previous_y = m_current_y;
      }
  }

        // Publish the standard distance and the new conditional distance
        Notify("ODOMETRY_DIST", m_total_distance);
        Notify("ODOMETRY_DIST_AT_DEPTH", m_dist_at_depth);

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
    // Add this new block to read the depth threshold
    if(param == "depth_thresh") {
      m_depth_thresh = atof(value.c_str());
    }
    // ... handle other parameters if any

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

  // Add this new registration
  Register("NAV_DEPTH", 0); 
}


//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "Odometry Status Report                           " << endl;
  m_msgs << "============================================" << endl;
  m_msgs << "Total Odometry:     " << m_total_distance << " meters\n";
  m_msgs << "Odometry at Depth:  " << m_dist_at_depth << " meters\n";
  m_msgs << "Depth Threshold:    " << m_depth_thresh << " meters\n";
  m_msgs << "Current Depth:      " << m_current_depth << " meters\n";
 // m_msgs << "Total Distance Traveled: " << m_total_distance << " meters" << endl;

  // ACTable actab(4);
  // actab << "Alpha | Bravo | Charlie | Delta";
  // actab.addHeaderLines();
  // actab << "one" << "two" << "three" << "four";
  // m_msgs << actab.getFormattedString();

  return(true);
}
