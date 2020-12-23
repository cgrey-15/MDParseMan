#include <fstream>
#include "md_parseman/MDParseMan.h"

void revitalizeViews(MDPMNode& node, const std::string& buffer);

bool whitespaceOnlyString(const std::string_view str);
std::vector<MDPMNode> testCases(std::istream& stm, bool& success, std::string& error, std::string& buffer) {
	struct silly_t {
		MDPMNode::type_e e;
		MDPMNode* ptr;
	};
	std::vector<MDPMNode> roots{};
	std::vector<silly_t> stk{};
	stk.reserve(16);
	roots.reserve(16);
	std::string& buffi = buffer;
	buffi.reserve(16);
	std::string parsedLine{};
	while (stm >> parsedLine) {
		if (parsedLine[0] == '+') {
			std::string_view tok(parsedLine.c_str() + 1);
			if (tok == "Document") {
				if (stk.empty()) {
					MDPMNode newRoot{MDPMNode::type_e::Document, true, true};
					roots.push_back(newRoot);
					stk.push_back({ MDPMNode::type_e::Document, &roots.back() });
				}
				else {
					success = false;
					error += "Document is already open. Document nodes can only be root parents (no child can be a Document node)";
					return {};
				}
			}
			else if (!stk.empty() && tok == "BlockQuote") {
				stk.back().ptr->children.push_back({ MDPMNode::type_e::Quote, true });
				stk.push_back({ MDPMNode::type_e::Quote, &stk.back().ptr->children.back() });
			}
			else if (!stk.empty() && tok == "Setext") {
				stk.back().ptr->children.push_back({ MDPMNode::type_e::SetextHeading, true });
				stk.push_back({ MDPMNode::type_e::SetextHeading, &stk.back().ptr->children.back() });
				tok = {};
				std::getline(stm, parsedLine);
				
				stk.back().ptr->posptr = buffi.size();
			}
			else if (!stk.empty() && tok == "Paragraph") {
				tok = {};
				bool open{};
				std::getline(stm, parsedLine);

				if (whitespaceOnlyString(parsedLine) || parsedLine.find('0') == std::string::npos ){
					open = true;
				}

				stk.back().ptr->children.push_back({ MDPMNode::type_e::Paragraph, open });
				stk.push_back({ MDPMNode::type_e::Paragraph, &stk.back().ptr->children.back() });

				//stk.back().ptr->body = std::string_view(&buffi.back());
				stk.back().ptr->posptr = buffi.size();
			}
			else if (!stk.empty() && tok == "List") {
				bool open{};
				std::getline(stm, parsedLine);
				if (whitespaceOnlyString(parsedLine) || parsedLine.find('0') == std::string::npos) {
					open = true;
				}
				stk.back().ptr->children.push_back({ MDPMNode::type_e::List, open, open });
				stk.push_back({ MDPMNode::type_e::List , &stk.back().ptr->children.back() });
			}
			else if (!stk.empty() && tok == "ListItem") {
				bool open{};
				std::getline(stm, parsedLine);
				if (whitespaceOnlyString(parsedLine) || parsedLine.find('0') == std::string::npos) {
					open = true;
				}
				stk.back().ptr->children.push_back({ MDPMNode::type_e::ListItem, open, open });
				stk.push_back({ MDPMNode::type_e::ListItem, &stk.back().ptr->children.back() });
			}
			else if (!stk.empty() && tok == "IndentedCode") {
				tok = {};
				bool open{};
				std::getline(stm, parsedLine);
				if (whitespaceOnlyString(parsedLine) || parsedLine.find('0') == std::string::npos) {
					open = true;
				}

				stk.back().ptr->children.push_back({ MDPMNode::type_e::IndentedCode, open, open });
				stk.push_back({ MDPMNode::type_e::IndentedCode, &stk.back().ptr->children.back() });
				tok = {};
			}
			else {
				success = false;
				if (stk.empty())
					error += "No Document is open. Every other node type has to be a child or grandchild of Document node.";
				else
					error += "Invalid token";
				return {};
			}
		}
		else if (parsedLine[0] == '/') {
			std::string_view tok{ parsedLine.c_str() + 1 };
			if (tok == "Document") {
				if (stk.size() == 1) {
					stk.pop_back();
				}
			}
			else if (tok == "Setext") {
				if (stk.back().e == MDPMNode::type_e::SetextHeading) {
					stk.pop_back();
				}
			}
			else if (tok == "BlockQuote") {
				if (stk.back().e == MDPMNode::type_e::Quote) {
					stk.pop_back();
				}
			}
			else if (tok == "Paragraph") {
				if (stk.back().e == MDPMNode::type_e::Paragraph) {
					if (stk.back().ptr->body.back() == '\n') {
						buffi.pop_back();
						stk.back().ptr->body = std::string_view(&buffi[stk.back().ptr->posptr]);
					}
					stk.pop_back();
				}
			}
			else if (tok == "IndentedCode") {
				if (stk.back().e == MDPMNode::type_e::IndentedCode) {
					if (stk.back().ptr->body.back() == '\n') {
						buffi.pop_back();
						stk.back().ptr->body = std::string_view(&buffi[stk.back().ptr->posptr]);
					}
					stk.pop_back();
				}
			}
			else if (tok == "List") {
				if (stk.back().e == MDPMNode::type_e::List) {
					stk.pop_back();
				}
			}
			else if (tok == "ListItem") {
				if (stk.back().e == MDPMNode::type_e::ListItem) {
					stk.pop_back();
				}
			}
		}
		else if (stk.back().e == MDPMNode::type_e::Paragraph || stk.back().e == MDPMNode::type_e::SetextHeading || stk.back().e == MDPMNode::type_e::IndentedCode) {
			if (buffi.size() == 1 && buffi[0] == '\0') {
				buffi.pop_back();
			}
			size_t tokLen = parsedLine.size();
			buffi.append(parsedLine);
			std::getline(stm, parsedLine);
			buffi.append(parsedLine);
			buffi.push_back('\n');
			stk.back().ptr->body = std::string_view(&buffi[stk.back().ptr->posptr], stk.back().ptr->body.size() + 1 + tokLen + parsedLine.size());
		}
		else if (parsedLine != "" && (stk.back().e == MDPMNode::type_e::List || stk.back().e == MDPMNode::type_e::ListItem)) {
			error += "There can't be any (non-blank) text lines after \"+List\" and \"+ListItem\"; corresponding nodes don't store text contents.";
			success = false;
			return {};
		}
	}
	for (auto& el : roots) {
		revitalizeViews(el, buffi);
	}
	success = true;
	return roots;
}

void revitalizeViews(MDPMNode& root, const std::string& buffer) {
	if (!root.body.empty()) {
		root.body = std::string_view(&buffer[root.posptr], root.body.size());
	}

	for (auto& child : root.children) {
		child.parent = &root;
		revitalizeViews(child, buffer);
	}
}

bool whitespaceOnlyString(const std::string_view str) {
	if (str.size() == 0) {
		return true;
	}
	for (auto el : str) {
		if (!(el == ' ' || el == '\t')) {
			return false;
		}
	}
	return true;
}
