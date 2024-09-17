add_mlir_translation_library(MLIRTargetSubstraitPB
  Export.cpp
  Import.cpp
  ProtobufUtils.cpp

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRSubstraitDialect
  MLIRSupport
  MLIRTransforms
  MLIRTranslateLib
  substrait_proto
  protobuf::libprotobuf
  )
