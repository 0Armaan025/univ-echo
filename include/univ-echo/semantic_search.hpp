#pragma once

#include "database.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace univecho {
std::vector<CommandRecord> semanticSearch(Database &database,
                                          const std::string &question);

}
