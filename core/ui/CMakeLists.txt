if(NOT LIBRETRO)
    set(CLARITY_DEFAULT OFF)
    if((WIN32 AND NOT WINDOWS_STORE) OR ANDROID OR (APPLE AND NOT IOS) OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CLARITY_DEFAULT ON)
    endif()
    option(FLYCAST_CLARITY "Build Clarity frontend" ${CLARITY_DEFAULT})
    if(FLYCAST_CLARITY)
        target_sources(${PROJECT_NAME} PRIVATE clarity/clarity.cpp clarity/settings_pages.cpp clarity/library.cpp)
        target_compile_definitions(${PROJECT_NAME} PRIVATE FLYCAST_CLARITY)
        if(WIN32 AND NOT WINDOWS_STORE)
            target_sources(${PROJECT_NAME} PRIVATE clarity/windows_picker.cpp)
            target_link_libraries(${PROJECT_NAME} PRIVATE ole32 shell32 uuid)
        elseif((APPLE AND NOT IOS) OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
            include(FetchContent)
            set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
            set(NFD_PORTAL ON CACHE BOOL "Use Linux desktop portal" FORCE)
            FetchContent_Declare(clarity_nfd
                GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
                GIT_TAG 65b2f070a0fcfee0d453a64e770a217ab0b6ff4f)
            FetchContent_MakeAvailable(clarity_nfd)
            target_sources(${PROJECT_NAME} PRIVATE clarity/desktop_picker.cpp)
            target_compile_definitions(${PROJECT_NAME} PRIVATE CLARITY_DESKTOP_PICKER)
            target_link_libraries(${PROJECT_NAME} PRIVATE nfd)
        elseif(ANDROID)
            target_sources(${PROJECT_NAME} PRIVATE clarity/android_picker.cpp)
            target_compile_definitions(${PROJECT_NAME} PRIVATE CLARITY_ANDROID_PICKER)
        endif()
        option(FLYCAST_CLARITY_SMOKE_TEST "Run deterministic desktop UI capture test (not a user build)" OFF)
        if(FLYCAST_CLARITY_SMOKE_TEST AND NOT WINDOWS_STORE AND NOT ANDROID AND ((WIN32 AND USE_DX11) OR (NOT WIN32 AND USE_OPENGL)))
            target_sources(${PROJECT_NAME} PRIVATE clarity/tests/ui_smoke.cpp)
            set_property(SOURCE
                "${CMAKE_CURRENT_SOURCE_DIR}/gui.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/clarity/clarity.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/clarity/library.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/clarity/settings_pages.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/settings_controls.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/gui_util.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/clarity/tests/ui_smoke.cpp"
                TARGET_DIRECTORY ${PROJECT_NAME}
                APPEND PROPERTY COMPILE_DEFINITIONS FLYCAST_CLARITY_SMOKE_TEST)
        endif()
    endif()
    target_sources(${PROJECT_NAME} PRIVATE
            game_scanner.cpp
            game_scanner.h
            imgui_driver.h
            gui.cpp
            gui.h
            gui_font.cpp
            gui_font.h
            gui_achievements.cpp
            vgamepad.cpp
            vgamepad.h
            gui_chat.h
            gui_cheats.cpp
            gui_util.cpp
            gui_util.h
            mainui.cpp
            mainui.h
            settings.cpp
            settings.h
            settings_about.cpp
            settings_audio.cpp
            settings_controls.cpp
            settings_general.cpp
            settings_network.cpp
            settings_video.cpp
            boxart/arcade_scraper.h
            boxart/boxart.cpp
            boxart/boxart.h
            boxart/gamesdb.cpp
            boxart/gamesdb.h
            boxart/pvrparser.h
            boxart/scraper.cpp
            boxart/scraper.h)

    if(USE_DISCORD)
        target_sources(${PROJECT_NAME} PRIVATE discord.cpp)
    endif()
endif()
