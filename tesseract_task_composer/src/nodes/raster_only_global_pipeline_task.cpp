/**
 * @file raster_only_global_pipeline_task.h
 * @brief Raster only global motion planning task
 *
 * @author Levi Armstrong
 * @date July 29. 2022
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2022, Levi Armstrong
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
#include <tesseract_task_composer/nodes/raster_only_global_pipeline_task.h>
#include <tesseract_task_composer/nodes/descartes_motion_pipeline_task.h>
#include <tesseract_task_composer/nodes/raster_only_motion_task.h>
#include <tesseract_command_language/composite_instruction.h>

namespace tesseract_planning
{
RasterOnlyGlobalPipelineTask::RasterOnlyGlobalPipelineTask(std::string input_key,
                                                           std::string output_key,
                                                           bool cartesian_transition,
                                                           std::string name)
  : TaskComposerGraph(std::move(name)), cartesian_transition_(cartesian_transition)
{
  input_keys_.push_back(std::move(input_key));
  output_keys_.push_back(std::move(output_key));

  auto global_task = std::make_unique<DescartesMotionPipelineTask>(
      input_keys_[0], output_keys_[0], true, true, false, false, false, "GlobalDescartesMotionPipelineTask");

  // Save dot graph
  std::ofstream tc_out_data;
  tc_out_data.open(tesseract_common::getTempPath() + "task_composer_global_descartes_pipeline.dot");
  global_task->dump(tc_out_data);  // dump the graph including dynamic tasks
  tc_out_data.close();

  auto global_uuid = addNode(std::move(global_task));

  auto raster_task =
      std::make_unique<RasterOnlyMotionTask>(output_keys_[0], output_keys_[0], cartesian_transition, false);
  auto raster_uuid = addNode(std::move(raster_task));

  addEdges(global_uuid, { raster_uuid });
}

TaskComposerNode::UPtr RasterOnlyGlobalPipelineTask::clone() const
{
  return std::make_unique<RasterOnlyGlobalPipelineTask>(input_keys_[0], output_keys_[0], cartesian_transition_, name_);
}

bool RasterOnlyGlobalPipelineTask::operator==(const RasterOnlyGlobalPipelineTask& rhs) const
{
  bool equal = true;
  equal &= (cartesian_transition_ == rhs.cartesian_transition_);
  equal &= TaskComposerGraph::operator==(rhs);
  return equal;
}
bool RasterOnlyGlobalPipelineTask::operator!=(const RasterOnlyGlobalPipelineTask& rhs) const
{
  return !operator==(rhs);
}

template <class Archive>
void RasterOnlyGlobalPipelineTask::serialize(Archive& ar, const unsigned int /*version*/)
{
  ar& BOOST_SERIALIZATION_NVP(cartesian_transition_);
  ar& BOOST_SERIALIZATION_BASE_OBJECT_NVP(TaskComposerGraph);
}

}  // namespace tesseract_planning

#include <tesseract_common/serialization.h>
TESSERACT_SERIALIZE_ARCHIVES_INSTANTIATE(tesseract_planning::RasterOnlyGlobalPipelineTask)
BOOST_CLASS_EXPORT_IMPLEMENT(tesseract_planning::RasterOnlyGlobalPipelineTask)
