cmake_minimum_required(VERSION 3.10)

# set the project name
project(tilegen)

find_package(PNG REQUIRED)

# add the executable
file(GLOB tilegen_src
	 "*.h"
	 "*.cc"
)

add_executable(tilegen ${tilegen_src})

target_compile_definitions(tilegen PRIVATE
	-DUSE_TILE
)

target_link_libraries(tilegen PRIVATE
	PNG::PNG
)

# set definitions
target_compile_definitions(tilegen PRIVATE -D_CONSOLE)

if(MSVC)
	add_custom_command(TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND cd $(SolutionDir)\\..\\crawl-ref\\source\\rltiles
		COMMAND echo Generating main.png...
		COMMAND "$(TargetPath)" dc-main.txt
		COMMAND echo Generating dngn.png...
		COMMAND "$(TargetPath)" dc-dngn.txt
		COMMAND echo Generating floor
		COMMAND "$(TargetPath)" dc-floor.txt
		COMMAND echo Generating wall
		COMMAND "$(TargetPath)" dc-wall.txt
		COMMAND echo Generating feat
		COMMAND "$(TargetPath)" dc-feat.txt
		COMMAND echo Generating player.png...
		COMMAND "$(TargetPath)" dc-player.txt
		COMMAND echo Generating gui.png...
		COMMAND "$(TargetPath)" dc-gui.txt
		COMMAND echo Generating icons..
		COMMAND "$(TargetPath)" dc-icons.txt
		COMMAND copy *.png ..\\dat\\tiles\\
		VERBATIM
	)
endif()