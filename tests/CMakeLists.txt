# =============================================================================
# tests/CMakeLists.txt
# 单元测试和集成测试配置
# =============================================================================

include(GoogleTest)

# Qt bin directory for DLL resolution at test runtime
get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(QT_BIN_DIR "${_qt_core_loc}" DIRECTORY)

# ==================== 1. Unit Tests ====================

# --- MeetingController 单元测试 ---
qt_add_executable(test_meeting_controller
    unit/test_meeting_controller.cpp
)
target_link_libraries(test_meeting_controller PRIVATE
    MeetingAppLib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_meeting_controller
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR};$<TARGET_FILE_DIR:test_meeting_controller>;$ENV{PATH}"
)

# --- ParticipantModel 单元测试 ---
qt_add_executable(test_participant_model
    unit/test_participant_model.cpp
)
target_link_libraries(test_participant_model PRIVATE
    MeetingAppLib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_participant_model
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR};$<TARGET_FILE_DIR:test_participant_model>;$ENV{PATH}"
)

# --- ChatModel 单元测试 ---
qt_add_executable(test_chat_model
    unit/test_chat_model.cpp
)
target_link_libraries(test_chat_model PRIVATE
    MeetingAppLib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_chat_model
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR};$<TARGET_FILE_DIR:test_chat_model>;$ENV{PATH}"
)

# --- LiveKitManager 单元测试 ---
qt_add_executable(test_livekit_manager
    unit/test_livekit_manager.cpp
)
target_link_libraries(test_livekit_manager PRIVATE
    MeetingAppLib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_livekit_manager
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR};$<TARGET_FILE_DIR:test_livekit_manager>;$ENV{PATH}"
)

# ==================== 2. 集成测试 ====================

# --- 会议流程集成测试 ---
qt_add_executable(test_meeting_flow
    integration/test_meeting_flow.cpp
)
target_link_libraries(test_meeting_flow PRIVATE
    MeetingAppLib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_meeting_flow
    PROPERTIES LABELS "integration"
    TIMEOUT 30
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR};$<TARGET_FILE_DIR:test_meeting_flow>;$ENV{PATH}"
)

# ==================== DLL 复制（测试可执行文件需要）====================
set(TEST_TARGETS
    test_meeting_controller
    test_participant_model
    test_chat_model
    test_livekit_manager
    test_meeting_flow
)

foreach(TEST_TARGET ${TEST_TARGETS})
    add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_BIN_DIR}/livekit.dll"
            $<TARGET_FILE_DIR:${TEST_TARGET}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_BIN_DIR}/livekit_ffi.dll"
            $<TARGET_FILE_DIR:${TEST_TARGET}>
        COMMENT "Copying LiveKit DLLs for ${TEST_TARGET}..."
    )
endforeach()
