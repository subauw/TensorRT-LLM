/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <cuda_fp16.h>

#include "tensorrt_llm/common/cudaUtils.h"
#include "tensorrt_llm/kernels/penaltyTypes.h"

namespace tensorrt_llm
{
namespace kernels
{

//! \brief Applies penalty to logits of the tokens that were generated.
//!
//! \param logits input/output buffer [batchSize, vocabSizePadded]. Logits to be modified by inplace.
//! \param repetition_penalties input buffer [batchSize]. Repetition penalties per request
//! \param presence_penalties input buffer [batchSize]. Presence penalties per request
//! \param frequency_penalties input buffer [batchSize]. Frequency penalties per request
//! \param outputIds input buffer [batchSize][maxSeqLen]. Contains pointers to rows [1, maxSeqLen]
//! with output tokens per request
//! \param sequenceLengths input buffer [batchSize]. Current sequence lengths of the request tokens.
//! \param batchSize batch size
//! \param vocabSize padded vocab size
//! \param maxSeqLen maximum sequence length
//! \param stream stream
//! \param use_repetition whether using the repetition penalty
//! \param use_presence whether using the presence penalty
//! \param use_frequency whether using the frequency penalty
template <typename T>
void invokeBatchApplyRepetitionPenalty(T* logits, const float* repetition_penalties, const float* presence_penalties,
    const float* frequency_penalties, const bool use_repetition, const bool use_presence, const bool use_frequency,
    const int** outputIds, const int* sequenceLengths, const int batchSize, const int vocabSize, int maxSeqLen,
    cudaStream_t stream);

//! \brief Applies temperature penalty logits' = (logit + bias) / temperature. Sets -MAX_FLOAT to padded logits
//!
//! \param logits input/output buffer [batchSize, vocabSizePadded]. Logits to be modified by inplace.
//! \param bias input buffer [vocabSize]. Bias to logit per token. Ignored if nullptr
//! \param temperatures softmax temperatures per request
//! \param batchSize batch size
//! \param vocabSize unpadded vocab size
//! \param vocabSizePadded padded vocab size
//! \param stream stream
template <typename T>
void invokeBatchApplyTemperaturePenalty(T* logits, const T* bias, const float* temperatures, const int batchSize,
    const int vocabSize, const int vocabSizePadded, cudaStream_t stream);

//! \brief Specialization of invokeBatchApplyTemperaturePenalty with single temperature instead of per request
template <typename T>
void invokeApplyTemperaturePenalty(T* logits, const T* bias, const float temperature, const int batchSize,
    const int vocabSize, const int vocabSizePadded, cudaStream_t stream);

//! \brief Sets logit of EOS token to -MAX_FLT if the generated length is smaller than threshold
//!
//! \param logits input/output buffer [batchSize, vocabSizePadded]. Logits to be modified by inplace.
//! \param minLengths input buffer [batchSize]. Minimum generated length per request
//! \param endIds input buffer [batchSize]. EOS token ids per request
//! \param sequenceLengths input buffer [batchSize]. Current sequence length per request
//! \param contextLengths input buffer [batchSize]. Context lengths per request
//! \param batchSize batch size
//! \param vocabSizePadded padded vocab size
//! \param stream stream
template <typename T>
void invokeMinLengthPenalty(T* logits, const int* minLengths, const int* endIds, const int* sequenceLengths,
    const int* contextLengths, const int batchSize, const int vocabSizePadded, cudaStream_t stream);

} // namespace kernels
} // namespace tensorrt_llm
