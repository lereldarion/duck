add_library (duck STATIC
	command_line.cpp
	view.cpp
	)

# Using raw source dir so that #include <duck/...> works, and .cpp in the same dir as .h
target_include_directories (duck PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
	)

target_compile_options (duck PRIVATE -Wall -Wextra)
target_link_libraries (duck PUBLIC fmt gsl)
