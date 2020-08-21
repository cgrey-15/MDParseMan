// MDParseMan.cpp : Defines the entry point for the application.
//

#include <cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <locale>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif
//#include <optional>

#include "MDParseMan.h"

using namespace std;

auto getTestCases(std::istream& strime, const std::string& section, bool useOther, int n) -> std::vector<std::string>;
template<typename CharT, typename std::basic_string<CharT>::size_type BufLen=32>
auto getMarkdown(std::basic_istream<CharT>& stm, bool makeSeparateLines)->std::vector<std::basic_string<CharT>>;
template<typename CharT>
auto getMarkdownLines(std::basic_istream<CharT>& stm)->std::vector<std::basic_string<CharT>>;

#ifdef WIN32
template <typename Facet>
struct deletable_facet : Facet {
	template<typename ...Args>
	deletable_facet(Args&& ...args) : Facet(std::forward<Args>(args) ...) {}
	~deletable_facet() {}
};
#endif

template<typename C>
void processString(std::basic_string<C> str);
template<typename C>
auto getDelim(C c, C cn = static_cast<C>(0)) -> std::optional<DelimType>;


int main(int argc, char* argv[])
{
	cxxopts::Options options("MDParseMan", "Markdown parsing library and testing");
	options.add_options()
		("j, json", "JSON filepath to open", cxxopts::value<std::string>())
		("s, json-section", "A token value to search for under the JSON key 'section'", cxxopts::value<std::string>())
		("o, other", "Some random file to open to test stuff", cxxopts::value<std::string>()->default_value("a_file.txt"))
		("use-other-file", "Use a random text file and not a json file", cxxopts::value<bool>()->default_value("false"))
		("test-case-mode-only", "Do testing only and test against filename fiven by -t", cxxopts::value<bool>()->default_value("false"))
		("t, test-file", "Test with an existing AST represented by a file", cxxopts::value<std::string>()->default_value(""))
		("m, markdown-file", "Open a markdown-format text file for testing", cxxopts::value<std::string>()->default_value(""))
		;

	auto argValues = options.parse(argc, argv);
	if (argValues["json"].count() == 0) {
		std::cerr << "Oh no...\n";
	}
	std::ifstream streamie{};
	std::string buffo{};
	std::vector<MDPMNode> what;
	if (argValues["test-file"].as<std::string>() != "") {
		streamie.open(argValues["test-file"].as<std::string>());
		if (!streamie) {
			return 1;
		}
		std::string error{};
		bool successful;
		what = testCases(streamie, successful, error, buffo);
		if (!successful) {
			std::cerr << error << "\n";
			return 1;
		}
		if (argValues["test-case-mode-only"].as<bool>()) {
			return 0;
		}
	}
	else {
		if (argValues["test-case-mode-only"].as<bool>()) {
			std::cerr << "\"test-case-mode\" is set, but no file specified. Exit\n";
			return 1;
		}
		what.push_back({ MDPMNode::type_e::Document, true, static_cast<size_t>(-1), {}, nullptr, {} });
	}
	bool isOtherFile{};
	std::vector<std::string> strings;
	if (argValues["use-other-file"].as<bool>()) {
		streamie.open(argValues["other"].as<std::string>());
		if (!streamie)
			return 1;
		isOtherFile = true;
		strings = getTestCases(streamie, "", true, 40);
	}
	else if(argValues["json"].count() != 0 && !argValues["json"].as<std::string>().empty()) {
		streamie.open(argValues["json"].as<std::string>());
		if (!streamie)
			return 1;
		isOtherFile = false;
		strings = getTestCases(streamie, argValues["json-section"].as<std::string>(), false, 40);
	}
	std::vector<std::string> mdStrings{};
	if (!argValues["markdown-file"].as<std::string>().empty() && argValues["json"].count() == 0/*argValues["json"].as<std::string>().empty()*/) {
		std::ifstream stormie{};
		if (streamie.is_open()) {
			streamie.close();
		}
		streamie.open(argValues["markdown-file"].as<std::string>());
		if (!streamie) {
			return 1;
		}
		mdStrings = getMarkdown(streamie, true);
	}

#if 0
	std::vector<std::string> lines{};
	for (std::string line{}; std::getline(streamie, line);) {
		lines.emplace_back(line);
	}
	std::string fullString{};
	for (const auto& line : lines) {
		fullString += line + "\n";
	}
#endif

#ifdef WIN32

	std::wstring_convert<deletable_facet<std::codecvt<char16_t, char, std::mbstate_t>>, char16_t> conv16;
	//_setmode(_fileno(stdout), _O_U16TEXT);
#endif
	MDPMNode node{};
	mdpm_parser::Context something{};
	something.freshestBlock = &what.back();
	something.textBuffer.reserve(1024);
	if (argValues["json"].count() != 0 && !argValues["json"].as<std::string>().empty()) {
		something.freshestBlock = &node;
		for (const auto& item : strings) {
#ifdef WIN32
			u16string result = conv16.from_bytes(item.c_str());
			wstring_view newresult{ reinterpret_cast<const wchar_t*>(result.c_str()) };
			const char16_t* str = result.c_str();
			wcout << newresult << L"\n";
#else
			cout << item << "\n";
#endif
			mdpm_parser::processLine(something, node, item);
			//processString(item);
		}
	}
	else {
		if (isOtherFile) {
			for (const auto& item : strings) {
#ifdef WIN32
				u16string result = conv16.from_bytes(item.c_str());
				wstring_view uniStr{ (reinterpret_cast<const wchar_t*>(result.c_str())) };
				wcout << uniStr << L"\n";
#else
				cout << item << "\n";
#endif
			}
		}
		else if (!argValues["markdown-file"].as<std::string>().empty()) {
			for (const auto& element : mdStrings) {
				auto& statey = what.back();
				mdpm_parser::processLine(something, what.back(), element);
			}
		}
	}
	return 0;
}

std::vector<std::string> getTestCases(std::istream& strime, const std::string& section, bool useOther, int n)
{
	std::vector<std::string> result{};
	if (!useOther) {
		nlohmann::json jc;
		strime >> jc;
		auto pos = std::find_if(jc.begin(), jc.end(), [&section](const nlohmann::json& v) {return v["section"] == section; });
		while ((*pos)["section"] == section and (n > 0)) {
			result.push_back((*pos)["markdown"]);
			++pos;
			--n;
		}
	}
	else {
		for (std::string line{}; std::getline(strime, line);) {
			result.emplace_back(line);
		}
	}
	return result;
}

template <typename C>
void processString(std::basic_string<C> str) {
	using character_t = C;
	std::basic_istringstream<C> stream{ str };
	std::list<text_info<character_t>> delimStack{};

	for (character_t sym; stream.get(sym); ) {
		std::optional<DelimType> result;
		if (sym == static_cast<character_t>('!')) {
			character_t sym2;
			if (not stream.get(sym2)) {
				break;
			}
			if (not (result = getDelim(sym, sym2))) {
				return;
			}
			else {
				std::basic_string<character_t> literals{};
				literals.push_back(sym);
				literals.push_back(sym2);
				delimStack.push_back({ DelimType::ExclamBracket, 1, true, CloserType::PotOpener, literals });
			}
		}
		else if (!(result = getDelim(sym))) {
			return;
		}
		else {
			std::basic_string<character_t> literals{};
		}
	}
}

#if 1
template<typename C>
std::optional<DelimType> getDelim(C c, C cn ) {
	switch (c) {
	case '[': return DelimType::SBracket;
		break;
	case '!': if (cn == static_cast<C>('[')) return DelimType::ExclamBracket;
		break;
	case '*': return DelimType::Star;
		break;
	case '_': return DelimType::Underline;
		break;
	default: return {};
		   break;
	}
	return {};
}
#endif

template<typename CharT, typename std::basic_string<CharT>::size_type BufLen>
auto getMarkdown(std::basic_istream<CharT>& stm, bool makeSeparateLines) -> std::vector<std::basic_string<CharT>> {
	std::vector<std::basic_string<CharT>> resi{};
	if (makeSeparateLines) {
		resi = getMarkdownLines(stm);
	}
	else {
		CharT buffer[BufLen];
		std::basic_string<CharT> outStr{};
		while (stm.read(buffer, BufLen)) {
			outStr.append(buffer, BufLen);
		}
		outStr.append(buffer, stm.gcount());
		resi.push_back(outStr);
	}
	return resi;
}

template<typename CharT>
auto getMarkdownLines(std::basic_istream<CharT>& stm) -> std::vector<std::basic_string<CharT>> {
	using stringi_t = std::basic_string<CharT>;

	std::vector<stringi_t> outputLines{};

	stringi_t line{};
	while (std::getline(stm, line)) {
		outputLines.push_back(line);
	}

	return outputLines;
}


