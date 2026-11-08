if(NOT LIBRETRO)
    option(FLYCAST_CLARITY "Build Clarity desktop settings" ${WIN32})
    if(FLYCAST_CLARITY)
        target_sources(${PROJECT_NAME} PRIVATE clarity/clarity.cpp)
        target_compile_definitions(${PROJECT_NAME} PRIVATE FLYCAST_CLARITY)
        option(FLYCAST_CLARITY_SMOKE_TEST "Run deterministic DX11 UI capture test (not a user build)" OFF)
        if(FLYCAST_CLARITY_SMOKE_TEST AND WIN32 AND USE_DX11)
            target_sources(${PROJECT_NAME} PRIVATE clarity/tests/ui_smoke.cpp)
            set_property(SOURCE
                "${CMAKE_CURRENT_SOURCE_DIR}/gui.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/clarity/clarity.cpp"
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
