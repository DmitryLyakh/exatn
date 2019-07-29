/** ExaTN:: Tensor Runtime: Execution layer for tensor operations
REVISION: 2019/07/29

Copyright (C) 2018-2019 Tiffany Mintz, Dmitry Lyakh, Alex McCaskey
Copyright (C) 2018-2019 Oak Ridge National Laboratory (UT-Battelle)

Rationale:
 (a) The execution space consists of one or more DAGs in which
     nodes represent tensor operations and directed edges represent
     dependencies between the corresponding nodes (tensor operations).
     Each DAG is associated with a uniquely named TAProL scope.
 (b) The DAG lifecycle:
     openScope(name): Opens a new TAProL scope and creates its associated empty DAG.
                      The .submit method can then be used to append new tensor
                      operations into the current DAG. The actual execution
                      of the submitted tensor operations may start at any time
                      after submission.
     pauseScope(): Completes the actual execution of all started tensor operations in the
                   current DAG and defers the execution of the rest of the DAG for later.
     resumeScope(name): Resumes the execution of a previously paused DAG, making it current.
     closeScope(): Completes all tensor operations in the current DAG and destroys it.
 (c) submit(TensorOperation): Submits a tensor operation for (generally deferred) execution.
     sync(TensorOperation): Tests for completion of a specific tensor operation.
     sync(tensor): Tests for completion of all submitted update operations on a given tensor.
 (d) Upon creation, the TensorRuntime object spawns an execution thread which will be executing
     operations in the course of DAG traversal. The execution thread will be joined upon TensorRuntime
     destruction. The main thread will return control to the client which will then be able to submit
     new operations into the current DAG. The submitted operations will be continuously executed by
     the execution thread. The DAG execution policy is specified by the polymorphic TensorGraphExecutor
     object. Correspondingly, the TensorGraphExecutor contains the polymorphic TensorNodeExecutor
     object which will actually execute stored operations via its associated computational backend.
**/

#ifndef EXATN_RUNTIME_TENSOR_RUNTIME_HPP_
#define EXATN_RUNTIME_TENSOR_RUNTIME_HPP_

#include "tensor_graph_executor.hpp"
#include "tensor_graph.hpp"
#include "tensor_operation.hpp"
#include "tensor_method.hpp"

#include <map>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

namespace exatn {
namespace runtime {

class TensorRuntime {

public:
  TensorRuntime(const std::string & graph_executor_name = "eager-dag-executor",
                const std::string & node_executor_name = "talsh-node-executor");
  TensorRuntime(const TensorRuntime &) = delete;
  TensorRuntime & operator=(const TensorRuntime &) = delete;
  TensorRuntime(TensorRuntime &&) noexcept = default;
  TensorRuntime & operator=(TensorRuntime &&) noexcept = default;

  /** Launches the execution thread which will be executing DAGs on the fly. **/
  void launchExecutionThread();

  /** Opens a new scope represented by a new execution graph (DAG). **/
  void openScope(const std::string & scope_name);

  /** Pauses the current scope by completing all outstanding tensor operations
      and pausing the further progress of the current execution graph until resume.
      Returns TRUE upon successful pausing, FALSE otherwise. **/
  void pauseScope();

  /** Resumes the execution of a previously paused scope (execution graph). **/
  void resumeScope(const std::string & scope_name);

  /** Closes the current scope, fully completing all tensor operations
      in the current execution graph. **/
  void closeScope();

  /** Returns TRUE if there is the current scope is set. **/
  inline bool currentScopeIsSet() const {return !(current_scope_.empty());}

  /** Submits a tensor operation into the current execution graph and returns its integer id.  **/
  VertexIdType submit(std::shared_ptr<TensorOperation> op);

  /** Tests for completion of a given tensor operation.
      If wait = TRUE, it will block until completion. **/
  bool sync(TensorOperation & op,
            bool wait = false);

  /** Tests for completion of all outstanding update operations on a given tensor.
      If wait = TRUE, it will block until completion. **/
  bool sync(const Tensor & tensor,
            bool wait = false);

  /** Returns an accessor to the elements of a given tensor. **/
  TensorDenseBlock getTensorData(const Tensor & tensor);

protected:
  ~TensorRuntime();

  /** The execution thread lives here **/
  void executionThreadWorkflow();

  /** Current tensor graph (DAG) executor **/
  std::shared_ptr<TensorGraphExecutor> graph_executor_;
  /** Active execution graphs (DAGs) **/
  std::map<std::string, std::shared_ptr<TensorGraph>> dags_;
  /** Name of the current scope (current DAG name) **/
  std::string current_scope_;
  /** Current DAG **/
  TensorGraph * current_dag_; //non-ownining pointer to the current DAG
  /** Current executing status (whether or not the execution thread is active) **/
  std::atomic<bool> executing_; //TRUE while the execution thread is executing the current DAG
  /** End of life flag **/
  std::atomic<bool> alive_; //TRUE while the main thread is accepting new operations from client
  /** Execution thread **/
  std::thread exec_thread_;
};

} // namespace runtime
} // namespace exatn

#endif //EXATN_RUNTIME_TENSOR_RUNTIME_HPP_
