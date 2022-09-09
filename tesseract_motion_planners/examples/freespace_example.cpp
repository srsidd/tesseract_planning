/**
 * @file freespace_example.cpp
 * @brief Freespace motion planning example
 *
 * @author Levi Armstrong
 * @date August 31, 2020
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2020, Southwest Research Institute
 *
 * @par License
 * Software License Agreement (Apache License)
 * @par
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * @par
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <console_bridge/console.h>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_common/types.h>

#include <tesseract_kinematics/core/utils.h>

#include <tesseract_environment/environment.h>

#include <tesseract_motion_planners/ompl/profile/ompl_default_plan_profile.h>
#include <tesseract_motion_planners/ompl/ompl_motion_planner.h>

#include <tesseract_motion_planners/trajopt/trajopt_motion_planner.h>
#include <tesseract_motion_planners/trajopt/profile/trajopt_default_plan_profile.h>
#include <tesseract_motion_planners/trajopt/profile/trajopt_default_composite_profile.h>

#include <tesseract_motion_planners/core/types.h>
#include <tesseract_motion_planners/core/utils.h>
#include <tesseract_motion_planners/interface_utils.h>

#include <tesseract_visualization/visualization_loader.h>
#include <tesseract_command_language/state_waypoint.h>
#include <tesseract_command_language/cartesian_waypoint.h>
#include <tesseract_command_language/move_instruction.h>
#include <tesseract_command_language/utils.h>
#include <tesseract_support/tesseract_support_resource_locator.h>

using namespace tesseract_planning;
using namespace tesseract_planning::profile_ns;

int main(int /*argc*/, char** /*argv*/)
{
  try
  {
    // Setup
    auto locator = std::make_shared<tesseract_common::TesseractSupportResourceLocator>();
    auto env = std::make_shared<tesseract_environment::Environment>();
    tesseract_common::fs::path urdf_path(std::string(TESSERACT_SUPPORT_DIR) + "/urdf/abb_irb2400.urdf");
    tesseract_common::fs::path srdf_path(std::string(TESSERACT_SUPPORT_DIR) + "/urdf/abb_irb2400.srdf");
    env->init(urdf_path, srdf_path, locator);

    // Dynamically load ignition visualizer if exist
    tesseract_visualization::VisualizationLoader loader;
    auto plotter = loader.get();

    if (plotter != nullptr)
    {
      plotter->waitForConnection();
      plotter->plotEnvironment(*env);
    }

    tesseract_common::ManipulatorInfo manip;
    manip.tcp_frame = "tool0";
    manip.working_frame = "base_link";
    manip.manipulator = "manipulator";
    manip.manipulator_ik_solver = "OPWInvKin";

    auto state_solver = env->getStateSolver();
    auto kin_group = env->getKinematicGroup(manip.manipulator, manip.manipulator_ik_solver);
    auto cur_state = env->getState();

    // Specify start location
    StateWaypointPoly wp0{ StateWaypoint(kin_group->getJointNames(), Eigen::VectorXd::Zero(6)) };

    // Specify freespace start waypoint
    CartesianWaypointPoly wp1{ CartesianWaypoint(Eigen::Isometry3d::Identity() * Eigen::Translation3d(0.8, -.20, 0.8) *
                                                 Eigen::Quaterniond(0, 0, -1.0, 0)) };

    // Define Plan Instructions
    MoveInstruction start_instruction(wp0, MoveInstructionType::START);
    MoveInstruction plan_f1(wp1, MoveInstructionType::FREESPACE, "DEFAULT");

    // Create program
    CompositeInstruction program;
    program.setStartInstruction(start_instruction);
    program.setManipulatorInfo(manip);
    program.appendMoveInstruction(plan_f1);

    // Plot Program
    if (plotter)
    {
    }

    // Create Profiles
    auto ompl_plan_profile = std::make_shared<OMPLDefaultPlanProfile>();
    auto trajopt_plan_profile = std::make_shared<TrajOptDefaultPlanProfile>();
    auto trajopt_composite_profile = std::make_shared<TrajOptDefaultCompositeProfile>();

    // Create a seed
    CompositeInstruction seed = generateInterpolatedProgram(program, cur_state, env);

    // Profile Dictionary
    auto profiles = std::make_shared<ProfileDictionary>();
    profiles->addProfile<OMPLPlanProfile>(OMPL_DEFAULT_NAMESPACE, "DEFAULT", ompl_plan_profile);
    profiles->addProfile<TrajOptPlanProfile>(TRAJOPT_DEFAULT_NAMESPACE, "DEFAULT", trajopt_plan_profile);
    profiles->addProfile<TrajOptCompositeProfile>(TRAJOPT_DEFAULT_NAMESPACE, "DEFAULT", trajopt_composite_profile);

    // Create Planning Request
    PlannerRequest request;
    request.seed = seed;
    request.instructions = program;
    request.env = env;
    request.env_state = cur_state;
    request.profiles = profiles;

    // Solve OMPL Plan
    OMPLMotionPlanner ompl_planner;
    PlannerResponse ompl_response = ompl_planner.solve(request);
    assert(ompl_response);

    // Plot OMPL Trajectory
    if (plotter)
    {
      plotter->waitForInput();
      plotter->plotTrajectory(toJointTrajectory(ompl_response.results), *state_solver);
    }

    // Update Seed
    request.seed = ompl_response.results;

    // Solve TrajOpt Plan
    TrajOptMotionPlanner trajopt_planner;
    PlannerResponse trajopt_response = trajopt_planner.solve(request);
    assert(trajopt_response);

    if (plotter)
    {
      plotter->waitForInput();
      plotter->plotTrajectory(toJointTrajectory(trajopt_response.results), *state_solver);
    }
  }
  catch (const std::exception& e)
  {
    CONSOLE_BRIDGE_logError("Example failed with message: %s", e.what());
    return -1;
  }
}
