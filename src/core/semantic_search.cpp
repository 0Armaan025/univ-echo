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

std::vector<Embedding> embed(const std::vector<std::string> &texts) {

  if (texts.empty()) {
    return {};
  }

  static const int initialized = [] {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw std::runtime_error("couldn't initalize libcurl");
    }

    return 1;
  }();
  (void)initialized;

  CURL *curl = curl_easy_init();

  if (!curl) {
    throw std::runtime_error("couldn't initalize ollama http client");
  }

  const json request = {
      {"model", "nomic-embed-text"}, {"input", texts}, {"keep_alive", "5m"}};

  const std::string body = request.dump();
  std::string response;

  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/embed");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveResponse);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

  struct curl_slist *headers = nullptr;

  headers = curl_slist_append(headers, "Content-Type: application/json");
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
                             ". Check that all-minilm is installed. ");
  }

  const json parsed = json::parse(response);

  std::vector<Embedding> embeddings;

  for (const auto &vector : parsed.at("embeddings")) {
    Embedding embedding;
    embedding.reserve(vector.size());

    for (const auto &value : vector) {
      embedding.push_back(value.get<float>());
    }

    embeddings.push_back(std::move(embedding));
  }

  if (embeddings.size() != texts.size()) {
    throw std::runtime_error(
        "Ollama returned an unexpected no of embeddings...");
  }

  return embeddings;
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

  const auto questionVectors = embed({"search_query: " + question});

  const auto &questionVector = questionVectors.front();

  std::vector<std::pair<float, CommandRecord>> ranked;
  ranked.reserve(records.size());

  constexpr std::size_t batchSize = 64;

  for (std::size_t start = 0; start < records.size(); start += batchSize) {
    const std::size_t end = std::min(start + batchSize, records.size());

    std::vector<std::string> commands;
    commands.reserve(end - start);

    for (std::size_t i = start; i < end; ++i) {
      commands.push_back("search_document: " + records[i].command);
    }

    const auto commandVectors = embed(commands);

    for (std::size_t i = 0; i < commandVectors.size(); ++i) {
      ranked.emplace_back(similarity(questionVector, commandVectors[i]),
                          records[start + i]);
    }
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  std::vector<CommandRecord> results;
  const std::size_t count = std::min<std::size_t>(100, ranked.size());
  results.reserve(count);

  for (std::size_t i = 0; i < count; ++i) {
    results.push_back(std::move(ranked[i].second));
  }

  return results;
}
} // namespace univecho
