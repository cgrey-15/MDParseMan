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
#include <stack>
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
		md_parseman::UInt   linePos;
		md_parseman::UInt indentDeficit;
		posptr_t           currLine;
		posptr_t              bound;
		posptr_t      nonWSCharTPos = std::numeric_limits<posptr_t>::max();
	};
}
namespace md = md_parseman;

MDPMNode* resolveNode(MDPMNode* n, std::vector<char>& buffer, bool isNonBlank) noexcept;

void _finalizeNode(MDPMNode& node, md::impl::Context& ctx);
void md::Parser::finalizeDocument() {

	{
		MDPMNode* nPtr = this->ctx_->freshestBlock;
		while (nPtr != this->document_->parent) {
			resolveNode(nPtr, ctx_->textBuffer, false);
			nPtr = nPtr->parent;
		}
	}

	using it_t = decltype(MDPMNode::children)::iterator;

	struct it_pair {
		it_t curr;
		it_t end;
	};
	std::vector<it_pair> iterators = { { this->document_->children.begin(), this->document_->children.end() } };/*this->document_->children.empty() ?
		std::vector<it_pair>{} : 
		std::vector<it_pair>{{ this->document_->children.begin(), this->document_->children.end() }};*/

	bool exitedChildNode = false;

	while (!iterators.empty()) {
		if (iterators.back().curr != iterators.back().end) {
			if (!exitedChildNode) {
				_finalizeNode(*iterators.back().curr, *this->ctx_);
			}
			if (!iterators.back().curr->children.empty() and !exitedChildNode) {
				iterators.push_back({ iterators.back().curr->children.begin(), iterators.back().curr->children.end() });
			}
			else {
				exitedChildNode = false;
				++iterators.back().curr;
			}
		}
		else {
			exitedChildNode = true;
			iterators.pop_back();
		}
		//else if (iterators.back().curr != iterators.back().end) {
		//}
	}
}

void _finalizeParagraphBlock(MDPMNode& n, md::impl::Context& ctx, bool toParseHardbreaks);
void _finalizeNode(MDPMNode& n, md::impl::Context& ctx) {
	switch (n.flavor) {
	case MDPMNode::type_e::Paragraph: _finalizeParagraphBlock(n, ctx, true);
		break;
	case MDPMNode::type_e::ATXHeading:
	case MDPMNode::type_e::SetextHeading: _finalizeParagraphBlock(n, ctx, false);
		break;
	default: break;
	}
}

namespace {
	struct TextCh {
		md_parseman::UInt beginI;
		md_parseman::UInt endI;
		bool b;
	};
}


TextCh _getTrimmedLine(const char* line, md_parseman::UInt size, bool removeTrailingPounds) noexcept;

void _finalizeParagraphBlock(MDPMNode& n, md::impl::Context& ctx, const bool toParseHardbreaks) {
	using ptr_line_t = md_parseman::UInt;
	//std::vector<std::string_view> lines{};
	ptr_line_t lastIndex{};

#if 1
	for (ptr_line_t i = 0, i_begin=0; (i = n.body.find('\n', i)) < n.body.size(); ++i) {
		if (n.body.empty() or n.body[0] == '\n') {
			continue;
		}
		auto l = _getTrimmedLine(&n.body[i_begin], i - i_begin, n.flavor == MDPMNode::type_e::ATXHeading);

		if (l.beginI != l.endI) {
			n.children.push_back({ MDPMNode::type_e::TextLiteral, false, {}, static_cast<size_t>(&n.body[i_begin + l.beginI] - ctx.textBuffer.data()),
				std::string_view{&n.body[i_begin + l.beginI], l.endI - l.beginI}, &n });
			assert(n.children.back().body[0] != '\n');
			if (toParseHardbreaks and l.b) {
				n.children.push_back({ MDPMNode::type_e::HardLineBreak, false, {}, 0, std::string_view{}, &n });
			}
		}
		i_begin = i + 1;
		lastIndex = i_begin;
	}
	if (lastIndex < n.body.size()) { // last line didn't end with a newline
		auto l = _getTrimmedLine(&n.body[lastIndex], n.body.size() - lastIndex, n.flavor == MDPMNode::type_e::ATXHeading);
		if (l.beginI != l.endI) {
			n.children.push_back({ MDPMNode::type_e::TextLiteral, false, {}, static_cast<size_t>(&n.body[lastIndex + l.beginI] - ctx.textBuffer.data()),
				std::string_view{&n.body[lastIndex + l.beginI], l.endI - l.beginI}, &n });
		}
	}
	n.body = {};
#if 0
	for (auto& el : lines) {
		n.children.push_back({ MDPMNode::type_e::TextLiteral, false, false, static_cast<size_t>(el.data() - ctx.textBuffer.data()), 
			std::string_view{std::move(el)}, &n });
	}
#endif
#endif
}
/*
 Returns object denoted ob where ob.b is true if line ended with two or more spaces

 Precondition is that line is always non-blank. If line is non-blank from
  end-padded pound and the rest of the line is only whitespaces, the
  size of text chunk will be 0
*/
TextCh _getTrimmedLine(const char* line, const md_parseman::UInt size, const bool removeTrailingPounds) noexcept {
	using ptr_line_t = md_parseman::UInt;
	ptr_line_t firstNonWhitespace;
	//const bool removeTrailingPounds{};
	{
		int64_t i = 0;
		for (/*i = textSize*/; i < size and (line[i] == ' ' or line[i] == '\t'/* or lineBegin[i-1] == '\n'*/); ++i) {}
		firstNonWhitespace = i;
	}

	ptr_line_t firstTrailingWhitespace = line[size - 1] == '\n' ? size - 1 : size;
	if (removeTrailingPounds) {
		ptr_line_t i = firstTrailingWhitespace;
		for (; i > firstNonWhitespace and (line[i - 1] == ' ' or line[i - 1] == '\t'); --i) {}
		firstTrailingWhitespace = i;
		for (; i > firstNonWhitespace and (line[i - 1] == '#'); --i) {}
		if (i == firstNonWhitespace or (line[i - 1] == ' ' or line[i - 1] == '\t')) {
			firstTrailingWhitespace = i; // TODO: double-check this line and part!
		}
	}
	bool endsWithHardBreak;
	{
		assert(size > 0);
		int64_t i = firstTrailingWhitespace;//line[size - 1] == '\n' ? size - 1 : size;
		char c = 0;
		for (/*i = textSize*/; i > firstNonWhitespace and (line[i - 1] == ' ' /* or lineBegin[i-1] == '\n'*/); --i) { c++; }

		endsWithHardBreak = not removeTrailingPounds and c > 1;

		for (/*i = textSize*/; i > firstNonWhitespace and (line[i-1] == ' ' or line[i-1] == '\t'/* or lineBegin[i-1] == '\n'*/); --i) {}
		firstTrailingWhitespace = i;
	}
	return { firstNonWhitespace, firstTrailingWhitespace, endsWithHardBreak };
}

auto findATXHeading(md::impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, IndentInformation preindent) noexcept ->Heading;
bool findThematicBreak(md::impl::Context& ctx, const IndentInformation& preindent) noexcept;
char findSetextHeading(md::impl::Context& ctx, const IndentInformation& preindent) noexcept;
//void matchThematic(md::impl::Context& contx, MDPMNode& node, const char* line);
auto findCode(md::impl::Context & ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept ->std::optional<IndentedCode>;
bool matchCode(md::impl::Context & ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept;
bool matchFencedCode(md::impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept;
auto findCode2(md::impl::Context& ctx, MDPMNode& node, IndentInformation preIndent) noexcept ->FencedCode;
auto findCodeCloser2(md::impl::Context& ctx, MDPMNode& node, const IndentInformation preIndent) noexcept ->FencedCode;
auto findListItem(md::impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, IndentInformation& preIndent) noexcept 
-> std::optional<std::pair<ListInfo, ListItemInfo>>;

bool matchListItem(md::impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept;
bool findBlockQuote(md::impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, bool continuation = false) noexcept;
bool matchParagraphLineNew(md::impl::Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
bool matchParagraphLine(md::impl::Context & ctx, MDPMNode& node, const /*std::string& line*/char*, mdpm_impl::BlobParsingState& currSitu) noexcept;


MDPMNode& resolveAncestorUntilContainer(MDPMNode& parent, MDPMNode::type_e childTypeTag, md_parseman::impl::Context& ctx) noexcept;

MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, md_parseman::impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) noexcept;
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, md_parseman::impl::Context& ctx, mdpm_impl::BlobParsingState& state, bool maybeLazy) noexcept;
MDPMNode* processNodeMatchExisting(MDPMNode& node, md_parseman::impl::Context& ctx, const char * line, mdpm_impl::BlobParsingState& state) noexcept;
void _injectText(md_parseman::impl::Context& ctx, MDPMNode& node, md_parseman::UInt excessIndent, /*const char* str, size_t len, size_t strpos, */bool isFencedCodeInfo = false) noexcept;

MDPMNode* processNodeMatchExisting(MDPMNode& node, md_parseman::impl::Context& ctx, const char * line, mdpm_impl::BlobParsingState& state) noexcept {
	return _processNodeMatchExistingImpl(node, ctx, line, state);
}
namespace mdpm0_impl = md_parseman::impl;

/**
returns: size_t homogenous tuple {a, b, c} where a is logical spaces counted, b is the number of bytes consumed, and
c is how many spaces that have over-extended past logicalSpaceIndent from de-tabifying
*/
std::tuple<md_parseman::UInt, md_parseman::UInt, md_parseman::UInt> bumpByLogicalSpaces(TAO_PEGTL_NAMESPACE::memory_input<TAO_PEGTL_NAMESPACE::tracking_mode::eager>& input, md_parseman::UInt logicalSpaceIndent, md_parseman::UInt absoluteLogicalIndent) noexcept;

md::Parser::Parser(md::Parser&& o) noexcept : currLine_{std::move(o.currLine_)},
document_{std::move(o.document_)},
latestBlock_{ std::move(o.latestBlock_) },
ctx_{ std::move(o.ctx_) }
{
	o.document_ = nullptr;
	o.latestBlock_ = nullptr;
	o.ctx_ = nullptr;
}

md::Parser::Parser(const char* begin, const char* end) :
	document_{nullptr /*new MDPMNode{ MDPMNode::type_e::Document, true, {}, static_cast<size_t>(-1), {}, new MDPMNode{MDPMNode::type_e::NULL_BLOCK}, {} }*/ },
	latestBlock_{ document_ },
	ctx_{ new impl::Context{{}, document_, 0, 0, std::optional<mdpm0_impl::Context::mem_input>{std::in_place, begin, end}, true} }
{
	MDPMNode* starter = new MDPMNode{ MDPMNode::type_e::NULL_BLOCK };
	starter->children.emplace_back(MDPMNode{ MDPMNode::type_e::Document, true, {}, static_cast<size_t>(-1), {}, starter, {} });
	document_ = &starter->children.back();
	document_->parent->parent = nullptr;
	latestBlock_ = document_;
	ctx_->freshestBlock = document_;
	ctx_->textBuffer.reserve(1024);
}

md::Parser::Parser() noexcept : currLine_{}, 
document_{ nullptr/*new MDPMNode{ MDPMNode::type_e::Document, true, {}, static_cast<size_t>(-1), {}, new MDPMNode{MDPMNode::type_e::NULL_BLOCK}, {} }*/ },
latestBlock_{document_}, 
ctx_{ new impl::Context{{}, document_, 0, 0, {std::nullopt}, false} }
{
	MDPMNode* starter = new MDPMNode{ MDPMNode::type_e::NULL_BLOCK };
	starter->children.emplace_back(MDPMNode{ MDPMNode::type_e::Document, true, {}, static_cast<size_t>(-1), {}, starter, {} });
	document_ = &starter->children.back();
	document_->parent->parent = nullptr;
	latestBlock_ = document_;
	ctx_->freshestBlock = document_;
	ctx_->textBuffer.reserve(1024);

}
md::Parser::~Parser() { delete document_->parent; /*delete document_; */delete ctx_; }

bool _processSingle(MDPMNode& n, mdpm0_impl::Context& ctx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) noexcept;
MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, mdpm0_impl::Context& cotx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) noexcept {
	bool matched{};
	if ( (matched = _processSingle(node, cotx, line, state)) and !node.children.empty() and node.children.back().isOpen ){//node.children.back().matching) {
		return _processNodeMatchExistingImpl(node.children.back(), cotx, line, state);
	}
	if (matched) {
		return &node;
	}
	else if (not matched and node.flavor == MDPMNode::type_e::FencedCode and not node.isOpen) {
		return nullptr;
	}
	return node.parent;
}
bool _processSingle(MDPMNode& n, mdpm0_impl::Context& ctx, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& state) noexcept {
	bool matching{};
	switch (n.flavor) {
	case MDPMNode::type_e::Document: matching = true;//do nothing?
		break;
	case MDPMNode::type_e::IndentedCode: matching = matchCode(ctx, n, state);
		break;
	case MDPMNode::type_e::FencedCode: matching = matchFencedCode(ctx, n, state);
		break;
	case MDPMNode::type_e::Paragraph: matching = matchParagraphLine(ctx, n, line, state);
		break;
	case MDPMNode::type_e::Quote: matching = findBlockQuote(ctx, n, state, true);
		break;
	case MDPMNode::type_e::List: matching = true;//matchList(ctx, n, line/*.c_str()*/, state);
		break;
	case MDPMNode::type_e::ATXHeading: matching = false;
		break;
	case MDPMNode::type_e::ListItem: matching = matchListItem(ctx, n, state);
		break;
	default:
		break;
	}
	return matching;
}

bool matchParagraphLine(mdpm0_impl::Context & ctx, MDPMNode& node, const /*std::string&*/char* line, mdpm_impl::BlobParsingState& currSitu) noexcept {
#if 0
#endif
	return mdpm_impl::tryNonBlank0(*ctx.inp_);//) {
}

bool findThematicBreak(mdpm0_impl::Context& ctx, const IndentInformation& preindent) noexcept {
	auto backupCopy = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preindent.whitespaces);
	return backupCopy(mdpm_impl::tryThematicBreak(*ctx.inp_));
}

char findSetextHeading(mdpm0_impl::Context& ctx, const IndentInformation& preindent) noexcept {
	auto backupCopy = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preindent.whitespaces);

	char res;
	backupCopy(res = mdpm_impl::trySetexHeading(*ctx.inp_));
	return res;
}

auto findCode(mdpm0_impl::Context & ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept ->std::optional<IndentedCode>
{
	auto [spaces, indent, defecit, match, __d] = mdpm_impl::tryIndentedCode(false, *ctx.inp_, currSitu.linePos, currSitu.indentDeficit);

	std::optional<IndentedCode> res;
	if (match) {
		currSitu.linePos += indent;
		currSitu.indentDeficit = defecit;
		res.emplace(IndentedCode{ md_parseman::NPOS, static_cast<uint8_t>(defecit) });
	}
	return res;
}
bool matchCode(mdpm0_impl::Context & ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept
{
#if 0
#else
	auto [spaces, indent, defecit, matching, nonBlankLine] = mdpm_impl::tryIndentedCode(true, *ctx.inp_, currSitu.linePos, currSitu.indentDeficit);
	if (matching) {
		currSitu.linePos += indent;
		currSitu.indentDeficit = defecit;
		std::get<IndentedCode>(node.crtrstc).spacePrefixes = defecit;
		if (auto& inf = std::get<IndentedCode>(node.crtrstc); !nonBlankLine) {
			if (inf.firstNewlineSequencePos == md_parseman::NPOS) {
				inf.firstNewlineSequencePos = ctx.textBuffer.size();
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

bool matchFencedCode(mdpm0_impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu ) noexcept {
	IndentInformation ind = mdpm_impl::countWhitespaces1(*ctx.inp_, std::get<FencedCode>(node.crtrstc).indent, currSitu.linePos, currSitu.indentDeficit);
	//auto prevState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(ind.whitespaces);

	// TODO: refactor this function to accommodate tabs
	//assert(ind.excessIndentation == 0);
	currSitu.indentDeficit += ind.excessIndentation;

	bool match = true;

	if (ind.indentSpaces < 4 and ind.isNonblank) {
		auto prevState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
		FencedCode f = findCodeCloser2(ctx, node, ind);
		if (f.type == std::get<FencedCode>(node.crtrstc).type and f.length >= std::get<FencedCode>(node.crtrstc).length) {
			match = not prevState(true); // false; discard prevState
			ctx.freshestBlock = resolveNode(&node, ctx.textBuffer, false);
		}
		/*else if (f.type != std::get<FencedCode>(node.crtrstc).type or f.length >= mdpm_impl::MIN_FENCE_SIZE) {
			match = not prevState(false); // true; reel input back to prevState
		}*/
		else {
			match = not prevState(false); // true; reel input back to prevState
		}
	}

	//return prevState(match);
	return match;
}

auto findCode2(mdpm0_impl::Context& ctx, MDPMNode& node, const IndentInformation preIndent) noexcept ->FencedCode {
	FencedCode res;

	auto prevState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preIndent.whitespaces);
	prevState((res = mdpm_impl::tryFencedCodeOpener(*ctx.inp_)).length);

	return res;
}
auto findCodeCloser2(mdpm0_impl::Context& ctx, MDPMNode& node, const IndentInformation preIndent) noexcept ->FencedCode {
	FencedCode res;

	auto prevState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preIndent.whitespaces);
	prevState((res = mdpm_impl::tryFencedCodeCloser(*ctx.inp_)).length);

	return res;
}

// If matching, use node's parent and add a new sibling node and close this node.
auto findListItem(mdpm0_impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, IndentInformation& preIndent) noexcept -> std::optional<std::pair<ListInfo, ListItemInfo>>
{
	if (preIndent.indentSpaces + currSitu.indentDeficit > 3) {
		return {};
	}
	auto saveState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preIndent.whitespaces);

	int outstandingDeficit = std::max(static_cast<int>(currSitu.indentDeficit) - preIndent.indentSpaces, 0);

	if (auto attempt = mdpm_impl::tryListItem0(*ctx.inp_, currSitu.linePos + preIndent.indentSpaces, /*currSitu.indentDeficit*/outstandingDeficit)) {
		if (node.flavor == MDPMNode::type_e::Paragraph and
			((std::get<ListInfo>(*attempt).orderedStartLen > 1 or (std::get<ListInfo>(*attempt).orderedStartLen == 1 and std::get<ListInfo>(*attempt).orderedStart0[0] != '1')) or
			not std::get<3>(*attempt) /*blank line*/ ) ) {
			return {};
		}
		saveState(true); // discard save state and keep changes to input

		auto& [lInfo, itemInfo, spacingInfo, isNonBlank] = *attempt;

		itemInfo.preIndent = preIndent.indentSpaces + currSitu.indentDeficit;

		if (not isNonBlank) {
			itemInfo.postIndent = 1;
		}

		ctx.inp_->bump(spacingInfo.charTCost);

		currSitu.linePos += preIndent.whitespaces + spacingInfo.indentConsumptionCost + spacingInfo.excessTabPadding;
		currSitu.indentDeficit = spacingInfo.excessTabPadding;

		return { { std::move(lInfo), std::move(itemInfo) } };
	}
	return {};
}
bool matchListItem(mdpm0_impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu) noexcept
{
	auto& listState = std::get<ListItemInfo>(node.crtrstc);
	int reqSize = listState.preIndent + listState.sizeW + listState.postIndent;
	auto whatNow = mdpm_impl::countWhitespaces0(*ctx.inp_, reqSize, currSitu.linePos, currSitu.indentDeficit);

	if (whatNow.indentSpaces + currSitu.indentDeficit >= reqSize) {
		currSitu.linePos += whatNow.indentSpaces;
		currSitu.indentDeficit = whatNow.excessIndentation;
		ctx.inp_->bump(whatNow.whitespaces);
		return true;
	}
	if (!whatNow.isNonblank && !node.children.empty()) {
		ctx.inp_->bump(whatNow.whitespaces);
		currSitu.linePos += whatNow.indentSpaces;//spaces;
		currSitu.indentDeficit = 0;//spacesDeficit;
		return true;
	}
	return false;
}

std::tuple<md_parseman::UInt, md_parseman::UInt, md_parseman::UInt> bumpByLogicalSpaces(TAO_PEGTL_NAMESPACE::memory_input<TAO_PEGTL_NAMESPACE::tracking_mode::eager>& input, const md_parseman::UInt logicalSpaceIndent, const md_parseman::UInt absoluteLogicalIndent) noexcept {
	using md_parseman::UInt;
	UInt indentsApplied = 0;
	UInt bytesConsumed = 0;

	constexpr char TAB_STOP = 4;
	size_t currIndentPos = absoluteLogicalIndent;

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

bool findBlockQuote(mdpm0_impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, bool continuation) noexcept {
	
#if 0
#else
	return mdpm_impl::tryBlockQuote(*ctx.inp_);
#endif
}

auto findATXHeading(mdpm0_impl::Context& ctx, MDPMNode& node, mdpm_impl::BlobParsingState& currSitu, const IndentInformation preindent) noexcept ->Heading{
	Heading res;
	auto prevSaveState = ctx.inp_->mark<TAO_PEGTL_NAMESPACE::rewind_mode::required>();
	ctx.inp_->bump(preindent.whitespaces);
	prevSaveState((res = mdpm_impl::tryATXHeading(*ctx.inp_)).lvl != 0);
	return res;
}
MDPMNode& resolveAncestorUntilContainer(MDPMNode& parent, const MDPMNode::type_e childTypeTag, mdpm0_impl::Context& ctx) noexcept {
	MDPMNode* anc = &parent;
#define I_HOP_
#ifdef I_HOP_
	if (anc->flavor == MDPMNode::type_e::List and childTypeTag != MDPMNode::type_e::ListItem) {
		anc = resolveNode(anc, ctx.textBuffer, true);//anc->parent;
	}
	else {
		while(anc->isLeaf()) {
#else
		while (anc->isLeaf() or (anc->flavor == MDPMNode::type_e::List and childTypeTag != MDPMNode::type_e::ListItem)) {
#endif
			//anc = anc->parent;
			anc = resolveNode(anc, ctx.textBuffer, true);
		}
#ifdef I_HOP_
	}
#endif
	return *anc;
}

// <OLD>pre: 'node' and its children are all unmatched, but its parent is still matched to 'line' being matched against
// pre: 'node' is a matched node, but its last child is not matched (and presumably all ancestors of that child)

bool _sameListType(ListInfo& a, ListInfo& b) noexcept;
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, mdpm0_impl::Context& ctx, mdpm_impl::BlobParsingState& state, bool maybeLazy) noexcept {
	if (node.flavor == MDPMNode::type_e::IndentedCode || node.flavor == MDPMNode::type_e::FencedCode || node.flavor == MDPMNode::type_e::ATXHeading
		|| node.flavor == MDPMNode::type_e::SetextHeading) {//|| node.flavor == MDPMNode::type_e::Paragraph) {
		return &node;
	}
	using li_i_t = decltype(findListItem(ctx, node, state, std::declval<IndentInformation&>()));

	MDPMNode* newborn{};

	IndentInformation preIndent = mdpm_impl::countWhitespaces0(*ctx.inp_, std::numeric_limits<int>::max(), state.linePos, state.indentDeficit);
	bool fullyIndented = preIndent.indentSpaces >= 4;

	if (Heading h; not fullyIndented and (h = findATXHeading(ctx, node, state, preIndent)).lvl) {
		newborn = &node.children.emplace_back(MDPMNode{ MDPMNode::type_e::ATXHeading, true, {}, ctx.textBuffer.size(), {}, &node, {}, h });
	}
	else if (std::optional<IndentedCode> indCodeInfo; not maybeLazy and (indCodeInfo = findCode(ctx, node, state))) {
		auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::IndentedCode, ctx);

		assert(!correctAncestorBlock.isLeaf());
		newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::IndentedCode, true, {}, ctx.textBuffer.size(), std::string_view{}, &correctAncestorBlock, std::list<MDPMNode>{
			}, *indCodeInfo });
	}
	else if (FencedCode f; not fullyIndented and (f = findCode2(ctx, node, preIndent)).length) {
		f.indent = preIndent.indentSpaces;
		auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::FencedCode, ctx);
		newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::FencedCode, true, {}, ctx.textBuffer.size(), std::string_view{}, &correctAncestorBlock, std::list<MDPMNode>{}, f });
	}
	else if (findBlockQuote(ctx, node, state)) {

		auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::Quote, ctx);

		assert(!correctAncestorBlock.isLeaf());
		correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::Quote,true, {}, 0, {}, &correctAncestorBlock, {}, {} });

		newborn = processNodeMatchAnyNew(correctAncestorBlock.children.back(), ctx, state, false);
	}
	else if (char lvl; not fullyIndented and node.flavor == MDPMNode::type_e::Paragraph and (lvl = findSetextHeading(ctx, preIndent))) {
		bool hasContent = true;
		if (hasContent) {
			node.flavor = MDPMNode::type_e::SetextHeading;
			node.crtrstc = Heading{ lvl, true };
		}
		newborn = &node;
	}
	else if (!fullyIndented and findThematicBreak(ctx, preIndent)) {
		auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::ThematicBreak, ctx);

		assert(!correctAncestorBlock.isLeaf());

		newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::ThematicBreak, true, {}, 0, std::string_view{}, &correctAncestorBlock, std::list<MDPMNode>{}, {} });
	}
	else if (li_i_t result; (!fullyIndented or node.flavor==MDPMNode::type_e::List) and (result = findListItem(ctx, node, state, preIndent))) {
		auto& [listTag, listItemTag] = *result;

		if (node.flavor != MDPMNode::type_e::List) {

			auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::List, ctx);

			//assert(!node.isLeaf());
			correctAncestorBlock.children.push_back({ MDPMNode::type_e::List, true, {}, 0, {}, &correctAncestorBlock, {}, std::move(listTag) });
			correctAncestorBlock.children.back().children.push_back({ MDPMNode::type_e::ListItem, true, {}, 0, {}, &correctAncestorBlock.children.back(), {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(correctAncestorBlock.children.back().children.back(), ctx, state, false);
		}
		else if (!_sameListType(std::get<ListInfo>(node.crtrstc), listTag)) {
			assert(!node.parent->isLeaf());
			auto& correctAncestorBlock = resolveAncestorUntilContainer(node, MDPMNode::type_e::List, ctx);
			correctAncestorBlock.children.push_back({ MDPMNode::type_e::List, true, {}, 0, {}, &correctAncestorBlock, {}, std::move(listTag) });
			correctAncestorBlock.children.back().children.push_back({ MDPMNode::type_e::ListItem, true, {}, 0, {}, &correctAncestorBlock.children.back(), {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(correctAncestorBlock.children.back().children.back(), ctx, state, false);
		}
		else {
			assert(!node.isLeaf());
			node.children.push_back({ MDPMNode::type_e::ListItem, true, {}, 0, {}, &node, {}, std::move(listItemTag) });
			newborn = processNodeMatchAnyNew(node.children.back(), ctx, state, false);
		}
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
bool _sameListType(ListInfo& a, ListInfo& b) noexcept {
	return a.symbolUsed == b.symbolUsed;
}

void _injectText(mdpm0_impl::Context& ctx, MDPMNode& node, const md_parseman::UInt excessIndent, /*const char* str, size_t len, size_t strpos, */const bool isFencedCodeInfo) noexcept {
	auto chunk = mdpm_impl::getTextData(*ctx.inp_, node.flavor == MDPMNode::type_e::Paragraph);

	if (uint8_t nSpaces; node.flavor == MDPMNode::type_e::IndentedCode and (nSpaces = std::get<IndentedCode>(node.crtrstc).spacePrefixes)) {
		static const char spaces[] = "   ";
		ctx.textBuffer.insert(ctx.textBuffer.end(), spaces, spaces + nSpaces);
	}
	if (excessIndent > 0 and node.flavor == MDPMNode::type_e::FencedCode) {
		static const char spaces[] = "   ";
		ctx.textBuffer.insert(ctx.textBuffer.end(), spaces, spaces + excessIndent);
	}

	ctx.textBuffer.insert(ctx.textBuffer.end(), chunk.first, chunk.first + chunk.second);
	if (/*appendNewline && false*/node.flavor==MDPMNode::type_e::Paragraph) {
		//ctx.textBuffer.push_back('\n');
	}
	if (not isFencedCodeInfo) {
		node.body = { ctx.textBuffer.data() + node.posptr, ctx.textBuffer.size() - node.posptr };
	}
	else {
		assert(node.flavor == MDPMNode::type_e::FencedCode);
		std::get<FencedCode>(node.crtrstc).infoStr = { ctx.textBuffer.data() + node.posptr, ctx.textBuffer.size() - node.posptr };
	}
}

void _trimParagraph(MDPMNode& n) noexcept;
MDPMNode* resolveNode(MDPMNode* n, std::vector<char>& buffer, const bool isNonBlank) noexcept {
	n->isOpen = false;

	switch (n->flavor) {
	case MDPMNode::type_e::IndentedCode:
	{
		auto& blockProperty = std::get<IndentedCode>(n->crtrstc);
		if (blockProperty.firstNewlineSequencePos != md_parseman::NPOS) {
			buffer.resize(blockProperty.firstNewlineSequencePos);
			n->body = { buffer.data() + n->posptr, buffer.size() - n->posptr };
			blockProperty.firstNewlineSequencePos = md_parseman::NPOS;
		}
	}
		break;
	case MDPMNode::type_e::List:
		using it_t = decltype(n->children.begin());
		{
			bool& nIsTightL = std::get<ListInfo>(n->crtrstc).isTight;
			nIsTightL = true;
			for (auto itemIt = n->children.begin(); itemIt != n->children.end(); ++itemIt) {
				if (itemIt->lastLineStatus.blank and (++it_t{ itemIt }) != n->children.end()) {
					nIsTightL = false;
					break;
				}
				for (auto subLiIt = itemIt->children.begin(); subLiIt != itemIt->children.end(); ++subLiIt) {
					if (((++it_t{ subLiIt }) != itemIt->children.end() or (++it_t{ itemIt }) != n->children.end()) and
						subLiIt->endsWithLastLineBlank()) {

						nIsTightL = false;
						break;
					}
				}
				if (!nIsTightL) {
					break;
				}
			}
		}
		break;
	case MDPMNode::type_e::Paragraph:
		_trimParagraph(*n);
		// ...
		break;
	default:
		break;
	}

	return n->parent;
}

void _trimParagraph(MDPMNode& n) noexcept {
	assert(n.flavor == MDPMNode::type_e::Paragraph);

	size_t newBodySize = 0;
	{
		int64_t i = n.body[n.body.size() - 1] == '\n' ? n.body.size() - 1 : n.body.size();
		for (/*i = textSize*/; i > 0 and (n.body[i-1] == ' ' or n.body[i-1] == '\t'); --i) {}
		newBodySize = i;
	}
	n.body = std::string_view{ n.body.data(), newBodySize };
}

void resolveDematchedChainLinksExtractText(mdpm0_impl::Context& ctx, 
	                                   MDPMNode* matchedChainLinkTail, 
	                                   MDPMNode* boilingChainLinkTail, 
	                                mdpm_impl::BlobParsingState state, 
	                                         const std::string_view s) noexcept
{
	size_t firstNonWhitespace = mdpm_impl::checkNonBlank(*ctx.inp_);
	bool isNonBlankLine = firstNonWhitespace != md_parseman::NPOS;

	if (not boilingChainLinkTail->children.empty() and !isNonBlankLine) {
		boilingChainLinkTail->children.back().lastLineStatus.blank = true;
	}

	/*
	* current line ends with blank line...and
	* - we're not set to fenced code
	* - we're not blockquote nor prefixed with corresponding marker
	* - a list item that's starting with a blank line
	*/
	if (!isNonBlankLine and 
		boilingChainLinkTail->flavor != MDPMNode::type_e::ATXHeading and
		boilingChainLinkTail->flavor != MDPMNode::type_e::SetextHeading and
		boilingChainLinkTail->flavor != MDPMNode::type_e::ThematicBreak and
		boilingChainLinkTail->flavor != MDPMNode::type_e::FencedCode and
		boilingChainLinkTail->flavor != MDPMNode::type_e::Quote and not
		(boilingChainLinkTail->flavor==MDPMNode::type_e::ListItem and boilingChainLinkTail->children.empty()/*and ...*/)) {

		boilingChainLinkTail->lastLineStatus.blank = true;
	}

	for (MDPMNode* tmp = boilingChainLinkTail; tmp->parent; tmp = tmp->parent) {
		tmp->parent->lastLineStatus.blank = false;
	}

	MDPMNode* newborn{};
	if (ctx.freshestBlock != matchedChainLinkTail &&
		matchedChainLinkTail == boilingChainLinkTail /* empty boiling chain link */ &&
		ctx.freshestBlock->flavor == MDPMNode::type_e::Paragraph &&
		isNonBlankLine)
	{ 
		//mdpm_impl::tryNonBlank(*ctx.inp_);
		ctx.inp_->bump(firstNonWhitespace);
		_injectText(ctx, *ctx.freshestBlock, state.indentDeficit);// , true); // This line is a lazy-continuation line; add text-only portion of line and move on
	}
	else {
		while (ctx.freshestBlock != matchedChainLinkTail) {
			ctx.freshestBlock = resolveNode(ctx.freshestBlock, ctx.textBuffer, isNonBlankLine);
		}
		if (boilingChainLinkTail->flavor == MDPMNode::type_e::IndentedCode) {
			_injectText(ctx, *boilingChainLinkTail, state.indentDeficit);
		}
		else if (boilingChainLinkTail->flavor == MDPMNode::type_e::FencedCode) {
			if (boilingChainLinkTail == matchedChainLinkTail) {
				_injectText(ctx, *boilingChainLinkTail, state.indentDeficit);
			}
			else if (isNonBlankLine) {
				_injectText(ctx, *boilingChainLinkTail, state.indentDeficit, true);
				boilingChainLinkTail->posptr = ctx.textBuffer.size();
			}
			else {
				mdpm_impl::getTextData(*ctx.inp_, false);
			}
		}
		else if (boilingChainLinkTail->flavor == MDPMNode::type_e::Paragraph and isNonBlankLine) {
			auto indentSize = firstNonWhitespace;
			//ctx.inp_->bump(firstNonWhitespace);
			_injectText(ctx, *boilingChainLinkTail, state.indentDeficit);
		}
		else if (boilingChainLinkTail->flavor == MDPMNode::type_e::ATXHeading and isNonBlankLine) {
			assert(state.bound <= s.size());
			_injectText(ctx, *boilingChainLinkTail, state.indentDeficit);
		}
		else if (!isNonBlankLine) {
			//nothing
			//mdpm_impl::consumeResidualWsNl(*ctx.inp_);
			mdpm_impl::getTextData(*ctx.inp_, false);
			//ctx.inp_->bump_to_next_line();
		}
		else {
			auto& correctAncestorBlock = resolveAncestorUntilContainer(*boilingChainLinkTail, MDPMNode::type_e::Paragraph, ctx);
			newborn = &correctAncestorBlock.children.emplace_back(MDPMNode{ MDPMNode::type_e::Paragraph, true, {}, ctx.textBuffer.size(), {}, &correctAncestorBlock, {}, Paragraph{false} });
			ctx.inp_->bump(firstNonWhitespace);
			_injectText(ctx, *newborn, state.indentDeficit, false);
			boilingChainLinkTail = newborn;
		}

		ctx.freshestBlock = boilingChainLinkTail;
	}
}

bool md::Parser::processLine(std::istream& in) {
	bool res{};
	if (std::getline(in, currLine_)) {
	    currLine_.push_back('\n');
		res = processLine(currLine_.data(), currLine_.size());
	}
	return res;
}

bool md::Parser::processLine(const char* data, const size_t len) noexcept {
	ctx_->inp_.emplace(data, len, "data");
	mdpm_impl::BlobParsingState lineState{};
	auto* matchChainLinkTail = processNodeMatchExisting(*document_, *ctx_, data, lineState);
	MDPMNode* newborn = processNodeMatchAnyNew(*matchChainLinkTail, *ctx_, lineState, ctx_->freshestBlock->flavor == MDPMNode::type_e::Paragraph);

	resolveDematchedChainLinksExtractText(*ctx_, matchChainLinkTail, newborn, lineState, data);
	latestBlock_ = ctx_->freshestBlock;
	ctx_->inp_.reset();
	return true;
}

bool md::Parser::processLine() {
	mdpm_impl::BlobParsingState lineState{};
	auto* matchChainLinkTail = processNodeMatchExisting(*document_, *ctx_, nullptr, lineState);
	std::string_view str{ "null" };
	if (matchChainLinkTail != nullptr) {
		MDPMNode* newbornContainer = processNodeMatchAnyNew(*matchChainLinkTail, *ctx_, lineState, ctx_->freshestBlock->flavor == MDPMNode::type_e::Paragraph);
		resolveDematchedChainLinksExtractText(*ctx_, matchChainLinkTail, newbornContainer, lineState, str);
	}
	else {
		assert(ctx_->freshestBlock->children.back().flavor == MDPMNode::type_e::FencedCode and not ctx_->freshestBlock->children.back().isOpen);
		//ctx_->inp_->bump_to_next_line();
	}
	latestBlock_ = ctx_->freshestBlock;
	return !ctx_->inp_->empty();
}

bool MDPMNode::endsWithLastLineBlank() noexcept {
	if (lastLineStatus.set) {
		return lastLineStatus.blank;
	}
	if ((flavor == MDPMNode::type_e::List or flavor == MDPMNode::type_e::ListItem) and
		not children.empty()) {
		lastLineStatus.set = true;
		return children.back().endsWithLastLineBlank();
	}
	lastLineStatus.set = true;
	return lastLineStatus.blank;
}

