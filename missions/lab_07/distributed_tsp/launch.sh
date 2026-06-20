#!/bin/bash
#----------------------------------------------------------
#  Script: launch.sh   ::  Lab 7 Distributed TSP
#  LastEd: June 19th 2026
#----------------------------------------------------------
#  Part 1: Set global var defaults
#----------------------------------------------------------
TIME_WARP=1
JUST_MAKE="no"
GUI="yes"           # set to "no" with --nogui for headless operation
HEADLESS="no"       # --headless: auto-deploy, run timed, then exit
RUN_SECS=60         # wall-clock seconds to run when --headless
REGION="false"      # --region: pPointAssign splits points East/West

VNAME1="gilda"
VNAME2="henry"
START_POS1="0,0"
START_POS2="80,0"
LOITER_POS1="x=0,y=-75"
LOITER_POS2="x=125,y=-50"

#-----------------------------------------------------------
#  Part 2: Check for and handle command-line arguments
#-----------------------------------------------------------
for ARGI; do
    if [ "${ARGI}" = "--help" -o "${ARGI}" = "-h" ]; then
	echo "launch.sh [SWITCHES] [time_warp] "
	echo "  --just_build, -j   Assemble targ files only, do not launch"
	echo "  --nogui            Launch without pMarineViewer (headless DB)"
	echo "  --headless[=SECS]  No GUI; auto-deploy; run SECS then exit"
	echo "  --help, -h         "
	exit 0;
    elif [ "${ARGI//[^0-9]/}" = "$ARGI" -a "$TIME_WARP" = 1 ]; then
        TIME_WARP=$ARGI
    elif [ "${ARGI}" = "--just_build" -o "${ARGI}" = "-j" ]; then
	JUST_MAKE="yes"
    elif [ "${ARGI}" = "--region" ]; then
	REGION="true"
    elif [ "${ARGI}" = "--nogui" ]; then
	GUI="no"
    elif [ "${ARGI}" = "--headless" ]; then
	GUI="no"; HEADLESS="yes"
    elif [ "${ARGI:0:11}" = "--headless=" ]; then
	GUI="no"; HEADLESS="yes"; RUN_SECS="${ARGI:11}"
    else
	echo "launch.sh Bad Arg:" $ARGI " Exit code 1."
	exit 1
    fi
done

#-----------------------------------------------------------
#  Part 3: Create the .moos and .bhv files.
#-----------------------------------------------------------
# Ports use a 91xx/93xx range to avoid colliding with other MOOS missions
# that may be running concurrently on the standard 90xx/92xx ports.
nsplug meta_vehicle.moos targ_$VNAME1.moos -f WARP=$TIME_WARP \
       VNAME=$VNAME1        START_POS=$START_POS1             \
       VPORT="9101"         PSHARE_PORT="9301"                \
       VTYPE="kayak"        SHORE_IP="localhost"              \
       SHORE_PSHARE="9300"  IP_ADDR="localhost"

nsplug meta_vehicle.moos targ_$VNAME2.moos -f WARP=$TIME_WARP \
       VNAME=$VNAME2        START_POS=$START_POS2             \
       VPORT="9102"         PSHARE_PORT="9302"                \
       VTYPE="kayak"        SHORE_IP="localhost"              \
       SHORE_PSHARE="9300"  IP_ADDR="localhost"

nsplug meta_vehicle.bhv targ_$VNAME1.bhv -f VNAME=$VNAME1     \
       START_POS=$START_POS1 LOITER_POS=$LOITER_POS1

nsplug meta_vehicle.bhv targ_$VNAME2.bhv -f VNAME=$VNAME2     \
       START_POS=$START_POS2 LOITER_POS=$LOITER_POS2

nsplug meta_shoreside.moos targ_shoreside.moos -f WARP=$TIME_WARP \
       VNAME="shoreside"  PSHARE_PORT="9300"                      \
       VPORT="9100"       IP_ADDR="localhost"   GUI=$GUI          \
       REGION=$REGION

if [ ${JUST_MAKE} = "yes" ]; then
    echo "Files assembled; nothing launched; exiting per request."
    exit 0
fi

#-----------------------------------------------------------
#  Part 4: Launch the processes
#-----------------------------------------------------------
echo "Launching Shoreside MOOS Community. WARP is" $TIME_WARP
pAntler targ_shoreside.moos >& /dev/null &
echo "Launching $VNAME1 MOOS Community. WARP is" $TIME_WARP
pAntler targ_$VNAME1.moos >& /dev/null &
echo "Launching $VNAME2 MOOS Community. WARP is" $TIME_WARP
pAntler targ_$VNAME2.moos >& /dev/null &
echo "Done"

#-----------------------------------------------------------
#  Part 5a: Headless auto-run (for automated validation)
#-----------------------------------------------------------
if [ "${HEADLESS}" = "yes" ]; then
    echo "Headless mode: waiting for communities to settle..."
    sleep 5
    echo "Poking shoreside: DEPLOY_ALL=true, MOOS_MANUAL_OVERRIDE_ALL=false"
    uPokeDB targ_shoreside.moos DEPLOY_ALL=true MOOS_MANUAL_OVERRIDE_ALL=false \
            STATION_KEEP_ALL=false RETURN_ALL=false >& /dev/null
    echo "Running for ${RUN_SECS} wall-clock seconds (warp ${TIME_WARP})..."
    sleep ${RUN_SECS}
    echo "Time elapsed; shutting down."
    # Clean up only this mission's processes (mission-specific patterns so we
    # never disturb other MOOS communities the user may be running).
    pkill -f "targ_shoreside.moos|targ_$VNAME1.moos|targ_$VNAME2.moos" >& /dev/null
    exit 0
fi

#-----------------------------------------------------------
#  Part 5b: Interactive - launch uMAC, kill everything on exit
#-----------------------------------------------------------
uMAC targ_shoreside.moos

kill -- -$$
