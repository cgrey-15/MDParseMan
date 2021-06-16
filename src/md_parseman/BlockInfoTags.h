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
	bool isTight;
};

struct IndentedCode {
	size_t firstNewlineSequencePos = 0;
	uint8_t spacePrefixes;
};

struct FencedCode {
	enum class symbol_e : char {
		Tilde = '~', BackTick = '`'
	};
	int32_t           length;
	symbol_e            type;
	int8_t            indent;
	std::string_view infoStr;
};

constexpr auto sizei = sizeof(FencedCode);

struct Paragraph {
	bool endsWithBlankLine;
};

struct Heading {
	char lvl;
	bool isSetext;
};

namespace md_parseman {


}

#endif