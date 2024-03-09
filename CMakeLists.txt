cmake_minimum_required (VERSION 3.5)

set(CMAKE_CXX_STANDARD 20)

project (ResourceCompiler)

add_executable(compilerc
	./src/ResourceCompiler.cpp
)

set_target_properties(compilerc
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

function(add_resource_directory)
	set(oneValueArgs INPUT_DIRECTORY NAMESPACE OUT_HEADER)

endfunction()

function(add_resource_header
	resource_target_name
	resource_root_dir
	resource_out_file
	resource_namespace
)
	add_custom_target( ${resource_target_name}
		DEPENDS compilerc
		COMMAND compilerc "${resource_root_dir}"
			"${resource_out_file}"
			"${resource_namespace}"
		BYPRODUCTS ${resource_out_file}
		VERBATIM
	)
endfunction()
