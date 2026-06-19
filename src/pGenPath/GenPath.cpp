/************************************************************/
/*    NAME: gprms                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.cpp                                        */
/*    DATE: JUNE 18th, 2026                             */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPath.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

pGenPath::pGenPath()
{
  m_receive_complete = false;
}


//---------------------------------------------------------
// Destructor

GenPath::~GenPath()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();
    string sval   = msg.GetString();

    if (key == "VISIT_POINT") {
      if (sval == "firstpoint") {
        m_nav_points.clear(); 
      }
      else if (sval == "lastpoint") {
        m_receive_complete = true; // Trigger the tour generation!
      }
      else {
        // 1. Parse the string (e.g., "x=8, y=9, id=1")
        // 2. Create an XYPoint
        // 3. Add it to m_nav_points
        // (Use MOOS/IvP string utilities like tokStringParse to do this)
      }
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenPath::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if (m_receive_complete) {
    XYSegList my_seglist;

    // ---------------------------------------------------------
    // ALGORITHM: Write your Greedy TSP sorting logic here!
    // Sort the coordinates in m_nav_points from shortest 
    // distance to shortest distance.
    // ---------------------------------------------------------

    // Add the newly sorted points to the XYSegList object
    for (int i = 0; i < m_nav_points.size(); i++) {
      my_seglist.add_vertex(m_nav_points[i].x(), m_nav_points[i].y());
    }

    // Format the string exactly as the waypoint behavior expects it
    string update_str = "points = ";
    update_str += my_seglist.get_spec();

    // Publish to the variable you specified in your .bhv file updates parameter
    Notify("WPT_UPDATES", update_str);

    // Reset the flag so we don't recalculate infinitely
    m_receive_complete = false;
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool GenPath::OnStartUp()
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

void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool GenPath::buildReport() 
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




