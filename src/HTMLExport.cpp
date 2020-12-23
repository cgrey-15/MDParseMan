#include "md_parseman/MDParseMan.h"
#include <string>
#include <string_view>
#include <ostream>
#include <sstream>

auto md_parseman::mdToHtml(std::string str) -> std::string {
	md_parseman::Parser parser{ str.data(), str.data() + str.size() };
	bool success = true;
	while (success) {
		success = parser.processLine();
	}
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
	std::ostringstream sout{};
	htmlExport(parser, sout);
	return sout.str();
}

namespace {
	bool processBlock(const MDPMNode& block, std::vector<const MDPMNode*>& stack, std::ostream& out);
	bool printEndTag(const MDPMNode& block, std::ostream& out);
	bool printBeginTagAndContent(const MDPMNode& block, std::ostream& out);
	bool printListTag(const bool isBeginTag, const ListInfo& tagInfo, std::ostream& out);
	bool printTag(const char* name, const bool useNewline, std::ostream& out);
	bool printLeaf(const char* name, const MDPMNode& block, std::ostream& out);
}

bool md_parseman::htmlExport(md_parseman::Parser& p, std::ostream& out) {
	constexpr auto what_size = sizeof(std::pair<MDPMNode::type_e, uint32_t>);
	//std::vector<std::pair<MDPMNode::type_e, uint32_t>> docStack = { {MDPMNode::type_e::NULL_BLOCK, 0xffffffff} };
	//std::vector<MDPMNode::type_e> tagStack{};
	std::vector<const MDPMNode*> nodeStack = {&(*p.end())};

	for (auto& el : p) {
		if (!::processBlock(el, nodeStack, out)) {
			return false;
		}
	}
	return true;
}

bool ::processBlock(const MDPMNode& block, std::vector<const MDPMNode*>& stack, std::ostream& out) {

	if (&block == stack.back()) {
		if (!::printEndTag(block, out)) {
			return false;
		}
		stack.pop_back();
	}
	else {
		if (!::printBeginTagAndContent(block, out)) {
			return false;
		}
		if (!block.isLeaf()) {
			stack.push_back(&block);
		}
	}
	return true;
}

bool ::printEndTag(const MDPMNode& block, std::ostream& out) {
	bool result = true;
	switch (block.flavor) {
	case MDPMNode::type_e::Document:
		break;
	case MDPMNode::type_e::List:
		result = ::printListTag(false, std::get<ListInfo>(block.crtrstc), out);
		break;
	case MDPMNode::type_e::ListItem:
		result = ::printTag("/li", true, out);
		break;
	case MDPMNode::type_e::Quote:
		result = ::printTag("/blockquote", true, out);
		break;
	case MDPMNode::type_e::NULL_BLOCK:
		result = ::printTag("/_INVALID_BLOCK", true, out);
		break;
	default: break;
	}
	return result;
}
bool ::printBeginTagAndContent(const MDPMNode& block, std::ostream& out) {
	bool result = true;
	switch (block.flavor) {
	case MDPMNode::type_e::Document:
		break;
	case MDPMNode::type_e::List:
		result = ::printListTag(true, std::get<ListInfo>(block.crtrstc), out);
		break;
	case MDPMNode::type_e::ListItem:
		result = ::printTag("li", true, out);
		break;
	case MDPMNode::type_e::IndentedCode:
		result = ::printLeaf("code", block, out);
		break;
	case MDPMNode::type_e::Paragraph:
		result = ::printLeaf("p", block, out);
		break;
	case MDPMNode::type_e::Quote:
		result = ::printTag("blockquote", true, out);
		break;
	case::MDPMNode::type_e::NULL_BLOCK:
		result = ::printTag("_INVALID_BLOCK", true, out);
		break;
	default: break;
	}
	return result;
}

bool ::printTag(const char* name, const bool useNewline, std::ostream& out) {
	const char* ending = useNewline ? ">\n" : ">";
	if (!(out << '<' << name << ending)) {
		return false;
	}
	return true;
}

bool ::printListTag(const bool isBeginTag, const ListInfo& tagInfo, std::ostream& out) {
	bool res = true;
	std::string tagType = tagInfo.orderedStartLen == 0 ? "ul" : "ol";

	if (!isBeginTag) {
		if (!(out << "</" << tagType << ">\n")) {
			return false;
		}
		return true;
	}

	if (tagInfo.orderedStartLen != 0 and !(tagInfo.orderedStart0[0] == '1' and tagInfo.orderedStartLen == 1)) {
		if (!(out << '<' << tagType << " start=\"" << std::string_view{tagInfo.orderedStart0, static_cast<size_t>(tagInfo.orderedStartLen)} << "\">\n")) {
			return false;
		}
		return true;
	}
	if((out << '<' << tagType << ">\n")) {
		return true;
	}
	return false;
}

bool ::printLeaf(const char* tag, const MDPMNode& block, std::ostream& out) {
	const char* ptr = tag;
	bool isCode;
	bool isPar = false;
	bool isSimplePar = false;
	if (isCode = block.flavor == MDPMNode::type_e::IndentedCode) {
		if (!(out << "<pre>")) {
			return false;
		}
	}
	else if (block.flavor == MDPMNode::type_e::Paragraph) {
		isPar = true;
	}
	std::string_view output = isPar && !std::get<Paragraph>(block.crtrstc).endsWithBlankLine? std::string_view(block.body.data(), (block.body.size() - 1)) : block.body;
    //|| (isPar && std::get<Paragraph>(block.crtrstc).endsWithBlankLine)
	if (isSimplePar = block.flavor == MDPMNode::type_e::Paragraph and block.parent->flavor == MDPMNode::type_e::ListItem and std::get<Paragraph>(block.crtrstc).endsWithBlankLine) {
		if (!(out << output)) {
			return false;
		}
	}
	else {
		if (!(out << '<' << ptr << '>' << output << "</" << ptr << '>')) {
			return false;
		}
	}
	if (isCode) {
		if (!(out << "</pre>\n")) {
			return false;
		}
	}
	else if(!(isSimplePar)) {
		if (!(out << '\n')) {
			return false;
		}
	}
	return true;
}


