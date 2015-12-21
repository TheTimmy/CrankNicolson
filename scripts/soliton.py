import CrankNicolson as cn
import utility as util
import sys

#example potentials
def noPotential(x):
	return 0.0

def potentialWell(x):
	if (x < 0.300 or x > 0.600):
        	return 100	
	return 0.0

def hamonicalPotential(x):
	omega = 10
	return 0.5 * omega ** 2 * (x - 0.5)**2

outfile = open("Soliton.dat", "w")

#set the current solver for the crank nicolson algorithm here the non linear hamiltonian solver to solve soliton waves
simulation.setSolver(cn.NonLinearHamiltonianSolver(simulation, noPotential, 0.01))

#add a new Gaussian wave to the simulation with the width of 50 at the position 400 and with wavevector length of 50000
simulation.addWave(cn.GaussianWave(100.0, 500.0, 50000.0))
#add a new Gaussian wave to the simulation with the width of 20 at the position 600 and with wavevector length of -50000
simulation.addWave(cn.GaussianWave(20.0, 600.0, -50000.0))

#add a observable for the simulation. In this case the observable is the Properbility which gets writen on stdout
simulation.addFilter(cn.ProperbilityObservable(outfile))

#add also an observable for the potential only writes ones to stdout at the start of the simulation
simulation.addFilter(cn.PotentialObservable(outfile, hamonicalPotential))

#write the plot file
util.plot.writeStaticPlotScript("Soliton.static.plot", "Soliton.dat", 1)
util.plot.writeAnimatedPlotScipt("Soliton.dynamic.plot", "Soliton.dat", simulation.getParameter().iterations)

