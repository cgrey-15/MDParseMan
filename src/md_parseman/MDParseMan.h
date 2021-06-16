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

// MDParseMan.h : Include file for standard system include files,
// or project specific include files.
#ifndef MD_PARSE_MAN_H
#define MD_PARSE_MAN_H
#include <list>
#include <iosfwd>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include "BlockInfoTags.h"
#include "mdparseman_export.h"

// TODO: Reference additional headers your program requires here.

namespace md_parseman {
	class Parser;
}

struct MDPMNode {
	enum class type_e : uint8_t {
		Document,
		IndentedCode,
		FencedCode,
		Quote,
		List,
		ListItem,
		Paragraph,
		ThematicBreak,
		SetextHeading,
		ATXHeading,
		TextLiteral,
		HardLineBreak,
		NULL_BLOCK
	};
	inline bool isLeaf() const noexcept {
		return (
			flavor == type_e::IndentedCode or
			flavor == type_e::FencedCode or
			flavor == type_e::Paragraph or
			flavor == type_e::ThematicBreak);
	}
	inline bool isInline() const noexcept {
		return (
			flavor == type_e::TextLiteral or
			flavor == type_e::HardLineBreak
			);
	}
	inline bool isPrintableBlock() const noexcept {
		return (
			isInline() or
			flavor == type_e::IndentedCode or
			flavor == type_e::FencedCode
			);
	}
	bool endsWithLastLineBlank() noexcept;

	type_e flavor;
	bool isOpen;

	struct LastLineRec {
		friend struct MDPMNode;
		bool blank = false;
	private:
		bool set = false;
	};
	LastLineRec lastLineStatus;

	size_t posptr;
	std::string_view body;

	MDPMNode* parent;
	std::list<MDPMNode> children;

	std::variant<std::monostate, ListItemInfo, ListInfo, IndentedCode, Paragraph, Heading, FencedCode> crtrstc;

	class public_iterator;
	class public_iterator {
		using It = MDPMNode::public_iterator;
	public:
		using self_type = MDPMNode::public_iterator;
		using value_type = MDPMNode;
		using pointer_type = MDPMNode*;
		using reference = MDPMNode&;

		public_iterator(pointer_type ptr) noexcept : _retracting{ false }, _valPtr { ptr } {}
		public_iterator(const std::list<MDPMNode>::iterator& it) noexcept : _retracting{ false }, _valPtr{ it }/*, _parents{ {it} }*/{}
		public_iterator() noexcept : _retracting{ false }, _valPtr{ nullptr } {}

		reference operator*() noexcept { return std::holds_alternative<pointer_type>(_valPtr) ? *std::get<0>(_valPtr) : *std::get<1>(_valPtr); }

		pointer_type operator->() noexcept { return std::holds_alternative<pointer_type>(_valPtr) ? std::get<0>(_valPtr) : &*std::get<1>(_valPtr); }

		MDPARSEMAN_EXPORT self_type& operator++() noexcept;
		MDPARSEMAN_EXPORT self_type operator++(int) noexcept;
		MDPARSEMAN_EXPORT const pointer_type peekNext() const noexcept;

		//MDPARSEMAN_EXPORT self_type& operator--();
		//MDPARSEMAN_EXPORT self_type operator--(int);

		bool operator==(const self_type& rhs) const { return this->_valPtr == rhs._valPtr and /*this->_root == rhs._root and*/ this->_parents == rhs._parents; }
		bool operator!=(const self_type& rhs) const { return !operator==(rhs); }

		bool isSameBlockTypeIfNextSibling() const noexcept;
		struct BlockType {
			bool isLeaf;
			MDPMNode::type_e leafType;
		};
		BlockType nextBlockIsParagraph() const noexcept;

	private:
		using storage_it_t = std::list<MDPMNode>::iterator;
		using const_storage_it_t = std::list<MDPMNode>::const_iterator;

		bool _retracting;
		std::variant<pointer_type, storage_it_t> _valPtr{};
		std::vector<storage_it_t> _parents;
	};

};

enum class DelimType {
	SBracket,
	ExclamBracket,
	Star,
	Underline
};
enum class CloserType {
	PotOpener,
	PotCloser,
	Both
};
template<typename C>
struct text_info {
	using count_t = int;
	DelimType         type;
	count_t     delimCount;
	bool          isActive;
	CloserType closingType;
	std::basic_string<C> textNode;
};


namespace md_parseman {
	constexpr size_t NPOS = static_cast<size_t>(-1);
	namespace impl {
		struct Context;
	}

	class Parser {
	public:
		Parser(const Parser&) = delete;
		MDPARSEMAN_EXPORT Parser(Parser&& o) noexcept;

		MDPARSEMAN_EXPORT Parser() noexcept;
		MDPARSEMAN_EXPORT Parser(const char* begin, const char* end);
		MDPARSEMAN_EXPORT virtual ~Parser();

		MDPARSEMAN_EXPORT bool processLine(std::istream& in);
		MDPARSEMAN_EXPORT bool processLine(const char* data, const size_t len) noexcept;
		MDPARSEMAN_EXPORT bool processLine();

		MDPARSEMAN_EXPORT void finalizeDocument();

		MDPARSEMAN_EXPORT MDPMNode::public_iterator begin() noexcept;
		MDPARSEMAN_EXPORT MDPMNode::public_iterator end() noexcept;
	private:
		void resetParserForNewLine();

		std::string  currLine_;
		MDPMNode*    document_;
		MDPMNode* latestBlock_;
		impl::Context*    ctx_;

		MDPMNode::public_iterator endIt_;
	};
	
	MDPARSEMAN_EXPORT auto mdToHtml(std::string str) -> std::string;
	MDPARSEMAN_EXPORT auto mdToHtml(std::istream& in) -> std::string;
	MDPARSEMAN_EXPORT bool htmlExport(Parser& p, std::ostream& out);

}

#endif
