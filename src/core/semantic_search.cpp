#include "../../include/univ-echo/semantic_search.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace univecho {
namespace {
using json = nlohmann::json;
using Embedding = std::vector<float>;

size_t receiveResponse(char *data, size_t size, size_t count, void *userData) {

  const size_t bytes = size * count;

  static_cast<std::string *>(userData)->append(data, bytes);

  return bytes;
}

} // namespace
} // namespace univecho
