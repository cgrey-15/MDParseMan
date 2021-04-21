/*
MIT License

Copyright (c) 2020 Christian Greyeyes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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

#include <CLI/CLI.hpp>
#include "md_parseman/MDParseMan.h"

template<typename CharT, typename std::basic_string<CharT>::size_type BufLen=32>
auto getMarkdown(std::basic_istream<CharT>& stm, bool makeSeparateLines)->std::variant<std::monostate, std::vector<std::basic_string<CharT>>, std::basic_string<CharT>>;
template<typename CharT>
auto getMarkdownLines(std::basic_istream<CharT>& stm)->std::vector<std::basic_string<CharT>>;

namespace mdsprig {
	enum class in_type : uint8_t {
		File, String, StdCIn
	};
	struct CmdArgInfo {
		in_type inSource;
		bool toFile;
		bool toPrependMd;
	};
}

void parseArgs(const CLI::App& argProcessor, mdsprig::CmdArgInfo& res);

#ifdef WIN32
template <typename Facet>
struct deletable_facet : Facet {
	template<typename ...Args>
	deletable_facet(Args&& ...args) : Facet(std::forward<Args>(args) ...) {}
	~deletable_facet() {}
};
#endif

void revitalizeViews(MDPMNode& node, const std::string& buffer);
void configureParser(CLI::App& cmdArgParser, mdsprig::CmdArgInfo& argInfo, std::string& dst);

std::string correctUserFilename(const std::string& rawStr, const bool useOutExtension=false) noexcept;

int main(int argc, char* argv[])
{
	std::string multPurposeStr;

	mdsprig::CmdArgInfo cmdArgResult{};

	CLI::App argProcessor{ "A basic markdown-to-html converter. Currently incomplete. HTML scrubbing is not supported.", "msdprig" };

	configureParser(argProcessor, cmdArgResult, multPurposeStr);

	CLI11_PARSE(argProcessor, argc, argv);

	parseArgs(argProcessor, cmdArgResult);

	std::ifstream streamie{};

	md_parseman::Parser parsey{};

	if (cmdArgResult.inSource == mdsprig::in_type::File){
		if (streamie.is_open()) {
			streamie.close();
		}
		std::vector<std::string> extraArgs = argProcessor.remaining();

		int failTally = 0;
		bool singleFile = extraArgs.size() == 1;

		std::string outputName;
		std::optional<std::ofstream> outFileOptional;
		if (cmdArgResult.toFile) {
			outputName = multPurposeStr;
			outFileOptional.emplace(outputName);
		}
		for (const auto& inFilename : extraArgs) {
			md_parseman::Parser parsey{};
			streamie.open(inFilename);
			if (!streamie) {
				std::cerr << "file not found; skipping " << inFilename << "\n";
				++failTally;
				continue;
			}
			if(!cmdArgResult.toFile) {
				if (cmdArgResult.toPrependMd) {
					outputName = correctUserFilename(inFilename, true);
					outFileOptional.emplace(outputName);
				}
				else {
					outputName = correctUserFilename(inFilename, false);
				}
			}

			std::ofstream result{};
			if (cmdArgResult.toPrependMd) {
				*outFileOptional << inFilename << ":\n";
				for (std::string inFileBuffer; streamie; std::getline(streamie, inFileBuffer)) {
					*outFileOptional << inFileBuffer << "\n";
					inFileBuffer.push_back('\n');
					parsey.processLine(inFileBuffer.data(), inFileBuffer.size());
				}
				*outFileOptional << "\n";
			}
			else {
				bool success{ true };
				while (success) {
					success = parsey.processLine(streamie);
				}
				if (!cmdArgResult.toFile) {
					outFileOptional.emplace(outputName);
				}
			}
#if 1
			if (!singleFile and !cmdArgResult.toPrependMd) {
				*outFileOptional << inFilename << ":\n";
			}
#endif
			if (!md_parseman::htmlExport(parsey, *outFileOptional)) {
				return 1;
			}
			if (!cmdArgResult.toFile) {
				outFileOptional.reset();
			}
			streamie.close();
		}
	}
	else if (cmdArgResult.inSource == mdsprig::in_type::StdCIn) {
		std::optional<std::ofstream> maybeFile;
		std::string maybeOutFilename;

		std::ostream* outStream = &std::cout;

		if (cmdArgResult.toFile and cmdArgResult.toPrependMd) {
			maybeFile.emplace(multPurposeStr);//maybeOutFilename);
			outStream = &*maybeFile;

			for (std::string inFileBuffer; std::cin; std::getline(std::cin, inFileBuffer) ) {
				*outStream << inFileBuffer << "\n";
				parsey.processLine(inFileBuffer.data(), inFileBuffer.size());
			}
			*outStream << "\n";
		}
		else {
			bool success = true;
			while (success) {
				success = parsey.processLine(std::cin);
			}
			if (cmdArgResult.toFile) {
				maybeOutFilename = correctUserFilename(multPurposeStr);
				maybeFile.emplace(maybeOutFilename);
				outStream = &*maybeFile;
			}
		}
		parsey.finalizeDocument();
		bool res = md_parseman::htmlExport(parsey, *outStream);
	}
	return 0;
}

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

void revitalizeViews(MDPMNode& root, const std::string& buffer) {
	if (!root.body.empty()) {
		root.body = std::string_view(&buffer[root.posptr], root.body.size());
	}

	for (auto& child : root.children) {
		child.parent = &root;
		revitalizeViews(child, buffer);
	}
}

void parseArgs(const CLI::App& argProcessor, mdsprig::CmdArgInfo& res) {
	const auto options = argProcessor.get_options();

	std::vector<std::vector<std::string>> pop{};

	if (argProcessor.remaining_size() != 0) {
		res.inSource = mdsprig::in_type::File;
	}
	else {
		res.inSource = mdsprig::in_type::StdCIn;
	}

	if (!options[1]->empty()) {
		res.toFile = true;
		if (!options[2]->empty()) {
			res.toPrependMd = true;
		}
	}
}

void configureParser(CLI::App& cmdArgParser, mdsprig::CmdArgInfo& argInfo, std::string& outFilename) {
	cmdArgParser.allow_extras();
	//cmdArgParser.add_option("-f, --markdown-file", outFilename, "Parse a markdown document file.");
	auto oOpt = cmdArgParser.add_option("-o, --output", outFilename, "Output file as this filename");
	cmdArgParser.add_flag("-p, --prepend-original", argInfo.toPrependMd, "Output both markdown and html into a file; -o required");// ->needs(oOpt);
}

std::string correctUserFilename(const std::string& rawStr, const bool useOutExtension) noexcept {
	size_t pos = std::min(rawStr.rfind('.'), rawStr.size());
	std::string correctedStr{ rawStr, 0, pos };
	if (!useOutExtension) {
		if (rawStr.compare(pos, 5, ".html") != 0) {
			correctedStr.append(".html");
		}
		else {
			correctedStr.append("COPY.html");
		}
	}
	else {
		correctedStr.append(".out");
	}
	return correctedStr;
}
