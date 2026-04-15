cmake_minimum_required(VERSION 3.20)

project(engine_design LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

set(APP_SOURCES
    app/main.cpp

    core/model/engine_validation.cpp

    core/kinematic/kinematic_model_builder.cpp
    core/kinematic/kinematic_solver.cpp

    core/dynamic/dynamic_input_factory.cpp
    core/dynamic/dynamic_calculator.cpp

    core/balancing/balancing_model_builder.cpp
    core/balancing/balancing_calculator.cpp
    core/balancing/balancing_composer.cpp
    core/balancing/balancing_pipeline.cpp
    core/balancing/balancing_input_factory.cpp
    core/balancing/balancing_scheme_library.cpp
    core/balancing/balancing_equivalent_target.cpp

    gui/frame/main_frame.cpp
    gui/frame/main_menu_builder.cpp

    gui/pages/input_page.cpp
    gui/pages/kinematic_result_page.cpp
    gui/pages/mass_properties_page.cpp
    gui/pages/dynamic_result_page.cpp
    gui/pages/counterweight_setup_page.cpp
    gui/pages/balancing_result_page.cpp

    gui/widgets/navigation_panel.cpp
    gui/widgets/engine_input_panel.cpp
    gui/widgets/engine_scheme_panel.cpp
    gui/widgets/kinematic_params_panel.cpp
    gui/widgets/kinematic_chart_panel.cpp
    gui/widgets/kinematic_legend_panel.cpp
    gui/widgets/kinematic_table_panel.cpp
    gui/widgets/dynamic_chart_panel.cpp
    gui/widgets/dynamic_table_panel.cpp
    gui/widgets/dynamic_legend_panel.cpp
    gui/widgets/counterweight_input_panel.cpp
    gui/widgets/balancing_chart_panel.cpp
    gui/widgets/balancing_table_panel.cpp

    gui/dialogs/settings_dialog.cpp
    gui/dialogs/help_dialog.cpp
    gui/dialogs/window_size_dialog.cpp
    core/balancing/balancing_synthesizer.cpp

    app_icon.rc
)

set(APP_HEADERS
    core/model/engine_types.h
    core/model/engine_model.h
    core/model/balancing_model.h
    core/model/mass_properties_input.h
    core/model/engine_validation.h

    core/kinematic/normalized_kinematic_model.h
    core/kinematic/kinematic_model_builder.h
    core/kinematic/kinematic_result.h
    core/kinematic/kinematic_solver.h

    core/dynamic/dynamic_input.h
    core/dynamic/dynamic_input_factory.h
    core/dynamic/dynamic_result.h
    core/dynamic/dynamic_calculator.h

    core/balancing/normalized_balancing_model.h
    core/balancing/balancing_input.h
    core/balancing/balancing_input_factory.h
    core/balancing/balancing_result.h
    core/balancing/balancing_model_builder.h
    core/balancing/balancing_calculator.h
    core/balancing/balancing_composed_result.h
    core/balancing/balancing_composer.h
    core/balancing/balancing_pipeline.h
    core/balancing/balancing_scheme_library.h
    core/balancing/balancing_equivalent_target.h

    gui/common/text_utf8.h

    gui/frame/main_frame.h
    gui/frame/main_menu_builder.h

    gui/pages/input_page.h
    gui/pages/kinematic_result_page.h
    gui/pages/mass_properties_page.h
    gui/pages/dynamic_result_page.h
    gui/pages/counterweight_setup_page.h
    gui/pages/balancing_result_page.h

    gui/widgets/navigation_panel.h
    gui/widgets/engine_input_panel.h
    gui/widgets/engine_scheme_panel.h
    gui/widgets/kinematic_params_panel.h
    gui/widgets/kinematic_chart_panel.h
    gui/widgets/kinematic_legend_panel.h
    gui/widgets/kinematic_metric.h
    gui/widgets/kinematic_table_panel.h

    gui/widgets/dynamic_metric.h
    gui/widgets/dynamic_component.h
    gui/widgets/dynamic_chart_panel.h
    gui/widgets/dynamic_table_panel.h
    gui/widgets/dynamic_legend_panel.h

    gui/widgets/counterweight_input_panel.h

    gui/widgets/balancing_metric.h
    gui/widgets/balancing_component.h
    gui/widgets/balancing_chart_panel.h
    gui/widgets/balancing_table_panel.h

    gui/dialogs/settings_dialog.h
    gui/dialogs/help_dialog.h
    gui/dialogs/window_size_dialog.h

    core/balancing/balancing_synthesizer.h
    core/balancing/balancing_synthesis.h
)

add_executable(engine_gui WIN32
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(engine_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(engine_gui PRIVATE ${wxWidgets_LIBRARIES})

if(MSVC)
    set_target_properties(engine_gui PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

install(TARGETS engine_gui RUNTIME DESTINATION .)

if(WIN32)
    foreach(wx_dll IN LISTS wxWidgets_LIBRARIES)
        if(EXISTS "${wx_dll}" AND wx_dll MATCHES "\\.dll$")
            install(FILES "${wx_dll}" DESTINATION .)
        endif()
    endforeach()
endif()