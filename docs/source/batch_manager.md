# The Batch Manager in TensorRT-LLM

TensorRT-LLM relies on a component, called the Batch Manager, to support
in-flight batching of requests (also known in the community as continuous
batching or iteration-level batching). That technique that aims at reducing
wait times in queues, eliminating the need for padding requests and allowing
for higher GPU utilization.

In more details, this feature allows for the inclusion of newly arrived
requests and the return of newly completed requests at each iteration of the
token generation loop. In-flight batching is accessed via a TensorRT-LLM component
called the *Batch Manager*. That batch manager exposes hooks for the user to
register function pointers to define how TensorRT-LLM reads in new requests and
how it returns completed requests to the user.

## The Batch Manager API

A software component (called the client in the text that follows) can interact
with the batch manager using two mandatory, and several optional callbacks. Their signatures are defined
in the [`callbacks.h`](source:cpp/include/tensorrt_llm/batch_manager/callbacks.h) file.

These callbacks are invoked in the generation loop at regular intervals and serve a variety of functions descibed below.

### Get and Send Callbacks

The entry point to pass new requests to the batch manager is a callback of type
`GetInferenceRequestsCallback`. An implementation of that callback must return
a list of requests (`std::list<std::shared_ptr<InferenceRequest>`) to be
processed by the batch manager. It takes a parameter indicating the maximum
number of requests that can be accepted (a negative value indicates that an
unbounded number of requests can be accepted). The complete signature of that
callback is:

```cpp
using GetInferenceRequestsCallback = std::function<std::list<std::shared_ptr<InferenceRequest>>(int32_t)>;
```

For each new request, the client must provide the batch manager with its input
tensors and a 64-bit unsigned number (`uint64_t`) that will uniquely identify
the request. That identifier is called the *request ID* in the text that
follows (and in the code of the batch manager). The input tensors are collected
in a map (`std::map<std::string, Tensor>`) that associates input names to
tensor. See
[`InferenceRequest.h`](source:cpp/include/tensorrt_llm/batch_manager/InferenceRequest.h)
for more details.

Responses are delivered to the client through a callback of type
`SendResponseCallback`. A conforming callback must accept the 64-bit
request ID that uniquely identifies the request, the list of output tensors,
a boolean (identifying the last response for the request when set to
`true`) and a potentially non-empty error message.
A non-empty error message indicates that an error has been encountered.
In that case, the boolean indicating that this is the last response will be set to true,
and the callback must properly handle the error.
Its signature is:

```cpp
using SendResponseCallback = std::function<void(uint64_t, std::list<std::shared_ptr<Tensor>> const&, bool, const std::string&)>;
```

Note that the batch manager will reject any request sent using the
`GetInferenceRequestsCallback` callback if the request ID passed by the
client corresponds to the request ID of a request that is being processed
by the batch manager.  A request ID can be reused after it appears in a
call to the `SendResponseCallback` callback marked as final (third argument set
to `true`).

### Request Interruption

The batch manager allows users to stop the execution of requests currently in-flight.
The set of request IDs to be stopped can be passed to the batch manager
through the callback:

```cpp
using PollStopSignalCallback = std::function<std::unordered_set<uint64_t>()>;
```

When an active request appears in the set of requests to be interrupted, the
batch manager will ensure that it is properly stopped.

### Statistics

The batch manager can report execution statistics when provided with the following
callback:

```cpp
using ReturnBatchManagerStatsCallback = std::function<void(const std::string&)>;
```

The statistics are packaged as a JSON string. That string contains three fields:
  * `Timestamp`, the timestamp of the request (obtained using
    `std::put_time(&tm, "%m-%d-%Y %H:%M:%S")`),
  * `Iteration Counter`, a counter value that corresponds to the execution of a
    given request,
  * `Active Request Count`, the number of active requests.

### GptManager Design

Batch Manager is designed to integrate into an inference server that's executing a pool of
active work items populated by a stream of requests actively received
by the server. GptManager assumes a GPT-style autoregressive model architecture.
GptManager spawns a worker thread in its constructor that then
persistently runs the token generation loop. The worker thread invokes `GetInferenceRequestsCallback`
at the start of each loop iteration, which is intended to read new
requests. It invokes `SendResponseCallback` at the end of each iteration when one or
more requests have generated a response to send back to the user. This response
can be a single token in the case of requests that have streaming mode enabled or
the full response when streaming mode is disabled.
`PollStopSignalCallback` and `ReturnBatchManagerStatsCallback`, if provided, are both invoked at the end of each
iteration loop. `ReturnBatchManagerStatsCallback` is not called when the system has no active requests.
The server can safely retire requests from its pool of work
items when notified of completion (via the final_response boolean argument) by the batch manager in
`SendResponseCallback`.  All TensorRT-LLM internal state related to that
request will have been freed before this point.
An instance of the batch manager to serve an
auto-regressive model like GPT can be created as follows:

```cpp
#include <tensorrt_llm/batch_manager/GptManager.h>

using namespace tensorrt_llm::batch_manager;

GptManager batchManager(pathToTrtEngine,                   // Path to the TensorRT engine of the model,
                        TrtGptModelType::InflightBatching, // Use in-flight batching,
                        maxBeamWidth,                      // Maximum beam width (must be >= 1),
                        schedulerPolicy,                   // Scheduling policy (see below),
                        maxNumRequests,                    // Maximum number of requests,
                        getInferenceRequestsCb,            // The Get callback (see above),
                        sendResponseCb);                   // The Send callback (see above).
```

The scheduler policy helps the batch manager adjust how requests are scheduled
for execution. The batch manager can try to maximize the utilization of the
GPUs by aggressively scheduling requests (`schedulerPolicy` set to
`MAX_UTILIZATION`) at the risk of having to pause requests if it runs short on
memory for KV caches. Note that any paused request will be automatically resumed
and the only user-visible effect may be increased latency.
It can also adopt a more conservative approach and schedule requests only when it
knows that the memory allocation will be sufficient to process all active requests
even in the worst case of KV cache consumption. That mode corresponds to a
`schedulerPolicy` set to `GUARANTEED_NO_EVICT`.

The `GptManager`'s worker thread terminates when the `GptManager` destructor is
called and there are no more active requests.

### Multi-GPU execution

When running on multiple GPUs using either tensor or pipeline parallelism, it
is assumed that the server launches as many processes as GPU ranks, and each
process runs its own instance of `GptManager`. The number of GPUs visible on a given
node can be controlled using the `CUDA_VISIBLE_DEVICES` environment variable.

Care must be taken to ensure all ranks see the same inputs at each iteration of
the generation loop. In TensorRT-LLM Triton backend, an MPI broadcast is
performed in `GetInferenceRequestsCallback` to ensure the same set of requests
is seen by each of the MPI ranks.  `ReturnBatchManagerStatsCallback` need only
be called from a single rank; all ranks hold identical copies of the final
results.

## In-flight Batching with the Triton Inference Server

A Triton Inference Server C++ backend is provided with TensorRT-LLM that
includes the mechanisms needed to serve models using in-flight batching. That
backend is also a good starting example how to implement in-flight batching using
the TensorRT-LLM batch manager.
