
#include <vector>
#include <string>
#include "ctre.hpp"
#include "ctll.hpp"
#include <charconv>

static constexpr auto pattern = ctll::fixed_string{ "([^,]+),([0-9]+)" };
std::vector<std::string>::const_iterator extractData(std::vector<std::pair<std::string, int>>& dest, const std::vector<std::string>& args) {
	size_t failed_index = 0;
	for (const auto& arg : args) {
		auto match = ctre::match<pattern>(arg);
		if (!match) {
			return args.cbegin() + failed_index;
		}
		int val{};
		auto result = std::from_chars(&*match.get<2>().begin(), &*match.get<2>().end(), val);
		dest.emplace_back(match.get<1>().to_string(), val);
		++failed_index;
	}
	return args.cend();
}
