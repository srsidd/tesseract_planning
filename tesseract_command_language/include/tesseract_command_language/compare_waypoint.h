/**
 * @file compare_waypoint.h
 * @brief This contains the comparison operators for Waypoint by recovering the type and comparing
 *
 * @author Levi Armstrong
 * @date February 24, 2021
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2021, Southwest Research Institute
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
#ifndef TESSERACT_COMMAND_LANGUAGE_COMPARE_WAYPOINT_H
#define TESSERACT_COMMAND_LANGUAGE_COMPARE_WAYPOINT_H

#include <tesseract_command_language/core/waypoint.h>

namespace tesseract_planning
{
/**
 * @brief Equal operator for Waypoint Types
 * @param lhs Waypoint
 * @param rhs Waypoint
 * @return True if equal, otherwise false
 */
bool operator==(const Waypoint& lhs, const Waypoint& rhs);

/**
 * @brief Not equal operator for Waypoint Types
 * @param lhs Waypoint
 * @param rhs Waypoint
 * @return True if not equal, otherwise false
 */
bool operator!=(const Waypoint& lhs, const Waypoint& rhs);
}  // namespace tesseract_planning

#endif  // TESSERACT_COMMAND_LANGUAGE_COMPARE_H
