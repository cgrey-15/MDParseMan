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

#include "md_parseman/MDParseMan.h"
#include <istream>
//#include <sstream>
#include <type_traits>
#include <vector>
#include <regex>
#include <string_view>
#include <cstring>
#include <charconv>
#include <assert.h>
#include <optional>
#include <tuple>
#include "MDParseImpl.h"

namespace md_parseman {
	namespace impl {
		struct Context {
			using mem_input = tao::pegtl::memory_input<tao::pegtl::tracking_mode::eager>;

			std::vector<char> textBuffer;
			MDPMNode* freshestBlock;
			size_t bodyOffset;
			size_t bodyLen;
			std::optional<mem_input> inp_;
			bool parseInMemory;
		};
	}
}


namespace mdpm_impl {
	// idk yet...
	struct LineProperties {
		bool isATXH{};
		bool isStextH{};
		bool isThematic{};
		bool isCode{};
		bool isParagraph{};
		bool isBlockQuote{};
		bool isListItem{};
	};
	struct LineQueryInfo {
		MDPMNode::type_e type;
		bool          matched;
		MDPMNode*      srcPtr;
	};

	struct NodeQry {
		MDPMNode::type_e type;
		bool         matching;
	};

	struct BlobParsingState {
		using posptr_t = size_t;
		MDPMNode::type_e targetType;
		bool               matching;
		bool                 preCmp;
		posptr_t                pos;
		size_t        indentDeficit;
		posptr_t           currLine;
		posptr_t              bound;
		posptr_t      nonWSCharTPos = std::numeric_limits<posptr_t>::max();
	};

}
namespace md = md_parseman;
bool findATXHeading(md::impl::Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
void matchSetextHeading(md::impl::Context& contx, MDPMNode& node, const char* line);;
void matchThematic(md::impl::Context& contx, MDPMNode& node, const char* line);
auto findCode(md::impl::Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)->std::optional<IndentedCode>;
bool matchCode(md::impl::Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
bool matchList(md::impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
auto findList(md::impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, char (&spacings)[3])->std::optional<ListInfo>;
auto findListItem(md::impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) -> std::optional<std::pair<ListInfo, ListItemInfo>>;
bool matchListItem(md::impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
bool findBlockQuote(md::impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, bool continuation = false);
bool matchParagraphLineNew(md::impl::Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
bool matchParagraphLine(md::impl::Context& contx, MDPMNode& node, const /*std::string& line*/char*, mdpm_impl::BlobParsingState& currSitu);
size_t matchParagraphLineLite(const std::string_view line, const mdpm_impl::BlobParsingState& state);

MDPMNode& pickCorrectAncestor(MDPMNode& parent, const MDPMNode::type_e childTypeTag);

MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, md_parseman::impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state);
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, md_parseman::impl::Context& cotx, const char * line, mdpm_impl::BlobParsingState& state);
MDPMNode* processNodeMatchExisting(MDPMNode& node, md_parseman::impl::Context& ctx, const char * line, mdpm_impl::BlobParsingState& state);
void _injectText(md_parseman::impl::Context& ctx, MDPMNode& node, const char* str, size_t len, size_t strpos, bool appendNewline = false);

MDPMNode* processNodeMatchExisting(MDPMNode& node, md_parseman::impl::Context& cotx, const char * line, mdpm_impl::BlobParsingState& state) {
	return _processNodeMatchExistingImpl(node, cotx, line, state);
}
namespace mdpm0_impl = md_parseman::impl;

/**
returns: size_t homogenous tuple {a, b, c} where a is logical spaces counted, b is the number of bytes consumed, and
c is how many spaces that have over-extended past logicalSpaceIndent from de-tabifying
*/
std::tuple<size_t, size_t, size_t> bumpByLogicalSpaces(TAO_PEGTL_NAMESPACE::memory_input<TAO_PEGTL_NAMESPACE::tracking_mode::eager>& input, const size_t logicalSpaceIndent, const size_t absoluteLogicalIndent);

md::Parser::Parser(md::Parser&& o) noexcept : currLine_{std::move(o.currLine_)},
root_{std::move(o.root_)},
latestBlock_{ std::move(o.latestBlock_) },
ctx_{ std::move(o.ctx_) }
{
	o.root_ = nullptr;
	o.latestBlock_ = nullptr;
	o.ctx_ = nullptr;
}

md::Parser::Parser(const char *begin, const char *end) :
	root_{ new MDPMNode{ MDPMNode::type_e::Document, true, static_cast<size_t>(-1), {}, new MDPMNode{MDPMNode::type_e::NULL_BLOCK}, {} } },
latestBlock_{ root_ },
ctx_{ new impl::Context{{}, root_, 0, 0, std::optional<mdpm0_impl::Context::mem_input>{std::in_place, begin, end}, true} }
{
	root_->parent->parent = nullptr;
	ctx_->textBuffer.reserve(1024);
}

md::Parser::Parser() : currLine_{}, 
root_{ new MDPMNode{ MDPMNode::type_e::Document, true, static_cast<size_t>(-1), {}, new MDPMNode{MDPMNode::type_e::NULL_BLOCK}, {} } },
latestBlock_{root_}, 
ctx_{ new impl::Context{{}, root_, 0, 0, {std::nullopt}, false} }
{
	root_->parent = nullptr;
	ctx_->textBuffer.reserve(1024);

}
md::Parser::~Parser() { delete root_->parent; delete root_; delete ctx_; }

bool _processSingle(MDPMNode& n, mdpm0_impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state);
MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, mdpm0_impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) {
	bool matched{};
	if ( (matched = _processSingle(node, cotx, line, state)) && !node.children.empty() && node.children.back().isOpen ){//node.children.back().matching) {
		return _processNodeMatchExistingImpl(node.children.back(), cotx, line, state);
	}
	if (matched) {
		return &node;
	}
	else {
		return node.parent;
	}
}
bool _processSingle(MDPMNode& n, mdpm0_impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) {
	bool matching{};
	switch (n.flavor) {
	case MDPMNode::type_e::Document: matching = true;//do nothing?
		break;
	case MDPMNode::type_e::IndentedCode: matching = matchCode(cotx, n, line/*.c_str()*/, state);
		break;
	case MDPMNode::type_e::Paragraph: matching = matchParagraphLine(cotx, n, line, state);
		break;
	case MDPMNode::type_e::Quote: matching = findBlockQuote(cotx, n, line/*.c_str()*/, state, true);
		break;
	case MDPMNode::type_e::List: matching = true;//matchList(cotx, n, line/*.c_str()*/, state);
		break;
	case MDPMNode::type_e::ATXHeading: matching = false;
		break;
	case MDPMNode::type_e::ListItem: matching = matchListItem(cotx, n, line/*.c_str()*/, state);
		break;
	default:
		break;
	}
	return matching;
}



std::regex parLineRegex("^ {0,3}([^ ])");
//std::regex parlineRegex3("^");
//std::regex parLine2Regex("^( {0,3})([+\\-*][^ ]?|(1[.)][^ ]?|1[^.)]))|(([^+\\-*>].*[^ ])|[^+\\-*> ])");
//std::regex parLine2Regex(" {0,3}([^ >].{0,2})|((([+\\-*][^ ].|1[.)][^ ]|1[^.)].)|[^+\\-*1> ]..).+)| +[^ ].*");
std::regex parLine2Regex(" {0,3}(([^ >][^ ]? *)|([+\\-*][^ ].|1[).][^ ]|1[^).].|[^+\\-*1> ]..).*|     *[^ ].*)");

#if 0
bool mdpm_parser::matchParagraphLineNew(Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) {
	using silly_t = std::variant<ListItemInfo>;

	std::cmatch matches{};

	decltype(matches.length()) pos = currSitu.pos;

	bool res = std::regex_search(line + pos, matches, parLineRegex);

	if (res) {
		pos += matches[1].first - matches[0].first;
		currSitu.matching = true;
		currSitu.pos = pos;
		assert(node.isOpen == true);
		return true;
	}
	else {
		return false;
	}
}
#endif
bool matchParagraphLine(mdpm0_impl::Context& cotx, MDPMNode& node, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& currSitu) {
#if 0
	using silly_t = std::variant<ListItemInfo>;
	std::cmatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	auto length = std::strlen(line);
	bool res = std::regex_match(line/*.cbegin()*/ + pos/*, line.cend()*/, matches, parLine2Regex);
	if (res) {
		pos += matches[1].first - matches[0].first;
		currSitu.matching = true;
		currSitu.pos = pos;
		assert(node.isOpen == true);
	}
	else {
		currSitu.matching = false;
		if (node.flavor == MDPMNode::type_e::Paragraph) {
			if (!node.children.empty()) {
			}
		}
	}
	return res;
#endif
	if (mdpm_impl::tryNonBlank(*cotx.inp_)) {
		return true;
	}
	else {
		//std::get<Paragraph>(node.crtrstc).endsWithBlankLine = true;
		return false;
	}
}
size_t matchParagraphLineLite(const std::string_view line, const mdpm_impl::BlobParsingState& state) {
	std::cmatch matches{};
	if (std::regex_search(line.data() + state.pos, matches, parLineRegex)) {
		return matches[1].first - matches[0].first;
	}
	else {
		return md_parseman::NPOS;
	}
}
std::regex indentedCodeRegex{ "^(    )|(	)(\\W+)" };
std::regex indentedCode2Regex{"^( {0,3})( |\\t)?([ \\t]*)([^ \\t])?.*"};

auto findCode(mdpm0_impl::Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)->std::optional<IndentedCode>
{
#if 0
	std::cmatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	bool res = std::regex_match(line + pos, matches, indentedCodeRegex);

	std::string knit{ line };
	mdpm_impl::testFunction2(knit, currSitu.pos);
	if (res) {

		if (matches[2].matched) {
			pos += (matches[2].second - matches[2].first);
		}
		else {
			pos += (matches[1].second - matches[1].first);
		}
		currSitu.matching = true;
		currSitu.pos = pos;

	}
#else
	auto [spaces, indent, defecit, match, __d] = mdpm_impl::tryIndentedCode(false, *cotx.inp_, currSitu.pos, currSitu.indentDeficit);
#endif
	std::optional<IndentedCode> res;
	if (match) {
		currSitu.pos += indent;
		currSitu.indentDeficit = defecit;
		res.emplace(md_parseman::NPOS, defecit );
	}
	return res;
}
bool matchCode(mdpm0_impl::Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
#if 0
	auto [isNonblank, whitspaceLen, indentLen] = mdpm_impl::countWhitespaces(*cotx.inp_, currSitu.pos);

	if (size_t count = 4; count <= indentLen) {
		auto [spaces, byteSpaces, spacesDeficit] = bumpByLogicalSpaces(*cotx.inp_, count, currSitu.pos);
		currSitu.pos += spaces;
		currSitu.indentDeficit = spacesDeficit;
		return true;
	}
	else if (!isNonblank && !node.children.empty()) {
		auto [spaces, byteSpaces, spacesDeficit] = bumpByLogicalSpaces(*cotx.inp_, indentLen, currSitu.pos);
		currSitu.pos += spaces;
		currSitu.indentDeficit = spacesDeficit;
		return true;
	}
	else {
		return false;
	}
#else
	auto [spaces, indent, defecit, matching, nonBlankLine] = mdpm_impl::tryIndentedCode(true, *cotx.inp_, currSitu.pos, currSitu.indentDeficit);
	if (matching) {
		currSitu.pos += indent;
		currSitu.indentDeficit = defecit;
		std::get<IndentedCode>(node.crtrstc).spacePrefixes = defecit;
		if (auto& inf = std::get<IndentedCode>(node.crtrstc); !nonBlankLine) {
			if (inf.firstNewlineSequencePos == md_parseman::NPOS) {
				inf.firstNewlineSequencePos = cotx.textBuffer.size();
			}
		}
		else {
			if (inf.firstNewlineSequencePos != md_parseman::NPOS) {
				inf.firstNewlineSequencePos = md_parseman::NPOS;
			}
		}
	}
#endif
	return matching;
}
std::regex listRegex("^ {0,3}([-+*]|(\\d\\d{0,8})[.)])? {1,4}( *)");
std::regex listItemRegex{ "^ {0,3}([-+*]|(\\d\\d{0,8})[.)]) {1,4}( *)" };
bool matchList(mdpm0_impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
	std::cmatch matches{};

	decltype(matches.length()) nSymCons = 0;
	bool resi = std::regex_search(line + currSitu.pos, matches, listRegex);
	bool matchRes{};

	if (resi) {
		if (matches[1].length() != 0) {
			if (matches[3].length() == 0) {
				nSymCons += matches.length();//` -1;
			}
			else {
				nSymCons += (matches[1].second - matches[0].first + 1);
			}
			if (matches[2].matched ) {
				if (std::get<ListInfo>(node.crtrstc).orderedStart < 0) {
					currSitu.matching = false;
					currSitu.preCmp = true;
				}
				else if (*matches[2].second != static_cast<unsigned char>(std::get<ListInfo>(node.crtrstc).symbolUsed)) {
					currSitu.matching = false;
					currSitu.preCmp = true;
				}
				else {
					matchRes = true;
					currSitu.matching = false; // This matches List but not 'active' ListItem child because we matched a starter-marker ListItem
					currSitu.preCmp = true;
					//currSitu.pos += nSymCons; // cancel this operation; this advancing would've assumed that matching was successful and we match within item list
				}

			}
			else if (*matches[1].first != static_cast<unsigned char>(std::get<ListInfo>(node.crtrstc).symbolUsed) ) {
				currSitu.matching = false;
				currSitu.preCmp = true;
			}
			else {
				matchRes = true;
				currSitu.matching = true;
				currSitu.preCmp = true;
				currSitu.pos += nSymCons;
			}
		}
		else {
			auto& lastChildInfo = std::get<ListItemInfo>(node.children.back().crtrstc);
			if (matches.length() >= lastChildInfo.preIndent + lastChildInfo.sizeW + lastChildInfo.postIndent) {
				matchRes = true;
				nSymCons += lastChildInfo.preIndent + lastChildInfo.sizeW + lastChildInfo.postIndent;
				currSitu.matching = true;
				currSitu.preCmp = true;
				currSitu.pos += nSymCons;
			}
			else {
				currSitu.matching = false;
				currSitu.preCmp = true;
			}
		}
	}
	return matchRes;
}
auto findList(mdpm0_impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, char(&spacings)[3]) -> std::optional<ListInfo>
{
	std::cmatch matches{};

	std::optional<ListInfo> retValue{};

	decltype(matches.length()) nSymCons = 0;
	bool resi = std::regex_search(line + currSitu.pos, matches, listItemRegex);

	if (resi) {
		std::string uppi{ line };
		//mdpm_impl::tryListItem(*ctx.inp_);
		retValue.emplace();

		if (matches[2].matched) {
			auto res = std::from_chars(matches[2].first, matches[2].second, retValue->orderedStart);
			if (res.ptr != matches[2].second) {
				std::exit(99);
			}
			retValue->symbolUsed = static_cast<ListInfo::symbol_e>(*matches[2].second);
		}
		else {
			retValue->orderedStart = -1;
			retValue->symbolUsed = static_cast<ListInfo::symbol_e>(*matches[1].first);
		}

		spacings[0] = static_cast<char>(matches[1].first - matches[0].first);
		spacings[1] = static_cast<char>(matches[1].second - matches[1].first);

		if (matches[3].length() == 0) {
			nSymCons += matches.length() - 1;
			spacings[2] = static_cast<char>(matches[3].first - matches[1].second);
		}
		else {
			nSymCons += (matches[1].second - matches[0].first + 1);
			spacings[2] = 1;
		}
		currSitu.matching = true;
		currSitu.preCmp = true;
		currSitu.pos += nSymCons;
	}
	return retValue;

}
// If matching, use node's parent and add a new sibling node and close this node.
auto findListItem(mdpm0_impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) -> std::optional<std::pair<ListInfo, ListItemInfo>>
{
	std::optional<std::pair<ListInfo, ListItemInfo>> fOut{std::nullopt};

	currSitu.matching = false;

	auto [isNonblank, whitspaceLen, indentLen] = mdpm_impl::countWhitespaces(*ctx.inp_, currSitu.pos);

	size_t potentialByteCost{};

	using memory_input = TAO_PEGTL_NAMESPACE::memory_input<TAO_PEGTL_NAMESPACE::tracking_mode::eager>;
	using parse_out_t = typename std::invoke_result_t<decltype(mdpm_impl::tryListItem), memory_input&, const size_t, const size_t>;

	if ( parse_out_t parseAttempt; indentLen <= 3 and (parseAttempt = mdpm_impl::tryListItem(*ctx.inp_, currSitu.pos, currSitu.indentDeficit)) ) {
		if (uint8_t c = std::get<5>(*parseAttempt); c == ' ' or c == '\t') {

			ctx.inp_->bump(std::get<2>(*parseAttempt) + std::get<1>(*parseAttempt).sizeW);

			auto [isNonblank0, wsLen, iLen] = mdpm_impl::countWhitespaces(*ctx.inp_, currSitu.pos);

			std::get<1>(*parseAttempt).preIndent = indentLen;
			std::get<1>(*parseAttempt).postIndent = iLen <= 4 ? iLen : 1;

			auto what = bumpByLogicalSpaces(*ctx.inp_, std::get<1>(*parseAttempt).postIndent, currSitu.pos + indentLen + std::get<1>(*parseAttempt).sizeW);

			currSitu.pos += indentLen + std::get<1>(*parseAttempt).sizeW + iLen;
			currSitu.indentDeficit = std::get<2>(what);

			fOut.emplace(std::move(std::get<0>(*parseAttempt)), std::move(std::get<1>(*parseAttempt)));
		}

	}

	return fOut;
	//return mdpm_impl::tryListItem(*ctx.inp_);
}
std::regex listItemRegex2{"^(  +)\\S"};
bool matchListItem(mdpm0_impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{

	auto [isNonblank, wsCharactersFound, indentsFound] = mdpm_impl::countWhitespaces(*ctx.inp_, currSitu.pos);

	auto& listState = std::get<ListItemInfo>(node.crtrstc);
	if (size_t count{}; (count = static_cast<size_t>(listState.preIndent) + listState.sizeW + listState.postIndent) <= indentsFound + currSitu.indentDeficit) {
		// TODO: please verify that over-indent amount is always LESS-THAN-OR-EQUAL-TO all of listState's indents for every call of this function
		assert(currSitu.indentDeficit <= count);
		auto [spaces, byteSpaces, spacesDeficit] = bumpByLogicalSpaces(*ctx.inp_, count - currSitu.indentDeficit, currSitu.pos);
		currSitu.pos += spaces;
		currSitu.indentDeficit = spacesDeficit;
		return true;
	}
	else if (!isNonblank && !node.children.empty()) {
		auto [spaces, byteSpaces, spacesDeficit] = bumpByLogicalSpaces(*ctx.inp_, indentsFound, currSitu.pos);
		currSitu.pos += spaces;
		currSitu.indentDeficit = spacesDeficit;
		return true;
	}
	else {
		return false;
	}

}

std::tuple<size_t, size_t, size_t> bumpByLogicalSpaces(TAO_PEGTL_NAMESPACE::memory_input<TAO_PEGTL_NAMESPACE::tracking_mode::eager>& input, const size_t logicalSpaceIndent, const size_t absoluteLogicalSpaceOffset) {
	size_t indentsApplied = 0;
	size_t bytesConsumed = 0;

	constexpr char TAB_STOP = 4;
	size_t currIndentPos = absoluteLogicalSpaceOffset;

	uint8_t c{};
	// TODO check this thingy!
	while ((c = input.peek_uint8()) && indentsApplied < logicalSpaceIndent && !input.empty() && (c == '\t' || c == ' ')) {
		char spaceToFill;
		switch (c) {
		case ' ': spaceToFill = 1;
			break;
		case '\t': spaceToFill = TAB_STOP - (currIndentPos % TAB_STOP);
			break;
		}
		indentsApplied += spaceToFill;
		currIndentPos += spaceToFill;
		input.bump();
		bytesConsumed++;
	}
	return {indentsApplied, bytesConsumed, indentsApplied - logicalSpaceIndent};
}

std::regex blockQuoteRegex("^ {0,3}> ?(.*)");
bool findBlockQuote(mdpm0_impl::Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, bool continuation) {
	
#if 0
	std::cmatch matches{};

	decltype(matches.length()) pos = currSitu.pos;
	bool resi = std::regex_search(line + pos, matches, blockQuoteRegex);

	if (resi) {
		pos += matches[1].first - matches[0].first;
		if (currSitu.targetType != MDPMNode::type_e::Quote) {
			currSitu.targetType = MDPMNode::type_e::Quote;
		}
		currSitu.matching = true;
		currSitu.pos = pos;
	}
	
	return resi;
#else
	return mdpm_impl::tryBlockQuote(*cotx.inp_);
#endif
}

std::regex ATXHeadingRegex{"^ {0,3}#+([ \\t]([^#]*)([ \\t]+#+)?)?[ \\t]*"};
bool findATXHeading(mdpm0_impl::Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) {

#if 0
	std::cmatch matches{};

	bool matchRes = std::regex_match(line + currSitu.pos, matches, ATXHeadingRegex);
	std::string liney{ line };

	auto whatwhat = mdpm_impl::testFunction(liney, currSitu.pos);
	if (matchRes) {
		decltype(currSitu.pos) nSymbCons = 0;


		if (matches[2].matched && (matches[2].first != matches[0].second) && (matches[2].first != matches[2].second)) {
			nSymbCons += (matches[2].first - matches[0].first);
			currSitu.bound = matches[2].second - matches[0].first;
		}
		else {
			nSymbCons += (matches[0].second - matches[0].first);
			currSitu.bound = matches[0].second - matches[0].first;
		}
		currSitu.pos += nSymbCons;
	}
#endif

	return mdpm_impl::tryBlockQuote(*ctx.inp_);
}

MDPMNode& pickCorrectAncestor(MDPMNode& parent, const MDPMNode::type_e childTypeTag) {
	MDPMNode* anc = &parent;
	if (parent.flavor == MDPMNode::type_e::List and childTypeTag != MDPMNode::type_e::ListItem) {
		anc = anc->parent;
	}
	else {
		while (anc->isLeaf()) {
			anc = anc->parent;
		}
	}
	return *anc;
}

// <OLD>pre: 'node' and its children are all unmatched, but its parent is still matched to 'line' being matched against
// pre: 'node' is a matched node, but its last child is not matched (and presumably all ancestors of that child)

bool _sameListType(ListInfo& a, ListInfo& b);
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, mdpm0_impl::Context& cotx, const char * line, mdpm_impl::BlobParsingState& state) {
	if (node.flavor == MDPMNode::type_e::IndentedCode || node.flavor == MDPMNode::type_e::FencedCode || node.flavor == MDPMNode::type_e::ATXHeading
		|| node.flavor == MDPMNode::type_e::SetextHeading) {//|| node.flavor == MDPMNode::type_e::Paragraph) {
		return &node;
	}
	MDPMNode* newborn{};

	char listItemWidths[3];

	if (auto indCodeInfo = findCode(cotx, node, line, state)) {
		auto& correctAncestorBlock = pickCorrectAncestor(node, MDPMNode::type_e::IndentedCode);

		newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::IndentedCode, true, cotx.textBuffer.size(), std::string_view{}, &correctAncestorBlock, std::list<MDPMNode>{
			}, *indCodeInfo/*{IndentedCode{static_cast<size_t>(-1)}}*/ });
	}
	else if (findBlockQuote(cotx, node, line, state)) {

		auto& correctAncestorBlock = pickCorrectAncestor(node, MDPMNode::type_e::Quote);

		correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::Quote,true, 0, {}, &correctAncestorBlock, {}, {} });

		newborn = processNodeMatchAnyNew(correctAncestorBlock.children.back(), cotx, line, state);
	}

	else if (auto result = findListItem(cotx, node, line, state)) {
		bool* cool = new bool{ true };
		auto& [listTag, listItemTag] = *result;

		if (node.flavor != MDPMNode::type_e::List) {
			node.children.push_back({ MDPMNode::type_e::List, true, 0, {}, &node, {}, std::move(listTag) });
			node.children.back().children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node.children.back(), {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(node.children.back().children.back(), cotx, line, state);
		}
		else if (!_sameListType(std::get<ListInfo>(node.crtrstc), listTag)) {
			node.parent->children.push_back({ MDPMNode::type_e::List, true, 0, {}, node.parent, {}, std::move(listTag) });
			node.parent->children.back().children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node.parent->children.back(), {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(node.parent->children.back().children.back(), cotx, line, state);
		}
		else {
			node.children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node, {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(node.children.back(), cotx, line, state);
		}
#if 0
		else {
			node.children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node, {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(node.children.back(), cotx, line, state);
		}
#endif
	}
	else if (findATXHeading(cotx, node, line, state)) {
		newborn = &node.children.emplace_back(MDPMNode{ MDPMNode::type_e::ATXHeading, true, cotx.textBuffer.size(), {}, &node, {}, {} });
	}
	else {
		if (node.flavor == MDPMNode::type_e::ListItem) {
			newborn = &node;
		}
		else {
			newborn = &node;
		}
	}
	return newborn;
}
bool _sameListType(ListInfo& a, ListInfo& b) {
	return a.symbolUsed == b.symbolUsed;
}

void _injectText(mdpm0_impl::Context& ctx, MDPMNode& node, const char* str, size_t len, size_t strpos, bool appendNewline) {
	auto chunk = mdpm_impl::getTextData(*ctx.inp_);

	if (uint8_t nSpaces; node.flavor == MDPMNode::type_e::IndentedCode and (nSpaces = std::get<IndentedCode>(node.crtrstc).spacePrefixes)) {
		static const char spaces[] = "   ";
		ctx.textBuffer.insert(ctx.textBuffer.end(), spaces, spaces + nSpaces);
	}

	ctx.textBuffer.insert(ctx.textBuffer.end(), chunk.first, chunk.first + chunk.second);
	if (appendNewline && false) {
		ctx.textBuffer.push_back('\n');
	}
	node.body = { ctx.textBuffer.data() + node.posptr, ctx.textBuffer.size() - node.posptr };
}
MDPMNode* resolveNode(MDPMNode* n, std::vector<char>& buffer) {
	n->isOpen = false;

	if (n->flavor == MDPMNode::type_e::IndentedCode) {
		auto& blockProperty = std::get<IndentedCode>(n->crtrstc);
		if (blockProperty.firstNewlineSequencePos != md_parseman::NPOS) {
			buffer.resize(blockProperty.firstNewlineSequencePos);
			n->body = { buffer.data() + n->posptr, buffer.size() - n->posptr };
			blockProperty.firstNewlineSequencePos = md_parseman::NPOS;
		}
	}

	return n->parent;
}

void resolveDematchedChainLinksExtractText(mdpm0_impl::Context& cotx, 
	                                   MDPMNode* matchedChainLinkTail, 
	                                   MDPMNode* boilingChainLinkTail, 
	                                mdpm_impl::BlobParsingState state, 
	                                         const std::string_view s) 
{
	size_t firstNonWhitespace = /*0;// = static_cast<size_t>(-1);
	size_t lineHasParagraph = */mdpm_impl::checkNonBlank(*cotx.inp_);// (firstNonWhitespace = matchParagraphLineLite(s, state)) != md_parseman::NPOS;
	//firstNonWhitespace = lineHasParagraph;
	bool lineHasParagraph = firstNonWhitespace != md_parseman::NPOS;

	MDPMNode* newborn{};
	if (cotx.freshestBlock != matchedChainLinkTail &&
		matchedChainLinkTail == boilingChainLinkTail /* empty boiling chain link */ &&
		cotx.freshestBlock->flavor == MDPMNode::type_e::Paragraph &&
		lineHasParagraph)
	{ 
		_injectText(cotx, *cotx.freshestBlock, s.data(), s.size(), state.pos, true); // This line is a lazy-continuation line; add text-only portion of line and move on
	}
	else {
		while (cotx.freshestBlock != matchedChainLinkTail) {
			cotx.freshestBlock = resolveNode(cotx.freshestBlock, cotx.textBuffer);
		}
		if (boilingChainLinkTail->flavor == MDPMNode::type_e::IndentedCode) {
			_injectText(cotx, *boilingChainLinkTail, s.data(), s.size(), state.pos, true);
		}
		else if (boilingChainLinkTail->flavor == MDPMNode::type_e::Paragraph and lineHasParagraph) {
			auto indentSize = firstNonWhitespace;
			//if (indentSize != md_parseman::NPOS) {
				//cotx.textBuffer.push_back('\n');
				_injectText(cotx, *boilingChainLinkTail, s.data(), s.size(), /*state.pos*/indentSize);
			//}
		}
		else if (boilingChainLinkTail->flavor == MDPMNode::type_e::ATXHeading) {
			assert(state.bound <= s.size());
			_injectText(cotx, *boilingChainLinkTail, s.data(), state.bound, state.pos, true);
		}
		else if (!lineHasParagraph) {
			//nothing
			mdpm_impl::consumeResidualWsNl(*cotx.inp_);
		}
		else {
			auto& correctAncestorBlock = pickCorrectAncestor(*boilingChainLinkTail, MDPMNode::type_e::Paragraph);
			newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::Paragraph, true, cotx.textBuffer.size(), {}, &correctAncestorBlock, {}, Paragraph{false} });
			cotx.inp_->bump(firstNonWhitespace);
			_injectText(cotx, *newborn, s.data(), s.size(), firstNonWhitespace);
			boilingChainLinkTail = newborn;
		}

		cotx.freshestBlock = boilingChainLinkTail;
	}
}

void md::processLine(mdpm0_impl::Context& cotx, MDPMNode& node, std::istream& inp) {
	mdpm_impl::BlobParsingState sam{};
	for (std::string line; std::getline(inp, line, '\n');) {
		//matchParagraphLineNew(cotx, node, line.c_str(), sam);
	}
}

bool md::Parser::processLine(std::istream& in) {
	bool res{};
	if (std::getline(in, currLine_)) {
		currLine_.push_back('\n');
		ctx_->inp_.emplace(currLine_, "currLine_");
		mdpm_impl::BlobParsingState lineState{};
		auto* matchChainLinkTail = processNodeMatchExisting(*root_, *ctx_, currLine_.c_str(), lineState);
		MDPMNode* newborn = processNodeMatchAnyNew(*matchChainLinkTail, *ctx_, currLine_.c_str(), lineState);

		resolveDematchedChainLinksExtractText(*ctx_, matchChainLinkTail, newborn, lineState, currLine_);
		latestBlock_ = ctx_->freshestBlock;
		ctx_->inp_.reset();
		res = true;
	}
	return res;
}

bool md::Parser::processLine() {
	mdpm_impl::BlobParsingState lineState{};
	auto* matchChainLinkTail = processNodeMatchExisting(*root_, *ctx_, nullptr, lineState);
#if 0
	for (MDPMNode* ptr = matchChainLinkTail; ptr != nullptr and !ptr->children.empty(); ptr = &ptr->children.back()) {
		if (ptr->children.back().flavor == MDPMNode::type_e::Paragraph) {
			//std::get<Paragraph>(ptr->children.back().crtrstc).endsWithBlankLine = true;
		}
	}
#endif
	std::string_view str{ "null" };
	MDPMNode* newbornContainer = processNodeMatchAnyNew(*matchChainLinkTail, *ctx_, nullptr, lineState);


	resolveDematchedChainLinksExtractText(*ctx_, matchChainLinkTail, newbornContainer, lineState, str);
	latestBlock_ = ctx_->freshestBlock;
	//bool huh = mdpm_impl::consumeResidualWsNl(*ctx_->inp_); //should be consuming exactly one newline every iteration of this function
	return !ctx_->inp_->empty();
}

void md::processLine(mdpm0_impl::Context& cotx, MDPMNode& root, const std::string& line) {
	mdpm_impl::BlobParsingState state{};
	auto * matchChainLinkTail = processNodeMatchExisting(root, cotx, line.c_str(), state);
	MDPMNode* nodeptr = &root;

	MDPMNode * newborn = processNodeMatchAnyNew(*matchChainLinkTail, cotx, line.c_str(), state);

	resolveDematchedChainLinksExtractText(cotx, matchChainLinkTail, newborn, state, line);
}

