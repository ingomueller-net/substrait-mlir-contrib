include(AddMLIRPython)

# Specifies that all MLIR packages are co-located under the `mlir_structured`
# top level package (the API has been embedded in a relocatable way).
add_compile_definitions("MLIR_PYTHON_PACKAGE_PREFIX=mlir_structured.")

# ###############################################################################
# Sources
# ###############################################################################
declare_mlir_python_sources(StructuredPythonSources)
declare_mlir_python_sources(StructuredPythonSources.Dialects
  ADD_TO_PARENT StructuredPythonSources)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT StructuredPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir_structured"
  TD_FILE dialects/SubstraitOps.td
  SOURCES
  dialects/substrait.py
  DIALECT_NAME substrait
)

declare_mlir_python_extension(StructuredPythonSources.DialectExtension
  MODULE_NAME _structuredDialects
  ADD_TO_PARENT StructuredPythonSources
  SOURCES
  StructuredDialects.cpp
  EMBED_CAPI_LINK_LIBS
  StructuredCAPI
)

# ###############################################################################
# Common CAPI
# ###############################################################################
add_mlir_python_common_capi_library(StructuredPythonCAPI
  INSTALL_COMPONENT StructuredPythonModules
  INSTALL_DESTINATION python_packages/structured/mlir_structured/_mlir_libs
  OUTPUT_DIRECTORY "${STRUCTURED_BINARY_DIR}/python_packages/mlir_structured/_mlir_libs"
  RELATIVE_INSTALL_ROOT "../../../.."
  DECLARED_SOURCES
  StructuredPythonSources
  MLIRPythonExtension.RegisterEverything
  MLIRPythonSources.Core
  MLIRPythonSources.Dialects
  MLIRPythonSources.ExecutionEngine
)

# ###############################################################################
# Instantiation of all Python modules
# ###############################################################################
add_mlir_python_modules(StructuredPythonModules
  ROOT_PREFIX "${STRUCTURED_BINARY_DIR}/python_packages/mlir_structured"
  INSTALL_PREFIX "python_packages/mlir_structured"
  DECLARED_SOURCES
  StructuredPythonSources
  LLVMSupport
  MLIRPythonExtension.RegisterEverything
  MLIRPythonSources.Core
  MLIRPythonSources.Dialects
  MLIRPythonSources.ExecutionEngine
  COMMON_CAPI_LINK_LIBS
  StructuredPythonCAPI
)

add_dependencies(structured-all StructuredPythonModules)
