#pragma once

#include "database.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace std;

namespace univecho {
std::vector<CommandRecord> semanticSearch(Database &database,
                                          const std::string &question);

}
