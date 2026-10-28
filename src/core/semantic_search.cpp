#include "../../include/univ-echo/semantic_search.hpp"

#include <curl/curl.h>
#include <ios>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <iostream>
namespace univecho {
namespace {
using json = nlohmann::json;
using Embedding = std::vector<float>;

namespace fs = std::filesystem;

// constexpr const char *MODEL = "nomic-embed-text";
constexpr const char *MODEL = "qwen3-embedding:0.6b";
constexpr std::size_t BATCH_SIZE = 64;
constexpr std::size_t RESULT_LIMIT = 100;

size_t receiveResponse(char *data, size_t size, size_t count, void *userData) {

  const size_t bytes = size * count;

  static_cast<std::string *>(userData)->append(data, bytes);

  return bytes;
}

// let's change too one request for uncached oness

bool isValidUtf8(const std::string &s) {
  const auto *p = reinterpret_cast<const unsigned char *>(s.data());

  std::size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = p[i];

    if (c <= 0x7F) {
      ++i;
      continue;
    }

    std::size_t length = 0;
    if (c >= 0xC2 && c <= 0xDF)
      length = 2;
    else if (c >= 0xE0 && c <= 0xEF)
      length = 3;
    else if (c >= 0xF0 && c <= 0xF4)
      length = 4;
    else
      return false;

    if (i + length > s.size())
      return false;

    for (std::size_t j = 1; j < length; ++j) {
      if ((p[i + j] & 0xC0) != 0x80)
        return false;
    }

    if (length == 3 &&
        ((c == 0xE0 && p[i + 1] < 0xA0) || (c == 0xED && p[i + 1] >= 0xA0)))
      return false;
    if (length == 4 &&
        ((c == 0xF0 && p[i + 1] < 0x90) || (c == 0xF4 && p[i + 1] >= 0x90)))
      return false;

    i += length;
  }

  return true;
}

std::string sanitizeUtf8(const std::string &input) {
  std::string output;

  output.reserve(input.size());

  const auto *p = reinterpret_cast<const unsigned char *>(input.data());

  std::size_t i = 0;

  while (i < input.size()) {
    const unsigned char c = p[i];

    if (c <= 0x7F) {
      output.push_back(static_cast<char>(c));
      ++i;
      continue;
    }

    std::size_t length = 0;

    if (c >= 0xC2 && c <= 0xDF)
      length = 2;

    else if (c >= 0xE0 && c <= 0xEF)
      length = 3;
    else if (c >= 0xF0 && c <= 0xF4)
      length = 4;

    bool valid = length != 0 && i + length <= input.size();

    if (valid) {
      for (std::size_t j = 1; j < length; ++j) {
        if ((p[i + j] & 0xC0) != 0x80) {
          valid = false;
          break;
        }
      }
    }

    if (valid && length == 3 &&
        ((c == 0xE0 && p[i + 1] < 0xA0) || (c == 0xED && p[i + 1] >= 0xA0))) {
      valid = false;
    }
    if (valid && length == 4 &&
        ((c == 0xF0 && p[i + 1] < 0x90) || (c == 0xF4 && p[i + 1] >= 0x90))) {
      valid = false;
    }

    if (valid) {
      output.append(input, i, length);
      i += length;
    } else {
      // using U+FFFD (unicode's replacement)
      output.append("\xEF\xBF\xBD");
      ++i;
    }
  }

  return output;
}

std::vector<Embedding> embedRemote(const std::vector<std::string> &texts) {

  if (texts.empty()) {
    return {};
  }

  // static const int initialized = [] {
  //   if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
  //     throw std::runtime_error("couldn't initalize libcurl");
  //   }
  //
  //   return 1;
  // }();
  // (void)initialized;
  //
  CURL *curl = curl_easy_init();

  if (!curl) {
    throw std::runtime_error("couldn't initalize ollama http client");
  }

  const json request = {
      {"model", MODEL}, {"input", texts}, {"keep_alive", "5m"}};

  for (std::size_t i = 0; i < texts.size(); ++i) {
    if (!isValidUtf8(texts[i])) {
      std::string message = "Invalid UTF-8 at batch index " +
                            std::to_string(i) + ", input length " +
                            std::to_string(texts[i].size()) + " bytes. Hex: ";

      constexpr char hex[] = "0123456789ABCDEF";

      for (unsigned char c : texts[i]) {
        message += hex[c >> 4];
        message += hex[c & 0x0F];
        message += ' ';
      }

      throw std::runtime_error(message);
    }
  }
  const std::string body = request.dump();
  std::string response;

  curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/embed");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveResponse);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  const CURLcode result = curl_easy_perform(curl);

  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    throw std::runtime_error(std::string("Ollama request failed: ") +
                             curl_easy_strerror(result) +
                             ". Is ollama service runiing?");
  }

  if (status != 200) {
    throw std::runtime_error("Ollama returned http " + std::to_string(status) +
                             ". Check that the model used is installed "
                             "(nomic-embed-text) is installed. ");
  }

  const json parsed = json::parse(response);

  std::vector<Embedding> embeddings;
  embeddings.reserve(texts.size());

  for (const auto &values : parsed.at("embeddings")) {
    Embedding embedding;
    embedding.reserve(values.size());

    for (const auto &value : values) {
      embedding.push_back(value.get<float>());
    }

    if (embedding.empty()) {
      throw std::runtime_error("Ollama returned an empty embedding");
    }

    embeddings.push_back(std::move(embedding));
  }

  if (embeddings.size() != texts.size()) {
    throw std::runtime_error(
        "Ollama returned an unexpected no of embeddings...");
  }

  return embeddings;
}

fs::path cachePath() {
  if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) {
    return fs::path(xdg) / "univecho" / "qwen3-embedding-0.6b-v1.bin";
  }

  if (const char *home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".cache" / "univecho" /
           "qwen3-embedding-0.6b-v1.bin";
  }

  throw std::runtime_error("Cannot locate a user cache directory");
}

class EmbeddingCache {
public:
  void load() {
    if (loaded_) {
      return;
    }
    loaded_ = true;

    std::ifstream in(cachePath(), std::ios::binary);

    if (!in) {
      return;
      // no cache yet
      //
    }

    char magic[8]{};
    in.read(magic, sizeof(magic));
    const char expected[8] = {'U', 'E', 'C', 'H', 'C', '0', '0', '1'};

    std::uint64_t count = 0;
    in.read(reinterpret_cast<char *>(&count), sizeof(count));

    if (!in ||
        !std::equal(std::begin(magic), std::end(magic), std::begin(expected)) ||
        count > 1000000) {
      entries_.clear();
      return;
    }

    for (std::uint64_t i = 0; i < count; ++i) {
      std::uint32_t keySize = 0;
      std::uint32_t dimensions = 0;

      in.read(reinterpret_cast<char *>(&keySize), sizeof(keySize));
      in.read(reinterpret_cast<char *>(&dimensions), sizeof(dimensions));

      if (!in || keySize == 0 || keySize > 1024 * 1024 || dimensions == 0 ||
          dimensions > 16384) {
        entries_.clear();
        return;
      }

      std::string key(keySize, '\0');
      Embedding vector(dimensions);

      in.read(key.data(), static_cast<std::streamsize>(key.size()));
      in.read(reinterpret_cast<char *>(vector.data()),
              static_cast<std::streamsize>(vector.size() * sizeof(float)));

      if (!in) {
        entries_.clear();
        return;
      }

      entries_.insert_or_assign(std::move(key), std::move(vector));
    }
  }

  const Embedding *find(const std::string &key) const {
    const auto it = entries_.find(key);
    return it == entries_.end() ? nullptr : &it->second;
  }

  void insert(const std::string &key, Embedding vector) {
    entries_.insert_or_assign(key, std::move(vector));
    dirty_ = true;
  }

  void save() {
    if (!dirty_) {
      return;
    }

    const fs::path destination = cachePath();
    const fs::path temporary = destination.string() + ".tmp";
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);

    if (ec) {
      throw std::runtime_error(
          "Cannot create embedding cache directory!!!!!!!!!!! " + ec.message());
      // i feel tired bro
    }

    {
      std::ofstream out(temporary, std::ios::binary | std::ios::trunc);

      if (!out) {
        throw std::runtime_error("Cannot write embedding cache");
      }

      const char magic[8] = {'U', 'E', 'C', 'H', 'C', '0', '0', '1'};

      const auto count = static_cast<std::uint64_t>(entries_.size());

      out.write(magic, sizeof(magic));

      out.write(reinterpret_cast<const char *>(&count), sizeof(count));

      for (const auto &[key, vector] : entries_) {
        if (key.size() > UINT32_MAX || vector.size() > UINT32_MAX) {
          throw std::runtime_error("embedding cache too large");
        }

        const auto keySize = static_cast<std::uint32_t>(key.size());
        const auto dimensions = static_cast<std::uint32_t>(vector.size());

        out.write(reinterpret_cast<const char *>(&keySize), sizeof(keySize));
        out.write(reinterpret_cast<const char *>(&dimensions),
                  sizeof(dimensions));

        out.write(key.data(), static_cast<std::streamsize>(key.size()));

        out.write(reinterpret_cast<const char *>(vector.data()),
                  static_cast<std::streamsize>(vector.size() * sizeof(float)));
      }

      if (!out) {
        throw std::runtime_error("failed while writing embedding cache");
      }
    }

    fs::rename(temporary, destination, ec);

    if (ec) {
      fs::remove(temporary);
      throw std::runtime_error("Cannot finalize embedding cache: " +
                               ec.message());
    }

    dirty_ = false;
  }

private:
  bool loaded_ = false;
  bool dirty_ = false;

  std::unordered_map<std::string, Embedding> entries_;
};

EmbeddingCache &embeddingCache() {
  static EmbeddingCache cache;

  return cache;
}

std::vector<Embedding> embedCache(const std::vector<std::string> &texts) {
  auto &cache = embeddingCache();
  cache.load();

  std::vector<Embedding> results(texts.size());
  std::vector<std::string> missing;
  std::vector<std::size_t> missingPositions;
  missing.reserve(texts.size());
  missingPositions.reserve(texts.size());

  for (std::size_t i = 0; i < texts.size(); ++i) {
    if (const auto *cached = cache.find(texts[i])) {
      results[i] = *cached;

    } else {
      missing.push_back(texts[i]);
      missingPositions.push_back(i);
    }
  }

  for (std::size_t start = 0; start < missing.size(); start += BATCH_SIZE) {
    const std::size_t end = std::min(start + BATCH_SIZE, missing.size());

    std::vector<std::string> batch(
        missing.begin() + static_cast<std::ptrdiff_t>(start),
        missing.begin() + static_cast<std::ptrdiff_t>(end));

    auto vectors = embedRemote(batch);

    for (std::size_t j = 0; j < vectors.size(); ++j) {
      const std::size_t position = missingPositions[start + j];
      results[position] = vectors[j];
      cache.insert(missing[start + j], std::move(vectors[j]));
    }
  }

  cache.save();

  return results;
}

float similarity(const Embedding &a, const Embedding &b) {
  if (a.size() != b.size() || a.empty()) {
    return -1.0F;
  }

  float dot = 0.0F;
  float normA = 0.0F;
  float normB = 0.0f;

  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    normA += a[i] * a[i];
    normB += b[i] * b[i];
  }

  if (normA == 0.0F || normB == 0.0F) {
    return -1.0F;
  }

  return dot / (std::sqrt(normA) * std::sqrt(normB));
}

} // namespace
//
//

std::vector<CommandRecord> semanticSearch(Database &database,
                                          const std::string &question) {
  if (question.empty()) {
    return {};
  }

  const auto records = database.listCommands();

  if (records.empty()) {
    return {};
  }

  const auto queryVectors =
      embedCache({"Instruct: retrieve the most relevant shell command for "
                  "user's request \nQuery:  " +
                  sanitizeUtf8(question)});

  const auto &queryVector = queryVectors.front();

  std::vector<std::pair<float, CommandRecord>> ranked;
  ranked.reserve(records.size());

  std::vector<std::string> documents;
  documents.reserve(records.size());

  for (const auto &record : records) {
    documents.push_back(sanitizeUtf8(record.command));
  }

  const auto documentVectors = embedCache(documents);

  for (std::size_t i = 0; i < documentVectors.size(); ++i) {

    ranked.emplace_back(similarity(queryVector, documentVectors[i]),
                        records[i]);
  }

  const std::size_t count = std::min(RESULT_LIMIT, ranked.size());

  if (count < ranked.size()) {
    std::partial_sort(
        ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(count),
        ranked.end(),
        [](const auto &a, const auto &b) { return a.first > b.first; });
  } else {
    std::sort(ranked.begin(), ranked.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });
  }

  std::vector<CommandRecord> results;

  results.reserve(count);

  for (std::size_t i = 0; i < count; ++i) {
    results.push_back(std::move(ranked[i].second));
  }

  return results;
}

} // namespace univecho
