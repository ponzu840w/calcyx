#include "completion_match.h"
#include <algorithm>
#include <cctype>

static bool ieq_char(char a, char b) {
    return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
}

bool completion_icontains(const std::string &hay, const std::string &needle) {
    if (needle.empty()) return true;
    return std::search(hay.begin(), hay.end(),
                       needle.begin(), needle.end(), ieq_char) != hay.end();
}

bool completion_istartswith(const std::string &s, const std::string &p) {
    if (p.size() > s.size()) return false;
    return std::equal(p.begin(), p.end(), s.begin(), ieq_char);
}
