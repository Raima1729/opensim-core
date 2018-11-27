/* -------------------------------------------------------------------------- *
 * OpenSim Muscollo: sandboxDoublePendulumSwingup.cpp                         *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Christopher Dembia                                              *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0          *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */
#include <OpenSim/Simulation/SimbodyEngine/PinJoint.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <Muscollo/osimMuscollo.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>

using namespace OpenSim;

/// This model is torque-actuated.
Model createDoublePendulumModel() {
    Model model;
    model.setName("double_pendulum");
    const auto& ground = model.getGround();

    using SimTK::Vec3;
    using SimTK::Inertia;

    // Create two links, each with a mass of 1 kg, center of mass at the body's
    // origin, and moments and products of inertia of zero.
    auto* b0 = new OpenSim::Body("b0", 1, Vec3(0), Inertia(1));
    model.addBody(b0);
    auto* b1  = new OpenSim::Body("b1", 1, Vec3(0), Inertia(1));
    model.addBody(b1);

    // Connect the bodies with pin joints. Assume each body is 1 m long.
    auto* j0 = new PinJoint("j0", ground, Vec3(0), Vec3(0),
            *b0, Vec3(-1, 0, 0), Vec3(0));
    auto& q0 = j0->updCoordinate();
    q0.setName("q0");
    auto* j1 = new PinJoint("j1",
            *b0, Vec3(0), Vec3(0), *b1, Vec3(-1, 0, 0), Vec3(0));
    auto& q1 = j1->updCoordinate();
    q1.setName("q1");
    model.addJoint(j0);
    model.addJoint(j1);

    auto* tau0 = new CoordinateActuator();
    tau0->setCoordinate(&j0->updCoordinate());
    tau0->setName("tau0");
    tau0->setOptimalForce(1);
    model.addComponent(tau0);

    auto* tau1 = new CoordinateActuator();
    tau1->setCoordinate(&j1->updCoordinate());
    tau1->setName("tau1");
    tau1->setOptimalForce(1);
    model.addComponent(tau1);

    auto* marker = new Marker("marker", *b1, Vec3(0));
    model.addMarker(marker);

    // Add display geometry.
    Ellipsoid bodyGeometry(0.5, 0.1, 0.1);
    bodyGeometry.setColor(SimTK::Gray);
    // Attach an ellipsoid to a frame located at the center of each body.
    PhysicalOffsetFrame* b0center = new PhysicalOffsetFrame(
        "b0center", *b0, SimTK::Transform(Vec3(-0.5, 0, 0)));
    b0->addComponent(b0center);
    b0center->attachGeometry(bodyGeometry.clone());
    PhysicalOffsetFrame* b1center = new PhysicalOffsetFrame(
        "b1center", *b1, SimTK::Transform(Vec3(-0.5, 0, 0)));
    b1->addComponent(b1center);
    b1center->attachGeometry(bodyGeometry.clone());

    Sphere target(0.1);
    target.setColor(SimTK::Red);
    PhysicalOffsetFrame* targetframe = new PhysicalOffsetFrame(
            "targetframe", ground, SimTK::Transform(Vec3(0, 2, 0)));
    model.updGround().addComponent(targetframe);
    targetframe->attachGeometry(target.clone());

    Sphere start(target);
    PhysicalOffsetFrame* startframe = new PhysicalOffsetFrame(
            "startframe", ground, SimTK::Transform(Vec3(2, 0, 0)));
    model.updGround().addComponent(startframe);
    start.setColor(SimTK::Green);
    startframe->attachGeometry(start.clone());

    model.finalizeConnections();

    return model;
}


int main() {

    MucoTool muco;
    muco.setName("double_pendulum_swingup");

    // Define the optimal control problem.
    // ===================================
    MucoProblem& mp = muco.updProblem();

    // Model (dynamics).
    // -----------------
    mp.setModelCopy(createDoublePendulumModel());

    // Bounds.
    // -------
    mp.setTimeBounds(0, {0, 5});
    mp.setStateInfo("/jointset/j0/q0/value", {-10, 10}, 0);
    mp.setStateInfo("/jointset/j0/q0/speed", {-50, 50}, 0, 0);
    mp.setStateInfo("/jointset/j1/q1/value", {-10, 10}, 0);
    mp.setStateInfo("/jointset/j1/q1/speed", {-50, 50}, 0, 0);
    mp.setControlInfo("/tau0", {-100, 100}); // TODO tighten.
    mp.setControlInfo("/tau1", {-100, 100});

    // Cost.
    // -----
    auto* ftCost = mp.addCost<MucoFinalTimeCost>();
    ftCost->set_weight(0.001);

    auto* endpointCost = mp.addCost<MucoMarkerEndpointCost>("endpoint");
    endpointCost->set_weight(1000.0);
    endpointCost->setPointName("/markerset/marker");
    endpointCost->setReferenceLocation(SimTK::Vec3(0, 2, 0));

    // Configure the solver.
    // =====================
    int N = 50;
    {
        MucoTropterSolver& solver = muco.initSolver();
        solver.set_dynamics_mode("explicit");
        solver.set_num_mesh_points(N);
        //solver.set_verbosity(2);
        //solver.set_optim_hessian_approximation("exact");

        MucoIterate guess = solver.createGuess();
        guess.setNumTimes(2);
        guess.setTime({0, 1});
        guess.setState("/jointset/j0/q0/value", {0, -SimTK::Pi});
        guess.setState("/jointset/j1/q1/value", {0, 2*SimTK::Pi});
        guess.setState("/jointset/j0/q0/speed", {0, 0});
        guess.setState("/jointset/j1/q1/speed", {0, 0});
        guess.setControl("/tau0", {0, 0});
        guess.setControl("/tau1", {0, 0});
        guess.resampleWithNumTimes(10);
        solver.setGuess(guess);

        muco.visualize(guess);
    }

    muco.print("double_pendulum_swingup.omuco");

    // Solve the problem.
    // ==================
    // TODO: The implicit problem doesn't solve if we first solve the explicit
    // problem.
    MucoSolution solution = muco.solve();
    muco.visualize(solution);

    // Use implicit differential equations.
    // ====================================
    // Making a copy doesn't actually help.
    MucoTool muco2 = MucoTool(muco);
    auto& solver2 = muco2.initSolver();
    // auto& muco2 = muco;
    muco2.setName("double_pendulum_swingup_implicit");
    solver2.set_dynamics_mode("implicit");
    MucoIterate guessImp = solver2.createGuess();
    guessImp.setNumTimes(2);
    guessImp.setTime({0, 1});
    guessImp.setState("/jointset/j0/q0/value", {0, -SimTK::Pi});
    guessImp.setState("/jointset/j1/q1/value", {0, 2*SimTK::Pi});
    guessImp.setState("/jointset/j0/q0/speed", {0, 0});
    guessImp.setState("/jointset/j1/q1/speed", {0, 0});
    guessImp.setControl("/tau0", {0, 0});
    guessImp.setControl("/tau1", {0, 0});
    guessImp.resampleWithNumTimes(10);
    solver2.setGuess(guessImp);

    MucoSolution solutionImplicit = muco2.solve().unseal();
    muco2.visualize(solutionImplicit);

    std::cout << solution.getTime()[N-1] << " "
            << solutionImplicit.getTime()[N-1] << std::endl;
    std::cout << "Comparison with implicit: " <<
       solutionImplicit.compareContinuousVariablesRMS(solution) << std::endl;

    // TODO compareContinuousVariables: handle derivatives.
    // TODO test reading/writing MucoIterate with derivatives.

    return EXIT_SUCCESS;
}

