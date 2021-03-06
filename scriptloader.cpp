#include "scriptloader.h"

#include <utility>
#include <boost/python.hpp>
#include <boost/python/class.hpp>
#include <boost/python/module_init.hpp>
#include <boost/python/def.hpp>
#include <boost/python/call_method.hpp>
#include <boost/ref.hpp>
#include <boost/utility.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/bind.hpp>

#include <iostream>

#include "wave.h"
#include "Vector.h"
#include "simulation.h"
#include "observable.h"
#include "gaussianwave.h"
#include "pythoninputdevice.h"

#include "potentialobservable.h"
#include "properbilityoberservable.h"
#include "energyeigenvalueobservable.h"
#include "expectationvalueobservable.h"

#include "streamdensity.h"

#include "linearhamiltonian.h"
#include "nonlinearhamiltonian.h"

using namespace boost;
using namespace python;
namespace fs = boost::filesystem;


class PythonSimulation;

template <typename T>
struct HamiltonianSolverCallback : public HamiltonianSolver<T> {
    HamiltonianSolverCallback(PyObject* o)
        : self(o) {
    }

    virtual Vector<T> solve(const Vector<T>& current) {
        return call_method<Vector<T>>(self, "solve", current);
    }

    virtual TridiagonalMatrix<T> getHamiltonianMatrix() {
        return call_method<TridiagonalMatrix<T>>(self, "getHamiltonianMatrix");
    }

    virtual TridiagonalMatrix<T> getLeftMatrix() {
        return call_method<TridiagonalMatrix<T>>(self, "getLeftMatrix");
    }

    virtual TridiagonalMatrix<T> getRightMatrix() {
        return call_method<TridiagonalMatrix<T>>(self, "getRightMatrix");
    }

    PyObject* self;
};


template <typename T>
std::string operator + (const std::string& str, const std::complex<T>& val) {
    return str + "[" + std::to_string(val.real()) + "," + std::to_string(val.imag()) + "]";
}

//Overwrite the vector () operator for python
template <typename T>
T getVectorValue(Vector<T>& vec, unsigned int index) {
    return vec[index];
}

template <typename T>
void setVectorValue(Vector<T>& vec, unsigned int index, T value) {
    vec[index] = value;
}

template <typename T>
std::string strVector(Vector<T>& vec) {
    std::string str = "(" + vec(0);
    for (unsigned int i = 1; i < vec.size(); ++i) {
        str += ", " + vec(i);
    }
    str += ")";
    return str;
}

template <typename T>
T getMatrixValue(TridiagonalMatrix<T>& mat, boost::python::tuple tup) {
    int val = extract<unsigned int>(tup[0]);
    typename TridiagonalMatrix<T>::Line line = (typename TridiagonalMatrix<T>::Line) val;

    return mat(line, extract<unsigned int>(tup[1]));
}

template <typename T>
void setMatrixValue(TridiagonalMatrix<T>& mat, boost::python::tuple tup, T value) {
    int val = extract<unsigned int>(tup[0]);
    typename TridiagonalMatrix<T>::Line line = (typename TridiagonalMatrix<T>::Line) val;
    mat(line, extract<unsigned int>(tup[1])) = value;
}

template <typename T>
Vector<T> multiplyMatrixVector(TridiagonalMatrix<T>& mat, Vector<T>& vec) {
    return mat * vec;
}

template <typename T>
std::string strMatrix(TridiagonalMatrix<T>& mat) {
    std::string str = "";

    typename TridiagonalMatrix<T>::Line lines[] = {
        TridiagonalMatrix<T>::Line::Upper, TridiagonalMatrix<T>::Line::Diagonal, TridiagonalMatrix<T>::Line::Lower
    };

    for (unsigned int i = 0; i < 3; ++i) {
        Vector<T> vec(mat.getSize());
        for (unsigned int j = 0; j < mat.getSize(); ++j) {
            vec(j) = mat(lines[i], j);
        }
        str += strVector(vec) + "\n";
    }
    return str;
}

/**
 * @brief The PythonSimulation class
 */
class PythonSimulation : public Simulation {
public:
    PythonSimulation(Simulation* sim) : Simulation(*sim) {
    }

    void addFilter(boost::python::object ob) {
        Observable& filter = extract<Observable&>(ob);
        filters.push_back(std::pair<boost::python::object, Observable&>(ob, filter));
        Simulation::addFilter(std::shared_ptr<Observable>(&filter, [] (Observable*) {}));
    }

    void addWave(boost::python::object ob) {
        ComplexWave& wave = extract<ComplexWave&>(ob);
        Simulation::addWave(&wave);
    }

    void setSolver(boost::python::object ob) {
        solver.clear();
        ComplexHamiltonianSolver& result = extract<ComplexHamiltonianSolver&>(ob);
        solver.push_back(std::pair<boost::python::object, ComplexHamiltonianSolver&>(ob, result));
        Simulation::setSolver(std::shared_ptr<ComplexHamiltonianSolver>(&result, [] (ComplexHamiltonianSolver*) {}));
    }

private:
    std::vector<std::pair<boost::python::object, ComplexHamiltonianSolver&>> solver;
    std::vector<std::pair<boost::python::object, Observable&>> filters;
};

/*class PythonObservable : public Observable {
public:
    PythonObservable(CheckTime time) : Observable(time) {
    }

    virtual void filter(PythonSimulation* simulation) {
        const Simulation& sim = static_cast<Simulation*>(simulation);
        filter(sim);
    }

    virtual void filter(const Simulation& simulation) = 0;
};*/

/**
 * @brief The PythonPropertyOberservable struct
 */
struct PythonPropertyOberservable : public Observable {
    PythonPropertyOberservable(boost::python::object ob)
        : Observable(CheckTime::Iteration) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(ob));
        prob.reset(new ProperbilityOberservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        prob->filter(sim);
        stream->flush();
    }

private:
    std::shared_ptr<ProperbilityOberservable> prob;
    std::shared_ptr<std::ostream> stream;
};

struct PythonRealPropertyOberservable : public Observable {
    PythonRealPropertyOberservable(boost::python::object ob)
        : Observable(CheckTime::Iteration) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(ob));
        prob.reset(new RealProperbilityOberservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        prob->filter(sim);
        stream->flush();
    }

private:
    std::shared_ptr<RealProperbilityOberservable> prob;
    std::shared_ptr<std::ostream> stream;
};

struct PythonImaginaryPropertyOberservable : public Observable {
    PythonImaginaryPropertyOberservable(boost::python::object ob)
        : Observable(CheckTime::Iteration) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(ob));
        prob.reset(new ImaginaryProperbilityOberservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        prob->filter(sim);
        stream->flush();
    }

private:
    std::shared_ptr<ImaginaryProperbilityOberservable> prob;
    std::shared_ptr<std::ostream> stream;
};

struct PythonPotentialObservable : public Observable {
    PythonPotentialObservable(boost::python::object output, boost::python::object f)
        : Observable(CheckTime::Startup) {
        func = f;
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(output));
        obs.reset(new PotentialObservable(*stream.get(), boost::bind(&PythonPotentialObservable::potential, this, _1)));
    }

    virtual void filter(const Simulation& sim) {
        obs->filter(sim);
        stream->flush();
    }
private:
    double potential(double x) {
        return boost::python::call<double>(func.ptr(), x);
    }

    std::shared_ptr<PotentialObservable> obs;
    std::shared_ptr<std::ostream> stream;
    boost::python::object func;
};


struct PythonProperbilityFluxObservable : public Observable {
    PythonProperbilityFluxObservable(boost::python::object output)
        : Observable(CheckTime::Iteration) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(output));
        obs.reset(new ProperbilityFluxObservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        obs->filter(sim);
        stream->flush();
    }
private:
    std::shared_ptr<ProperbilityFluxObservable> obs;
    std::shared_ptr<std::ostream> stream;
};

struct PythonEnergyValueObservable : public Observable {
    PythonEnergyValueObservable(boost::python::object output)
        : Observable(CheckTime::Startup) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(output));
        obs.reset(new EnergyEigenvalueObservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        obs->filter(sim);
        stream->flush();
    }
private:
    std::shared_ptr<EnergyEigenvalueObservable> obs;
    std::shared_ptr<std::ostream> stream;
};


struct PythonExpectationValueObservable : public Observable {
    PythonExpectationValueObservable(boost::python::object output)
        : Observable(CheckTime::Iteration) {
        stream.reset(new boost::iostreams::stream<PythonOutputDevice>(output));
        obs.reset(new ExpectationValueObservable(*stream.get()));
    }

    virtual void filter(const Simulation& sim) {
        obs->filter(sim);
        stream->flush();
    }
private:
    std::shared_ptr<ExpectationValueObservable> obs;
    std::shared_ptr<std::ostream> stream;
};


template <typename T>
class PythonLinearHamiltonianSolver : public HamiltonianSolver<T> {
public:
    PythonLinearHamiltonianSolver(PythonSimulation* sim, boost::python::object f)
        : func(f) {
        solver.reset(new LinearHamiltonianSolver<T>(sim->getParameter(), boost::bind(&PythonLinearHamiltonianSolver<T>::potential, this, _1)));
    }

    virtual Vector<T> solve(const Vector<T>& current) {
        return solver->solve(current);
    }

    virtual TridiagonalMatrix<T> getHamiltonianMatrix() {
        return solver->getHamiltonianMatrix();
    }

    virtual TridiagonalMatrix<T> getLeftMatrix() {
        return solver->getLeftMatrix();
    }

    virtual TridiagonalMatrix<T> getRightMatrix() {
        return solver->getRightMatrix();
    }

private:
    double potential(double x) {
        return boost::python::call<double>(func.ptr(), x);
    }

    boost::python::object func;
    std::shared_ptr<LinearHamiltonianSolver<T>> solver;
};

template <typename T>
class PythonNonLinearHamiltonianSolver : public HamiltonianSolver<T> {
public:
    PythonNonLinearHamiltonianSolver(PythonSimulation* sim, boost::python::object f, double factor)
        : func(f) {
        solver.reset(new NonLinearHamiltonianSolver<T>(sim->getParameter(), boost::bind(&PythonNonLinearHamiltonianSolver<T>::potential, this, _1), factor));
    }

    virtual Vector<T> solve(const Vector<T>& current) {
        return solver->solve(current);
    }

    virtual TridiagonalMatrix<T> getHamiltonianMatrix() {
        return solver->getHamiltonianMatrix();
    }

    virtual TridiagonalMatrix<T> getLeftMatrix() {
        return solver->getLeftMatrix();
    }

    virtual TridiagonalMatrix<T> getRightMatrix() {
        return solver->getRightMatrix();
    }

private:
    double potential(double x) {
        return boost::python::call<double>(func.ptr(), x);
    }

    boost::python::object func;
    std::shared_ptr<NonLinearHamiltonianSolver<T>> solver;
};

struct WaveCallback : Wave<std::complex<double>> {
    WaveCallback(PyObject *p)
        : self(p) {
    }

    std::complex<double> getDisplacement(unsigned int index) const {
        return call_method<unsigned int>(self, "getDisplacement", index);
    }

    PyObject* self;
};

struct ObservableCallback : Observable {
    ObservableCallback(PyObject *p, CheckTime Time)
        : Observable(Time), self(p) {
    }

    virtual void filter(boost::shared_ptr<PythonSimulation> sim) {
        call_method<boost::shared_ptr<PythonSimulation>>(self, "filter", sim);
    }

    virtual void filter(const Simulation& sim) {
        Simulation* k = const_cast<Simulation*>(&sim);
        filter(boost::shared_ptr<PythonSimulation>(static_cast<PythonSimulation*>(k), [&](PythonSimulation* sim) {}));
    }

    PyObject* self;
};


BOOST_PYTHON_MODULE(CrankNicolson) {
    // math stuff
    class_<Vector<std::complex<double>>>("Vector", init<unsigned int>())
            .def("__getitem__", &getVectorValue<std::complex<double>>)
            .def("__setitem__", &setVectorValue<std::complex<double>>)
            .def("__add__", &Vector<std::complex<double>>::operator+)
            .def("__sub__", &Vector<std::complex<double>>::operator-)
            .def("__mul__", &Vector<std::complex<double>>::operator*)
            .def("__str__", &strVector<std::complex<double>>)
            .def("size", &Vector<std::complex<double>>::size)
            .def("length", &Vector<std::complex<double>>::length)
            .def("normalize", &Vector<std::complex<double>>::normalised)
            .def("dot", &Vector<std::complex<double>>::dot)
    ;

    enum_<typename TridiagonalMatrix<std::complex<double>>::Line>("Line")
            .value("Upper", TridiagonalMatrix<std::complex<double>>::Upper)
            .value("Diagonal", TridiagonalMatrix<std::complex<double>>::Diagonal)
            .value("Lower", TridiagonalMatrix<std::complex<double>>::Lower)
    ;

    class_<TridiagonalMatrix<std::complex<double>>>("TridiagonalMatrix", init<unsigned int>())
            .def("__getitem__", &getMatrixValue<std::complex<double>>)
            .def("__setitem__", &setMatrixValue<std::complex<double>>)
            .def("__str__", &strMatrix<std::complex<double>>)
            .def("__add__", &TridiagonalMatrix<std::complex<double>>::operator+)
            .def("__sub__", &TridiagonalMatrix<std::complex<double>>::operator-)
            .def("__mul__", &multiplyMatrixVector<std::complex<double>>)
            .def("identity", &TridiagonalMatrix<std::complex<double>>::identity)
            .def("solve", &TridiagonalMatrix<std::complex<double>>::solve)
            .def("size", &TridiagonalMatrix<std::complex<double>>::getSize)
            .staticmethod("identity")
    ;

    // Simulation stuff
    enum_<Observable::CheckTime>("CheckTime")
            .value("Startup", Observable::Startup)
            .value("Iteration", Observable::Iteration)
            .value("Cooldown", Observable::Cooldown)
    ;

    class_<SimulationParameter>("SimulationParameter", no_init)
            .def_readonly("dx", &SimulationParameter::dx)
            .def_readonly("dt", &SimulationParameter::dt)
            .def_readonly("lambda", &SimulationParameter::lambda)
            .def_readonly("iterations", &SimulationParameter::iterations)
            .def_readonly("atomCount", &SimulationParameter::atomCount)
    ;

    class_<PythonSimulation, boost::noncopyable, boost::shared_ptr<PythonSimulation>>("Simulation", no_init)
            .def("setSolver", &PythonSimulation::setSolver)
            .def("getSolver", &PythonSimulation::getSolver, return_value_policy<reference_existing_object>())
            .def("addWave", &PythonSimulation::addWave)
            .def("addFilter", &PythonSimulation::addFilter)
            .def("run", &PythonSimulation::run)
            .def("getParameter", &PythonSimulation::getParameter)
            .def("getAtoms", &PythonSimulation::getAtoms)
    ;
    class_<Wave<std::complex<double>>, boost::noncopyable, boost::shared_ptr<WaveCallback>>("Wave")
            .def("getDisplacement", &Wave<std::complex<double>>::getDisplacement)
    ;
    class_<Observable, boost::noncopyable, boost::shared_ptr<ObservableCallback>>("Observable", init<Observable::CheckTime>())
            .def("filter", &Observable::filter)
    ;
    class_<HamiltonianSolver<std::complex<double>>, boost::noncopyable, boost::shared_ptr<HamiltonianSolverCallback<std::complex<double>>>>("HamiltonianSolver", init<>())
            .def("solve", &HamiltonianSolver<std::complex<double>>::solve)
            .def("getHamiltonianMatrix", &HamiltonianSolver<std::complex<double>>::getHamiltonianMatrix)
            .def("getLeftMatrix", &HamiltonianSolver<std::complex<double>>::getLeftMatrix)
            .def("getRightMatrix", &HamiltonianSolver<std::complex<double>>::getRightMatrix)
    ;

    //basic waves
    class_<GaussianWave<std::complex<double>>, bases<Wave<std::complex<double>>>>("GaussianWave", init<double, double, double>());

    //basic observables
    class_<PythonPropertyOberservable, bases<Observable>>("ProperbilityObservable", init<boost::python::object>());
    class_<PythonRealPropertyOberservable, bases<Observable>>("RealProperbilityObservable", init<boost::python::object>());
    class_<PythonImaginaryPropertyOberservable, bases<Observable>>("ImaginaryProperbilityObservable", init<boost::python::object>());
    class_<PythonPotentialObservable, bases<Observable>>("PotentialObservable", init<boost::python::object, boost::python::object>());
    class_<PythonProperbilityFluxObservable, bases<Observable>>("ProperbilityFluxObservable", init<boost::python::object>());
    class_<PythonExpectationValueObservable, bases<Observable>>("ExpectationValueObservable", init<boost::python::object>());
    class_<PythonEnergyValueObservable, bases<Observable>>("EnergyEigenvalueObservable", init<boost::python::object>());

    //basic solver
    class_<PythonLinearHamiltonianSolver<std::complex<double>>, bases<HamiltonianSolver<std::complex<double>>>>("LinearHamiltonianSolver", init<PythonSimulation*, boost::python::object>());
    class_<PythonNonLinearHamiltonianSolver<std::complex<double>>, bases<HamiltonianSolver<std::complex<double>>>>("NonLinearHamiltonianSolver", init<PythonSimulation*, boost::python::object, double>());
}

ScriptExecutor::ScriptExecutor(Simulation &simulation,
                           const std::string& scriptFile) {
    namespace python = boost::python;
    Py_Initialize();

    // Set working dir in python
    boost::filesystem::path workingDir = boost::filesystem::absolute(boost::filesystem::path(scriptFile).parent_path()).normalize();
    PyObject* sysPath = PySys_GetObject("path");
    PyList_Insert(sysPath, 0, PyString_FromString(workingDir.string().c_str()));

    try {
        python::object main = import("__main__");
        python::object mainNamespace = main.attr("__dict__");

        PythonSimulation sim(&simulation);
        initCrankNicolson();

        python::scope scope(main);
        scope.attr("simulation") = object(ptr(&sim));
        exec_file(scriptFile.c_str(), mainNamespace, mainNamespace);

        sim.run();
    } catch(...) {
        PyErr_Print();
    }
}

ScriptExecutor::~ScriptExecutor() {
    Py_Finalize();
}

std::vector<boost::filesystem::path> ScriptExecutor::getScriptFiles(const std::string& dir) {
    std::vector<boost::filesystem::path> paths;
    fs::path someDir(dir);
    fs::directory_iterator end_iter;

    if (fs::exists(someDir) && fs::is_directory(someDir)) {
        for (fs::directory_iterator dir_iter(someDir); dir_iter != end_iter; ++dir_iter) {
            if (fs::is_regular_file(dir_iter->status()) && fs::extension(*dir_iter) == ".py") {
                paths.push_back(*dir_iter);
            }
        }
    }
    return paths;
}
