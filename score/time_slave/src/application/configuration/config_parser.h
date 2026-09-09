/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_TIME_SLAVE_CONFIGURATION_CONFIG_PARSER_H
#define SCORE_TIME_SLAVE_CONFIGURATION_CONFIG_PARSER_H

#include "score/time_slave/src/application/configuration/time_slave_config.h"

#include <string>

namespace score
{
namespace ts
{

/**
 * @brief Parse a TimeSlave JSON configuration file from @p path.
 *
 * On success, the returned config is fully populated with values from the
 * file; missing optional fields fall back to their defaults as described in
 * the JSON schema.
 *
 * On failure (file not found, invalid JSON, schema-violating structure)
 * the process terminates via LogFatal.
 *
 * @param path  Filesystem path to the JSON config file.
 * @return Parsed TimeSlaveConfig.
 */
TimeSlaveConfig ParseConfig(const std::string& path);

}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_CONFIGURATION_CONFIG_PARSER_H
