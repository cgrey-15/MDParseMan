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
		Paragraph,
		IndentedCode,
		FencedCode,
		Quote,
		TextLiteral,
		List,
		ListItem,
		SetextHeading,
		ATXHeading,
		NULL_BLOCK
	};
	inline bool isLeaf() const {
		return (
			flavor == type_e::IndentedCode or
			flavor == type_e::FencedCode or
			flavor == type_e::Paragraph);
	}
	type_e flavor;
	bool   isOpen;
	size_t posptr;
	std::string_view body;

	MDPMNode* parent;
	std::list<MDPMNode> children;

	std::variant<std::monostate, ListItemInfo, ListInfo, IndentedCode, Paragraph> crtrstc;

	class public_iterator;
	class public_iterator {
		using It = MDPMNode::public_iterator;
	public:
		using self_type = MDPMNode::public_iterator;
		using value_type = MDPMNode;
		using pointer_type = MDPMNode*;
		using reference = MDPMNode&;

		public_iterator(pointer_type ptr) : _retracting{ false }, _valPtr { ptr } {}
		public_iterator() : _retracting{ false }, _valPtr{ nullptr } {}

		reference operator*() { return std::holds_alternative<pointer_type>(_valPtr) ? *std::get<0>(_valPtr) : *std::get<1>(_valPtr); }

		pointer_type operator->() { return std::holds_alternative<pointer_type>(_valPtr) ? std::get<0>(_valPtr) : &*std::get<1>(_valPtr); }

		MDPARSEMAN_EXPORT self_type& operator++();
		MDPARSEMAN_EXPORT self_type operator++(int);

		//MDPARSEMAN_EXPORT self_type& operator--();
		//MDPARSEMAN_EXPORT self_type operator--(int);

		bool operator==(const self_type& rhs) const { return this->_valPtr == rhs._valPtr and /*this->_root == rhs._root and*/ this->_parents == rhs._parents; }
		bool operator!=(const self_type& rhs) const { return !operator==(rhs); }

	private:
		bool _retracting;
		std::variant<pointer_type, std::list<MDPMNode>::iterator> _valPtr{};
		//pointer_type _root{};
		std::vector<std::list<MDPMNode>::iterator> _parents;
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

		MDPARSEMAN_EXPORT Parser();
		MDPARSEMAN_EXPORT Parser(const char* begin, const char* end);
		MDPARSEMAN_EXPORT virtual ~Parser();

		MDPARSEMAN_EXPORT bool processLine(std::istream& in);
		MDPARSEMAN_EXPORT bool processLine();

		MDPARSEMAN_EXPORT MDPMNode::public_iterator begin();
		MDPARSEMAN_EXPORT MDPMNode::public_iterator end();
	private:
		void resetParserForNewLine();

		std::string  currLine_;
		MDPMNode*        root_;
		MDPMNode* latestBlock_;
		impl::Context*    ctx_;

		MDPMNode::public_iterator endIt_;
	};
	


	MDPARSEMAN_EXPORT void processLine(md_parseman::impl::Context& contx, MDPMNode& node, std::istream& stream);
	MDPARSEMAN_EXPORT void processLine(md_parseman::impl::Context& contx, MDPMNode& node, const std::string& line);

	MDPARSEMAN_EXPORT auto mdToHtml(std::string str) -> std::string;
	MDPARSEMAN_EXPORT auto mdToHtml(std::istream& in) -> std::string;
	MDPARSEMAN_EXPORT bool htmlExport(Parser& p, std::ostream& out);

}

#endif
