#ifndef STRING_PARTS_H
#define STRING_PARTS_H
#include <string_view>
using tgstr = const char*;
using tgsv = std::string_view;

tgstr includes =
"#include \"md_parseman/MDParseMan.h\"\n#include <gtest/gtest.h>\n\n";

tgstr anonNamespaceBegin = "namespace {\n";
tgstr anonNamespaceEnd = "}\n";


#endif