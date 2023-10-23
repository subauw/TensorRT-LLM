/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "tensorrt_llm/runtime/common.h"

#include <NvInferRuntime.h>
#include <optional>
#include <vector>

namespace tensorrt_llm::runtime
{
class WorldConfig
{
public:
    static SizeType constexpr kDefaultGpusPerNode = 8;

    constexpr explicit WorldConfig(SizeType tensorParallelism = 1, SizeType pipelineParallelism = 1, SizeType rank = 0,
        SizeType gpusPerNode = kDefaultGpusPerNode)
        : mTensorParallelism{tensorParallelism}
        , mPipelineParallelism{pipelineParallelism}
        , mRank{rank}
        , mGpusPerNode{gpusPerNode}
    {
    }

    [[nodiscard]] SizeType constexpr getSize() const noexcept
    {
        return mTensorParallelism * mPipelineParallelism;
    }

    [[nodiscard]] SizeType constexpr getTensorParallelism() const noexcept
    {
        return mTensorParallelism;
    }

    [[nodiscard]] bool constexpr isTensorParallel() const noexcept
    {
        return mTensorParallelism > 1;
    }

    [[nodiscard]] SizeType constexpr getPipelineParallelism() const noexcept
    {
        return mPipelineParallelism;
    }

    [[nodiscard]] bool constexpr isPipelineParallel() const noexcept
    {
        return mPipelineParallelism > 1;
    }

    [[nodiscard]] SizeType constexpr getRank() const noexcept
    {
        return mRank;
    }

    [[nodiscard]] SizeType constexpr getGpusPerNode() const noexcept
    {
        return mGpusPerNode;
    }

    [[nodiscard]] SizeType constexpr getDevice() const noexcept
    {
        return mRank % mGpusPerNode;
    }

    [[nodiscard]] SizeType constexpr getPipelineParallelRank() const noexcept
    {
        return mRank / mTensorParallelism;
    }

    [[nodiscard]] SizeType constexpr getTensorParallelRank() const noexcept
    {
        return mRank % mTensorParallelism;
    }

    [[nodiscard]] bool constexpr isFirstPipelineParallelRank() const noexcept
    {
        return getPipelineParallelRank() == 0;
    }

    [[nodiscard]] bool constexpr isLastPipelineParallelRank() const noexcept
    {
        return getPipelineParallelRank() == getPipelineParallelism() - 1;
    }

    [[nodiscard]] std::vector<SizeType> getPipelineParallelGroup() const;

    static bool validConfig(nvinfer1::ILogger& logger, SizeType tensorParallelism, SizeType pipelineParallelism);

    static WorldConfig mpi(nvinfer1::ILogger& logger, SizeType gpusPerNode = kDefaultGpusPerNode,
        std::optional<SizeType> tensorParallelism = std::nullopt,
        std::optional<SizeType> pipelineParallelism = std::nullopt);

    static WorldConfig mpi(SizeType gpusPerNode = kDefaultGpusPerNode,
        std::optional<SizeType> tensorParallelism = std::nullopt,
        std::optional<SizeType> pipelineParallelism = std::nullopt);

private:
    SizeType mTensorParallelism;
    SizeType mPipelineParallelism;
    SizeType mRank;
    SizeType mGpusPerNode;
};

} // namespace tensorrt_llm::runtime
