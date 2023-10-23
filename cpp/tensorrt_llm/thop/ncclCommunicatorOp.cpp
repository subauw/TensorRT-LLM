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

#include "tensorrt_llm/thop/ncclCommunicatorOp.h"
#include "tensorrt_llm/runtime/tllmLogger.h"

namespace torch_ext
{

NcclCommunicatorOp::NcclCommunicatorOp(int64_t tpSize, int64_t ppSize, int64_t rank)
    : mLogger(std::make_shared<tensorrt_llm::runtime::TllmLogger>())
    , mRank(static_cast<int32_t>(rank))
{
    tensorrt_llm::runtime::WorldConfig worldConfig{
        static_cast<int32_t>(tpSize), static_cast<int32_t>(ppSize), static_cast<int32_t>(rank)};
    mPipelineComm = tensorrt_llm::runtime::NcclCommunicator::createPipelineComm(worldConfig, *mLogger);
}

void NcclCommunicatorOp::send(th::Tensor tensor, int64_t toRank) const
{
    auto ptr = reinterpret_cast<std::uint8_t*>(get_ptr<int8_t>(tensor));
    size_t const size = tensor.numel() * th::elementSize(th::typeMetaToScalarType(tensor.dtype()));
    tensorrt_llm::runtime::CudaStream cudaStream{at::cuda::getCurrentCUDAStream().stream(), mRank, false};
    mPipelineComm->send(ptr, size, static_cast<int32_t>(toRank), cudaStream, *mLogger);
}

void NcclCommunicatorOp::recv(th::Tensor& tensor, int64_t fromRank) const
{
    auto ptr = reinterpret_cast<std::uint8_t*>(get_ptr<int8_t>(tensor));
    size_t const size = tensor.numel() * th::elementSize(th::typeMetaToScalarType(tensor.dtype()));
    tensorrt_llm::runtime::CudaStream cudaStream{at::cuda::getCurrentCUDAStream().stream(), mRank, false};
    mPipelineComm->receive(ptr, size, static_cast<int32_t>(fromRank), cudaStream, *mLogger);
}

} // namespace torch_ext

static auto fasterTransformerNcclCommunicator
    = torch::jit::class_<torch_ext::NcclCommunicatorOp>("FasterTransformer", "NcclCommunicatorOp")
          .def(torch::jit::init<int64_t, int64_t, int64_t>())
          .def("send", &torch_ext::NcclCommunicatorOp::send)
          .def("recv", &torch_ext::NcclCommunicatorOp::recv);
