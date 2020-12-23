//#include <cxxopts.hpp>
#include <filesystem>
#include <charconv>
#include <CLI/CLI.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include <iomanip>

namespace jtin {
	struct info {
		std::string workingDir;
		std::string outputDir;
		std::string outputName;
		std::vector<std::string> classes;
	};

	struct TestRecord {
		std::string_view className;
		size_t id;
		std::string original;
		std::string html;
	};
}
bool getDirectoryContents(std::vector<std::vector<std::pair<uint32_t,std::string>>>& dest, const std::vector<std::pair<std::string, int>>& classes, std::string_view wDir);
void cofigurArgParser(CLI::App& app, jtin::info& argVals);
auto extractData(std::vector<std::pair<std::string, int>>& dest, const std::vector<std::string> &args)->std::vector<std::string>::const_iterator;
bool extractFileContents(std::vector<jtin::TestRecord>& dest, const std::vector<std::vector<std::pair<uint32_t, std::string>>>& filenames, const std::vector<std::pair<std::string, int>>& classes);
bool renderJson(nlohmann::json& jc, const std::vector<jtin::TestRecord>& records);
void checkArgs(jtin::info& argVals);

int main(int argc, char* argv[]) {

	jtin::info argVals;
	CLI::App app{ "Create test cases in JSON format from class of files distinguished by names", "TestGenJSONOutput" };

	cofigurArgParser(app, argVals);

	CLI11_PARSE(app, argc, argv);

	checkArgs(argVals);

	if (app.remaining_size() == 0) {
		std::cerr << "Please specify file class names";
		return 1;
	}
	argVals.classes = app.remaining();

	std::vector<std::pair<std::string, int>> classNames{};
	if (auto it = extractData(classNames, argVals.classes); it != argVals.classes.cend()) {
		std::cerr << "'" << *it << "' is an invalid argument";
		return 2;
	}

	//return 0;
	std::vector<std::vector<std::pair<uint32_t,std::string>>> files(classNames.size());

	bool isSuccess = getDirectoryContents(files, classNames, argVals.workingDir);
	using typi_t = decltype(files)::value_type;
	std::vector<jtin::TestRecord> fileContentsAndInfo(std::accumulate(files.cbegin(), files.cend(), 0ull, [](const size_t& acc, const typi_t& classes) {return acc + classes.size(); }));
	bool isSuccess2 = extractFileContents(fileContentsAndInfo, files, classNames);
	nlohmann::json jc;
	bool isSuccess3 = renderJson(jc, fileContentsAndInfo);
	std::ofstream fout{argVals.outputDir + "/" + argVals.outputName + ".json"};
	fout << std::setw(4) << jc << std::endl;
	fout.close();
	return 0;
}

bool getDirectoryContents(std::vector<std::vector<std::pair<uint32_t, std::string>>>& dest, const std::vector<std::pair<std::string, int>>& classes, std::string_view wDir) {
	for (auto& fileEntry : std::filesystem::directory_iterator(wDir)) {
		auto name = fileEntry.path().filename().string();
		auto pos_ix = name.rfind('.');
		auto pos_i = name.rfind('.', pos_ix-1);
		if (pos_i != std::string::npos) {
			for (size_t i = 0, n = classes.size(); i < n; ++i) {
#if 0
				if (classes[i].first == std::string_view{ name.data(), pos_i - classes[i].second }) {
#else
				if(classes[i].first.compare(0, std::string::npos, name.data(), pos_i-classes[i].second) == 0) {
#endif
					int id{};
					auto result = std::from_chars(name.data() + pos_i - classes[i].second, name.data() + pos_i, id);
					dest[i].emplace_back(id, fileEntry.path().string());
				}
			}
			//pos_i = pos_ix;
		}
		
	}
	for (auto& container : dest) {
		using tuple_t = std::pair<uint32_t, std::string>;
		std::sort(container.begin(), container.end(), [](tuple_t& a, tuple_t& b) {return a.first < b.first; });
	}
	return true;
}

bool extractFileContents(std::vector<jtin::TestRecord>& dest, const std::vector<std::vector<std::pair<uint32_t, std::string>>>& filenames, const std::vector<std::pair<std::string, int>>& classes) {
	size_t i = 0;
	size_t i_class = 0;
	std::ifstream in{};
	std::string scratchBuffer{};

	for (auto& name : filenames) {
		for (auto& [_unused, filename] : name) {
			dest[i].className = classes[i_class].first;
			dest[i].id = i;
			in.open(filename);
			while ((std::getline(in, scratchBuffer)) && scratchBuffer != "\\\\\\__OUT_FILE:") {
				dest[i].original.append(scratchBuffer);
				dest[i].original.push_back('\n');
			}
			while (std::getline(in, scratchBuffer)) {
				dest[i].html.append(scratchBuffer);
				dest[i].html.push_back('\n');
			}
			in.close();
			++i;
		}
		++i_class;
	}
	return true;
}

bool renderJson(nlohmann::json& jc, const std::vector<jtin::TestRecord>& r) {
	for (auto& rec : r) {
		jc.push_back({ {"markdown", rec.original}, {"html", rec.html}, {"example", rec.id}, {"section", rec.className} });
	}
	return true;
}

void cofigurArgParser(CLI::App& app, jtin::info& argVals) {
	app.allow_extras();
	app.add_option("-s, --source-dir", argVals.workingDir, "specify a working directory for files to be processed");
	app.add_option("-d, --output-dir", argVals.outputDir, "specify a list of names designated for each class of test files separated by commas");
	app.add_option("-o, --name", argVals.outputName, "set a name for the final json text file");
}
void checkArgs(jtin::info& argVals) {
	if (argVals.outputName.empty()) {
		argVals.outputName = "mdpm_tests";
	}
	if (auto i = argVals.outputName.rfind('.'); (i != std::string::npos) && (argVals.outputName.compare(i, 5, ".json") == 0) && (i+5==argVals.outputName.size())) {
		argVals.outputName.erase(i, 5);
	}
}