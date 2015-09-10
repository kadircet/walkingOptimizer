import copy
import dy
from dyExt import *
import time
import numpy
from numpy import array
import pickle
import os

power = None
speed = None
paramsTested = None
paramsChanged = None
tripodTime = None
standAdjTime = None
cpgPeriod = None
smooth = None
curx = None
cury = None
curz = None
anglex = None
anglez = None
angley = None
speedcmd = None
turncmd = None
pause = None

def initOptimizer():
	global power,speed,paramsTested,paramsChanged,tripodTime,standAdjTime,cpgPeriod,smooth,curx,cury,curz,anglex,angley,anglez,speedcmd,turncmd,pause
	power = dyData("robot.avgpower")
	speed = dyData("robot.avgspeed")
	paramsTested = dyData("robot.paramstested")
	paramsChanged = dyData("robot.walking.paramsChanged")
	tripodTime = dyData("robot.walking.params.tripodTime")
	standAdjTime = dyData("robot.walking.params.standAdjTime")
	cpgPeriod = dyData("robot.walking.params.cpgPeriod")
	smooth = dyData("robot.walking.params.smooth")
	curx = dyData("robot.position.x")
	cury = dyData("robot.position.y")
	curz = dyData("robot.position.z")
	anglex = dyData("robot.angle.x")
	angley = dyData("robot.angle.y")
	anglez = dyData("robot.angle.z")
        speedcmd = dyData("robot.walking.speed")
        turncmd = dyData("robot.walking.turn")
        pause = dyData("robot.pause")
"""
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
"""

def goTo(target):
    print "goto start with target:", target
    cur = getPosition()
    print "current position:", cur
    flag = False
    speedcmd.set(-.2)
    while sum(map(lambda x,y:abs(x-y), target, cur))>.3:
        cur = getPosition()
        if target[1]>cur[1] and not flag:
            break
        elif target[1]<cur[1] and flag:
            break
    print "goto exit"
    speedcmd.set(.0)

def getPower():
	return power.get()

def getSpeed(start):
	return sum(map(lambda x,y:(x-y)**2, getPosition(), start))**.5

def isParamsTested():
	return bool(paramsTested.get())

def getPosition():
	return [curx.get(), cury.get(), curz.get()]

def getAngle():
	return [anglex.get(), angley.get(), anglez.get()]

def setParams(x):
	tripodTime.set(x[0])
	standAdjTime.set(x[1])
	cpgPeriod.set(x[2])
	smooth.set(x[3])
	paramsChanged.set(1.)
	
def costF(x):
	print "Testing for:", x
	g=9.8
	m=10.
	res=.0
	for i in range(1):
		startp = getPosition()
		starta = getAngle()
		setParams(x)
		while not isParamsTested():
			time.sleep(.1)
		paramsTested.set(0.)
		res+=getPower()/(m*g*getSpeed(startp))
		goTo(startp)
	print "Got:", res/1.
        while True:
            time.sleep(1)
        return res/1.	
	
def nelder_mead(f, x_start,
				step=0.1, no_improve_thr=10e-6,
				no_improv_break=10, max_iter=0,
				alpha=1., gamma=2., rho=-0.5, sigma=0.5):
	'''
		@param f (function): function to optimize, must return a scalar score
			and operate over a numpy array of the same dimensions as x_start
		@param x_start (numpy array): initial position
		@param step (float): look-around radius in initial step
		@no_improv_thr,  no_improv_break (float, int): break after no_improv_break iterations with
			an improvement lower than no_improv_thr
		@max_iter (int): always break after this number of iterations.
			Set it to 0 to loop indefinitely.
		@alpha, gamma, rho, sigma (floats): parameters of the algorithm
			(see Wikipedia page for reference)

		return: tuple (best parameter array, best score)
	'''

	# init
        dim = len(x_start)
        no_improv = 0
        if os.path.isfile("session.p"):
                res = pickle.load(open("session.p", "rb"))
                print "Found old session, resuming with", res
        else:
            prev_best = f(x_start)
            res = [[x_start, prev_best]]
        for i in range(dim-len(res)+1):
		x = copy.copy(x_start)
		x[i] = x[i] + step
		score = f(x)
		res.append([x, score])
        prev_best = res[0][1]
	# simplex iter
	iters = 0
	while 1:
		# order
		res.sort(key=lambda x: x[1])
		pickle.dump(res, open("session.p", "wb"))
		best = res[0][1]

		# break after max_iter
		if max_iter and iters >= max_iter:
			return res[0]
		iters += 1

		# break after no_improv_break iterations with no improvement
		print '...best so far:', best

		if best < prev_best - no_improve_thr:
			no_improv = 0
			prev_best = best
		else:
			no_improv += 1

		if no_improv >= no_improv_break:
			return res[0]

		# centroid
		x0 = [0.] * dim
		for tup in res[:-1]:
			for i, c in enumerate(tup[0]):
				x0[i] += c / (len(res)-1)

		# reflection
		xr = x0 + alpha*(x0 - res[-1][0])
		rscore = f(xr)
		if res[0][1] <= rscore < res[-2][1]:
			del res[-1]
			res.append([xr, rscore])
			continue

		# expansion
		if rscore < res[0][1]:
			xe = x0 + gamma*(x0 - res[-1][0])
			escore = f(xe)
			if escore < rscore:
				del res[-1]
				res.append([xe, escore])
				continue
			else:
				del res[-1]
				res.append([xr, rscore])
				continue

		# contraction
		xc = x0 + rho*(x0 - res[-1][0])
		cscore = f(xc)
		if cscore < res[-1][1]:
			del res[-1]
			res.append([xc, cscore])
			continue

		# reduction
		x1 = res[0][0]
		nres = []
		for tup in res:
			redx = x1 + sigma*(tup[0] - x1)
			score = f(redx)
			nres.append([redx, score])
		res = nres
	
def optimizer(argv):
	dy.init()
	dy.network.connect(argv[1], int(argv[2]))
        dy.network.connect("localhost", 8650)
        time.sleep(10.)
        initOptimizer()
	print nelder_mead(costF, numpy.array([0.265, .2, .53, .11]))
	print "DONE"

import sys
optimizer(sys.argv)
