import CrankNicolson as cn
import utility as util
import sys

# execute the green function on the current hamiltonian
class GreenFunction(cn.Observable):
	def __init__(self, r, c, out):
		cn.Observable.__init__(self, cn.CheckTime.Startup)
		self.r = r
		self.c = c
		self.out = out

	def filter(self, sim):
		for i in range(0, self.r):
			self.out.write(str(i) + " " + str(self.greenFunction(float(i) / self.c, sim)) + "\n")
		self.out.write("\n\n")

	def greenFunction(self, energy, sim):
		mat = sim.getSolver().getHamiltonianMatrix()
		x = energy - mat[(cn.Line.Diagonal, mat.size() - 1)]
		for i in range(mat.size() - 2, 1, -1):
			x = energy - mat[(cn.Line.Diagonal, i)] - mat[(cn.Line.Upper, i)] * mat[(cn.Line.Upper, i)] / x
        	return abs(1.0 / x)

def noPotential(x):
	return 0.0

outfile = open("Green.dat", "w")

#add a new Gaussian wave to the simulation with the width of 50 at the position 400 and with wavevector length of 50000
simulation.addWave(cn.GaussianWave(100.0, 500.0, 50000.0))

#add the custom filter to the simulation
simulation.addFilter(GreenFunction(100, 0.1, outfile))

util.plot.writeStaticPlotScript("Green.dynamic.plot", "Green.dat", 1)

