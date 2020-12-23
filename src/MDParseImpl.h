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
