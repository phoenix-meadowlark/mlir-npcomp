//===- graph_importer.h -----------------------------------------*- C++ -*-===//
//
// This file is licensed under a pytorch-style license
// See frontends/pytorch/LICENSE for license information.
//
//===----------------------------------------------------------------------===//

#ifndef NPCOMP_FRONTENDS_PYTORCH_CSRC_GRAPH_IMPORTER_H
#define NPCOMP_FRONTENDS_PYTORCH_CSRC_GRAPH_IMPORTER_H

#include <memory>

#include "../pybind.h"
#include "func_builder.h"

#include "mlir-c/IR.h"

#include <torch/csrc/jit/api/compilation_unit.h>
#include <torch/csrc/jit/ir/ir.h>

namespace torch_mlir {

/// Main entry-point for importing torch::jit::Graph instances (and structures
/// surrounding them such as modules and methods).
///
/// In torch terminology, a Graph is a function. Later in the compiler, we may
/// specialize multiple versions of it.
///
/// Since graph functions typically have enough annotations for the most
/// generic form of every type (i.e. Tensor, List, etc), and since we often
/// want to multi-version specific specializations, we take the approach of
/// generating a '$generic' suffixed function at that level and then generate
/// the actual named function with using a 'numpy.generic_call' op to invoke
/// the generic function with metadata controlling how it is legal to
/// specialize. This leaves the process of inlining and expanding the
/// specializations to compiler passes.
class GraphImporter : public std::enable_shared_from_this<GraphImporter> {
public:
  /// Options for mapping Graph concepts to MLIR. In addition to things like
  /// names and type mappings, this includes various policy options such as
  /// when to import globals as constants vs shared arrays, etc.
  struct MlirMappingOptions {
    MlirContext context;
    c10::optional<std::string> genericFuncName;
    c10::optional<std::string> funcName;
    TypeMapper &typeMapper;
    FuncBuilder::Inserter &inserter;
  };
  /// Construct an importer.
  GraphImporter(std::shared_ptr<torch::jit::Graph> graph,
                MlirMappingOptions mappingOptions);

  /// Helper to create a graph importer from a traced/scripted python function.
  /// If the funcName of the mapping options is not set, it is set from the
  /// function name. It is the responsibility of the caller to ensure that the
  /// funcObj and associated graph outlives this instance.
  static std::shared_ptr<GraphImporter>
  forPythonJitFunc(torch::jit::Function *function,
                   MlirMappingOptions mappingOptions);

  /// Initialize for import. This is separate from the constructor purely for
  /// ergonomics and must be called post-construction. Initialization activities
  /// that throw go here.
  void initialize();

  /// Imports the generic function into the module.
  void importGenericFunc();

private:
  class NodeScope;
  class NodeImporter;

  MlirContext context() { return mappingOptions.context; }
  TypeMapper &type_mapper() { return mappingOptions.typeMapper; }
  MlirLocation extractCallstackLoc(torch::jit::Node *node,
                                   bool useDefault = true);
  std::shared_ptr<torch::jit::Graph> graph;
  MlirMappingOptions mappingOptions;

  /// Default function location, to be used when a more specific is not
  /// available.
  MlirLocation defaultLoc;

  /// Argument and return types for the generic func.
  std::vector<MlirType> genericFuncArgTypes;
  std::vector<MlirType> genericFuncReturnTypes;
};

} // namespace torch_mlir

#endif // NPCOMP_FRONTENDS_PYTORCH_CSRC_GRAPH_IMPORTER_H
