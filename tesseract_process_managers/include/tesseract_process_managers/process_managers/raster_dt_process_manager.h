/**
 * @file raster_dt_process_manager.h
 * @brief Plans raster paths with dual transitions
 *
 * @author Levi Armstrong
 * @date August 28, 2020
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
#ifndef TESSERACT_PROCESS_MANAGERS_RASTER_DT_PROCESS_MANAGER_H
#define TESSERACT_PROCESS_MANAGERS_RASTER_DT_PROCESS_MANAGER_H

#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <functional>
#include <vector>
#include <thread>
#include <taskflow/taskflow.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_process_managers/process_manager.h>
#include <tesseract_process_managers/taskflow_generator.h>

namespace tesseract_planning
{
/**
 * @brief This class provides a process manager for a raster process.
 *
 * Given a ProcessInput in the correct format, it handles the creation of the process dependencies and uses Taskflow to
 * execute them efficiently in a parallel based on those dependencies.
 *
 * The required format is below. Note that a transition is planned from both the start and end of each raster to allow
 * for skipping of rasters without replanning. This logic must be handled in the execute process.
 *
 * Composite
 * {
 *   Composite - from start
 *   Composite - Raster segment
 *   Unordered Composite - Transitions
 *   {
 *     Composite - Transition from end
 *     Composite - Transition to start
 *   }
 *   Composite - Raster segment
 *   Unordered Composite - Transitions
 *   {
 *     Composite - Transition from end
 *     Composite - Transition to start
 *   }
 *   Composite - Raster segment
 *   Composite - to end
 * }
 */
class RasterDTProcessManager : public ProcessManager
{
public:
  using Ptr = std::shared_ptr<RasterDTProcessManager>;
  using ConstPtr = std::shared_ptr<const RasterDTProcessManager>;

  RasterDTProcessManager(TaskflowGenerator::UPtr freespace_taskflow_generator,
                         TaskflowGenerator::UPtr transition_taskflow_generator,
                         TaskflowGenerator::UPtr raster_taskflow_generator,
                         std::size_t n = std::thread::hardware_concurrency());
  ~RasterDTProcessManager() override = default;
  RasterDTProcessManager(const RasterDTProcessManager&) = delete;
  RasterDTProcessManager& operator=(const RasterDTProcessManager&) = delete;
  RasterDTProcessManager(RasterDTProcessManager&&) = delete;
  RasterDTProcessManager& operator=(RasterDTProcessManager&&) = delete;

  bool init(ProcessInput input) override;

  bool execute() override;

  bool terminate() override;

  bool clear() override;

  void enableDebug(bool enabled) override;

  void enableProfile(bool enabled) override;

private:
  void successCallback(std::string message);
  void failureCallback(std::string message);
  bool success_{ false };
  bool debug_{ false };
  bool profile_{ false };

  TaskflowGenerator::UPtr freespace_taskflow_generator_;
  TaskflowGenerator::UPtr transition_taskflow_generator_;
  TaskflowGenerator::UPtr raster_taskflow_generator_;
  tf::Executor executor_;
  tf::Taskflow taskflow_;
  std::vector<tf::Task> freespace_tasks_;
  std::vector<tf::Task> transition_tasks_;
  std::vector<tf::Task> raster_tasks_;

  /**
   * @brief Checks that the ProcessInput is in the correct format.
   * @param input ProcessInput to be checked
   * @return True if in the correct format
   */
  bool checkProcessInput(const ProcessInput& input) const;
};

}  // namespace tesseract_planning

#endif  // TESSERACT_PROCESS_MANAGERS_RASTER_DT_PROCESS_MANAGER_H