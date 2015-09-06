import copy
import dy
from dyExt import *
import time
import numpy
from numpy import array
import pickle
import os

a = [[array([ 0.5603125,  0.2140625,  0.1925   ,  0.1303125]), 1.3133382155107036], [array([ 0.47828125,  0.31015625,  0.28625   ,  0.13578125]), 1.7078600626103295], [array([ 0.34 ,  0.275,  0.33 ,  0.185]), 2.8181672657131087], [array([ 0.39625,  0.20625,  0.38   ,  0.24125]), 2.830223576766543], [array([ 0.48306641,  0.27705078,  0.18078125,  0.20462891]), 1.357284867282448]]

print a

power = None
speed = None
paramsTested = None
paramsChanged = None
tripodTime = None
standAdjTime = None
cpgPeriod = None
smooth = None

def initOptimizer():
	global power,speed,paramsTested,paramsChanged,tripodTime,standAdjTime,cpgPeriod,smooth
	power = dyData("robot.avgpower")
	speed = dyData("robot.avgspeed")
	paramsTested = dyData("robot.paramstested")
	paramsChanged = dyData("robot.walking.paramsChanged")
	tripodTime = dyData("robot.walking.params.tripodTime")
	standAdjTime = dyData("robot.walking.params.standAdjTime")
	cpgPeriod = dyData("robot.walking.params.cpgPeriod")
	smooth = dyData("robot.walking.params.smooth")

def getPower():
	return power.get()
	
def getSpeed():
	return speed.get()	

def isParamsTested():
	return bool(paramsTested.get())

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
		setParams(x)
		while not isParamsTested():
			time.sleep(.1)
		paramsTested.set(0.)
		res+=getPower()/(m*g*getSpeed())
	print "Got:", res/1.
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
	prev_best = f(x_start)
	no_improv = 0
	res = [[x_start, prev_best]]
        if os.path.isfile("session.p"):
                res = pickle.load(open("session.p", "rb"))
	for i in range(dim-len(res)+1):
		x = copy.copy(x_start)
		x[i] = x[i] + step
		score = f(x)
		res.append([x, score])

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
	while len(dySet('').map)==0:
		time.sleep(.1)
	initOptimizer()
	print nelder_mead(costF, numpy.array([0.265, .2, .53, .11]))
	print "DONE"

import sys
optimizer(sys.argv)
