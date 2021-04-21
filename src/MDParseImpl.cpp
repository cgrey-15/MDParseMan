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

#include <string>
#include <tuple>
#include <cstdint>
#include <algorithm>
#include "MDParseImpl.h"
//#define TAO_PEGTL_NAMESPACE mdlang::pegtl

#include "tao/pegtl/contrib/trace.hpp"
#include "tao/pegtl.hpp"

namespace mdpm_impl {
	using md_parseman::UTinyInt;
	using md_parseman::UInt;
}

namespace mdlang {
	enum class gm : int8_t {
		nil = 0,
		atx_sfx = 20,
		atx_mid = 44
	};
}

namespace {
	struct ThematicBreakState {
		char value;
		md_parseman::UInt valueCount;
		md_parseman::UInt charsElapsed;
	};
}

namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;//pegtl;

	struct whitespace0 : one<' ', '\t'> {};
	struct thematic_break_symbol : one<'-', '_', '*'> {};
	template<uint32_t N>
	struct thematic_break_repeat_rule {
		using rule_t = thematic_break_repeat_rule;
		using subs_t = TAO_PEGTL_NAMESPACE::empty_list;

		template < TAO_PEGTL_NAMESPACE::apply_mode A,
			TAO_PEGTL_NAMESPACE::rewind_mode M,
			template< typename... > class Action,
			template< typename... > class Control,
			typename ParseInput,
			typename... States >
		static bool match(ParseInput& in, ThematicBreakState& s, States&&...) {
			size_t i = 0;
			while (i < in.size() and (in.peek_char(i) == ' ' or in.peek_char(i) == '\t')) {
				++i;
			}
			while (i < in.size() and (in.peek_char(i) == s.value)) {
				++i;
				++s.valueCount;
				while (i < in.size() and (in.peek_char(i) == ' ' or in.peek_char(i) == '\t')) {
					++i;
				}
			}
			if (i == in.size()) {
				if (N > s.valueCount) {
					return false;
				}
				in.bump(i);
				return true;
			}
			if (in.peek_char(i) == '\n' or in.peek_char(i) == '\r') {
				if (N > s.valueCount) {
					return false;
				}
				in.bump(i);
				return true;
			}
			return false;
		}
	};
	struct thematic_break_rule : seq<thematic_break_symbol, thematic_break_repeat_rule<3>, at<eolf>> {};
	template <typename Rule>
	struct thematic_break_action : nothing<Rule> {};
	template <>
	struct thematic_break_action<thematic_break_symbol> : require_apply {
		template<typename ParseInput>
		static void apply(ParseInput& in, ThematicBreakState& s) noexcept {
			s.value = in.begin()[0];
			++s.charsElapsed;
			++s.valueCount;
		}
	};
}

bool mdpm_impl::tryThematicBreak(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept {
	return mdlang::parse<mdlang::thematic_break_rule, mdlang::thematic_break_action>(input, ThematicBreakState{ 0, 0, 0 });
}

namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;

	template <uint8_t ... CharTs>
	struct content : seq<plus<not_one<' ', '\t', CharTs...>>, star<seq<plus<space>, plus<not_one<' ', '\t', CharTs...>>>>> {};
	struct non_ws_or_pound : not_one<' ', '\t', '#'> {};

	struct atx_grammar_prefix : rep_min_max<1, 6, one<'#'>> {};
	struct atx_grammar_suffix : seq< plus<one<'#'>>, star<whitespace0> > {};
	struct atx_mid : seq<plus<whitespace0>, opt<seq<not_one<' ', '\t', '#'>, star<not_one<'#'>> >> > {};
	struct atx_line : seq< atx_grammar_prefix, sor<one<' ', '\t'>, at<eolf>> > {};//opt <atx_mid>, opt<atx_grammar_suffix>, eolf> {};

	template <typename Rule>
	struct atx_dbg_action : nothing<Rule> {};

	template<>
	struct atx_dbg_action<atx_grammar_suffix> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, gm(&arg0)[4], std::pair<size_t, size_t>& v) {
			arg0[0] = gm::atx_sfx;
		}
	};
	template<>
	struct atx_dbg_action<atx_mid> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, gm(&arg0)[4], std::pair<size_t, size_t>& v) {
			arg0[1] = gm::atx_mid;
		}
	};
	template<>
	struct atx_dbg_action<content<'#'>> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, gm(&arg0)[4], std::pair<size_t, size_t>& arg1) {
			arg1.first = in.position().byte;
			arg1.second = in.size();
		}
	};

	template <typename Rule>
	struct atx_header_action : nothing<Rule> {};

	template <>
	struct atx_header_action<atx_grammar_prefix> {
		template <typename ParseInput>
		static void apply(const ParseInput& in, md_parseman::UInt& lvl, bool&, md_parseman::UInt&) noexcept {
			lvl = static_cast<std::remove_reference_t<decltype(lvl)>>(in.size());
		}
	};

}

auto mdpm_impl::tryATXHeading(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept ->Heading {
	md_parseman::UInt level = std::numeric_limits<decltype(level)>::max();
	bool b;
	md_parseman::UInt i;

	if (mdlang::parse<mdlang::atx_line, mdlang::atx_header_action>(input, level, b, i)) {
		assert(level != std::numeric_limits<decltype(level)>::max());
	}
	else {
		level = 0;
	}
	return { static_cast<decltype(Heading::lvl)>(level), false };
}
namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;

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
}
namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;
	struct setex_lvl_1_marker : plus<one<'='>> {};
	struct setex_lvl_2_marker : plus<one<'-'>> {};
	struct setex_rule : seq<sor<setex_lvl_1_marker, setex_lvl_2_marker>, star<whitespace0>, at<eolf>> {};

	template<typename Rule>
	struct setex_action : nothing<Rule> {};

	template<>
	struct setex_action<setex_lvl_1_marker> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, char& lvl) noexcept {
			lvl = 1;
		}
	};
	template<>
	struct setex_action<setex_lvl_2_marker> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, char& lvl) noexcept {
			lvl = 2;
		}
	};
}

char mdpm_impl::trySetexHeading(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept {
	char res{};
	if (not peggi::parse<mdlang::setex_rule, mdlang::setex_action>(input, res)) {
		return 0;
	}
	return res;
}

namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;

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
		static void apply(const ParseInput& in, PtrDiffT& nonWsPos) noexcept {
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
		static void apply(ParseInput& in, md_parseman::UInt& destBytes, md_parseman::UInt& destColumns, md_parseman::UInt& absoluteLogicalIndentationAcc) {
			using md_parseman::UInt;
			constexpr size_t TAB_LENGTH = 4;
			UInt offset = 0;
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
namespace pegtl = TAO_PEGTL_NAMESPACE;

#ifdef WIN32
using uni_char_t = uint8_t;
#elif defined __cpp_lib_char8_t
using uni_char_t = char8_t;
#else
using uni_char_t = uint8_t
#endif

template <typename T>
inline T tabFillSpace(const T logicalSpacesOffset) noexcept {
	constexpr T TAB_STOP = 4;
	static_assert(std::is_integral_v<T> and not std::is_floating_point_v<T>);
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
	bool isExactAmount;
	char indentWorthsOverconsumed;
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

					uint8_t deficitConsumedSpaces = std::min(spacesObtained, deficitSpacesToIndent);

					uint8_t tabCount = 0;
					if (*in.end() == '\t') {
						tabCount = 1;
					}
					ctx.bytesToConsume += tabCount + (spacesObtained - deficitConsumedSpaces);

					uint8_t newSpacesToRConsume = std::max<int8_t>(static_cast<int8_t>(newSpacesToIndent) - ctx.spaceDeficit, 0);
					uint8_t newSpacesNotConsume = ctx.spaceDeficit;
					uint8_t extraExcessIndentation = (0 + ctx.spacesIndentedToSkip + ctx.spacesIndented) - 4;
					ctx.spaceDeficit = ctx.spaceDeficit - deficitConsumedSpaces + extraExcessIndentation;
				}
				else {
					uint8_t newSpacesToIndent = spacesObtained + tabSpaceFiller;
					uint8_t deficitSpacesToIndent = ctx.spaceDeficit;

					ctx.spacesIndented += newSpacesToIndent;
					ctx.spacesIndentedToSkip += deficitSpacesToIndent;

					uint8_t deficitConsumedSpaces = std::min(spacesObtained, deficitSpacesToIndent);

					uint8_t tabCount = 0;
					if (*in.end() == '\t') {
						tabCount = 1;
					}
					ctx.bytesToConsume += tabCount + (spacesObtained - deficitConsumedSpaces);

					uint8_t newSpacesToRConsume = std::max<int8_t>(static_cast<int8_t>(newSpacesToIndent) - deficitSpacesToIndent, 0);
					uint8_t newSpacesNotConsume = ctx.spaceDeficit;
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
		static void apply(ParseInput& in, IndCodeParResult& res, const md_parseman::UInt lineIndent, uint8_t& preindent) noexcept {
			if (res.indentMatched) {
				return;
			}
			using md_parseman::UInt;
			UInt tabCounts = static_cast<UInt>(in.end() - in.begin());
			res.indentsCounted += tabFillSpace(lineIndent + res.indentsCounted);
			++res.charTsCounted;
			
			if ((res.indentsCounted + preindent < 4) and tabCounts > 1) {
				//redundantSpaces = preindent;
				preindent = 0;
				res.indentsCounted += tabFillSpace(lineIndent + res.indentsCounted);
				++res.charTsCounted;
			}
			if (res.indentsCounted + preindent >= 4) {
				res.indentMatched = true;
				res.indentWorthsOverconsumed = res.indentsCounted - 4;
				res.indentsCounted += preindent;
			}
		}
	};
	template<>
	struct ind_code_action0<plus<one<' '>>> : require_apply {
		template <typename ParseInput>
		static void apply(ParseInput& in, IndCodeParResult& res, const md_parseman::UInt lineIndent, uint8_t& preindent) noexcept {
			if (res.indentMatched) {
				return;
			}
			using md_parseman::UInt;
			UInt spaceCounts = static_cast<UInt>(in.end() - in.begin());
			int charTForConsume = std::min(spaceCounts, 4 - (res.indentsCounted + preindent));

			res.indentsCounted += charTForConsume;
			//preindent = 0;
			res.charTsCounted += charTForConsume;

			if (res.indentsCounted + preindent >= 4) {
				res.indentMatched = true;
				res.indentsCounted += preindent;
				assert(res.indentsCounted == 4);
			}
		}
	};
}


namespace peggi = TAO_PEGTL_NAMESPACE;
auto mdpm_impl::tryIndentedCode(const bool isContPrefix,
	     peggi::memory_input<peggi::tracking_mode::eager>& input, 
	                                          const md_parseman::UInt absoluteLogicalIndent,
	                                          const md_parseman::UInt indentDeficit) noexcept -> std::tuple<md_parseman::UInt, md_parseman::UInt, char, bool, bool>
{
	assert(indentDeficit <= 4);
	constexpr md_parseman::UInt zeroVal = 0;
	std::tuple<md_parseman::UInt, md_parseman::UInt, char, bool, bool> result = std::make_tuple(zeroVal, zeroVal, '\0', false, true);

	auto& [rByte, rIndent, rDeficit, rMatch, rNonBlankLine] = result;

	IndCodeParResult res{};
	uint8_t preindent = static_cast<uint8_t>(indentDeficit);
	bool matchedAndNonblank = peggi::parse<mdlang::ind_code_counter_nonblank, mdlang::ind_code_action0>(input, res, static_cast<md_parseman::UInt>(absoluteLogicalIndent), preindent);

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
	//return result;
}

namespace mdlang {
	using namespace TAO_PEGTL_NAMESPACE;
	struct code_fence : sor< seq< rep_min<3, one<'`'>>, opt<at< star<not_one<'`'>>, eolf>> >, rep_min<3, one<'~'>> > {};
	struct fenced_code_rule : code_fence {};

	template <typename Rule>
	struct fenced_code_action : nothing<Rule> {};

	template <auto C>
	struct fenced_code_action<rep_min<3, one<C>>> {
		template <typename ParseInput>
		static void apply(const ParseInput& in, FencedCode& codeInfo) noexcept {
			codeInfo.length = in.size();
			codeInfo.type = static_cast<FencedCode::symbol_e>(C);
		}
	};
}

auto tryFencedCode(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept ->FencedCode {
	FencedCode info;
	md_parseman::UHalfInt predScalar = peggi::parse<mdlang::fenced_code_rule, mdlang::fenced_code_action>(input, info);
	return { info.length * predScalar, info.type };
}


struct LIIndentInfo {
	md_parseman::UTinyInt spaceDeficit;
	md_parseman::UTinyInt absoluteSlotPos;
	md_parseman::UInt spacesIndented;
	md_parseman::UInt bytesToConsume;
	bool indentStartsWithTab;
};
struct LIParResult {
	bool hasSomeIndent;
	uint8_t indentWorthsOverconsumed;
	uint8_t indentWorthsConsumed;
	uint32_t charTsToConsume;
	uint32_t indentsCounted;
};
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
	template<int K>
	struct indent_counter : sor<plus<one<' '>>, one<'\t'>> {};
	template<int K>
	struct item_indent : indent_counter<K> {};

	struct list_item_prefix1 : my_at<list_marker, sor< seq<plus<item_indent<1>>, sor<non_space, eolf>>, eolf> > {};

	template <typename Rule>
	struct li_action : nothing<Rule> {};

	template<int K>
	struct li_action<item_indent<K>> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, LIIndentInfo& pre, LIParResult& preRes, LIIndentInfo& post, LIParResult& postRes, char& finalPostIndent, bool& maybeNonBlank, char& mLen, ListInfo&) noexcept {
			using md_parseman::UBigInt;
			using md_parseman::UInt;
			LIIndentInfo* which{};
			LIParResult* whichRes{};
			if constexpr (K == 3) {
				which = &pre;
				whichRes = &preRes;
			}
			else if (K == 1) {
				which = &post;
				whichRes = &postRes;
				if (which->spacesIndented > 4) {
					return;
				}
			}
			LIIndentInfo& inIndent = *which;
			LIParResult& inRes = *whichRes;

			int isTab = false;

			UInt indentCount{};
			if (*in.begin() == ' ') {
				indentCount = static_cast<UInt>(in.end() - in.begin());
			}
			else if (*in.begin() == '\t') {
				if constexpr (K == 3) {
					indentCount = tabFillSpace(inIndent.absoluteSlotPos + inIndent.spacesIndented);
				}
				else {
					indentCount = tabFillSpace(inIndent.absoluteSlotPos + mLen + inIndent.spacesIndented);
				}
				isTab = true;
				if constexpr (K == 1) {
					if (inIndent.spacesIndented + inIndent.spaceDeficit == 0) {
						inIndent.indentStartsWithTab = true;
					}
				}
			}

			UInt outstandingIndentBalance = static_cast<UInt>(std::max(static_cast<int>(K - inIndent.spaceDeficit), 0));

			if (inIndent.spacesIndented + indentCount >= outstandingIndentBalance) {
				if constexpr (K == 1) {
					int remainingIndentConsumedThisMatchRaw = isTab ? indentCount : outstandingIndentBalance - inIndent.spacesIndented;

					int totalIndentCountedThisMatch = inIndent.spacesIndented + remainingIndentConsumedThisMatchRaw + inIndent.spaceDeficit;

					if (totalIndentCountedThisMatch > 4) {
						if (inRes.indentsCounted != 1) {
							inRes.indentsCounted = 1;
							if (inIndent.indentStartsWithTab) {
								inRes.indentWorthsConsumed = tabFillSpace(inIndent.absoluteSlotPos + mLen);
								inRes.indentWorthsOverconsumed = inRes.indentWorthsConsumed - 1;
								inRes.charTsToConsume = 1;
							}
						}
					}
					else {

						inIndent.spacesIndented += indentCount;
						inIndent.bytesToConsume += isTab ? 1 : indentCount;

						inRes.hasSomeIndent = true;
						inRes.indentsCounted = inIndent.spacesIndented;
						inRes.indentWorthsConsumed = inIndent.spacesIndented;
						inRes.charTsToConsume = inIndent.bytesToConsume;
					}
				}
				else {
					if (inIndent.spacesIndented + indentCount > outstandingIndentBalance) {
						inRes.hasSomeIndent = false;
					}
					UInt remainingIndentConsumedThisMatchRaw = isTab ? indentCount : outstandingIndentBalance - inIndent.spacesIndented;
					UInt bytesToConsume = isTab ? 1 : indentCount;
					UInt totalIndentCountedThisMatch = inIndent.spacesIndented + remainingIndentConsumedThisMatchRaw + inIndent.spaceDeficit;
					inIndent.spacesIndented += remainingIndentConsumedThisMatchRaw;
					inRes.indentWorthsConsumed = inIndent.spacesIndented;
					inRes.indentsCounted = totalIndentCountedThisMatch;
					inRes.charTsToConsume += bytesToConsume;
				}
			}
			else
			{
				if constexpr (K == 3) {
					inRes.hasSomeIndent = true;
				}
				inIndent.spacesIndented += indentCount;
				inIndent.bytesToConsume += isTab ? 1 : indentCount;
				inRes.indentWorthsConsumed = inIndent.spacesIndented;
				inRes.charTsToConsume = inIndent.bytesToConsume;
			}
		}
	};

	template <>
	struct li_action<natural_number> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, LIIndentInfo&, LIParResult&, LIIndentInfo&, LIParResult&, char&, bool&, char& markerLen, ListInfo& lInfo) noexcept {
			int i = 0;
			for (auto it = in.begin(), end = in.end(); it != end; ++it) {
				lInfo.orderedStart0[i++] = *it;
			}
			lInfo.orderedStartLen = i;
			lInfo.symbolUsed = static_cast<ListInfo::symbol_e>(*in.end());
			markerLen = i + 1;
		}
	};
	template<>
	struct li_action< ul_marker > : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, LIIndentInfo&, LIParResult&, LIIndentInfo&, LIParResult&, char&, bool&, char& markerLen, ListInfo& lInfo ) noexcept {
			lInfo.symbolUsed = static_cast<ListInfo::symbol_e>(*in.begin());
			markerLen = 1;
		}
	};

	template<>
	struct li_action<non_space> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, LIIndentInfo&, LIParResult&, LIIndentInfo&, LIParResult&, char&, bool& maybeNonblank, char&, ListInfo&) noexcept {
			maybeNonblank = true;
		}
	};

	template <typename Rule>
	struct list_item_action : nothing<Rule> {};

	template <>
	struct list_item_action<natural_number> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) noexcept {
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
		static void apply(const ParseInput& in, ListInfo& lInfo, ListItemInfo& liInfo, size_t&, size_t&, size_t&, uint8_t& ) noexcept {
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
		static void apply(const ParseInput& in, ListInfo&, ListItemInfo&, size_t& destBytes, size_t& destColumns, size_t& absoluteLogicalIndentationAcc, uint8_t&) noexcept {
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
		static void apply(const ParseInput& in, ListInfo&, ListItemInfo&, size_t&, size_t&, size_t&, uint8_t& postListMarker) noexcept {
			postListMarker = *in.end();
		}
	};
}

auto mdpm_impl::tryListItem(peggi::memory_input<peggi::tracking_mode::eager>& input,
	const size_t absoluteLogicalIndent,
	const size_t indentDeficit) -> std::optional<std::tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>> {

	auto parseOutput = std::make_tuple<ListInfo, ListItemInfo, size_t, size_t, size_t, uint8_t>({}, {}, 0, 0, 0, 0);
	std::optional<decltype(parseOutput)> retValue{ std::nullopt };

	size_t byteLen = 0, indLen = 0, deficit = indentDeficit;
	size_t absoluteLogIndCopy = absoluteLogicalIndent;
	uint8_t c = 0;

	if (peggi::parse<mdlang::list_item_prefix0, mdlang::list_item_action>(input, std::get<0>(parseOutput), std::get<1>(parseOutput), byteLen, indLen, absoluteLogIndCopy, c)) {
		retValue.emplace(std::move(std::get<0>(parseOutput)), std::move(std::get<1>(parseOutput)), byteLen, indLen, deficit, c);
	}
	return retValue;
}
auto mdpm_impl::tryListItem0(peggi::memory_input<peggi::tracking_mode::eager>& input,
	const size_t absoluteLogicalIndent,
	const UTinyInt indentDeficit) noexcept -> std::optional<std::tuple<ListInfo, ListItemInfo, WhitespaceParseResult, bool>> {

	decltype(mdpm_impl::tryListItem0(input, 0, 0)) retVal{};

	LIIndentInfo infoPre{static_cast<uint8_t>(indentDeficit), static_cast<uint8_t>(absoluteLogicalIndent)};
	LIParResult resultPre{true};

	LIIndentInfo infoPost{};
	LIParResult resultPost{};

	char finalCountThing{};
	bool maybeNonblankLine{};
	bool parseIsValid{};

	ListInfo listInformationFound{};
	char markerLen{};

	parseIsValid = peggi::parse<mdlang::list_item_prefix1, mdlang::li_action>(input, infoPre, resultPre, infoPost, resultPost, 
		finalCountThing, maybeNonblankLine, markerLen, listInformationFound);

	/*
	struct LIIndentInfo {
		uint8_t spaceDeficit;
		uint8_t absoluteSlotPos;
		uint8_t spacesIndented;
		uint8_t spacesIndentedToSkip;
		uint8_t bytesToConsume;
		bool indentStartsWithTab;
		int indentAmtRequired = -1;
	};
	struct LIParResult {
		bool hasSomeIndent;
		bool matchesIfSpecificIndent;
		uint8_t indentWorthsOverconsumed;
		uint8_t indentWorthsConsumed;
		uint32_t charTsCounted;
		uint32_t indentsCounted;
	};
	*/
	using md_parseman::UInt;
	if (parseIsValid) {
		if (resultPre.hasSomeIndent and (resultPost.hasSomeIndent or not maybeNonblankLine)) {
			assert(resultPre.indentWorthsOverconsumed == 0);
			uint8_t spaceDeficit = resultPost.indentWorthsOverconsumed;
			ListItemInfo itemResult{};
			itemResult.preIndent = resultPre.indentWorthsConsumed + indentDeficit;
			itemResult.sizeW = markerLen;
			itemResult.postIndent = resultPost.indentsCounted;

			WhitespaceParseResult finalInfo{};
			finalInfo.charTCost = resultPre.charTsToConsume + markerLen + resultPost.charTsToConsume;
			finalInfo.excessTabPadding = spaceDeficit;
			finalInfo.indentConsumptionCost = resultPre.indentWorthsConsumed + markerLen + resultPost.indentWorthsConsumed;
			retVal.emplace(std::move(listInformationFound), std::move(itemResult), std::move(finalInfo), maybeNonblankLine);
		}
	}
	return retVal;
}

bool mdpm_impl::tryBlockQuote(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept {
	return peggi::parse<mdlang::block_quote_prefix>(input);
}

auto mdpm_impl::getTextData(peggi::memory_input<peggi::tracking_mode::eager>& input, const bool trimTrailingWs) noexcept->std::pair<const char*, size_t> {
	using offset_line_t = md_parseman::UInt;
	size_t beginIndex = input.byte();
	peggi::parse<mdlang::text_contents>(input);

	offset_line_t textSize = input.byte() - beginIndex;

	const char* lineBegin = &input.begin()[beginIndex];

	return std::make_pair(lineBegin, static_cast<size_t>(textSize));
}

bool mdpm_impl::tryNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input) {
	return peggi::parse<mdlang::non_blank_line>(input);
}
bool mdpm_impl::tryNonBlank0(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept {
	return peggi::parse<mdlang::non_blank_0>(input);
}

size_t mdpm_impl::checkNonBlank(peggi::memory_input<peggi::tracking_mode::eager>& input) noexcept {
	size_t nonblankPos;
	if (peggi::parse<mdlang::non_blank_0, mdlang::non_blank_line_action>(input, nonblankPos)) {
		return nonblankPos;
	}
	return static_cast<size_t>(-1);
}
#if 0
auto mdpm_impl::countWhitespaces(peggi::memory_input<peggi::tracking_mode::eager>& input, md_parseman::UInt absoluteLogicalSpaces) -> std::tuple<bool, md_parseman::UInt, md_parseman::UInt> {
	using md_parseman::UInt;
	std::tuple<bool, UInt, UInt> res{};
	std::get<0>(res) = peggi::parse<mdlang::non_blank_0, mdlang::misc_action>(input, std::get<1>(res), std::get<2>(res), absoluteLogicalSpaces);
	return res;
}
#endif
namespace mdlang {

	struct indent_counter_gen : sor<plus<one<' '>>, plus<one<'\t'>>> {};
	struct indented_text_line : my_at<star<indent_counter_gen>, sor<non_space, eolf>> {};

	template <typename Rule>
	struct indent_action : nothing<Rule> {};

	template<>
	struct indent_action<plus<one<' '>>> : require_apply {
		template<typename ParseInput>
		static void apply(const ParseInput& in, const uint8_t sizeN, IndentInformation& info, IndentInformation& lingering, const md_parseman::UInt indentOfLine, const uint8_t excessTabPadding) noexcept {
			assert( not (excessTabPadding < 0) and excessTabPadding <= 3);
			if (info.indentSpaces < std::max(sizeN-excessTabPadding, 0)) {
				using md_parseman::UTinyInt;
				using md_parseman::UInt;
				UTinyInt n = std::max(sizeN - excessTabPadding, 0);
				UInt count = static_cast<UInt>(in.end() - in.begin());
				if (info.indentSpaces + count >= n) {
					int remainingReq = count - (count - (n - info.indentSpaces));
					info.indentSpaces += remainingReq;
					info.whitespaces += remainingReq;
					lingering.indentSpaces += (count - remainingReq);
					lingering.whitespaces += (count - remainingReq);
				}
				else {
					info.indentSpaces += count;
					info.whitespaces += count;
				}
			}
			else {
				using md_parseman::UInt;
				lingering.indentSpaces += static_cast<UInt>(in.end() - in.begin());
				lingering.whitespaces += static_cast<UInt>(in.end() - in.begin());
			}
		}
	};
	template<>
	struct indent_action<plus<one<'\t'>>> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, const int sizeN, IndentInformation& info, IndentInformation& lingering, const md_parseman::UInt indentOfLine, const md_parseman::UTinyInt excessTabPadding) noexcept {
			assert( not (excessTabPadding < 0) and excessTabPadding <= 3);
			if (info.indentSpaces < std::max(sizeN - excessTabPadding, 0)) {
				using md_parseman::UTinyInt;
				using md_parseman::UInt;
				UTinyInt n = std::max(sizeN - excessTabPadding, 0);
				UInt tabPadding = tabFillSpace(indentOfLine + info.indentSpaces);
				UInt firstTabPadding = tabPadding%4;
				UInt remainingIndents = ((((static_cast<UInt>(in.end() - in.begin()) - 1) * 4)+tabPadding)/4)*4;

				if (info.indentSpaces + firstTabPadding >= n) {
					int remainingReq = firstTabPadding;
					info.indentSpaces += remainingReq;
					info.whitespaces += 1;
					info.excessIndentation = info.indentSpaces - n;
					lingering.indentSpaces += remainingIndents;
					lingering.whitespaces += (remainingIndents / 4);
				}
				else if (info.indentSpaces + firstTabPadding + remainingIndents >= n) {
					UInt remaining = n - (info.indentSpaces + firstTabPadding);

					UInt finalRemaining = ((remaining / 4) * 4) + ((remaining % 4) + ((4 - (remaining % 4)) % 4));

					assert(finalRemaining <= remainingIndents);

					info.indentSpaces += finalRemaining;
					info.whitespaces += (finalRemaining / 4);
					info.excessIndentation = info.indentSpaces - n;
					lingering.indentSpaces += (remainingIndents - finalRemaining);
					lingering.whitespaces += ((remainingIndents - finalRemaining) / 4);
				}
				else {
					info.indentSpaces += firstTabPadding + remainingIndents;
					info.whitespaces += (((firstTabPadding % 4) + ((4 - (firstTabPadding % 4)) % 4)) / 4) + (remainingIndents / 4);
				}
			}
			else {
				using md_parseman::UInt;
				UInt tabPadding = tabFillSpace(indentOfLine + info.indentSpaces + lingering.indentSpaces);
				UInt firstTabPadding = tabPadding % 4;
				UInt remainingIndents = ((((static_cast<UInt>(in.end() - in.begin()) - 1) * 4) + tabPadding) / 4) * 4;
				lingering.indentSpaces += (firstTabPadding + remainingIndents);
				lingering.whitespaces += (((firstTabPadding % 4) + ((4 - (firstTabPadding % 4)) % 4)) / 4) + (remainingIndents / 4);

			}
		}
	};
	template<>
	struct indent_action<non_space> : require_apply {
		template <typename ParseInput>
		static void apply(const ParseInput& in, const int, IndentInformation& info, IndentInformation&, const md_parseman::UInt, const md_parseman::UTinyInt) noexcept {
			info.isNonblank = true;
		}
	};
}

using md_parseman::UInt;
using md_parseman::UTinyInt;

auto mdpm_impl::countWhitespaces0(peggi::memory_input<peggi::tracking_mode::eager>& input, const int sizeN, const UInt absoluteLogicalSpaces, const UTinyInt deficit) noexcept -> IndentInformation {
	IndentInformation info{};
	IndentInformation extraIndents{};

	peggi::parse<mdlang::indented_text_line, mdlang::indent_action>(input, sizeN, info, extraIndents, absoluteLogicalSpaces, deficit);
	if (not info.isNonblank) {
		info.indentSpaces += extraIndents.indentSpaces;
		info.whitespaces += extraIndents.whitespaces;
	}
	return info;
}





