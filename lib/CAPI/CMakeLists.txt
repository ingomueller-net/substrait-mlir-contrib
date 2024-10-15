add_mlir_public_c_api_library(StructuredCAPI
    Dialects.cpp

  LINK_LIBS PUBLIC
    MLIRCAPIIR
    MLIRSubstraitDialect
    MLIRTargetSubstraitPB
    MLIRPass
)
