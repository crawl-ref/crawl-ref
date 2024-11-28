cmake_minimum_required(VERSION 3.10)

# Set the project name
project(crawl)

# Options
option(TILES "Determines whether to use graphical tiles" OFF)

# Global definitions
if(MSVC)
	# using Visual Studio C++
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# tilegen
add_subdirectory("rltiles/tool")

# vcpkg dependencies
find_package(ZLIB REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(unofficial-pcre CONFIG REQUIRED)

if(TILES)
	find_package(PNG REQUIRED)
	find_package(Freetype REQUIRED)
	find_package(SDL2 CONFIG REQUIRED)
	find_package(SDL2_image CONFIG REQUIRED)
endif()

# We can't take lua from vcpkg, since we require older version
# hence we build it ourselves
set(LUA_FOLDER "contrib/lua/src/")

set(LUA_SOURCES
	${LUA_FOLDER}lapi.c
	${LUA_FOLDER}lauxlib.c
	${LUA_FOLDER}lbaselib.c
	${LUA_FOLDER}lcode.c
	${LUA_FOLDER}ldblib.c
	${LUA_FOLDER}ldebug.c
	${LUA_FOLDER}ldo.c
	${LUA_FOLDER}ldump.c
	${LUA_FOLDER}lfunc.c
	${LUA_FOLDER}lgc.c
	${LUA_FOLDER}linit.c
	${LUA_FOLDER}liolib.c
	${LUA_FOLDER}llex.c
	${LUA_FOLDER}lmathlib.c
	${LUA_FOLDER}lmem.c
	${LUA_FOLDER}loadlib.c
	${LUA_FOLDER}lobject.c
	${LUA_FOLDER}lopcodes.c
	${LUA_FOLDER}loslib.c
	${LUA_FOLDER}lparser.c
	${LUA_FOLDER}lstate.c
	${LUA_FOLDER}lstring.c
	${LUA_FOLDER}lstrlib.c
	${LUA_FOLDER}ltable.c
	${LUA_FOLDER}ltablib.c
	${LUA_FOLDER}ltm.c
	${LUA_FOLDER}lundump.c
	${LUA_FOLDER}lvm.c
	${LUA_FOLDER}lzio.c
	${LUA_FOLDER}print.c
)

set(LUA_HEADERS    
	${LUA_FOLDER}lapi.h
	${LUA_FOLDER}lauxlib.h
	${LUA_FOLDER}lcode.h
	${LUA_FOLDER}ldebug.h
	${LUA_FOLDER}ldo.h
	${LUA_FOLDER}lfunc.h
	${LUA_FOLDER}lgc.h
	${LUA_FOLDER}llex.h
	${LUA_FOLDER}llimits.h
	${LUA_FOLDER}lmem.h
	${LUA_FOLDER}lobject.h
	${LUA_FOLDER}lopcodes.h
	${LUA_FOLDER}lparser.h
	${LUA_FOLDER}lstate.h
	${LUA_FOLDER}lstring.h
	${LUA_FOLDER}ltable.h
	${LUA_FOLDER}ltm.h
	${LUA_FOLDER}lua.h
	${LUA_FOLDER}luaconf.h
	${LUA_FOLDER}lualib.h
	${LUA_FOLDER}lundump.h
	${LUA_FOLDER}lvm.h
	${LUA_FOLDER}lzio.h
)

add_library(lua STATIC ${LUA_SOURCES} ${LUA_HEADERS})

target_compile_definitions(lua PRIVATE "-D_LIB")

# crawl itself
file(GLOB CRAWL_SOURCES
	 "*.h"
	 "*.cc"
	 "prebuilt/*.h"
	 "prebuilt/*.cc"
	"rltiles/*.h"
	"rltiles/*.cc"
)

list(REMOVE_ITEM CRAWL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/libunix.cc)

add_executable(crawl ${CRAWL_SOURCES})

# include
target_include_directories(crawl PRIVATE
	"MSVC/include"
	"contrib/lua/src"
	"rltiles"
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

# set definitions
target_compile_definitions(crawl PRIVATE
	-D__STDC_LIMIT_MACROS
	-D_USE_MATH_DEFINES
	-D_ALLOW_KEYWORD_MACROS
	-DWIZARD
	-DUSE_GL
	-DCLUA_BINDINGS
)

if(TILES)
	target_compile_definitions(crawl PRIVATE
		-DPROPORTIONAL_FONT="../../crawl-ref/source/contrib/fonts/DejaVuSans.ttf"
		-DMONOSPACED_FONT="../../crawl-ref/source/contrib/fonts/DejaVuSansMono.ttf"
		-DUSE_TILE
		-DUSE_TILE_LOCAL
		-DUSE_FT
		-DUSE_SDL
	)
endif()

target_link_libraries(crawl
	lua
	ZLIB::ZLIB
	unofficial::pcre::pcre
	unofficial::pcre::pcre16
	unofficial::pcre::pcre32
	unofficial::pcre::pcrecpp
	unofficial::sqlite3::sqlite3
)

if(TILES)
	target_link_libraries(crawl
		PNG::PNG
		Freetype::Freetype
		$<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
		$<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
		opengl32
		glu32
	)
endif()

add_dependencies(crawl tilegen)

if(MSVC)
	add_custom_command(TARGET crawl
		PRE_BUILD
		COMMAND cd $(SolutionDir)/../crawl-ref/source
		COMMAND perl.exe "util/gen_ver_msvc.pl" build.h
		COMMAND perl.exe "util/gen-cflg.pl" compflag.h "<UNKNOWN>" "<UNKNOWN>"
		COMMAND xcopy $(SolutionDir)..\\crawl-ref\\source\\dat\\ $(OutDir)dat\\ /s /e /d /y
		VERBATIM
	)
endif()