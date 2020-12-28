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

namespace mdpm_impl {
	namespace peggi = TAO_PEGTL_NAMESPACE;
	auto tryIndentedCode(const bool isContPrefix,
		peggi::memory_input<peggi::tracking_mode::eager>& input,
		const size_t absoluteLogicalIndent, 
		const size_t indentDeficit)->std::tuple<size_t, size_t, char, bool, bool>;

	auto tryListItem(peggi::memory_input<peggi::tracking_mode::eager>& input,
		const size_t absoluteLogicalIndent,
		const size_t indentDeficit) -> std::optional<std::tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>>;

	bool tryBlockQuote(peggi::memory_input<peggi::tracking_mode::eager>& input);
	auto getTextData(peggi::memory_input<peggi::tracking_mode::eager>& input)->std::pair<const char*, size_t>;
	bool tryNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input);
	auto countWhitespaces(peggi::memory_input<peggi::tracking_mode::eager>& input, size_t absoluteLogicalSpaces) -> std::tuple<bool, size_t, size_t>;
	size_t checkNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input);
	bool consumeResidualWsNl(peggi::memory_input<peggi::tracking_mode::eager>& input);

#if 0
	class persistent_mem_input {
		struct reader_impl;
	public:
		persistent_mem_input(const char* begin, const char* end);
		persistent_mem_input(persistent_mem_input&) = delete;
		persistent_mem_input(persistent_mem_input&&) = delete;
		reader_impl* holder_;
	};
#endif
}



#endif
