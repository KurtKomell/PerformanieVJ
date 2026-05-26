add_library(pvj_core STATIC
    Model.h
    EnumStrings.h
    EnumStrings.cpp
    FilterParamSchema.h
    FilterParamSchema.cpp
    PropertyRegistry.h
    PropertyRegistry.cpp
    FilterCatalog.h
    FilterCatalog.cpp
    FilterEffectIds.h
    FilterEffectIds.cpp
    Project.h
    Project.cpp
    PvjSerializer.h
    PvjSerializer.cpp
    Vj2Importer.h
    Vj2Importer.cpp
    AvcImporter.h
    AvcImporter.cpp
)

target_link_libraries(pvj_core
    PUBLIC
        Qt6::Core
)

target_include_directories(pvj_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(pvj_core PROPERTIES
    AUTOMOC OFF
    AUTORCC OFF
    AUTOUIC OFF
)
