#include "MDParseMan.h"
#include <istream>
#include <sstream>
#include <regex>
#include <string_view>
#include <cstring>
#include <charconv>
#include <assert.h>

/*
struct mdpm_parser::MDPMContext {
};*/


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
		posptr_t           currLine;
	};
}
MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state);
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state);
MDPMNode* processNodeMatchExisting(MDPMNode& node, mdpm_parser::Context& ctx, const std::string& line, mdpm_impl::BlobParsingState& state);
void _injectText(mdpm_parser::Context& ctx, MDPMNode& node, const char* str, size_t len, size_t strpos);



MDPMNode* processNodeMatchExisting(MDPMNode& node, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state) {
	return _processNodeMatchExistingImpl(node, cotx, line, state);
}

bool _processSingle(MDPMNode& n, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state);
MDPMNode* _processNodeMatchExistingImpl(MDPMNode& node, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state) {
	//_processSingle(node, cotx, line, state);
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
bool _processSingle(MDPMNode& n, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state) {
	bool matching{};
	switch (n.flavor) {
	case MDPMNode::type_e::Document: matching = true;//do nothing?
		break;
	case MDPMNode::type_e::IndentedCode: matching = mdpm_parser::matchCode(cotx, n, line.c_str(), state);
		break;
	case MDPMNode::type_e::Paragraph: matching = mdpm_parser::matchParagraphLine(cotx, n, line, state);
		break;
	case MDPMNode::type_e::Quote: matching = mdpm_parser::findBlockQuote(cotx, n, line.c_str(), state, true);
		break;
	case MDPMNode::type_e::List: matching = mdpm_parser::matchList(cotx, n, line.c_str(), state);
		break;
#if 1
	case MDPMNode::type_e::ListItem: matching = mdpm_parser::matchListItem(cotx, n, line.c_str(), state);
		break;
#endif
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
		//silly_t mebo{};
		//auto& sumtn = std::get<ListItemInfo>(mebo);

		//node.children.push_back({ MDPMNode::type_e::Paragraph , true, true, 0, {}, &node, {}, silly_t{} });
		return true;
	}
	else {
		return false;
	}
}
bool mdpm_parser::matchParagraphLine(Context& cotx, MDPMNode& node, const std::string& line, mdpm_impl::BlobParsingState& currSitu) {
	using silly_t = std::variant<ListItemInfo>;
	std::smatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	bool res = std::regex_match(line.cbegin() + pos, line.cend(), matches, parLine2Regex);
	if (res) {
		pos += matches[1].first - matches[0].first;
		currSitu.matching = true;
		currSitu.pos = pos;
		assert(node.isOpen == true);
		if (node.flavor == MDPMNode::type_e::Paragraph) {
#if 0
			auto& sameNode = node.children.back();
			cotx.textBuffer.push_back('\n');
			cotx.textBuffer.insert(cotx.textBuffer.end(), line.cbegin() + currSitu.pos, line.cend());
			sameNode.body = {cotx.textBuffer.data() + sameNode.posptr, cotx.textBuffer.size() - sameNode.posptr};
#else
			//cotx.textBuffer.push_back('\n');
			//_injectText(cotx, node, line.c_str(), line.size(), currSitu.pos);
#endif
		}
	}
	else {
		currSitu.matching = false;
		if (node.flavor == MDPMNode::type_e::Paragraph) {
			//node.matching = false;
			if (!node.children.empty()) {
				//node.children.back().matching = false;
			}
		}
	}
	return res;
}
size_t mdpm_parser::matchParagraphLineLite(const std::string_view line, const mdpm_impl::BlobParsingState& state) {
	std::cmatch matches{};
	if (std::regex_search(line.data() + state.pos, matches, parLineRegex)) {
		return matches[1].first - matches[0].first;
	}
	else {
		return NPOS;
	}
}
std::regex indentedCodeRegex{ "^(    )|(	)(\\W+)" };
bool mdpm_parser::findCode(Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
	std::cmatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	bool res = std::regex_search(line + pos, matches, indentedCodeRegex);
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
	return res;
}
std::regex indentedCode2Regex{"^( {0,3})( |\\t)?[ \\t]*([^ \\t])?.*"};
bool mdpm_parser::matchCode(Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
	std::cmatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	bool matchRes{};
	bool res = std::regex_match(line + pos, matches, indentedCode2Regex);
	if (res) {
		if ((matches[0].first == matches[0].second) || !matches[3].matched) {
			if (matches[2].matched) {
				pos += (matches[2].second - matches[0].first); // add any additional whitespaces after code-indent prefix if they exist
			}
			else {
				pos += (matches[2].second - matches[0].first); // less than 4 spaces (and no tab present for 3 last char positions) and nothing else: will be transformed to newline only
			}

			auto& property = std::get<IndentedCode>(node.crtrstc);
			if (property.firstNewlineSequencePos == mdpm_parser::NPOS) {
				property.firstNewlineSequencePos = cotx.textBuffer.size();
			}
			matchRes = true;
		}
		else if (matches[2].matched) {
			pos += (matches[2].second - matches[0].first);

			auto& property = std::get<IndentedCode>(node.crtrstc);
			if (property.firstNewlineSequencePos != mdpm_parser::NPOS) {
				property.firstNewlineSequencePos = mdpm_parser::NPOS;
			}

			matchRes = true;
		}
		if (matchRes) {
			currSitu.pos = pos;
			currSitu.matching = true;
		}
	}
#if 0
	else {
		MDPMNode* n = &node;
		n->matching = false;
		while (!n->children.empty()) {
			n = &n->children.back();
			n->matching = false;
		}
	}
#endif
	return matchRes;
}
std::regex listRegex("^ {0,3}([-+*]|(\\d\\d{0,8})[.)])? {1,4}( *)");
std::regex listItemRegex{ "^ {0,3}([-+*]|(\\d\\d{0,8})[.)]) {1,4}( *)" };
bool mdpm_parser::matchList(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
	//std::optional<ListInfo> retValue{};
	std::cmatch matches{};

	decltype(matches.length()) nSymCons = 0;
	bool resi = std::regex_search(line + currSitu.pos, matches, listRegex);
	bool matchRes{};

	if (resi) {
		if (matches[1].length() != 0) {
			//retValue.emplace();

#if 0
			if (matches[2].matched) {
				auto res = std::from_chars(matches[2].first, matches[2].second, retValue->orderedStart);
				if (res.ptr != matches[2].second) {
					std::exit(99);
				}
				retValue->symbolUsed = static_cast<ListInfo::symbol_e>(*matches[2].second);
			}
#endif

			//spacings[0] = static_cast<char>(matches[1].first - matches[0].first);
			//spacings[1] = static_cast<char>(matches[1].second - matches[1].first);

			if (matches[3].length() == 0) {
				nSymCons += matches.length();//` -1;
				//spacings[2] = static_cast<char>(matches[3].first - matches[1].second);
			}
			else {
				nSymCons += (matches[1].second - matches[0].first + 1);
				//spacings[2] = 1;
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
	if (!matchRes && node.flavor == MDPMNode::type_e::List) {
		MDPMNode* n = &node;
#if 0
		n->matching = false;
		while (!n->children.empty()) {
			n = &n->children.back();
			n->matching = false;
		}
#endif
	}
	return matchRes;
	//return retValue;
}
auto mdpm_parser::findList(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, char(&spacings)[3]) -> std::optional<ListInfo>
{
	std::cmatch matches{};

	std::optional<ListInfo> retValue{};

	decltype(matches.length()) nSymCons = 0;
	bool resi = std::regex_search(line + currSitu.pos, matches, listItemRegex);

	if (resi) {
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
auto mdpm_parser::findListItem(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu) -> std::optional<ListItemInfo>
{
	currSitu.matching = false;
	std::optional<ListItemInfo> retValue{};
	std::cmatch matches{};

	decltype(matches.length()) pos = currSitu.pos;
	bool resi = std::regex_search(line + pos/*line2*/, matches, listItemRegex);

	if (resi) {
		retValue.emplace();
		auto& charact = *retValue; //std::get<ListItemInfo>(newItemNode.crtrstc);

		charact.preIndent = static_cast<char>(matches[1].first - matches[0].first);
		charact.sizeW = static_cast<char>(matches[1].second - matches[1].first);

		if (matches[3].length() == 0) {
			pos += matches.length() - 1;
			charact.postIndent = static_cast<char>((matches[3].first - matches[1].second));
		}
		else {
			pos += (matches[1].second - matches[0].first + 1);
			charact.postIndent = 1;
		}


		if (matches[2].matched) {
		}
		else {
		}

		if (currSitu.targetType != MDPMNode::type_e::ListItem) {
			currSitu.targetType = MDPMNode::type_e::ListItem;
		}
		currSitu.matching = true;
		currSitu.pos = pos;
	}
	return retValue;
}
std::regex listItemRegex2{"^(  +)\\S"};
bool mdpm_parser::matchListItem(Context& ctx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu)
{
	bool resi{};
	std::cmatch matches{};
	decltype(matches.length()) pos = currSitu.pos;
	bool matching{};

	if (currSitu.preCmp) {
		if (currSitu.matching) {

			matching = true;
			currSitu.preCmp = false;
			currSitu.matching = true;
			if (currSitu.targetType != MDPMNode::type_e::ListItem) {
				currSitu.targetType = MDPMNode::type_e::ListItem;
			}
		}
		else {
			resi = false;
		}
		// positioned already, no need to do it again.
		//return;
	}
	else {
		resi = std::regex_search(line + pos, matches, listItemRegex2);
	}
	decltype(pos) startingPos = 0;
	decltype(pos) endPos = 0;

	if (resi) {
		startingPos = matches.position();
		endPos = matches[1].second - matches[1].first;

		
		if (currSitu.targetType != MDPMNode::type_e::ListItem) {
			currSitu.targetType = MDPMNode::type_e::ListItem;
		}

		auto& listState = std::get<ListItemInfo>(node.crtrstc);
		if ((listState.preIndent + listState.sizeW + listState.postIndent) <= (endPos - startingPos)) {
			pos += (listState.preIndent + listState.sizeW + listState.postIndent);
			matching = true;
			currSitu.matching = true;
			currSitu.pos = pos;
		}
	}
	if (!matching && (node.flavor == MDPMNode::type_e::ListItem)) {
		MDPMNode* n = &node;
#if 0
		n->matching = false;
		while (!n->children.empty()) {
			n = &n->children.back();
			n->matching = false;
		}
#endif
	}
	return matching;
}

//, std::regex::extended);
std::regex blockQuoteRegex("^ {0,3}> ?(.*)");
bool mdpm_parser::findBlockQuote(Context& cotx, MDPMNode& node, const char* line, mdpm_impl::BlobParsingState& currSitu, bool continuation) {
	
	std::cmatch matches{};

	decltype(matches.length()) pos = currSitu.pos;
#if 0
	bool resi{};
	while (resi = std::regex_search(line + pos, matches, blockQuoteRegex)) {
		auto matchPos = matches.position();
		pos += matches.length();
	}
#else
	bool resi = std::regex_search(line + pos, matches, blockQuoteRegex);
#endif

	if (resi) {
		pos += matches[1].first - matches[0].first;
		if (currSitu.targetType != MDPMNode::type_e::Quote) {
			currSitu.targetType = MDPMNode::type_e::Quote;
		}
		currSitu.matching = true;
		currSitu.pos = pos;
		/*
		if (continuation && node.flavor == MDPMNode::type_e::Quote) {
			cotx.textBuffer.push_back('\n');
			_injectText(cotx, node.children.back(), line, std::strlen(line), currSitu.pos);
		}
		*/
	}
	else if (continuation && node.flavor == MDPMNode::type_e::Quote) {
		MDPMNode* n = &node;
#if 0
		n->matching = false;
		while (!n->children.empty()) {
			n = &n->children.back();
			n->matching = false;
		}
#endif
	}
	return resi;
}

// <OLD>pre: 'node' and its children are all unmatched, but its parent is still matched to 'line' being matched against
// pre: 'node' is a matched node, but its last child is not matched (and presumably all ancestors of that child)
MDPMNode* processNodeMatchAnyNew(MDPMNode& node, mdpm_parser::Context& cotx, const std::string& line, mdpm_impl::BlobParsingState& state) {
	if (node.flavor == MDPMNode::type_e::TextLiteral) {
		return nullptr;
	}
	MDPMNode* newborn{};
	//mdpm_impl::BlobParsingState state{};
	//state.pos = pos;
	//MDPMNode* ni = &node;
#if 0
	while (!ni->children.empty() && ni->matching) {
		ni = &ni->children.back();
	}
#endif

	char listItemWidths[3];

	if (mdpm_parser::findCode(cotx, node, line.c_str(), state)) {
		if (!node.children.empty()) {
			MDPMNode* n = &node.children.back();
#if 0
			do {
				n->isOpen = false;
			} while (!n->children.empty() && (n = &n->children.back()));
#endif
		}
		newborn = &node.children.emplace_back(MDPMNode{ MDPMNode::type_e::IndentedCode, true, cotx.textBuffer.size(), std::string_view{}, &node, std::list<MDPMNode>{
			/*{MDPMNode::type_e::TextLiteral, true, true, cotx.textBuffer.size(), {}, nullptr, {} }*/
			}, {IndentedCode{static_cast<size_t>(-1)}} });
#if 0
		auto& textNode = node.children.back().children.back();
		textNode.parent = &node.children.back();
#endif
		//_injectText(cotx, node.children.back(), line.data(), line.size(), state.pos);
	}
	else if (mdpm_parser::findBlockQuote(cotx, node, line.c_str(), state)) {
		if (!node.children.empty()) {
			MDPMNode* n = &node.children.back();
			//n = ni;
			do {
				n->isOpen = false;
			} while (!n->children.empty() && (n = &n->children.back()));
		}
		node.children.emplace_back(MDPMNode{ MDPMNode::type_e::Quote,true, 0, {}, &node, {}, {} });

		newborn = processNodeMatchAnyNew(node.children.back(), cotx, line, state);
	}

	else if (std::optional<ListInfo> result;  (node.flavor != MDPMNode::type_e::List) && (result = mdpm_parser::findList(cotx, node, line.c_str(), state, listItemWidths))) {
		if (!node.children.empty()) {
			MDPMNode* n = &node.children.back();
			bool* cool = new bool{ true };
			//n = ni;
			do {
				n->isOpen = false;
			} while (!n->children.empty() && (n = &n->children.back()));
		}

		node.children.push_back({ MDPMNode::type_e::List, true, 0, {}, &node,  {
			{MDPMNode::type_e::ListItem, true, 0, {}, nullptr, {}, {ListItemInfo{ listItemWidths[0], listItemWidths[1], listItemWidths[2]}}}
			}, *result
			});
		node.children.back().children.back().parent = &node.children.back();

		newborn = processNodeMatchAnyNew(node.children.back().children.back(), cotx, line, state);
	}
	else if (auto result = mdpm_parser::findListItem(cotx, node, line.c_str(), state)) {
		MDPMNode* n = &node.children.back();
		bool* cool = new bool{ true };
		//n = ni;
		do {
			n->isOpen = false;
		} while (!n->children.empty() && (n = &n->children.back()));

		if (node.flavor != MDPMNode::type_e::List) {
			node.children.push_back({ MDPMNode::type_e::List, true, 0, {}, &node, {}, {} });
			node.children.back().children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node.children.back(), {}, *result });
			newborn = processNodeMatchAnyNew(node.children.back().children.back(), cotx, line, state);
		}
		else {
			node.children.push_back({ MDPMNode::type_e::ListItem, true, 0, {}, &node, {}, *result });
			newborn = processNodeMatchAnyNew(node.children.back(), cotx, line, state);
		}
#if 0
		if (node.parent) {
			n = node.parent;
		}
		else if (node.flavor == MDPMNode::type_e::Document) {
			//this must be document root
			n = &node;
		}
		else {
			n = reinterpret_cast<MDPMNode*>(0xffffffffffffffff);
		}
		if (!n->children.empty()) {
			auto& e = n->children.back();
			e.isOpen = false;
		}
		n->children.push_back({});

#endif
	}
	/*
	else if( mdpm_parser::matchParagraphLineNew(cotx, node, line.c_str(), state) ){
		
		if (!node.children.empty()) {
			MDPMNode* n = &node.children.back();
			do {
				n->isOpen = false;
			} while (!n->children.empty() && (n = &n->children.back()));
		}

		newborn = &node.children.emplace_back(MDPMNode{ MDPMNode::type_e::Paragraph, true, true, cotx.textBuffer.size(), {}, &node, {
			/*{MDPMNode::type_e::TextLiteral, true, true, cotx.textBuffer.size(), {}, nullptr, {} }*//*
			} });
//#if 0
		auto& textNode = node.children.back().children.back();
		textNode.parent = &node.children.back();

		cotx.textBuffer.insert(cotx.textBuffer.end(), std::begin(line) + state.pos, std::cend(line));
		textNode.body = std::string_view{cotx.textBuffer.data() + textNode.posptr, cotx.textBuffer.size() - textNode.posptr};
//#endif
		_injectText(cotx, node.children.back(), line.c_str(), line.size(), state.pos);
	}*/
	else {
		if (node.flavor == MDPMNode::type_e::ListItem) {
			newborn = &node;
		}
		else {
			newborn = &node;
		}
	}
#if 0
	auto it = node.children.end();
	if ((it != node.children.begin()) && !(--it)->matching) {
		return &(*it);
	}
#endif
	return newborn;//node.children.end();
}
void _injectText(mdpm_parser::Context& ctx, MDPMNode& node, const char* str, size_t len, size_t strpos) {
	ctx.textBuffer.insert(ctx.textBuffer.end(), str + strpos, str+len);
	node.body = { ctx.textBuffer.data() + node.posptr, ctx.textBuffer.size() - node.posptr };
}
MDPMNode* resolveNode(MDPMNode* n) {
	n->isOpen = false;
	return n->parent;
}

void resolveDematchedChainLinksExtractText(mdpm_parser::Context& cotx, 
	                                   MDPMNode* matchedChainLinkTail, 
	                                   MDPMNode* boilingChainLinkTail, 
	                                mdpm_impl::BlobParsingState state, 
	                                         const std::string_view s) 
{
	MDPMNode* newborn{};
	while (cotx.freshestBlock != matchedChainLinkTail) {
		cotx.freshestBlock = resolveNode(cotx.freshestBlock);
	}
	if (boilingChainLinkTail->flavor == MDPMNode::type_e::IndentedCode) {
		//cotx.textBuffer.push_back('\n');
		_injectText(cotx, *boilingChainLinkTail, s.data(), s.size(), state.pos);
	}
	else if (boilingChainLinkTail->flavor == MDPMNode::type_e::Paragraph) {
		auto indentSize = mdpm_parser::matchParagraphLineLite(s, state);
		if (indentSize != mdpm_parser::NPOS) {
			cotx.textBuffer.push_back('\n');
			_injectText(cotx, *boilingChainLinkTail, s.data(), s.size(), state.pos);
		}
	}
	else if (mdpm_parser::matchParagraphLineLite(s, state) == mdpm_parser::NPOS) {
		//nothing
	}
	else {
		newborn = &boilingChainLinkTail->children.emplace_back(MDPMNode{ MDPMNode::type_e::Paragraph, true, cotx.textBuffer.size(), {}, boilingChainLinkTail, {} });
		_injectText(cotx, *newborn, s.data(), s.size(), state.pos);
		boilingChainLinkTail = newborn;
	}
	cotx.freshestBlock = boilingChainLinkTail;

}


void mdpm_parser::processLine(Context& cotx, MDPMNode& node, std::istream& inp) {
	mdpm_impl::BlobParsingState sam{};
	for (std::string line; std::getline(inp, line, '\n');) {
		matchParagraphLineNew(cotx, node, line.c_str(), sam);
	}
}

void mdpm_parser::processLine(Context& cotx, MDPMNode& root, const std::string& line) {
	mdpm_impl::BlobParsingState state{};
	std::vector<mdpm_impl::NodeQry> depthStack{};
	depthStack.reserve(8);
	auto * matchChainLinkTail = processNodeMatchExisting(root, cotx, line, state);
	MDPMNode* nodeptr = &root;

	MDPMNode * newborn = processNodeMatchAnyNew(*matchChainLinkTail/*nodeptr*/, cotx, line, state);

	resolveDematchedChainLinksExtractText(cotx, matchChainLinkTail, newborn, state, line);
}

