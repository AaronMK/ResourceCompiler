#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#if __has_include("../resource_out/out.h")
#	include "../resource_out/out.h"
#endif

using namespace std;
using namespace std::filesystem;

constexpr const char* strResourceClass = 
	"\tclass Resource\n"
	"\t{\n"
	"\tprivate:\n"
	"\t\tconst void*      mData;\n"
	"\t\tconst uint64_t   mSize;\n"
	"\n"
	"\tpublic:\n"
	"\t\tconstexpr Resource(const void* _data, size_t _size)\n"
	"\t\t\t: mData(_data), mSize(_size)\n"
	"\t\t{\n"
	"\t\t}\n"
	"\n"
	"\t\tconstexpr const void* data() const\n"
	"\t\t{\n"
	"\t\t\treturn mData;\n"
	"\t\t}\n"
	"\n"
	"\t\tconstexpr uint64_t size() const\n"
	"\t\t{\n"
	"\t\t\treturn mSize;\n"
	"\t\t}\n"
	"\t};";

constexpr const char* strCompareFunctions =
	"\t\tauto strLength = [](const char* str)\n"
	"\t\t{\n"
	"\t\t\tsize_t len = 0;\n"
	"\t\t\twhile ( str[len] )\n"
	"\t\t\t\t++len;\n"
	"\n"
	"\t\t\treturn len;\n"
	"\t\t};\n"
	"\t\t\n"
	"\t\tauto isSame = [&](const char* left, const char* right)\n"
	"\t\t{\n"
	"\t\t\tsize_t l_left = strLength(left);\n"
	"\t\t\tsize_t l_right = strLength(right);\n"
	"\n"
	"\t\t\tif ( l_left != l_right )\n"
	"\t\t\t\treturn false;\n"
	"\n"
	"\t\t\tfor ( size_t i = 0; i < l_left; ++i )\n"
	"\t\t\t{\n"
	"\t\t\t\tif ( left[i] != right[i] )\n"
	"\t\t\t\t\treturn false;\n"
	"\t\t\t}\n"
	"\n"
	"\t\t\treturn true;\n"
	"\t\t};";

static std::string generateFileData(const path& _path, uint32_t block_level)
{
	constexpr uint32_t bytes_per_row = 48;
	std::string block_tabs(block_level, '\t');

	ifstream in_stream(_path, ios_base::binary);

	if ( !in_stream.is_open() )
		throw runtime_error("Failed to open input file.");

	size_t bytes_remaining = file_size(_path);
	size_t char_read_count = 0;

	stringstream data_stream;
	std::array<uint8_t, 4096> in_buffer;

	data_stream << block_tabs;

	while ( bytes_remaining > 0 )
	{
		size_t bytes_to_read = std::min(in_buffer.size(), bytes_remaining);
		
		in_stream.read(
			(char*)in_buffer.data(),
			std::min(in_buffer.size(), bytes_remaining)
		);

		for (size_t i = 0; i < bytes_to_read; ++i)
		{
			if ( char_read_count > 0 && (char_read_count % bytes_per_row) == 0 )
				data_stream << '\n' << block_tabs;

			data_stream << uint32_t(in_buffer[i]);
			--bytes_remaining;
			char_read_count++;

			if ( bytes_remaining != 0 )
				data_stream << ", ";
		}
	}

	return data_stream.str();
}

static void displayHelp()
{
	std::cout << "Syntax for compilerc: " << endl
	          << "$compilerc input_directory output_file namespace [options]" << endl
	          << "\tinput_directory - the directory containing the files to be compiled into resources" << endl
	          << "\toutput_directory - the output directory of created source files" << endl
	          << "\tnamespace - the root namespace of the resource, and the name of the package if cmake is used." << endl
	          << endl
	          << "options:" << endl
	          << "-clean -" << endl
	          << "Regenerates all sources file from scratch." << endl
	          << endl
	          << "-single_source - " << endl
	          << "\tA single source file is generated for all resources.  It is easier" << endl
	          << "\tto manually include in an external project, but must be completely regenerated" << endl
	          << "\tanytime a single file changes." << endl
	          << endl
	          << "-header_only - " << endl
	          << "\tA single header is generated for all resources.  It is easier" << endl
	          << "\tto manually include in an external project, but must be completely regenerated" << endl
	          << "\tanytime a single file changes. It is also heavy on compilation each time it is included," << endl
	          << "\tbut is constexpr in looking up data and symbols.";
}

int main(int argc, char**argv)
{
	directory_entry input_directory;
	path output_file;
	
	if ( argc <= 1 || 0 == strcmp(argv[1], "--help") )
	{
		displayHelp();
		return 0;
	}

	if ( argc < 4 )
	{
		cerr << "Error: source directory destination file, and namespace must be provided." << endl;
		return -1;
	}
	
	input_directory = directory_entry(filesystem::path(argv[1]));
	output_file = filesystem::path(argv[2]);

	if ( !input_directory.exists() )
	{
		cerr << "Error: Input directory does not exist - " << argv[1] << endl;
		return -1;
	}

	auto isUpdateNeeded = [&]() -> bool
	{
		if ( !filesystem::exists(output_file) )
			return true;

		auto out_last_modified = filesystem::last_write_time(output_file);

		if ( filesystem::last_write_time(input_directory) > out_last_modified )
			return true;

		for ( auto const& entry : recursive_directory_iterator(input_directory) )
		{
			if ( filesystem::last_write_time(entry) > out_last_modified )
				return true;
		}

		return false;
	};

	if ( !isUpdateNeeded() )
	{
		cout << "Resource Compiler: Output file is newer than input file(s).  Skipping generation." << endl;
		return 0;
	}

	std::string inc_guard_name = std::string(argv[3]);
	inc_guard_name = std::regex_replace(inc_guard_name, std::regex("::"), "_" );
	
	stringstream resource_function;
	resource_function << "\tstatic constexpr Resource getResource(const char* _path)\n\t{\n"
		<< strCompareFunctions;
	
	ofstream out_text(output_file.native(), ios::trunc);
	out_text << "#ifndef _Resource_" << inc_guard_name << "_h_\n"
		"#define _Resource_" << inc_guard_name << "_h_\n\n"
		"#include <array>\n"
		"#include <cstdint>\n"
		"#include <stdexcept>\n"
		"#include <string>\n\n"
		"namespace " << argv[3] << "\n{\n"
		<< strResourceClass << "\n\n";

	std::stringstream data_array;
	data_array << "\tstatic uint8_t resource_data[] = {\n";

	std::stringstream conditionals_code;
	size_t bytes_written = 0;

	for ( auto const& entry : recursive_directory_iterator(input_directory) )
	{
		if ( entry.is_regular_file() )
		{
			filesystem::path native_path(entry.path());

			auto relative_path = filesystem::relative(native_path, input_directory);

			std::string strPath = relative_path.generic_string();
			auto f_size = filesystem::file_size(native_path);

			cout << "Generating data for \'" << strPath << "\"" << std::endl;

			if ( bytes_written != 0 )
				data_array << ",\n";

			data_array << "\n\t\t// " << strPath << "\n";
			data_array << generateFileData(native_path, 2);

			conditionals_code << "\n\n\t\t" << "if ( isSame(_path, \"" << strPath << "\") )\n"
				"\t\t\treturn Resource(&resource_data[" << bytes_written << "], " << f_size << ");";

			bytes_written += filesystem::file_size(native_path);
		}
	}

	data_array << "\n\t};\n\n";
	resource_function << conditionals_code.str() << "\n\n\t\tthrow std::runtime_error(\"Reource not found.\");\n\t};\n";

	out_text << data_array.str() << resource_function.str() << "}\n#endif";

	return 0;
}
