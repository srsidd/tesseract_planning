
#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <taskflow/taskflow.hpp>
#include <fstream>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include "raster_example_program.h"

#include <tesseract_task_composer/taskflow_utils.h>
#include <tesseract_task_composer/task_composer_graph.h>
#include <tesseract_task_composer/task_composer_data_storage.h>
#include <tesseract_task_composer/nodes/raster_motion_task.h>

#include <tesseract_common/types.h>
#include <tesseract_environment/environment.h>
#include <tesseract_command_language/utils.h>
#include <tesseract_visualization/visualization_loader.h>
//#include <tesseract_process_managers/utils/task_info_statistics.h>
#include <tesseract_support/tesseract_support_resource_locator.h>

using namespace tesseract_planning;

int main()
{
  // --------------------
  // Perform setup
  // --------------------
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
    plotter->waitForConnection(3);
    if (plotter->isConnected())
      plotter->plotEnvironment(*env);
  }

  // Define profiles
  auto profiles = std::make_shared<ProfileDictionary>();

  // Define the program
  CompositeInstruction program = rasterExampleProgram();
  program.print();

  auto task_data = std::make_shared<TaskComposerDataStorage>();
  task_data->setData("input_program", program);

  auto task_input = std::make_shared<TaskComposerInput>(env, profiles, task_data);

  TaskComposerTask::UPtr task_composer = std::make_unique<RasterMotionTask>("input_program", "output_program");

  std::ofstream tc_out_data;
  tc_out_data.open(tesseract_common::getTempPath() + "task_composer_raster_example.dot");
  task_composer->dump(tc_out_data);  // dump the graph including dynamic tasks
  tc_out_data.close();

  TaskComposerTaskflowContainer taskflow = convertToTaskflow(*task_composer, *task_input);

  std::ofstream out_data;
  out_data.open(tesseract_common::getTempPath() + "task_composer_raster_example_tf.dot");
  taskflow.top->dump(out_data);  // dump the graph including dynamic tasks
  out_data.close();

  tf::Executor executor;
  executor.run(*taskflow.top);
  executor.wait_for_all();

  // Plot Process Trajectory
  auto output_program = task_data->getData("output_program").as<CompositeInstruction>();
  if (plotter != nullptr && plotter->isConnected())
  {
    plotter->waitForInput();
    plotter->plotTrajectory(toJointTrajectory(output_program), *env->getStateSolver());
  }

  std::cout << "Execution Complete" << std::endl;

  //  // Create Process Planning Server
  //  ProcessPlanningServer planning_server(std::make_shared<ProcessEnvironmentCache>(env), 1);
  //  planning_server.loadDefaultProcessPlanners();

  //  // Create Process Planning Request
  //  ProcessPlanningRequest request;
  //  request.name = process_planner_names::RASTER_G_FT_PLANNER_NAME;

  //  // Define the program
  //  CompositeInstruction program = rasterExampleProgram();
  //  request.instructions = InstructionPoly(program);

  //  // Print Diagnostics
  //  request.instructions.print("Program: ");

  //  // Solve process plan
  //  ProcessPlanningFuture response = planning_server.run(request);
  //  planning_server.waitForAll();

  //  // Plot Process Trajectory
  //  if (plotter != nullptr && plotter->isConnected())
  //  {
  //    plotter->waitForInput();
  //    plotter->plotTrajectory(toJointTrajectory(response.problem->results->as<CompositeInstruction>()),
  //                            *env->getStateSolver());
  //  }

  //  std::cout << "Execution Complete" << std::endl;

  //  // Print summary statistics
  //  std::map<std::size_t, TaskInfo::UPtr> info_map = response.interface->getTaskInfoMap();
  //  TaskInfoProfiler profiler;
  //  profiler.load(info_map);
  //  profiler.print();

  return 0;
}
