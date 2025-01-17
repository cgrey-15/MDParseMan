#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <map>
#include <tuple>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <string>
#include "StringParts.h"

std::string outResult(nlohmann::json& jc, int indentLvl);
std::string func(nlohmann::json& jc, int indentLvl);
std::string body_(nlohmann::json::const_iterator it, size_t n, int indentLvl);
std::map<nlohmann::json::iterator, size_t> organizeJson(nlohmann::json& root);

template<char OldVal, char NewVal, bool NeedEscape>
void escapeBackslashString(std::string& str);
void removeChar(std::string& str, char c);

std::string getPrettyStrLiteral(const std::string& src, const size_t preIndent, const size_t padding);

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "INVALID args\n";
		return 1;
	}
	nlohmann::json jcee{};
	std::fstream fileIo{ argv[1], std::fstream::in };
	if (!fileIo) {
		std::cerr << "File not found\n";
		return 1;
	}
	fileIo >> jcee;
	for (auto& el : jcee) {
		auto& arg = el["markdown"].get_ref<std::string&>();
		escapeBackslashString<'\\', '\\', true>(arg);
		escapeBackslashString<'\n', 'n', true>( arg );
		escapeBackslashString<'\t', 't', true>(arg);
		escapeBackslashString<'"', '"', true>(arg);
		auto& arg2 = el["html"].get_ref<std::string&>();
		escapeBackslashString<'\\', '\\', true>(arg2);
		escapeBackslashString<'\n', 'n', true>( arg2 );
		escapeBackslashString<'\t', 't', true>(arg2);
		escapeBackslashString<'"', '"', true>(arg2);
	}

	fileIo.close();

	//auto mappings = organizeJson(jcee);
	fileIo.open("MarkdownTestGenerated.cpp", std::fstream::out);
	fileIo << outResult(jcee, 0);
	return 0;
}

std::string outResult(nlohmann::json& jc, int indentLvl) {
	std::string res{};
	return std::string{ includes } + anonNamespaceBegin + func(jc, indentLvl+1) + anonNamespaceEnd;
}

std::string func(nlohmann::json& jc, int indentLvl) {
	std::string testName = "MDPMTest";
	std::string subTestName = "HiFunc";
	auto groups = organizeJson(jc);
	//std::cout << "groups size: " << groups.size() << "\n";
	std::string res{};
	int i{};
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		//std::cout << "??" << i++ << '\n';
		//std::cerr << "!!! " << it->second << "\n";
		removeChar((*it->first)["section"].get_ref<std::string&>(), ' ');
		res.append(fmt::format("{0: <{3}}TEST({1}, {2}) {{\n{4}\n{0: <{3}}}}\n", "", testName, (*it->first)["section"], indentLvl * 4, body_(it->first, it->second, indentLvl + 1)));
		using what_t = decltype(*it->first);
	}
	return res;
}

std::string body_(nlohmann::json::const_iterator it, size_t n, int indentLvl) {
	std::string res{};
	for (size_t i = 0; i < n; ++i) {

		constexpr int strASize = std::string_view{"auto strXXXX = md_parseman::mdToHtml("}.size();
		int preIndentA = (indentLvl * 4) + strASize;
		int paddingA = 0;
		while ((preIndentA + paddingA) % 4 != 3) { ++paddingA; }
		std::string finalMarkdownStr = getPrettyStrLiteral((*it)["markdown"].get_ref<const std::string&>(), preIndentA + paddingA, /*paddingA*/0);

		constexpr int strBSize = std::string_view{"EXPECT_STREQ(strXXXX.c_str(), "}.size();
		int preIndentB = (indentLvl * 4) + strBSize;
		int paddingB = 0;
		while ((preIndentB + paddingB) % 4 != 3) { ++paddingB; }
		std::string finalMarkdownStrB = getPrettyStrLiteral((*it)["html"].get_ref<const std::string&>(), preIndentB + paddingB, /*paddingB*/0);

		res.append(fmt::format("{0: <{4}}auto str{3:0>4d} = md_parseman::mdToHtml({0: >{5}}\"{1}\");\n\n"
			"{0: <{4}}EXPECT_STREQ(str{3:0>4d}.c_str(), {0: >{6}}\"{2}\");\n\n", "", finalMarkdownStr, finalMarkdownStrB, (*it)["example"].get<unsigned int>(), indentLvl * 4, paddingA, paddingB));
		++it;
	}
	return res;
#if 0
	return fmt::format("{0: <{2}}auto str = mdToHtml(\"{1}\");\n"
		"{0: <{2}}EXPECT_EQ(0, std::strcmp( \"\", str.c_str() ));", "", "This is test markdown", indentLvl * 4);//fmt::format("{0: <{1}}// hello-world...\n", "", indentLvl*4);
#endif
}

std::map<nlohmann::json::iterator, size_t> organizeJson(nlohmann::json& root) {
	std::map<nlohmann::json::iterator, size_t> res{};
	std::map<std::string, nlohmann::json::iterator> lookup{};
	std::string prevSection = "";
	int i{};
	for (auto it = root.begin(); it != root.end(); ++it) {
		if (lookup.find((*it)["section"].get_ref<std::string&>()) != lookup.end() ) {
			using what_t = decltype(res.find(it));
			res[lookup[(*it)["section"].get_ref<std::string&>()]]++;
		}
		else {
			lookup.emplace(std::make_pair((*it)["section"].get<std::string>(), it));
			res.emplace( std::make_pair(it, 1ull) );
		}
		++i;
	}
	return res;
}

template<char OldVal, char NewVal, bool NeedEscape=false>
void escapeBackslashString(std::string& str) {
	for (size_t i = 0; i < str.size(); ++i) {
		if (i = str.find(OldVal, i); i != std::string::npos) {
			str[i] = NewVal;
			if (NeedEscape) {
				str.insert(str.begin() + i, '\\');
			}
			++i;
		}
		else {
			break;
		}
	}
}
void removeChar(std::string& str, char c) {
	for (size_t i = 0; i < str.size(); ++i) {
		if (i = str.find(c, i); i != std::string::npos) {
			str.erase(i, 1);
			--i;
		}
		else {
			break;
		}
	}
}

std::string getPrettyStrLiteral(const std::string& src, const size_t preIndent, const size_t padding) {
	std::string finalMarkdownStr{};
	std::string skippedStr{};
	bool consumedFirstNewline = false;
	for (size_t j = src.find('\\'), i = static_cast<size_t>(0); j != std::string::npos; i = j + 2, j = src.find('\\', j + 2)) {

		if (src[j + 1] != 'n') {
			skippedStr.append(src.cbegin() + i, src.cbegin() + j + 2);
		}
		else {
			if (consumedFirstNewline) {
				finalMarkdownStr.append(preIndent + padding, ' ');
				finalMarkdownStr.push_back('"');
			}
			else {
				finalMarkdownStr.append(padding, ' ');
			}
			consumedFirstNewline = true;
			finalMarkdownStr.append(skippedStr);
			skippedStr.clear();
			finalMarkdownStr.append(src.cbegin() + i, src.cbegin() + j + 2);
			if (src.cbegin() + j + 2 != src.cend()) {
				finalMarkdownStr.append("\"\n");
			}
			else {
				//finalMarkdownStr.append("\"");
			}
		}
	}
	return finalMarkdownStr;
}
