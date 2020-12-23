#include <string>
#include <tuple>
#include <cstdint>
#include <algorithm>
#include "MDParseImpl.h"
#include "md_parseman/BlockInfoTags.h"
//#define TAO_PEGTL_NAMESPACE mdlang::pegtl

#include "tao/pegtl/contrib/trace.hpp"
#include "tao/pegtl.hpp"

namespace mdlang {
	enum class gm : int8_t {
		nil = 0,
		atx_sfx = 20,
		atx_mid = 44
	};
	using namespace TAO_PEGTL_NAMESPACE;//pegtl;
	template <uint8_t ... CharTs>
	struct content : seq<plus<not_one<' ', '\t', CharTs...>>, star<seq<plus<space>, plus<not_one<' ', '\t', CharTs...>>>>> {};

	struct pound : one<'#'> {};
	struct atx_grammar_prefix : seq<rep_max<3, space>, plus<pound>> {};
	struct atx_grammar_suffix : seq<plus<space>, opt< seq<plus<pound>, star<space>> >> {};//, pegtl::star<ws>> {};
	struct atx_mid : seq<plus<space>, opt<content<'#'>>> {};
	struct atx_line : seq < atx_grammar_prefix, opt <atx_mid>, opt<atx_grammar_suffix>, eof>{};

	template <typename Rule>
	struct atx_dbg_action : nothing<Rule> {};

	template<>
	struct atx_dbg_action<atx_grammar_suffix> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, gm (&arg0)[4], std::pair<size_t, size_t>& v) {
			arg0[0] = gm::atx_sfx;
		}
	};
	template<>
	struct atx_dbg_action<atx_mid> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, gm (&arg0)[4], std::pair<size_t, size_t>& v) {
			arg0[1] = gm::atx_mid;
		}
	};
	template<>
	struct atx_dbg_action<content<'#'>> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, gm (&arg0)[4], std::pair<size_t, size_t>& arg1) {
			arg1.first = in.position().byte;
			arg1.second = in.size();
		}
	};



	//template<>
	//struct list_item_action<
	template <typename... Rules>
	struct my_at : my_at<seq<Rules...>> {};
	template <>
	struct my_at<> : TAO_PEGTL_NAMESPACE::success {};
	template <typename Rule>
	struct my_at<Rule> {
		using rule_t = my_at;
		using subs_t = TAO_PEGTL_NAMESPACE::type_list<Rule>;

		template< apply_mode A,
			      rewind_mode,
			      template<typename...>
		          class Action,
				  template<typename...>
				  class Control,
				  typename ParseInput,
				  typename... States >
		[[nodiscard]] static bool match(ParseInput& in, States&&... st) 
		{
			const auto m = in.template mark< rewind_mode::required >();
			return Control<Rule>::template match<A, rewind_mode::active, Action, Control >(in, st ...);
		}
	};

	struct non_space : not_one<' ', '\n', '\r', '\t', '\v', '\f'> {};





	struct whitespaces : star<one<' ', '\t'>>{};
	struct non_blank_line : seq<whitespaces, at<non_space>> {};
	struct non_blank_0 : my_at<whitespaces, non_space> {};
	struct residualWsNl : seq<whitespaces, eolf> {};

	template <typename Rule>
	struct non_blank_line_action : nothing<Rule> {};
	template <>
	struct non_blank_line_action<whitespaces> : require_apply {
		template<typename ParseInput, typename PtrDiffT>
		static void apply(const ParseInput& in, PtrDiffT& nonWsPos) {
			nonWsPos = in.end() - in.begin();
		}
	};

	template <typename Rule>
	struct misc_action : nothing<Rule> {};
#if 0
	template <>
	struct misc_action<non_space> : require_apply {
		template<typename ParseInput>
		static void apply(ParseInput& in, size_t& pos, size_t&) {
			pos = in.begin() - in.input().begin();
		}
	};
#endif
	template<>
	struct misc_action<one<' ', '\t'>> : require_apply {
		template<typename ParseInput>
		static void apply(ParseInput& in, size_t& destBytes, size_t& destColumns, size_t& absoluteLogicalIndentationAcc) {
			constexpr size_t TAB_LENGTH = 4;
			size_t offset = 0;
			switch (*in.begin()) {
			case ' ': offset = 1;
				break;
			case '\t': offset = TAB_LENGTH - (absoluteLogicalIndentationAcc % TAB_LENGTH);
				break;
			}
			destColumns += offset;
			absoluteLogicalIndentationAcc += offset;
			++destBytes;
		}
	};



	template<typename Rule>
	struct ws_action : nothing<Rule> {};
	template<>
	struct ws_action<whitespaces> {//pegtl::change_control<my_first_control> {//pegtl::require_apply {
#if 1
		template<typename ParseInput>
		static void apply(const ParseInput& in, int& a, unsigned char& b) {
			a = -1;
			b = 32;
		}
#endif

	};

	struct block_quote_prefix : seq<rep_max<3, one<' '>>, one<'>'>, opt<one<' '>>> {};

	struct text_contents : until<eolf> {};


}
#if 1
namespace TAO_PEGTL_NAMESPACE::internal {
	template<typename... Rules>
	inline constexpr bool enable_control< mdlang::my_at<Rules...> > = false;
}
#endif
namespace pegtl = TAO_PEGTL_NAMESPACE;//mdlang::pegtl;

#ifdef WIN32
using uni_char_t = uint8_t;
#elif defined __cpp_lib_char8_t
using uni_char_t = char8_t;
#else
using uni_char_t = uint8_t
#endif

inline size_t tabFillSpace(size_t logicalSpacesOffset) {
	constexpr size_t TAB_STOP = 4;
	return TAB_STOP - (logicalSpacesOffset % TAB_STOP);
}

//struct IndentInfo;
struct IndentInfo {
	uint8_t spaceDeficit;
	uint8_t absoluteSlotPos;
	uint8_t spacesIndented;
	uint8_t spacesIndentedToSkip;
	size_t bytesToConsume;
};
struct IndCodeParResult {
	bool indentMatched;
	uint32_t charTsCounted;
	uint32_t indentsCounted;
};

namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;
	struct ind_code_line : seq< sor<seq<rep_opt<3, one<' '>>, one<'\t'>>, rep<4, one<' '>> >, at<star<blank>, non_space>> {};
	struct ind_code_endl : at<star<blank>, non_space> {};
	struct ind_code_indent : 
		my_at<
			seq <
				sor<
					seq<
						//rep_max<3, one<' '>>, sor<at<eolf>, one<'\t'>>
						rep_max<3, one<' '>>, one<'\t'>, rep_max<3, one<' '>>, sor<at<eolf>, one<'\t'>, not_one<'\t', ' '>>
					>,
					rep<4, one<' '>>
				>,
				star<blank>,
				non_space
			> 
		>
	{};
	struct ind_code_counter_nonblank :
		my_at<plus< sor<plus<one<'\t'>>, plus<one<' '>> > >, non_space> {};

	template <typename Rule>
	struct ind_code_action : nothing<Rule> {};

	template<>
	struct ind_code_action<rep_max<3, one<' '>>> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, IndentInfo& ctx, size_t& spaceCounter, bool& bonk) {
			spaceCounter = in.end() - in.begin();
			uint8_t spacesObtained = in.end() - in.begin();
			uint8_t tabSpaceFiller{};
			if (*in.end() == '\t') {
				tabSpaceFiller = tabFillSpace(ctx.absoluteSlotPos + ctx.spacesIndented + spacesObtained + 4);
			}
			if (ctx.spacesIndented + ctx.spacesIndentedToSkip + ctx.spaceDeficit < 4) {
				if ((ctx.spaceDeficit + ctx.spacesIndentedToSkip + ctx.spacesIndented + spacesObtained ) >= 4) {
					uint8_t excessSpaces = (ctx.spaceDeficit + ctx.spacesIndentedToSkip + ctx.spacesIndented + spacesObtained) - 4;
					uint8_t newSpacesToIndent = spacesObtained - excessSpaces;
					uint8_t newSpacesToRConsume = std::max<int8_t>(static_cast<int8_t>(newSpacesToIndent) - ctx.spaceDeficit, 0);
					uint8_t newSpacesNotConsume = ctx.spaceDeficit;
					ctx.spacesIndented += newSpacesToRConsume; // SHOULD BE OKAY
					ctx.spacesIndentedToSkip += newSpacesNotConsume;
					if (newSpacesToIndent < ctx.spaceDeficit) {
						ctx.spaceDeficit -= newSpacesToIndent;
					}
					else {
						ctx.bytesToConsume += (newSpacesToIndent - ctx.spaceDeficit);
						ctx.spaceDeficit -= std::min(ctx.spaceDeficit, newSpacesToIndent);
					}
				}
				else if ((ctx.spaceDeficit + ctx.spacesIndentedToSkip + ctx.spacesIndented + spacesObtained + tabSpaceFiller) >= 4) {
					// INDENTATION OPERATIOINS
					uint8_t newSpacesToIndent = spacesObtained + tabSpaceFiller;//0;
					uint8_t deficitSpacesToIndent = ctx.spaceDeficit;

					ctx.spacesIndented +=       newSpacesToIndent;
					ctx.spacesIndentedToSkip += deficitSpacesToIndent;

					//uint8_t deficitConsumedSpaces = std::max<int>((static_cast<int>(spacesObtained) - deficitSpacesToIndent), spacesObtained);
					uint8_t deficitConsumedSpaces = std::min(spacesObtained, deficitSpacesToIndent);

					uint8_t tabCount = 0;
					if (*in.end() == '\t') {
						tabCount = 1;
					}
					ctx.bytesToConsume += tabCount + (spacesObtained - deficitConsumedSpaces);

					uint8_t newSpacesToRConsume = std::max<int8_t>(static_cast<int8_t>(newSpacesToIndent) - ctx.spaceDeficit, 0);
					uint8_t newSpacesNotConsume = ctx.spaceDeficit;
					uint8_t extraExcessIndentation = (/*deficitSpacesForIndent*/0 + ctx.spacesIndentedToSkip + ctx.spacesIndented/* + spacesObtained + tabSpaceFiller*/) - 4;
					ctx.spaceDeficit = ctx.spaceDeficit - deficitConsumedSpaces + extraExcessIndentation;
				}
				else {
					uint8_t newSpacesToIndent = spacesObtained + tabSpaceFiller;
					uint8_t deficitSpacesToIndent = ctx.spaceDeficit;

					ctx.spacesIndented += newSpacesToIndent;
					ctx.spacesIndentedToSkip += deficitSpacesToIndent;

					//uint8_t deficitConsumedSpaces = std::max<int>((static_cast<int>(spacesObtained) - deficitSpacesToIndent), spacesObtained);
					uint8_t deficitConsumedSpaces = std::min(spacesObtained, deficitSpacesToIndent);

					uint8_t tabCount = 0;
					if (*in.end() == '\t') {
						tabCount = 1;
					}
					ctx.bytesToConsume += tabCount + (spacesObtained - deficitConsumedSpaces);

					uint8_t newSpacesToRConsume = std::max<int8_t>(static_cast<int8_t>(newSpacesToIndent) - deficitSpacesToIndent, 0);
					uint8_t newSpacesNotConsume = ctx.spaceDeficit;
					//uint8_t extraExcessIndentation = (/*deficitSpacesForIndent*/0 + ctx.spacesIndentedToSkip + ctx.spacesIndented + spacesObtained + tabSpaceFiller) - 4;
					//ctx.spaceDeficit = ctx.spaceDeficit - deficitConsumedSpaces + extraExcessIndentation;
					ctx.spaceDeficit = ctx.spaceDeficit - deficitConsumedSpaces;
				}
			}
		}
	};
	template<>
	struct ind_code_action<rep<4, one<' '>>> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, IndentInfo& inf, size_t& spaceCounter, bool&) {
			spaceCounter = in.end() - in.begin();
			inf.spacesIndented = spaceCounter;
			inf.bytesToConsume = spaceCounter;
		}
	};

	template<typename Rule>
	struct ind_code_action0 : nothing<Rule> {};

	template<>
	struct ind_code_action0<plus<one<'\t'>>> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, IndCodeParResult& res, const size_t lineIndent, uint8_t& preindent) {
			if (res.indentMatched) {
				return;
			}
			int tabCounts = in.end() - in.begin();
			res.indentsCounted += tabFillSpace(lineIndent + res.indentsCounted);
			++res.charTsCounted;
			
			uint8_t redundantSpaces{};
			if ((res.indentsCounted + preindent < 4) and tabCounts > 1) {
				// TODO: verify that last char consumed befor this match-run is a space
				redundantSpaces = preindent;
				preindent = 0;
				res.indentsCounted += tabFillSpace(lineIndent + res.indentsCounted);
				++res.charTsCounted;
			}
			if (res.indentsCounted + preindent >= 4) {
				res.indentMatched = true;
				res.indentsCounted += preindent;
				redundantSpaces += preindent;
				preindent = 0;
			}
		}
	};
	template<>
	struct ind_code_action0<plus<one<' '>>> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, IndCodeParResult& res, const size_t lineIndent, uint8_t& preindent) {
			if (res.indentMatched) {
				return;
			}
			int spaceCounts = in.end() - in.begin();
			int charTForConsume = std::min(spaceCounts, static_cast<int>(4 - (res.indentsCounted + preindent)));

			res.indentsCounted += charTForConsume + preindent;
			preindent = 0;
			res.charTsCounted += charTForConsume;

			if (res.indentsCounted + preindent >= 4) {
				res.indentMatched = true;
			}
		}
	};
}


constexpr auto what = sizeof(IndentInfo);

namespace peggi = TAO_PEGTL_NAMESPACE;
auto mdpm_impl::tryIndentedCode(const bool isContPrefix,
	     peggi::memory_input<peggi::tracking_mode::eager>& input, 
	                                          const size_t absoluteLogicalIndent,
	                                          const size_t spaceDeficit) -> std::tuple<size_t, size_t, char, bool, bool>
{
	constexpr size_t CODE_INDENT_SPACES = 4;
	assert(spaceDeficit <= 4);
	std::tuple<size_t, size_t, char, bool, bool> result = std::make_tuple(0ull, 0ull, '\0', false, true);

	auto& [rByte, rIndent, rDeficit, rMatch, rNonBlankLine] = result;

	size_t unneeded;
	size_t spaceSize = 0;

#if 1
	IndCodeParResult res{};
	uint8_t preindent = static_cast<uint8_t>(spaceDeficit);
	bool matchedAndNonblank = peggi::parse<mdlang::ind_code_counter_nonblank, mdlang::ind_code_action0>(input, res, absoluteLogicalIndent, preindent);

	if (matchedAndNonblank and res.indentMatched) {
		rByte = res.charTsCounted;
		rIndent = res.indentsCounted;
		rDeficit = res.indentsCounted - 4;
		rMatch = true;
		rNonBlankLine = true;
		input.bump(rByte);
	}
	else if (isContPrefix and !mdlang::parse<mdlang::non_blank_0>(input)) {
		rMatch = true;
		rNonBlankLine = false;
		if (res.indentMatched) {
			rByte = res.charTsCounted;
			rIndent = res.indentsCounted;
			input.bump(rByte);
		}
	}
	else {
		rMatch = false;
	}
	return result;

#else
	bool hasTab = *input.current() == '\t';
	IndentInfo indentCtx{ static_cast<uint8_t>(spaceDeficit), static_cast<uint8_t>(absoluteLogicalIndent % 4) };
	bool testVar = peggi::parse<mdlang::ind_code_indent, mdlang::ind_code_action>(input, indentCtx, spaceSize, hasTab);
	if (isContPrefix or testVar) {
		size_t totalSpaces = spaceSize + spaceDeficit;
		if (indentCtx.spacesIndented + indentCtx.spacesIndentedToSkip >= CODE_INDENT_SPACES) {
			rMatch = true;
			rByte = indentCtx.bytesToConsume;
			rIndent = indentCtx.spacesIndented + indentCtx.spacesIndentedToSkip;
			rDeficit = indentCtx.spaceDeficit;
			input.bump(rByte);
		}
		// TODO: outdated code; please inspect (07-11-2020)
		else if (hasTab and(spaceDeficit + spaceSize + tabFillSpace(absoluteLogicalIndent + spaceSize) >= 4)) {
			rMatch = true;
			rByte = spaceSize + 1;
			rIndent = spaceSize + tabFillSpace(absoluteLogicalIndent + spaceSize);
			rDeficit = (spaceDeficit + spaceSize + tabFillSpace(absoluteLogicalIndent + spaceSize)) - CODE_INDENT_SPACES;
			input.bump(rByte);
		}
		else if (!peggi::parse<mdlang::non_blank_0>(input)) {
			rMatch = true;
			rNonBlankLine = false;
		}

		bool matched = hasTab ? totalSpaces + tabFillSpace(absoluteLogicalIndent + spaceSize) >= 4 : totalSpaces <= 4;
	}
#endif
	return result;
	//return { isContPrefix ? peggi::parse<mdlang::ind_code_cont>(input) : peggi::parse<mdlang::ind_code_line>(input), {}, {} };
}

namespace mdlang {
	struct non_blank : not_one<' ', '\t'> {};
	struct pre_indent : rep_min_max<0, 3, blank> {};
	struct post_indent : rep_min_max<1, 4, blank> {};
	struct ul_marker : one<'*', '-', '+'> {};
	struct ol_marker : one<'.', ')'> {};
	struct natural_number : rep_min_max<1, 9, digit> {};
	struct list_item_prefix : seq < pre_indent, sor<seq<natural_number, ol_marker>, ul_marker>, sor<seq<star<blank>, eolf>, seq<post_indent, at<any>>, blank>>{};
	struct list_marker : sor< seq<natural_number, ol_marker>, ul_marker >{};
	struct list_item_prefix0 : my_at <whitespaces, list_marker > {};
	//struct li_prefix_indent_count : my_at < plus<sor<plus<one<'t'>>, plus<one<' '>>>> {};

	template <typename Rule>
	struct list_item_action : nothing<Rule> {};

	template <>
	struct list_item_action<natural_number> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) {
			int i = 0;
			for (auto it = in.begin(), end = in.end(); it != end; ++it) {
				lInfo.orderedStart0[i++] = *it;
			}
			lInfo.orderedStartLen = i;
			lInfo.symbolUsed = static_cast<ListInfo::symbol_e>(*in.end());
			liInfo.sizeW = i + 1;
		}
	};
	template<>
	struct list_item_action< ul_marker > : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) {
			lInfo.symbolUsed = static_cast<ListInfo::symbol_e>(*in.begin());
			liInfo.sizeW = 1;
		}
	};
	template<>
	struct list_item_action< pre_indent > : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) {
			liInfo.preIndent = in.end() - in.begin();
		}
	};
	template<>
	struct list_item_action< post_indent > : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) {
			liInfo.postIndent = in.end() - in.begin();
		}
	};
	template<>
	struct list_item_action<one<' ', '\t'>> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, ListInfo&, ListItemInfo&, size_t& destBytes, size_t& destColumns, size_t& absoluteLogicalIndentationAcc, uint8_t&) {
			constexpr size_t TAB_LENGTH = 4;
			size_t offset = 0;
			switch (*in.begin()) {
			case ' ': offset = 1;
				break;
			case '\t': offset = TAB_LENGTH - (absoluteLogicalIndentationAcc % TAB_LENGTH);
				break;
			}
			destColumns += offset;
			absoluteLogicalIndentationAcc += offset;
			++destBytes;
		}
	};

	template<>
	struct list_item_action<list_marker> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, ListInfo&, ListItemInfo&, size_t&, size_t&, size_t&, uint8_t& postListMarker) {
			postListMarker = *in.end();
		}
	};
}

auto mdpm_impl::tryListItem(peggi::memory_input<peggi::tracking_mode::eager>& input,
	const size_t absoluteLogicalIndent,
	const size_t indentDeficit) -> std::optional<std::tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>> {

	auto parseOutput = std::make_tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>({}, {}, 0, 0, 0, 0);
	std::optional<decltype(parseOutput)> retValue{ std::nullopt };

	//auto [isNonBlank, whitespaceLen, indentLen] = countWhitespaces(input, 0);
	size_t byteLen = 0, indLen = 0, deficit = indentDeficit;
	size_t absoluteLogIndCopy = absoluteLogicalIndent;
	uint8_t c = 0;

	if (peggi::parse<mdlang::list_item_prefix0, mdlang::list_item_action>(input, std::get<0>(parseOutput), std::get<1>(parseOutput), byteLen, indLen, absoluteLogIndCopy, c)) {
		retValue.emplace(std::move(std::get<0>(parseOutput)), std::move(std::get<1>(parseOutput)), byteLen, indLen, deficit, c);
	}
	return retValue;
}

bool mdpm_impl::tryBlockQuote(peggi::memory_input<peggi::tracking_mode::eager>& input) {
	return peggi::parse<mdlang::block_quote_prefix>(input);
}

auto mdpm_impl::getTextData(peggi::memory_input<peggi::tracking_mode::eager>& input)->std::pair<const char*, size_t> {
	size_t beginIndex = input.byte();
	peggi::parse<mdlang::text_contents>(input);
	return std::make_pair(&input.begin()[beginIndex], input.byte() - beginIndex);
}

bool mdpm_impl::tryNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input) {
	return peggi::parse<mdlang::non_blank_line>(input);
}

size_t mdpm_impl::checkNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input) {
	size_t nonblankPos;
	if (peggi::parse<mdlang::non_blank_0, mdlang::non_blank_line_action>(input, nonblankPos)) {
		return nonblankPos;
	}
	else {
		return static_cast<size_t>(-1);
	}
}
auto mdpm_impl::countWhitespaces(peggi::memory_input<peggi::tracking_mode::eager>& input, size_t absoluteLogicalSpaces) -> std::tuple<bool, size_t, size_t> {
	std::tuple<bool, size_t, size_t> res{};
	std::get<0>(res) = peggi::parse<mdlang::non_blank_0, mdlang::misc_action>(input, std::get<1>(res), std::get<2>(res), absoluteLogicalSpaces);
	return res;
}
bool mdpm_impl::consumeResidualWsNl(peggi::memory_input<peggi::tracking_mode::eager>& input) {
	int fun;
	unsigned char buck;
	return peggi::parse<mdlang::residualWsNl>(input, fun, buck);
}
#if 0
struct mdpm_impl::persistent_mem_input::reader_impl {
	pegtl::memory_input<mdlang::pegtl::tracking_mode::eager> inp_;
};
mdpm_impl::persistent_mem_input::persistent_mem_input(const char* begin, const char* end) :
	holder_{ new reader_impl{{begin, end, ""}} } {}
#endif


