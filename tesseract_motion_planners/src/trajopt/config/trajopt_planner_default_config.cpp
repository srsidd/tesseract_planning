#include "tesseract_motion_planners/trajopt/config/trajopt_planner_default_config.h"
#include "tesseract_motion_planners/trajopt/config/utils.h"

namespace tesseract_motion_planners
{
TrajOptPlannerDefaultConfig::TrajOptPlannerDefaultConfig(const tesseract::Tesseract::ConstPtr& tesseract_,
                                           const std::string& manipulator_,
                                           const std::string& link_,
                                           const tesseract_common::VectorIsometry3d& tcp_)
  : TrajOptPlannerConfigBase()
  , tesseract(tesseract_)
  , manipulator(manipulator_)
  , link(link_)
  , tcp(tcp_)
{
}

TrajOptPlannerDefaultConfig::TrajOptPlannerDefaultConfig(const tesseract::Tesseract::ConstPtr& tesseract_,
                                           const std::string& manipulator_,
                                           const std::string& link_,
                                           const Eigen::Isometry3d& tcp_)
  : TrajOptPlannerDefaultConfig(tesseract_, manipulator_, link_, tesseract_common::VectorIsometry3d(1, tcp_))
{
}

std::shared_ptr<trajopt::ProblemConstructionInfo> TrajOptPlannerDefaultConfig::generatePCI() const
{
  using namespace trajopt;

  // Check that parameters are valid
  if (tesseract == nullptr)
  {
    CONSOLE_BRIDGE_logError("In trajopt_array_planner: tesseract_ is a required parameter and has not been set");
    return nullptr;
  }

  if (target_waypoints.size() < 2)
  {
    CONSOLE_BRIDGE_logError("TrajOpt Planner Config requires at least 2 waypoints");
    return nullptr;
  }

  if (tcp.size() != target_waypoints.size() && tcp.size() != 1)
  {
    std::stringstream ss;
    ss << "Number of TCP transforms (" << tcp.size() << ") does not match the number of waypoints ("
       << target_waypoints.size() << ") and is also not 1";
    CONSOLE_BRIDGE_logError(ss.str().c_str());
    return nullptr;
  }

  // -------- Construct the problem ------------
  // -------------------------------------------
  ProblemConstructionInfo pci(tesseract);
  pci.kin = pci.getManipulator(manipulator);

  if (pci.kin == nullptr)
  {
    CONSOLE_BRIDGE_logError("In trajopt_array_planner: manipulator_ does not exist in kin_map_");
    return nullptr;
  }

  // Populate Basic Info
  pci.basic_info.n_steps = static_cast<int>(target_waypoints.size());
  pci.basic_info.manip = manipulator;
  pci.basic_info.start_fixed = false;
  pci.basic_info.use_time = false;

  // Populate Init Info
  pci.init_info.type = init_type;
  if (init_type == trajopt::InitInfo::GIVEN_TRAJ)
    pci.init_info.data = seed_trajectory;

  // Add constraints
  for (std::size_t ind = 0; ind < target_waypoints.size(); ind++)
  {
    tesseract_environment::Environment::ConstPtr env = tesseract->getEnvironmentConst();
    tesseract_kinematics::ForwardKinematics::ConstPtr kin =
        tesseract->getFwdKinematicsManagerConst()->getFwdKinematicSolver(manipulator);
    tesseract_environment::AdjacencyMap map(
        env->getSceneGraph(), kin->getActiveLinkNames(), env->getCurrentState()->transforms);
    std::vector<std::string> adjacency_links = map.getActiveLinkNames();

    WaypointTermInfo term_info;
    if (tcp.size() == target_waypoints.size())
    {
      term_info = createWaypointTermInfo(target_waypoints[ind], ind, pci.kin->getJointNames(), adjacency_links, link, tcp[ind]);
    }
    else
    {
      term_info = createWaypointTermInfo(target_waypoints[ind], ind, pci.kin->getJointNames(), adjacency_links, link, tcp.front());
    }

    pci.cnt_infos.insert(pci.cnt_infos.end(), term_info.cnt.begin(), term_info.cnt.end());
    pci.cost_infos.insert(pci.cost_infos.end(), term_info.cost.begin(), term_info.cost.end());
  }

  // Set costs for the rest of the points
  if (collision_check)
  {
    pci.cost_infos.push_back(
        createCollisionTermInfo(pci.basic_info.n_steps, collision_safety_margin, collision_continuous));
  }
  if (smooth_velocities)
  {
    pci.cost_infos.push_back(createSmoothVelocityTermInfo(pci.basic_info.n_steps, pci.kin->numJoints()));
  }
  if (smooth_accelerations)
  {
    pci.cost_infos.push_back(createSmoothAccelerationTermInfo(pci.basic_info.n_steps, pci.kin->numJoints()));
  }
  if (smooth_jerks)
  {
    pci.cost_infos.push_back(createSmoothJerkTermInfo(pci.basic_info.n_steps, pci.kin->numJoints()));
  }
  if (configuration != nullptr)
  {
    pci.cost_infos.push_back(createConfigurationTermInfo(configuration, pci.kin->getJointNames(), pci.basic_info.n_steps));
  }

  return std::make_shared<trajopt::ProblemConstructionInfo>(pci);
}

bool TrajOptPlannerDefaultConfig::generate()
{
  std::shared_ptr<trajopt::ProblemConstructionInfo> pci = generatePCI();
  if (!pci)
  {
    CONSOLE_BRIDGE_logError("Failed to construct problem from problem construction information");
    return false;
  }
  prob = trajopt::ConstructProblem(*pci);
  return true;
}

} // namespace tesseract_motion_planners