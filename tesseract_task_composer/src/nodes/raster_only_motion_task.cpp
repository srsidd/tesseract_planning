/**
 * @file raster_only_motion_task.cpp
 * @brief Plans raster paths
 *
 * @author Matthew Powelson
 * @date July 15, 2020
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
#include <boost/serialization/string.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP
#include <tesseract_common/timer.h>

#include <tesseract_task_composer/task_composer_future.h>
#include <tesseract_task_composer/task_composer_executor.h>
#include <tesseract_task_composer/nodes/raster_only_motion_task.h>
#include <tesseract_task_composer/nodes/start_task.h>
#include <tesseract_task_composer/nodes/cartesian_motion_pipeline_task.h>
#include <tesseract_task_composer/nodes/freespace_motion_pipeline_task.h>
#include <tesseract_task_composer/nodes/transition_mux_task.h>
#include <tesseract_task_composer/nodes/update_end_state_task.h>
#include <tesseract_task_composer/nodes/update_start_state_task.h>
#include <tesseract_command_language/composite_instruction.h>

namespace tesseract_planning
{
RasterOnlyMotionTask::RasterOnlyMotionTask(std::string input_key,
                                           std::string output_key,
                                           bool cartesian_transition,
                                           bool run_simple_planner,
                                           bool is_conditional,
                                           std::string name)
  : TaskComposerTask(is_conditional, std::move(name))
  , run_simple_planner_(run_simple_planner)
  , cartesian_transition_(cartesian_transition)
{
  input_keys_.push_back(std::move(input_key));
  output_keys_.push_back(std::move(output_key));
}

int RasterOnlyMotionTask::run(TaskComposerInput& input, OptionalTaskComposerExecutor executor) const
{
  if (input.isAborted())
    return 0;

  auto info = std::make_unique<RasterOnlyMotionTaskInfo>(uuid_, name_);
  info->return_value = 0;
  tesseract_common::Timer timer;
  timer.start();
  //  saveInputs(*info, input);

  auto input_data_poly = input.data_storage->getData(input_keys_[0]);

  // --------------------
  // Check that inputs are valid
  // --------------------
  try
  {
    checkTaskInput(input_data_poly);
  }
  catch (const std::exception& e)
  {
    info->message = e.what();
    CONSOLE_BRIDGE_logError("%s", info->message.c_str());
    //    saveOutputs(*info, input);
    info->elapsed_time = timer.elapsedSeconds();
    input.addTaskInfo(std::move(info));
    return 0;
  }

  auto& program = input_data_poly.as<CompositeInstruction>();
  TaskComposerGraph task_graph;

  tesseract_common::ManipulatorInfo program_manip_info = program.getManipulatorInfo().getCombined(input.manip_info);

  auto start_task = std::make_unique<StartTask>();
  auto start_uuid = task_graph.addNode(std::move(start_task));

  std::vector<std::pair<boost::uuids::uuid, std::string>> raster_tasks;
  raster_tasks.reserve(program.size());

  // Generate all of the raster tasks. They don't depend on anything
  std::size_t raster_idx = 0;
  for (std::size_t idx = 0; idx < program.size(); idx += 2)
  {
    // Get Raster program
    auto raster_input = program[idx].as<CompositeInstruction>();

    // Set the manipulator info
    raster_input.setManipulatorInfo(raster_input.getManipulatorInfo().getCombined(program_manip_info));

    // Get Start Plan Instruction
    MoveInstructionPoly start_instruction;
    if (idx == 0)
    {
      start_instruction = program.getStartInstruction();
    }
    else
    {
      const InstructionPoly& pre_input_instruction = program[idx - 1];
      assert(pre_input_instruction.isCompositeInstruction());
      const auto& tci = pre_input_instruction.as<CompositeInstruction>();
      const auto* li = tci.getLastMoveInstruction();
      assert(li != nullptr);
      start_instruction = *li;
    }

    // Update start state
    start_instruction.setMoveType(MoveInstructionType::START);
    raster_input.setStartInstruction(start_instruction);

    auto raster_pipeline_task = std::make_unique<CartesianMotionPipelineTask>(
        true,
        run_simple_planner_,
        true,
        false,
        "Raster #" + std::to_string(raster_idx + 1) + ": " + raster_input.getDescription());
    std::string raster_pipeline_key = raster_pipeline_task->getUUIDString();
    auto raster_pipeline_uuid = task_graph.addNode(std::move(raster_pipeline_task));
    raster_tasks.emplace_back(raster_pipeline_uuid, raster_pipeline_key);
    input.data_storage->setData(raster_pipeline_key, raster_input);

    task_graph.addEdges(start_uuid, { raster_pipeline_uuid });

    raster_idx++;
  }

  // Loop over all transitions
  std::vector<std::string> transition_keys;
  transition_keys.reserve(program.size());
  std::size_t transition_idx = 0;
  for (std::size_t idx = 1; idx < program.size() - 1; idx += 2)
  {
    // Get transition program
    auto transition_input = program[idx].as<CompositeInstruction>();

    // Set the manipulator info
    transition_input.setManipulatorInfo(transition_input.getManipulatorInfo().getCombined(program_manip_info));

    std::string transition_pipeline_key;
    boost::uuids::uuid transition_pipeline_uuid{};
    if (cartesian_transition_)
    {
      auto transition_pipeline_task = std::make_unique<CartesianMotionPipelineTask>(
          true,
          run_simple_planner_,
          true,
          false,
          "Transition #" + std::to_string(transition_idx + 1) + ": " + transition_input.getDescription());
      transition_pipeline_key = transition_pipeline_task->getUUIDString();
      transition_pipeline_uuid = task_graph.addNode(std::move(transition_pipeline_task));
      transition_keys.push_back(transition_pipeline_key);
    }
    else
    {
      auto transition_pipeline_task = std::make_unique<FreespaceMotionPipelineTask>(
          true,
          run_simple_planner_,
          true,
          false,
          "Transition #" + std::to_string(transition_idx + 1) + ": " + transition_input.getDescription());
      transition_pipeline_key = transition_pipeline_task->getUUIDString();
      transition_pipeline_uuid = task_graph.addNode(std::move(transition_pipeline_task));
      transition_keys.push_back(transition_pipeline_key);
    }

    const auto& prev = raster_tasks[transition_idx];
    const auto& next = raster_tasks[transition_idx + 1];
    auto transition_mux_task =
        std::make_unique<TransitionMuxTask>(prev.second, next.second, transition_pipeline_key, false);
    std::string transition_mux_key = transition_mux_task->getUUIDString();
    auto transition_mux_uuid = task_graph.addNode(std::move(transition_mux_task));

    input.data_storage->setData(transition_mux_key, transition_input);

    task_graph.addEdges(transition_mux_uuid, { transition_pipeline_uuid });
    task_graph.addEdges(prev.first, { transition_mux_uuid });
    task_graph.addEdges(next.first, { transition_mux_uuid });

    transition_idx++;
  }

  // Debug remove
  std::ofstream tc_out_data;
  tc_out_data.open(tesseract_common::getTempPath() + "task_composer_raster_subgraph_example.dot");
  task_graph.dump(tc_out_data);  // dump the graph including dynamic tasks
  tc_out_data.close();

  TaskComposerFuture::UPtr future = executor.value().get().run(task_graph, input);
  future->wait();

  if (input.isAborted())
  {
    info->message = "Raster subgraph failed";
    CONSOLE_BRIDGE_logError("%s", info->message.c_str());
    //    saveOutputs(*info, input);
    info->elapsed_time = timer.elapsedSeconds();
    input.addTaskInfo(std::move(info));
    return 0;
  }

  program.clear();
  for (std::size_t i = 0; i < raster_tasks.size(); ++i)
  {
    program.appendInstruction(input.data_storage->getData(raster_tasks[i].second).as<CompositeInstruction>());
    if (i < raster_tasks.size() - 1)
      program.appendInstruction(input.data_storage->getData(transition_keys[i]).as<CompositeInstruction>());
  }

  input.data_storage->setData(output_keys_[0], program);

  //  saveOutputs(*info, input);
  info->elapsed_time = timer.elapsedSeconds();
  input.addTaskInfo(std::move(info));
  return 1;
}

void RasterOnlyMotionTask::checkTaskInput(const tesseract_common::Any& input)
{
  // -------------
  // Check Input
  // -------------
  if (input.isNull())
    throw std::runtime_error("RasterOnlyMotionTask, input is null");

  if (input.getType() != std::type_index(typeid(CompositeInstruction)))
    throw std::runtime_error("RasterOnlyMotionTask, input is not a composite instruction");

  const auto& composite = input.as<CompositeInstruction>();

  // Check that it has a start instruction
  if (!composite.hasStartInstruction())
    throw std::runtime_error("RasterOnlyMotionTask, input should have a start instruction");

  // Check rasters and transitions
  for (std::size_t index = 0; index < composite.size(); index++)
  {
    // Both rasters and transitions should be a composite
    if (!composite.at(index).isCompositeInstruction())
      throw std::runtime_error("RasterOnlyMotionTask, Both rasters and transitions should be a composite");
  }
}

TaskComposerNode::UPtr RasterOnlyMotionTask::clone() const
{
  return std::make_unique<RasterOnlyMotionTask>(
      input_keys_[0], output_keys_[0], run_simple_planner_, cartesian_transition_, is_conditional_, name_);
}

bool RasterOnlyMotionTask::operator==(const RasterOnlyMotionTask& rhs) const
{
  bool equal = true;
  equal &= (run_simple_planner_ == rhs.run_simple_planner_);
  equal &= (cartesian_transition_ == rhs.cartesian_transition_);
  equal &= TaskComposerTask::operator==(rhs);
  return equal;
}
bool RasterOnlyMotionTask::operator!=(const RasterOnlyMotionTask& rhs) const { return !operator==(rhs); }

template <class Archive>
void RasterOnlyMotionTask::serialize(Archive& ar, const unsigned int /*version*/)
{
  ar& BOOST_SERIALIZATION_NVP(run_simple_planner_);
  ar& BOOST_SERIALIZATION_NVP(cartesian_transition_);
  ar& BOOST_SERIALIZATION_BASE_OBJECT_NVP(TaskComposerTask);
}

RasterOnlyMotionTaskInfo::RasterOnlyMotionTaskInfo(boost::uuids::uuid uuid, std::string name)
  : TaskComposerNodeInfo(uuid, std::move(name))
{
}

TaskComposerNodeInfo::UPtr RasterOnlyMotionTaskInfo::clone() const
{
  return std::make_unique<RasterOnlyMotionTaskInfo>(*this);
}

bool RasterOnlyMotionTaskInfo::operator==(const RasterOnlyMotionTaskInfo& rhs) const
{
  bool equal = true;
  equal &= TaskComposerNodeInfo::operator==(rhs);
  return equal;
}
bool RasterOnlyMotionTaskInfo::operator!=(const RasterOnlyMotionTaskInfo& rhs) const { return !operator==(rhs); }

template <class Archive>
void RasterOnlyMotionTaskInfo::serialize(Archive& ar, const unsigned int /*version*/)
{
  ar& BOOST_SERIALIZATION_BASE_OBJECT_NVP(TaskComposerNodeInfo);
}
}  // namespace tesseract_planning

#include <tesseract_common/serialization.h>
TESSERACT_SERIALIZE_ARCHIVES_INSTANTIATE(tesseract_planning::RasterOnlyMotionTask)
BOOST_CLASS_EXPORT_IMPLEMENT(tesseract_planning::RasterOnlyMotionTask)
TESSERACT_SERIALIZE_ARCHIVES_INSTANTIATE(tesseract_planning::RasterOnlyMotionTaskInfo)
BOOST_CLASS_EXPORT_IMPLEMENT(tesseract_planning::RasterOnlyMotionTaskInfo)
