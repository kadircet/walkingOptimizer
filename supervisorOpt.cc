#include <signal.h>
#include <dy.h>
#include "ModuleManager.hh"
#include "modules/RHexModules.hh"
#include "modules/EncoderReader.hh"
#include "modules/PositionControl.hh"
#include "modules/Profiler.hh"
#include "modules/CalibMachine.hh"
#include "modules/StandMachine.hh"
#include "modules/WalkMachine.hh"
#include "modules/SitMachine.hh"
#include "modules/AvgPower.hh"
#define SIMTIME 5.
#define NETWORKINTERVAL .1

extern bool __printData;

void exit_on_ctrl_c(int)
{
	static bool control_c_invoked = false;
	if (control_c_invoked)
		return;
	control_c_invoked = true;
	MMMessage( "User Ctrl-C: Shutting down!\n");
	MMExitMainLoop();
}

class SafetyModule : public Module
{
public:
	SafetyModule ( void ) : Module( "safety", 0, false ) { };

	void	init ( void ) { };
	void	uninit ( void ) { };
	void	activate ( void ) { };
	void	deactivate ( void ) { };
	void	update ( void ) {

	if ( MMkbhit() ) {
		 MMMessage( "User interrupt: Shutting down!\n" );
		 MMExitMainLoop( );
	 }
	}
};

WalkParam_t newp, orgp;

DyData *dppuase, *avgpower, *paramstested, *paramschanged, *tripodtime,
	*standadjtime, *smooth, *speed, *turn, *cpgperiod;

class mySup : public Module
{
public:
	mySup() : Module("mysup", 0, false) {};
	void init ( void )
	{
		for(int i=0;i<6;i++)
			calib[i] = ( CalibMachine * ) MMFindModule( CALIBMACHINE_NAME, i );
		stand	= (StandMachine *)MMFindModule(STANDMACHINE_NAME, 0);
		walk	= (WalkMachine *)MMFindModule(WALKMACHINE_NAME,	0);
		sit		= (SitMachine *)MMFindModule(SITMACHINE_NAME,	 0);
		avgpwr	= (APModule *)MMFindModule(APMODULE_NAME, 0);
		lock	= false;
		state	= 0;
		dstate	= 0;
		calibrated = false;
		walk->getParams(&newp);
		walk->getParams(&orgp);
		lastpull= MMReadTime();
		
		__printData=true;
	}
	void uninit ( void ) {};
	void activate ( void ){};
	void deactivate ( void ) {};
	void update()
	{
		//pullDY();
		if(state==0 && !calibrated)//calibrate
		{
			if(!lock)
			{ 
				lock = true;
				for(int i=0;i<6;i++)
				{
					calib[i]->setMode(CalibMachine::GROUND);
					MMGrabModule(calib[i], this);
				}
			}
			else
			{
				for(int i=0;i<6;i++)
					if(calib[i]->getStatus()!=CalibMachine::SUCCESS)
						return;
				for(int i=0;i<6;i++)
					MMReleaseModule(calib[i], this);
				lock = false;
				calibrated=true;
				dstate=1;
			}
		}
		else if(dstate==1)//stand
		{
			if(!lock)
			{
				lock=true;
				for(int i=0;i<6;i++)
					RHexHipHW::instance()->enable(i, true);
				MMGrabModule(stand, this);
			}
			else
			{
				if(!stand->isDone())
					return;
				lock=false;
				state=dstate;
				dstate=2;
			}
		}
		else if(dstate==2 && (state==1||state==2))//walk
		{
			if(readyToWalk() && walk->getOwner()!=this)
			{
				MMReleaseModule(stand, this);
				MMGrabModule(walk, this);
				getWalkParams(&newp);
				walk->setParams(&newp);
				walk->setSpeedCommand(1.0);
				walk->setTurnCommand(.0);
				mark=MMReadTime();
				avgpwr->reset();
				state=2;
			}
			if(state==2 && MMReadTime()-mark>SIMTIME)
				state=3;
		}
		else if(state==3)//turn back
		{
			if(!lock)
			{
				dy_data_set_float(avgpower, avgpwr->getPower());
				dy_data_set_float(paramstested, 1.);
				pushDY();
				lock=true;
				walk->setSpeedCommand(.0);
				walk->setParams(&orgp);
			}
			else
			{
				walk->setSpeedCommand(dy_data_get_float(speed));
				walk->setTurnCommand(dy_data_get_float(turn));
				if(readyToWalk())
				{
					lock = false;
					if(walk->getOwner()==this)
					{
						MMReleaseModule(walk, this);
						MMGrabModule(stand, this);
					}
					state=1;
				}
			}
		}
		if(hasPaused())
		{
			lock=false;
			state=1;
			if(walk->getOwner()==this)
			{
				MMReleaseModule(walk, this);
				MMGrabModule(stand, this);
			}
		}
	}
private:
	bool readyToWalk()
	{
		return (bool)dy_data_get_float(paramschanged) 
			&& !(bool)dy_data_get_float(dppuase);
	}
	
	bool hasPaused()
	{
		return (bool)dy_data_get_float(dppuase);
	}
	
	void getWalkParams(WalkParam_t *wp)
	{
		pullDY(true);
		wp->tripodTime = dy_data_get_float(tripodtime);
		wp->standAdjTime = dy_data_get_float(standadjtime);
		wp->cpgPeriod = dy_data_get_float(cpgperiod);
		wp->smooth = dy_data_get_float(smooth);
	}
	
	void pullDY(bool pullParams=false)
	{
		if(MMReadTime()-lastpull<NETWORKINTERVAL)
			return;
		
		lastpull = MMReadTime();
		if(pullParams)
		{
			dy_network_pull("robot.walking.params.tripodTime");
			dy_network_pull("robot.walking.params.standAdjTime");
			dy_network_pull("robot.walking.params.cpgPeriod");
			dy_network_pull("robot.walking.params.smooth");
			
			dy_data_set_float(paramschanged, .0);
			dy_network_push("robot.walking.paramsChanged");
		}
		else
		{
			dy_network_pull("robot.walking.paramsChanged");
			dy_network_pull("robot.pause");
			dy_network_pull("robot.walking.speed");
			dy_network_pull("robot.walking.turn");
		}
	}

	void pushDY()
	{
		dy_network_push("robot.avgpower");
		dy_network_push("robot.paramstested");
	}
	
	CalibMachine *calib[6];
	StandMachine *stand;
	WalkMachine *walk;
	SitMachine *sit;
	APModule *avgpwr;
	bool lock, calibrated;
	int dstate,state;
	unsigned int mark, lastpull;
};

void startDY()
{
	dy_init(0, NULL);
	dy_data_set_float(paramschanged=dy_data_create(DY_FLOAT, "robot.walking.paramsChanged"), 0);
	dy_data_set_float(tripodtime=dy_data_create(DY_FLOAT, "robot.walking.params.tripodTime"), WALK_TRIPODTIME_DFLT);
	dy_data_set_float(standadjtime=dy_data_create(DY_FLOAT, "robot.walking.params.standAdjTime"), WALK_ADJTIME_DFLT);
	dy_data_set_float(cpgperiod=dy_data_create(DY_FLOAT, "robot.walking.params.cpgPeriod"), WALK_CPGPERIOD_DFLT);
	dy_data_set_float(smooth=dy_data_create(DY_FLOAT, "robot.walking.params.smooth"), WALK_SMOOTH_DFLT);
	dy_data_set_float(avgpower=dy_data_create(DY_FLOAT, "robot.avgpower"), 0);
	dy_data_set_float(speed=dy_data_create(DY_FLOAT, "robot.walking.speed"), 0);
	dy_data_set_float(turn=dy_data_create(DY_FLOAT, "robot.walking.turn"), 0);
	dy_data_set_float(paramstested=dy_data_create(DY_FLOAT, "robot.paramstested"), 0);
	dy_data_set_float(dppuase=dy_data_create(DY_FLOAT, "robot.pause"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.x"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.y"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.z"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.x"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.y"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.z"), 0);
}

int main( int argc, char** argv )
{
	MMReadConfigFile( "list.rc" );
	MMReadConfigFile( "robotlist.rc" );
	signal(SIGINT, exit_on_ctrl_c);

	initHardware();
	startDY();
	
	RHexAddModules();
	RHexActivateModules();
	
	Module *mm = new mySup();
	MMAddModule(mm, 1, 0, BEHAVIORAL_CONTROLLERS);
	MMActivateModule(mm);
	SafetyModule *sm = new SafetyModule();
	MMAddModule( sm, 100, 0, OTHER_MODULES );
	MMActivateModule( sm );
	MMPrintModules();
	MMMainLoop();

	cleanupHardware();
}
