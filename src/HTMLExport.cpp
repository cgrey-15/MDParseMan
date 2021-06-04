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
#include <string>
#include <string_view>
#include <ostream>
#include <sstream>
#if defined(__INTELLISENSE__) && defined(__clang__)
#include<ciso646>
#endif

auto md_parseman::mdToHtml(std::string str) -> std::string {
	md_parseman::Parser parser{ str.data(), str.data() + str.size() };
	bool success = true;
	while (success) {
		success = parser.processLine();
	}
	parser.finalizeDocument();
	std::ostringstream sout{};
	htmlExport(parser, sout);
	return sout.str();
}
auto md_parseman::mdToHtml(std::istream& in) -> std::string {
	md_parseman::Parser parser{};
	bool success = true;
	while (success) {
		success = parser.processLine(in);
	}
	parser.finalizeDocument();
	std::ostringstream sout{};
	htmlExport(parser, sout);
	return sout.str();
}

namespace {
	struct LeafBlockAdj {
		enum class Pos : uint8_t {
			Neither, NextIt, PrevIt, BothIt
		};
		MDPMNode::type_e blockType;
		Pos                  whereIs;
		MDPMNode::type_e extraBlockType;
	};

	class StateFlag {
	public:
		StateFlag() noexcept : holder_{ nullptr }, isInQuote_{false} {}
		explicit StateFlag(const MDPMNode& n) noexcept : holder_{ &n }, isInQuote_{false} {/* StateFlag(); */}
		StateFlag(const StateFlag& other) noexcept : holder_{ other.holder_ }, isInQuote_{ other.isInQuote_ } {}
		StateFlag& operator=(const StateFlag& otherObj) noexcept {
			holder_ = otherObj.holder_;
			isInQuote_ = otherObj.isInQuote_;
			return *this;
		}

		operator bool() const noexcept { return holder_ != nullptr; }
		bool operator==(const MDPMNode& rhs) const noexcept { return holder_ == &rhs; }
		bool operator==(const MDPMNode* rhs) const noexcept { return holder_ == rhs; }
		void toogleIsInQuote() noexcept { isInQuote_ = !isInQuote_; }
		bool isInQuote() const noexcept { return isInQuote_; }
	private:
		const MDPMNode* holder_;
		bool isInQuote_;
	};

	struct IterationProperty {
		const MDPMNode* curr;
		const MDPMNode* next;
		const MDPMNode* stackTop;
	};

	bool processBlock(MDPMNode::public_iterator& blockIt, std::vector<const MDPMNode*>& stack, std::ostream& out, LeafBlockAdj adj, StateFlag& listTightness);
	bool printTextBlock(const MDPMNode& block, std::ostream& out, bool prependNl);
	bool printEndTag(const IterationProperty& run, std::ostream& out, LeafBlockAdj adjacency, StateFlag& listTightness);
	bool printBeginTagAndContent(const IterationProperty& run, std::ostream& out, LeafBlockAdj adjacency, StateFlag& listTightness);
	bool inTightList(const IterationProperty& run);
	bool isTightList(const IterationProperty& run);

	void printListTag(bool isBeginTag, const ListInfo& tagInfo, std::ostream& out, bool prependNl, bool appendNl);
	void printCodeTag(bool isBeginTag, const FencedCode& codeInfo, std::ostream& out, bool preNl, bool postNl);
	void printTag(const char* name, bool prependNewline, bool useNewline, std::ostream& out);
	void printMiscTag(const char* name, bool prependNewline, bool appendNewline, std::ostream& out);
	void printLeaf(const char* tag, const MDPMNode& block, std::ostream& out, bool nextBlockIsStrangerNonLeaf = false);
	void printLine(const MDPMNode& block, std::ostream& out, bool prependNl);

	const MDPMNode* getListAncestor(const MDPMNode& block) noexcept;
}

bool md_parseman::htmlExport(md_parseman::Parser& p, std::ostream& out) {
	std::vector<const MDPMNode*> nodeStack = {&(*p.end())};

	std::optional<LeafBlockAdj> isParPrev{};

	bool success{ true };

	StateFlag listTightness{};
	for (auto it = p.begin(); it != p.end() and success; ++it) {
		auto& itRef = *it;
		LeafBlockAdj adj{};

		auto [isLeaf, blockTag] = it.nextBlockIsParagraph();
		if (isLeaf and isParPrev) {
			adj = { blockTag, LeafBlockAdj::Pos::BothIt, isParPrev->blockType };
			isParPrev.reset();
		}
		else if (isLeaf) {// and *maybeLeafTag == MDPMNode::type_e::Paragraph) {
			adj = { blockTag, LeafBlockAdj::Pos::NextIt };
		}
		else if (isParPrev) {
			adj = { isParPrev->blockType, LeafBlockAdj::Pos::PrevIt };
			isParPrev.reset();
		}
		else {
			adj = { blockTag, LeafBlockAdj::Pos::Neither };
		}
		success = processBlock(it, nodeStack, out, adj, listTightness);
		if (it->isLeaf()) {
			isParPrev = { it->flavor, LeafBlockAdj::Pos::PrevIt };
		}
	}
	return success;
}

namespace {
	bool processBlock(MDPMNode::public_iterator& blockIt, std::vector<const MDPMNode*>& stack, std::ostream& out, LeafBlockAdj adj, StateFlag& listTightness) {

#if 0
		if (stack.back()->isInline()) {
			printTextBlock(*stack.back(), out, stack.back() != &*stack.back()->parent->children.cbegin());
			stack.pop_back();
		}
#endif
		if (stack.back()->isPrintableBlock()) {
			if (stack.back()->flavor != MDPMNode::type_e::ThematicBreak) {
				if (stack.back()->isInline()) {
					if (!(printTextBlock(*stack.back(), out, stack.back() != &*stack.back()->parent->children.cbegin()))) {
						return false;
					}
				}
				else if (stack.back()->isPrintableBlock()) {
					printLeaf("", *stack.back(), out, true);
					if (out.fail()) {
						return false;
					}
				}
				if (!printEndTag({ stack.back(), /*nullptr*/&*blockIt, stack.back() }, out, adj, listTightness)) {
					return false;
				}
			}
			else {
				printTag("hr /", listTightness, true, out);
			}
			stack.pop_back();
		}

		if (&*blockIt == stack.back()) {
			stack.pop_back();
			if (blockIt->flavor != MDPMNode::type_e::ThematicBreak) {
				if (blockIt->isPrintableBlock()) {
					printLeaf("", *blockIt, out, true);
				}
				const MDPMNode* top = stack.empty() ? nullptr : stack.back();
				if (!printEndTag({ &*blockIt, blockIt.peekNext(), top }, out, adj, listTightness)) {
					return false;
				}
			}
			else {
				printTag("hr /", listTightness, true, out);
			}
		}
		else {
			if (!::printBeginTagAndContent({ &*blockIt, blockIt.peekNext(), stack.back() }, out, adj, listTightness)) {
				return false;
			}
			//if (!blockIt->isLeaf()) {
			stack.push_back(&*blockIt);
			//}
		}
		return true;
	}


	bool printTextBlock(const MDPMNode& block, std::ostream& out, const bool prependNl) {
		switch (block.flavor) {
		case MDPMNode::type_e::TextLiteral: printLine(block, out, prependNl);
			break;
		case MDPMNode::type_e::HardLineBreak: printMiscTag("br /", false, false, out);
			break;
		default: break;
		}
		return not out.fail();
	}

	void printMiscTag(const char* name, const bool prependNewline, const bool appendNewline, std::ostream& out) {
		const char* beginTag = prependNewline ? "\n<": "<";
		const char* endTag = appendNewline ? ">\n": ">";
		out << beginTag << name << endTag;
	}
	void printLine(const MDPMNode& block, std::ostream& out, const bool prependNl) {
		const char* prependStr = prependNl ? "\n" : "";
		out << prependStr << block.body;
	}

	bool printEndTag(const IterationProperty& run, std::ostream& out, LeafBlockAdj adjacency, StateFlag& listTightness) {
		bool prependNl = false;

		bool toAppendNl{};
		//const char* headingTag{};
		char headingTag[] = "/h0";
		switch (run.curr->flavor) {
		case MDPMNode::type_e::Document:
			break;
		case MDPMNode::type_e::Paragraph:
		{
			prependNl = false;
			if (not inTightList(run) /*listTightness*//* or run.curr->parent->flavor == MDPMNode::type_e::Quote*/) {
				printTag("/p", /*prependNl*/false, /*appendNl*//*true*/true, out);
			}
			else {
				if (run.next and (not run.next->isLeaf() and run.next != run.stackTop)) {
					out << "\n";
				}
			}
		}
		break;
		case MDPMNode::type_e::List:
			if (listTightness and listTightness == *run.curr) {
				listTightness = {};
			}

			toAppendNl = true;
			prependNl = false;
			printListTag(false, std::get<ListInfo>(run.curr->crtrstc), out, prependNl, toAppendNl);
			break;
		case MDPMNode::type_e::ListItem:
			printTag("/li", false, true, out);
			break;
		case MDPMNode::type_e::Quote:
			printTag("/blockquote", false, true, out);
			break;
		case MDPMNode::type_e::FencedCode:
			printCodeTag(false, std::get<FencedCode>(run.curr->crtrstc), out, false, true);
			break;
		case MDPMNode::type_e::IndentedCode:
			out << "</code></pre>\n";
			break;
		case MDPMNode::type_e::ATXHeading:
		case MDPMNode::type_e::SetextHeading:
			//headingTag = std::get<Heading>(run.curr->crtrstc).lvl == 1 ? "/h1" : "/h2";
			headingTag[(sizeof(headingTag)/sizeof(char)) - 2] += std::get<Heading>(run.curr->crtrstc).lvl;
			printTag(headingTag, /*true*/false, /*false*/true, out);
			break;
		case MDPMNode::type_e::NULL_BLOCK:
			printTag("/_INVALID_BLOCK", false, true, out);
			break;
		default: break;
		}
		return not out.fail();
	}
	bool printBeginTagAndContent(const IterationProperty& run, std::ostream& out, LeafBlockAdj adjacency, StateFlag& listTightness) {
		bool prependNl = false;
		bool appendNl = true;

		prependNl = false;
		appendNl = true;
		//const char* headingTag{};
		char headingTag[] = "h0";

		bool isEmptyListItem{ false };
		switch (run.curr->flavor) {
		case MDPMNode::type_e::Document:
			break;
		case MDPMNode::type_e::List:
			if (not listTightness and std::get<ListInfo>(run.curr->crtrstc).isTight) {
				listTightness = StateFlag{ *run.curr };
			}
			printListTag(true, std::get<ListInfo>(run.curr->crtrstc), out, prependNl, appendNl);
			break;
		case MDPMNode::type_e::ListItem:
			isEmptyListItem = run.curr->children.empty();
			appendNl = run.next ? not(run.next->flavor == MDPMNode::type_e::Paragraph and isTightList(run)) and not isEmptyListItem : true;
			printTag("li", prependNl, appendNl, out);
			break;
		case MDPMNode::type_e::ATXHeading:
		case MDPMNode::type_e::SetextHeading:
			//headingTag = std::get<Heading>(run.curr->crtrstc).lvl == 1 ? "h1" : "h2";
			headingTag[(sizeof(headingTag) / sizeof(char)) - 2] += std::get<Heading>(run.curr->crtrstc).lvl;
			printTag(headingTag, prependNl, false, out);
			break;
		case MDPMNode::type_e::FencedCode:
			printCodeTag(true, std::get<FencedCode>(run.curr->crtrstc), out, false, false);
			break;
		case MDPMNode::type_e::IndentedCode:
			out << "<pre><code>";
			break;
		case MDPMNode::type_e::Paragraph:
		{
			bool toPrependNl = false;
			if (not inTightList(run)) {
				printTag("p", toPrependNl, false, out);
			}
			if (not listTightness or run.curr->parent->flavor == MDPMNode::type_e::Quote) {

			}
		}
		//result = printLeaf("p", block, out, nextIsContainer);
		break;
		case MDPMNode::type_e::Quote:
			//prependNl = false;
			//appendNl = true;
			printTag("blockquote", prependNl, appendNl, out);
			break;
		case::MDPMNode::type_e::NULL_BLOCK:
			printTag("_INVALID_BLOCK", false, true, out);
			break;
		default: break;
		}
		return not out.fail();
	}

	bool inTightList(const IterationProperty& run)
	{
		const MDPMNode* n;
		return (n = getListAncestor(*run.curr)) and  std::get<ListInfo>(n->crtrstc).isTight;
	}

	bool isTightList(const IterationProperty& run)
	{
		return std::get<ListInfo>(run.curr->parent->crtrstc).isTight;
	}

	void printTag(const char* name, const bool prependNewline, const bool useNewline, std::ostream& out) {
		const char* beginning = prependNewline ? "\n<" : "<";
		const char* ending = useNewline ? ">\n" : ">";
		out << beginning << name << ending;
	}

	void printListTag(const bool isBeginTag, const ListInfo& tagInfo, std::ostream& out, bool prependNl, bool appendNl) {
		std::string tagType = tagInfo.orderedStartLen == 0 ? "ul" : "ol";

		if (prependNl) {
			out << '\n';
		}

		if (!isBeginTag) {
			out << "</" << tagType << ">";
			if (appendNl) {
				out << '\n';
			}
			return;
		}

		if (tagInfo.orderedStartLen != 0 and !(tagInfo.orderedStart0[0] == '1' and tagInfo.orderedStartLen == 1)) {
			out << '<' << tagType << " start=\"" << std::string_view{ tagInfo.orderedStart0, static_cast<size_t>(tagInfo.orderedStartLen) } << "\">";
			if (appendNl) {
				out << '\n';
			}
			return;
		}
		out << "<" << tagType << ">";
		if (appendNl) {
			out << '\n';
		}
	}
	void printCodeTag(bool isBeginTag, const FencedCode& codeInfo, std::ostream& out, const bool preNl, const bool postNl) {
		using namespace std::string_literals;
		const std::string beginFragmentOpener = "<pre><code";
		const std::string fragmentCloser = "</code></pre>";

		const std::string prependant = preNl ? "\n"s : ""s;
		const std::string appendant = postNl ? "\n"s : ""s;

		const std::string optInfo = codeInfo.infoStr.empty() ? ""s : " class=\"language-"s + std::string{ codeInfo.infoStr.substr(0, codeInfo.infoStr.find_first_of(" \t")) } + "\""s;

		const std::string printStr = prependant + (isBeginTag ? beginFragmentOpener + optInfo + ">"s : fragmentCloser) + appendant;

		out << printStr;
	}
	void printLeaf(const char* tag, const MDPMNode& block, std::ostream& out, const bool nextBlockIsStrangerNonleaf) {
		bool isPar = block.flavor == MDPMNode::type_e::Paragraph;

		std::string_view output = isPar ? std::string_view(block.body.data(), (block.body.size() - 1)) : block.body;

		out << output;
	}

	const MDPMNode* getListAncestor(const MDPMNode& block) noexcept {
		const MDPMNode* listPtr = nullptr;

		if (block.parent->flavor == MDPMNode::type_e::ListItem) {
			listPtr = block.parent->parent;
		}
		return listPtr;
	}
}
