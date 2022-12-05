#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <cstdint>

using namespace std;
using namespace std::filesystem;

static constexpr char* strResourceClass = 
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

static constexpr char* strCompareFunctions =
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

static void displayHelp()
{
	std::cout << "Syntax for srcrsc: " << endl
	          << "$srcrsc input_directory output_file namespace [options]" << endl
	          << "\tinput_directory - the directory containing the files to be compiled into resources" << endl
	          << "\toutput_directory - the output directory of created source files" << endl
	          << "\tnamespace - the root namespace of the resource, and the name of the package if cmake is used." << endl
	          << endl
	          << "options:" << endl
	          << "-cmake <package_name> -" << endl
	          << "\tgenerates a cmake package in the output directory of the build" << endl
	          << "\tthat will compile the resources as a static library." << endl
	          << endl
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
		cerr << "Error: Input directory does not exist." << endl;
		return -1;
	}

	std::string inc_guard_name = std::string(argv[3]);
	inc_guard_name = std::regex_replace(inc_guard_name, std::regex("::"), "_" );
	
	stringstream resource_function;
	resource_function << "\tstatic constexpr Resource getResource(const char* _path)" << endl << "\t{" << endl
		<< strCompareFunctions << endl << endl;
	
	ofstream out_text(output_file.native(), ios::trunc);
	out_text << "#ifndef _Resource_" << inc_guard_name << "_h_" << endl;
	out_text << "#define _Resource_" << inc_guard_name << "_h_" << endl << endl;
	out_text << "#include <exception>" << endl;
	out_text << "#include <array>" << endl;
	out_text << "#include <cstdint>" << endl;
	out_text << "#include <string>" << endl << endl;
	out_text << "namespace " << argv[3] << endl << "{" << endl;
	out_text << strResourceClass << endl << endl;

	size_t data_count = 0;

	auto generateSymbolArray = [&](const path& _path)
	{
		constexpr size_t bytes_per_row = 48;

		ifstream in_stream;
		stringstream data_stream;

		in_stream.open(_path, ios_base::binary | ios::ate );
		size_t file_size = in_stream.tellg();
		in_stream.seekg(0);

		auto outputByte = [&]()
		{
			uint8_t out_byte = 0;
			in_stream.read((char*)&out_byte, 1);

			data_stream << setfill(' ') << setw(3) << static_cast<uint16_t>(out_byte);
		};

		std::string strPath = _path.generic_string();
		strPath = strPath.substr( strPath.find('/') + 1 );

		resource_function << "\t\t" << "if ( isSame(_path, \"" << strPath << "\") )" << endl
			<< "\t\t{" << endl 
			<< "\t\t\tconstexpr std::array<uint8_t, " << file_size << "> objData = {";

		for (size_t char_count = 0; char_count < file_size; ++char_count )
		{
			if ( 0 == (char_count % bytes_per_row) )
				data_stream << endl << "\t\t\t\t";

			outputByte();

			if ( char_count < file_size - 1 )
				data_stream << ", ";
		}

		resource_function << data_stream.str() << endl << "\t\t\t};" << endl << endl
			<< "\t\t\treturn Resource(objData.data(), objData.size());"
			<< endl
			<< "\t\t}" << endl << endl;

		++data_count;
	};

	for ( auto const& entry : recursive_directory_iterator(input_directory) )
	{
		if ( entry.is_regular_file() )
			generateSymbolArray(entry.path());
	}

	resource_function << "\t\tthrow std::runtime_error(\"Reource not found.\");" 
		<< endl << "\t};" << endl;

	out_text << resource_function.str() << "}" << endl << "#endif";

	return 0;
}
