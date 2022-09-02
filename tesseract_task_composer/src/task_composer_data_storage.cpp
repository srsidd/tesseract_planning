/**
 * @file task_composer_data_storage.h
 * @brief Data storage used when executing the pipeline
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

#include <tesseract_task_composer/task_composer_data_storage.h>
namespace tesseract_planning
{
TaskComposerDataStorage::TaskComposerDataStorage(const TaskComposerDataStorage& other) { *this = other; }
TaskComposerDataStorage& TaskComposerDataStorage::operator=(const TaskComposerDataStorage& other)
{
  std::shared_lock lock(other.mutex_);
  data_ = other.data_;
  return *this;
}

bool TaskComposerDataStorage::hasKey(const std::string& key)
{
  std::shared_lock lock(mutex_);
  return (data_.find(key) != data_.end());
}

void TaskComposerDataStorage::setData(const std::string& key, tesseract_common::Any data)
{
  std::unique_lock lock(mutex_);
  data_[key] = std::move(data);
}

tesseract_common::Any TaskComposerDataStorage::getData(const std::string& key) const
{
  std::shared_lock lock(mutex_);
  auto it = data_.find(key);
  if (it == data_.end())
    return {};

  return it->second;
}

void TaskComposerDataStorage::removeData(const std::string& key)
{
  std::unique_lock lock(mutex_);
  data_.erase(key);
}

}  // namespace tesseract_planning
