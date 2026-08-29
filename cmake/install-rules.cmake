install(
    TARGETS cpp_navigation_game_exe
    RUNTIME COMPONENT cpp_navigation_game_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
