if(NOT TARGET games-frame-pacing::swappy)
add_library(games-frame-pacing::swappy SHARED IMPORTED)
set_target_properties(games-frame-pacing::swappy PROPERTIES
    IMPORTED_LOCATION "C:/Users/donod/.gradle/caches/8.10.2/transforms/9bab4a4d24e6f7a966f44deca28fb384/transformed/games-frame-pacing-2.1.3/prefab/modules/swappy/libs/android.x86_64/libswappy.so"
    INTERFACE_INCLUDE_DIRECTORIES "C:/Users/donod/.gradle/caches/8.10.2/transforms/9bab4a4d24e6f7a966f44deca28fb384/transformed/games-frame-pacing-2.1.3/prefab/modules/swappy/include"
    INTERFACE_LINK_LIBRARIES ""
)
endif()

if(NOT TARGET games-frame-pacing::swappy_static)
add_library(games-frame-pacing::swappy_static STATIC IMPORTED)
set_target_properties(games-frame-pacing::swappy_static PROPERTIES
    IMPORTED_LOCATION "C:/Users/donod/.gradle/caches/8.10.2/transforms/9bab4a4d24e6f7a966f44deca28fb384/transformed/games-frame-pacing-2.1.3/prefab/modules/swappy_static/libs/android.x86_64/libswappy_static.a"
    INTERFACE_INCLUDE_DIRECTORIES "C:/Users/donod/.gradle/caches/8.10.2/transforms/9bab4a4d24e6f7a966f44deca28fb384/transformed/games-frame-pacing-2.1.3/prefab/modules/swappy_static/include"
    INTERFACE_LINK_LIBRARIES ""
)
endif()

