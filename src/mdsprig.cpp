// MDParseMan.cpp : Defines the entry point for the application.
//

#include <cxxopts.hpp>
//#include <nlohmann/json.hpp>
#include <variant>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <locale>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif
//#include <optional>

#include "md_parseman/MDParseMan.h"

//using namespace std;
std::vector<MDPMNode> testCases(std::istream& stm, bool& success, std::string& error, std::string& buffer);

auto getTestCases(std::istream& strime, const std::string& section, bool useOther, int n) -> std::vector<std::string>;
template<typename CharT, typename std::basic_string<CharT>::size_type BufLen=32>
auto getMarkdown(std::basic_istream<CharT>& stm, bool makeSeparateLines)->std::variant<std::monostate, std::vector<std::basic_string<CharT>>, std::basic_string<CharT>>;
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
		("s, test-string", "Use a string argument---surrounded in qotation---as input for parser. Other options will be ignored if supplied.", cxxopts::value<std::string>())
		("f, markdown-file", "Open a markdown-format text file for testing. If no file or string arguments are given, user input will be used.", cxxopts::value<std::string>()->default_value(""))
		("separate-lines", "Split text lines into a collection of separate strings. Ignored if no file is given", cxxopts::value<bool>()->default_value("false"))
		;

	auto argValues = options.parse(argc, argv);

	std::ifstream streamie{};
	std::vector<MDPMNode> what;

	bool isOtherFile{};
	std::string markdownInput;

	if (!argValues["markdown-file"].as<std::string>().empty()) {
		std::ifstream stormie{};
		if (streamie.is_open()) {
			streamie.close();
		}
		streamie.open(argValues["markdown-file"].as<std::string>());
		if (!streamie) {
			return 1;
		}
		if (!argValues["separate-lines"].as<bool>()) {
			markdownInput = std::get<std::string>(getMarkdown(streamie, false));
			streamie.close();
		}
	}

	md_parseman::Parser parsey{};
	MDPMNode node{};

	// hello commenti

	if (argValues["test-string"].count() != 0) {
		markdownInput = argValues["test-string"].as<std::string>();
		md_parseman::Parser parsi{ markdownInput.c_str(), markdownInput.c_str() + markdownInput.size() };
		bool success = true;
		while (success) {
			success = parsi.processLine();
		}
	}
	else if (!argValues["markdown-file"].as<std::string>().empty() and argValues["separate-lines"].as<bool>()) {
		bool success{ true };
		while (success) {
			success = parsey.processLine(streamie);
		}
#if 0
		for (const auto& element : mdStrings) {
			auto& statey = what.back();
			md_parseman::processLine(something, what.back(), element);
		}
#endif
	}
	else if (!argValues["markdown-file"].as<std::string>().empty()) {
		md_parseman::Parser parsi{ markdownInput.c_str(), markdownInput.c_str() + markdownInput.size() };
		bool success = true;
		while (success) {
			success =  parsi.processLine();
		}
		bool res = md_parseman::htmlExport(parsi, std::cout);
		constexpr auto sizezi = sizeof(MDPMNode::public_iterator);
		//for (auto it : parsi ) { //= parsi.begin(), end = parsi.end(); it != end; ++it) {
			//processBlock(it, docStack);
			//const auto& el = *it;
			//int i = 0;
		//}
	}
	else {
		bool success = true;
		while (success) {
			success = parsey.processLine(std::cin);
		}
	}
#if 0
	else if (bool useFile; (useFile = argValues["test-string"].count() != 0) or (argValues["markdown-file"].count() != 0 and !argValues["separate-lines"].as<bool>())) {
		std::string markdownInput;
		if (useFile) {
		}
	}
#endif
	return 0;
}

std::vector<std::string> getTestCases(std::istream& strime, const std::string& section, bool useOther, int n)
{
	std::vector<std::string> result{};
	if (!useOther) {
		//nlohmann::json jc;
		//strime >> jc;
		//auto pos = std::find_if(jc.begin(), jc.end(), [&section](const nlohmann::json& v) {return v["section"] == section; });
		//while ((*pos)["section"] == section && (n > 0)) {
		//	result.push_back((*pos)["markdown"]);
		//	++pos;
		//	--n;
		//}
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
auto getMarkdown(std::basic_istream<CharT>& stm, bool makeSeparateLines)->std::variant < std::monostate, std::vector<std::basic_string<CharT>>, std::basic_string<CharT>> {
	if (makeSeparateLines) {
		std::vector<std::basic_string<CharT>> resi{};
		resi = getMarkdownLines(stm);
		return resi;
	}
	else {
		CharT buffer[BufLen];
		std::basic_string<CharT> outStr{};
		while (stm.read(buffer, BufLen)) {
			outStr.append(buffer, BufLen);
		}
		outStr.append(buffer, stm.gcount());
		return outStr;
	}
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


