// MDParseMan.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <list>
#include <fstream>
#include <iosfwd>
#include <string>
#include <vector>
#include <variant>
#include <optional>

// TODO: Reference additional headers your program requires here.

struct ListItemInfo {
	char       preIndent;
	char           sizeW;
	char      postIndent;
};

struct ListInfo {
	enum class symbol_e : uint8_t {
		dash = '-',
		plus = '+',
		star = '*',
		dot = '.',
		paranth = ')'
	};
	int32_t orderedStart;
	symbol_e  symbolUsed;
};

struct IndentedCode {
	size_t firstNewlineSequencePos = 0;
};
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
		ATXHeading
	};
	type_e flavor;
	bool   isOpen;
	size_t posptr;
	std::string_view body;

	MDPMNode* parent;
	std::list<MDPMNode> children;

	std::variant<std::monostate, ListItemInfo, ListInfo, IndentedCode> crtrstc;
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

namespace mdpm_impl {
	struct BlobParsingState;
}

namespace mdpm_parser {
	constexpr size_t NPOS = static_cast<size_t>(-1);
	struct Context {
		std::vector<char> textBuffer;
		MDPMNode* freshestBlock;
		size_t bodyOffset;
		size_t bodyLen;
	};

	void processLine(Context& contx, MDPMNode& node, std::istream& stream);
	void processLine(Context& contx, MDPMNode& node, const std::string& line);

	void matchATXHeading(Context& contx, MDPMNode& node, const char* line);
	void matchSetextHeading(Context& contx, MDPMNode& node, const char* line);
	void matchThematic(Context& contx, MDPMNode& node, const char* line);
	bool findCode(Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
	bool matchCode(Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
	bool matchList(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
	auto findList(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, char (&spacings)[3])->std::optional<ListInfo>;
	auto findListItem(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) -> std::optional<ListItemInfo>;
	bool matchListItem(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
	bool findBlockQuote(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, bool continuation = false);
	bool matchParagraphLineNew(Context& contx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu);
	bool matchParagraphLine(Context& contx, MDPMNode& node, const std::string& line, mdpm_impl::BlobParsingState& currSitu);
	size_t matchParagraphLineLite(const std::string_view line, const mdpm_impl::BlobParsingState& state);
}

std::vector<MDPMNode> testCases(std::istream& stm, bool& success, std::string& error, std::string& buffer);
