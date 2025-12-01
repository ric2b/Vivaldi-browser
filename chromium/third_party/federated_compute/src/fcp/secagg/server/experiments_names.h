/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_SERVER_EXPERIMENTS_NAMES_H_
#define FCP_SECAGG_SERVER_EXPERIMENTS_NAMES_H_

namespace fcp {
namespace secagg {

// Names of predefined experiments
static constexpr char kFullgraphSecAggExperiment[] = "FULLGRAPH_SECAGG";
static constexpr char kForceSubgraphSecAggExperiment[] =
    "FORCE_SUBGRAPH_SECAGG_FOR_TEST";
static constexpr char kSubgraphSecAggCuriousServerExperiment[] =
    "SUBGRAPH_SECAGG_CURIOUS_SERVER";
static constexpr char kSecAggAsyncRound2Experiment[] = "secagg_async_round_2";

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_EXPERIMENTS_NAMES_H_
