#ifndef BLOCK_INFO_TAGS_H
#define BLOCK_INFO_TAGS_H
#include <cstddef>

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
	char orderedStart0[9];
	char orderedStartLen;
	symbol_e  symbolUsed;
	int32_t orderedStart;
};

struct IndentedCode {
	size_t firstNewlineSequencePos = 0;
	uint8_t spacePrefixes;
};

struct Paragraph {
	bool endsWithBlankLine;
};

#endif