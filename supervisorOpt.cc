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
struct point
{
	float x,y,z;
	
	float operator-(const point &rhs)
	{
		return sqrt((x-rhs.x)*(x-rhs.x)+(y-rhs.y)*(y-rhs.y)+(z-rhs.z)*(z-rhs.z));
	}
};

void pushDY();

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
		dstate= 0;
		calibrated = false;
		walk->getParams(&newp);
		walk->getParams(&orgp);
		__printData=true;
	}
	void uninit ( void ) {};
	void activate ( void ){};
	void deactivate ( void ) {};
	void update()
	{
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
				dy_data_set_float(dy_data_retrieve("robot.walking.paramsChanged"), .0);
				start = getPosition();
				organg = getAngle();
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
		else if(state==3)//turn back
		{
			if(!lock)
			{
				dy_data_set_float(dy_data_retrieve("robot.avgpower"), avgpwr->getPower());
				dy_data_set_float(dy_data_retrieve("robot.avgspeed"), (getPosition()-start)/SIMTIME);
				dy_data_set_float(dy_data_retrieve("robot.paramstested"), 1.);
				lock=true;
				walk->setSpeedCommand(.0);
				walk->setParams(&orgp);
			}
			else
			{
				point pos = getPosition();
				if(turnTo(atan2(start.x-pos.x, start.y-pos.y)+M_PI) && goTo(start) && turnTo(organg.z))
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

		pushDY();
	}
private:
	bool readyToWalk()
	{
		return (bool)dy_data_get_float(dy_data_retrieve("robot.walking.paramsChanged")) 
			&& !(bool)dy_data_get_float(dy_data_retrieve("robot.pause"));
	}
	
	bool hasPaused()
	{
		return (bool)dy_data_get_float(dy_data_retrieve("robot.pause"));
	}
	
	point getPosition()
	{
		point p;
		p.x = dy_data_get_float(dy_data_retrieve("robot.position.x"));
		p.y = dy_data_get_float(dy_data_retrieve("robot.position.y"));
		p.z = dy_data_get_float(dy_data_retrieve("robot.position.z"));
		return p;
	}
	
	point getAngle()
	{
		point p;
		p.x = dy_data_get_float(dy_data_retrieve("robot.angle.x"));
		p.y = dy_data_get_float(dy_data_retrieve("robot.angle.y"));
		p.z = dy_data_get_float(dy_data_retrieve("robot.angle.z"));
		return p;
	}
	
	void getWalkParams(WalkParam_t *wp)
	{
		wp->tripodTime = dy_data_get_double(dy_data_retrieve("robot.walking.params.tripodTime"));
		wp->standAdjTime = dy_data_get_double(dy_data_retrieve("robot.walking.params.standAdjTime"));
		wp->cpgPeriod = dy_data_get_double(dy_data_retrieve("robot.walking.params.cpgPeriod"));
		wp->smooth = dy_data_get_float(dy_data_retrieve("robot.walking.params.smooth"));
	}
	
	bool turnTo(float theta)
	{
		return true;
		while(theta>M_PI*2)
			theta-=M_PI*2;
		while(theta<-M_PI*2)
			theta+=M_PI*2;
		point ang = getAngle();
		if(fabs(ang.z-theta)>.05)
		{
			walk->setTurnCommand(1.*(ang.z>theta?-1:1));
			return false;
		}
		else
		{
			walk->setTurnCommand(.0);
			return true;
		}
	}
	
	bool goTo(point target)
	{
		point pos = getPosition();
		if(fabs(pos.y-target.y)>.03)
		//if(fabs(pos.x-target.x)+fabs(pos.y-target.y)+fabs(pos.z-target.z)>.03)
		{
			walk->setSpeedCommand(-1.);
			return false;
		}
		else
		{
			walk->setSpeedCommand(.0);
			return true;
		}
	}
	
	CalibMachine *calib[6];
	StandMachine *stand;
	WalkMachine *walk;
	SitMachine *sit;
	APModule *avgpwr;
	bool lock, calibrated;
	int dstate,state;
	unsigned int mark;
	point start, organg;
};

void pullDY()
{
	dy_network_pull("robot.position.x");
	dy_network_pull("robot.position.y");
	dy_network_pull("robot.position.z");
	dy_network_pull("robot.angle.x");
	dy_network_pull("robot.angle.y");
	dy_network_pull("robot.angle.z");
	dy_network_pull("robot.walking.paramsChanged");
	dy_network_pull("robot.walking.params.tripodTime");
	dy_network_pull("robot.walking.params.standAdjTime");
	dy_network_pull("robot.walking.params.cpgPeriod");
	dy_network_pull("robot.walking.params.smooth");
	dy_network_pull("robot.pause");
}

void pushDY()
{
	dy_network_push("robot.position.x");
	dy_network_push("robot.position.y");
	dy_network_push("robot.position.z");
	dy_network_push("robot.angle.x");
	dy_network_push("robot.angle.y");
	dy_network_push("robot.angle.z");
	dy_network_push("robot.walking.paramsChanged");
	dy_network_push("robot.walking.params.tripodTime");
	dy_network_push("robot.walking.params.standAdjTime");
	dy_network_push("robot.walking.params.cpgPeriod");
	dy_network_push("robot.walking.params.smooth");
	dy_network_push("robot.avgpower");
	dy_network_push("robot.avgspeed");
	dy_network_push("robot.paramstested");
	dy_network_push("robot.pause");
}

void startDY()
{	
	dy_init(0, NULL);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.x"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.y"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.position.z"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.x"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.y"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.angle.z"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.walking.paramsChanged"), 0);
	dy_data_set_double(dy_data_create(DY_DOUBLE, "robot.walking.params.tripodTime"), WALK_TRIPODTIME_DFLT);
	dy_data_set_double(dy_data_create(DY_DOUBLE, "robot.walking.params.standAdjTime"), WALK_ADJTIME_DFLT);
	dy_data_set_double(dy_data_create(DY_DOUBLE, "robot.walking.params.cpgPeriod"), WALK_CPGPERIOD_DFLT);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.walking.params.smooth"), WALK_SMOOTH_DFLT);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.avgpower"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.avgspeed"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.paramstested"), 0);
	dy_data_set_float(dy_data_create(DY_FLOAT, "robot.pause"), 0);
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
