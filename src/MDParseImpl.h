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

#ifndef MD_PARSE_IMPL_H
#define MD_PARSE_IMPL_H
#include <string>
#include <optional>
#include <tuple>
//#define TAO_PEGTL_NAMESPACE mdlang::pegtl
#include <tao/pegtl/memory_input.hpp>
#include "md_parseman/BlockInfoTags.h"
#include "md_parseman/IntegralTypes.h"

struct IndentInformation {
	int whitespaces;
	int indentSpaces;
	char excessIndentation;
	bool isNonblank;
};

struct WhitespaceParseResult{
	uint32_t indentConsumptionCost;
	uint32_t charTCost;
	uint32_t excessTabPadding;
};

namespace mdpm_impl {
	namespace peggi = TAO_PEGTL_NAMESPACE;

	bool tryThematicBreak(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept;

	auto tryATXHeading(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept ->Heading;

	auto tryIndentedCode(const bool isContPrefix,
		peggi::memory_input<peggi::tracking_mode::eager>& input,
		const md_parseman::UInt absoluteLogicalIndent, 
		const md_parseman::UInt indentDeficit) noexcept ->std::tuple<md_parseman::UInt, md_parseman::UInt, char, bool, bool>;

	constexpr md_parseman::Int MIN_FENCE_SIZE = 3;
	auto tryFencedCodeOpener(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept ->FencedCode;
	auto tryFencedCodeCloser(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept ->FencedCode;

	auto tryListItem(peggi::memory_input<peggi::tracking_mode::eager>& input,
		const size_t absoluteLogicalIndent,
		const size_t indentDeficit) -> std::optional<std::tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>>;

	auto tryListItem0(peggi::memory_input<peggi::tracking_mode::eager>& input,
		const size_t absoluteLogicalIndent,
		const md_parseman::UTinyInt indentDeficit) noexcept ->std::optional<std::tuple<ListInfo, ListItemInfo, WhitespaceParseResult, bool>>;

	char trySetexHeading(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept;

	bool tryBlockQuote(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept;
	auto getTextData(peggi::memory_input<peggi::tracking_mode::eager>& input, const bool trimTrailingWs) noexcept->std::pair<const char*, size_t>;
	bool tryNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input);
	bool tryNonBlank0(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept;

	//auto countWhitespaces(peggi::memory_input<peggi::tracking_mode::eager>& input, md_parseman::UInt absoluteLogicalSpaces) -> std::tuple<bool, md_parseman::UInt, md_parseman::UInt>;
	auto countWhitespaces0(peggi::memory_input<peggi::tracking_mode::eager>& input, const int sizeN, const md_parseman::UInt absoluteLogicalSpaces, const md_parseman::UTinyInt deficit) noexcept ->IndentInformation;

	auto countWhitespaces1(peggi::memory_input<peggi::tracking_mode::eager>& input, const int sizeN, const md_parseman::UInt absoluteLogicalSpaces, const md_parseman::UTinyInt deficit) noexcept -> IndentInformation;

	size_t checkNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept;
}
#endif
