/********************************************
 * Copyright (c) 2026 Shun/修海 (@shun4midx) *
 * Project: HyperLogLogPlusPlus-Autocorrect *
 * File Type: C++ file                      *
 * File: Autocorrector.cpp                  *
 ****************************************** */

// ======== INCLUDE ======== //
#include "../include/HLLPP/Autocorrector.h"

#include <iomanip>
#include <sstream>
#include <utility>

// ======== FUNCTIONS ======== //
static std::string lower_string(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());

    std::transform(str.begin(), str.end(), std::back_inserter(lower),
        [](unsigned char c){
            return std::tolower(c);
        }
    );

    return lower;
}

static std::string strip_string(const std::string& str) {
    std::size_t first = str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    std::size_t last = str.find_last_not_of(" \t\r\n");

    return str.substr(first, last - first + 1);
}

static bool looks_like_path(const std::string& str) {
    if (str.size() >= 4 && str.substr(str.size() - 4) == ".txt") {
        return true;
    }

    return str.find('/') != std::string::npos || str.find('\\') != std::string::npos;
}

static std::vector<std::string> read_source(StrVec src) {
    if (auto arr = std::get_if<std::vector<std::string>>(&src)) {
        return *arr;
    }

    if (auto str = std::get_if<std::string>(&src)) {
        std::filesystem::path path{*str};

        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
            std::ifstream in{path};

            if (!in) {
                throw std::runtime_error("Unable to open " + path.string());
            }

            std::vector<std::string> raw;
            std::string line;

            while (std::getline(in, line)) {
                line = strip_string(line);

                if (!line.empty()) {
                    raw.push_back(line);
                }
            }

            return raw;
        }

        if (looks_like_path(*str)) {
            throw std::runtime_error("Input file not found: " + std::filesystem::absolute(path).string());
        }

        return {*str};
    }

    throw std::invalid_argument("src must be a vector of strings, a file path, or a string");
}

std::vector<std::string> extract_qgrams(std::string& word, int q, bool fuzzier) {
    // `fuzzier` is retained only for backwards compatibility and has no effect
    (void)fuzzier;

    if (word.length() < q) {
        return {};
    }

    std::string padded = " " + word + " ";

    if (q != 2) {
        std::unordered_set<std::string> unique;

        for (int i = 0; i < (int)padded.length() - q + 1; ++i) {
            unique.insert(padded.substr(i, q));
        }

        return std::vector<std::string>(unique.begin(), unique.end());
    }

    std::vector<std::string> qgrams;

    for (int i = 0; i < (int)padded.length() - 1; ++i) {
        std::string qgram = padded.substr(i, 2);

        qgrams.push_back(qgram);
        qgrams.push_back(std::string(1, qgram[1]) + std::string(1, qgram[0]));
    }

    return qgrams;
}

bool is_valid(std::string& word, std::unordered_set<char> letters) {
    if (letters.empty()) {
        return true;
    }

    std::string lower = lower_string(word);

    return std::all_of(
        lower.begin(), lower.end(),
        [&](char c){
            return letters.find(c) != letters.end();
        }
    );
}

WordData load_words(std::vector<std::string>& arr, std::unordered_set<char> letters) {
    WordData wd;

    wd.words.reserve(arr.size());
    wd.display.reserve(arr.size());

    for (auto& raw : arr) {
        if (!is_valid(raw, letters)) {
            continue;
        }

        std::string lower = lower_string(raw);

        wd.words.push_back(lower);
        wd.display[lower] = raw;
    }

    return wd;
}

WordData load_words(std::string& str, std::unordered_set<char> letters) {
    StrVec src = str;
    std::vector<std::string> raw = read_source(src);

    return load_words(raw, letters);
}

WordData load_words(StrVec sv, std::unordered_set<char> letters) {
    std::vector<std::string> raw = read_source(sv);

    return load_words(raw, letters);
}

std::vector<std::pair<std::string, std::string>> load_queries(std::vector<std::string>& arr) {
    std::vector<std::pair<std::string, std::string>> result;

    result.reserve(arr.size());

    for (auto& raw : arr) {
        result.push_back({raw, lower_string(raw)});
    }

    return result;
}

std::vector<std::pair<std::string, std::string>> load_queries(std::string& str) {
    StrVec src = str;
    std::vector<std::string> raw = read_source(src);

    return load_queries(raw);
}

std::vector<std::pair<std::string, std::string>> load_queries(StrVec sv) {
    std::vector<std::string> raw = read_source(sv);

    return load_queries(raw);
}

// ======== Autocorrector CLASS: PUBLIC ======== //
Autocorrector::Autocorrector(AutocorrectorCfg& cfg) : Autocorrector(
    cfg.dictionary_list,
    cfg.valid_letters,
    cfg.keyboard,
    cfg.alpha,
    cfg.beta,
    cfg.b,
    cfg.shortlist_size,
    cfg.keyboard_shortlist_size,
    cfg.transposition_bonus
) {}

Autocorrector::Autocorrector(StrVec _dictionary_list, StrVec _valid_letters, StrVec _keyboard, double _alpha, double _beta, int _b, int _shortlist_size, int _keyboard_shortlist_size, double _transposition_bonus) {
    // Deal with allowed letters only
    std::vector<std::string> normalized_letters = StrVecToVec(_valid_letters);

    letters.clear();

    if (!normalized_letters.empty()) {
        for (auto& letter : normalized_letters) {
            if (letter == "a-z") {
                for (char i = 'a'; i <= 'z'; ++i) {
                    letters.insert(i);
                }
            } else if (letter == "0-9") {
                for (char i = '0'; i <= '9'; ++i) {
                    letters.insert(i);
                }
            } else if (letter.length() == 1 && letter != " ") {
                letters.insert(std::tolower((unsigned char)letter[0]));
            } else {
                throw std::invalid_argument("valid_letters must contain single non-space characters or the abbreviations a-z and 0-9");
            }
        }
    }

    // Deal with keyboard
    keyboard.clear();

    if (auto p = std::get_if<std::string>(&_keyboard)) {
        if (*p == "qwerty") {
            keyboard = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
        } else if (*p == "azerty") {
            keyboard = {"1234567890", "azertyuiop", "qsdfghjklm", "wxcvbn"};
        } else if (*p == "qwertz") {
            keyboard = {"1234567890", "qwertzuiopü", "asdfghjklöä", "yxcvbnm"};
        } else if (*p == "dvorak") {
            keyboard = {"1234567890", "'  pyfgcrl", "aoeuidhtns", " qjkxbmwvz"};
        } else if (*p == "colemak") {
            keyboard = {"1234567890", "qwfpgjluy", "arstdhneio", "zxcvbkm"};
        } else {
            throw std::invalid_argument("keyboard must be qwerty, azerty, qwertz, dvorak, colemak, or a custom list of rows");
        }
    } else {
        keyboard = std::get<std::vector<std::string>>(_keyboard);
    }

    KEY_POS.clear();

    for (int i = 0; i < (int)keyboard.size(); ++i) {
        for (int j = 0; j < (int)keyboard[i].size(); ++j) {
            KEY_POS[keyboard[i][j]].x = i;
            KEY_POS[keyboard[i][j]].y = j;
        }
    }

    // Deal with dictionary
    std::vector<std::string> raw;

    if (auto pvec = std::get_if<std::vector<std::string>>(&_dictionary_list)) {
        raw = *pvec;
    } else if (auto pstr = std::get_if<std::string>(&_dictionary_list)) {
        const std::string& key = *pstr;

        if (std::find(addon_files.begin(), addon_files.end(), key) != addon_files.end()) {
            std::filesystem::path base_dir = std::filesystem::path(__FILE__).parent_path().parent_path();
            std::filesystem::path base_path = base_dir / "test_files" / "20k_database.txt";
            std::filesystem::path addon_path = base_dir / "test_files" / (key + ".txt");

            auto load_into = [&](const std::filesystem::path& path) {
                std::ifstream in{path};

                if (!in) {
                    throw std::runtime_error("Cannot open " + path.string());
                }

                std::string line;

                while (std::getline(in, line)) {
                    line = strip_string(line);

                    if (!line.empty()) {
                        raw.push_back(line);
                    }
                }
            };

            load_into(base_path);
            load_into(addon_path);
        } else {
            std::filesystem::path path{key};

            if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
                std::ifstream in{path};

                if (!in) {
                    throw std::runtime_error("Cannot open " + path.string());
                }

                std::string line;

                while (std::getline(in, line)) {
                    line = strip_string(line);

                    if (!line.empty()) {
                        raw.push_back(line);
                    }
                }
            } else {
                std::filesystem::path base_dir = std::filesystem::path(__FILE__).parent_path().parent_path();
                std::filesystem::path relative = base_dir / key;

                if (!(std::filesystem::exists(relative) && std::filesystem::is_regular_file(relative))) {
                    throw std::runtime_error("Dictionary file not found: " + relative.string());
                }

                std::ifstream in{relative};
                std::string line;

                while (std::getline(in, line)) {
                    line = strip_string(line);

                    if (!line.empty()) {
                        raw.push_back(line);
                    }
                }
            }
        }
    }

    WordData wd = load_words(raw, letters);

    word_dict = std::move(wd.words);
    display_map = std::move(wd.display);

    alpha = _alpha; // Retained only for backwards compatibility
    beta = _beta;
    b = _b;
    shortlist_size = _shortlist_size;
    keyboard_shortlist_size = _keyboard_shortlist_size;
    transposition_bonus = _transposition_bonus;

    if (shortlist_size <= 0) {
        throw std::invalid_argument("shortlist_size must be positive");
    }

    if (keyboard_shortlist_size <= 0) {
        throw std::invalid_argument("keyboard_shortlist_size must be positive");
    }

    removed_words.clear();
    compact_threshold = 0.1;

    save_dictionary();
}

void Autocorrector::save_dictionary() {
    t0 = std::chrono::steady_clock::now();

    q = 2;

    cfg = SketchConfig{};
    cfg.b = b;
    cfg.sparse = true;

    word_sketches.clear();
    word_estimates.clear();
    qgram_word_indices.clear();
    word_to_idx.clear();

    WORD_COUNT = word_dict.size();
    ACTIVE_WORD_COUNT = WORD_COUNT - removed_words.size();

    if (WORD_COUNT == 0) {
        throw std::invalid_argument("Dictionary cannot be empty");
    }

    word_sketches.reserve(WORD_COUNT);
    word_estimates.reserve(WORD_COUNT);
    word_to_idx.reserve(WORD_COUNT);

    for (std::size_t idx = 0; idx < word_dict.size(); ++idx) {
        const std::string& word = word_dict[idx];

        word_to_idx[word] = idx;

        std::string mutable_word = word;
        std::vector<std::string> raw_qgrams = extract_qgrams(mutable_word, q);
        std::unordered_set<std::string> qgrams(raw_qgrams.begin(), raw_qgrams.end());

        HyperLogLogPlusPlus word_sketch(cfg);

        for (auto& gram : qgrams) {
            qgram_word_indices[gram].push_back(idx);
            word_sketch.insert("feature:" + gram);
        }

        word_estimates.push_back(word_sketch.estimate());
        word_sketches.push_back(std::move(word_sketch));
    }

    auto end = std::chrono::steady_clock::now();

    preprocessing_time = std::chrono::duration<double>(end - t0).count();
}

std::vector<std::string> Autocorrector::add_dictionary(StrVec to_be_added) {
    WordData worddata = load_words(to_be_added, letters);

    std::vector<std::string> added;
    std::vector<std::string> restored;

    for (auto& word : worddata.words) {
        auto it = word_to_idx.find(word);

        if (it != word_to_idx.end()) {
            if (removed_words.find(word) != removed_words.end()) {
                removed_words.erase(word);
                ++ACTIVE_WORD_COUNT;

                auto display_it = worddata.display.find(word);

                if (display_it != worddata.display.end()) {
                    display_map[word] = display_it->second;
                }

                restored.push_back(word);
            }

            continue;
        }

        std::string display = word;

        auto display_it = worddata.display.find(word);

        if (display_it != worddata.display.end()) {
            display = display_it->second;
        }

        append_word(word, display);
        added.push_back(word);
    }

    added.insert(added.end(), restored.begin(), restored.end());

    return added;
}

std::vector<std::string> Autocorrector::remove_dictionary(StrVec to_be_removed) {
    WordData worddata = load_words(to_be_removed, letters);

    std::vector<std::string> removed;

    for (auto& word : worddata.words) {
        if (word_to_idx.find(word) != word_to_idx.end() && removed_words.find(word) == removed_words.end()) {
            removed_words.insert(word);
            --ACTIVE_WORD_COUNT;
            removed.push_back(word);
        }
    }

    // Rebuild only after a constant fraction of tombstones
    if (ACTIVE_WORD_COUNT > 0 && !word_dict.empty() && removed_words.size() >= word_dict.size() * compact_threshold) {
        std::vector<std::string> compacted;
        compacted.reserve(ACTIVE_WORD_COUNT);

        for (auto& word : word_dict) {
            if (removed_words.find(word) == removed_words.end()) {
                compacted.push_back(word);
            } else {
                display_map.erase(word);
            }
        }

        word_dict = std::move(compacted);
        removed_words.clear();

        save_dictionary();
    }

    return removed;
}

Result Autocorrector::autocorrect(const std::initializer_list<std::string> queries_list, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    return autocorrect((std::vector<std::string>)(queries_list), output_file, use_keyboard, return_invalid_words, print_details, print_times);
}

Results Autocorrector::top_k(const std::initializer_list<std::string> queries_list, int k, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    return top_k((std::vector<std::string>)(queries_list), k, output_file, use_keyboard, return_invalid_words, print_details, print_times);
}

Results Autocorrector::top3(const std::initializer_list<std::string> queries_list, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    return top_k((std::vector<std::string>)(queries_list), 3, output_file, use_keyboard, return_invalid_words, print_details, print_times);
}

Result Autocorrector::autocorrect(const StrVec& queries_list, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    if (print_times) {
        save_dictionary();
    }

    std::vector<std::pair<std::string, std::string>> queries = load_queries(queries_list);

    t2 = std::chrono::steady_clock::now();

    std::vector<std::string> output;
    std::unordered_map<std::string, std::string> suggestions;
    std::unordered_map<std::string, double> final_scores;

    for (auto& [query_display, query] : queries) {
        if (!is_valid(query)) {
            std::string replacement = (return_invalid_words ? query_display : "");

            suggestions[query_display] = replacement;
            final_scores[query_display] = 0.0;
            output.push_back(replacement);

            continue;
        }

        std::vector<RankedCandidate> ranked = rank_candidates(query, use_keyboard, print_details);

        if (ranked.empty()) {
            std::string replacement = (return_invalid_words ? query_display : "");

            suggestions[query_display] = replacement;
            final_scores[query_display] = 0.0;
            output.push_back(replacement);

            continue;
        }

        RankedCandidate best = ranked[0];

        std::string picked = word_dict[best.idx];
        std::string displayed_picked = picked;

        auto display_it = display_map.find(picked);

        if (display_it != display_map.end()) {
            displayed_picked = display_it->second;
        }

        if (print_details) {
            std::cout << "Selected " << std::quoted(displayed_picked)
                      << " for " << std::quoted(query_display)
                      << ": final=" << best.score
                      << ", containment=" << best.structural_score
                      << ", retrieval_overlap=" << best.exact_overlap << "\n";

            std::cout << std::string(60, '-') << "\n";
        }

        suggestions[query_display] = displayed_picked;
        final_scores[query_display] = best.score;
        output.push_back(displayed_picked);
    }

    t3 = std::chrono::steady_clock::now();

    if (output_file != std::filesystem::path("None")) {
        std::ofstream out(output_file);

        if (!out) {
            throw std::runtime_error("Failed to open output file: " + output_file.string());
        }

        for (std::size_t i = 0; i < output.size(); ++i) {
            out << output[i];

            if (i + 1 < output.size()) {
                out << "\n";
            }
        }
    }

    if (print_times) {
        double query_time = std::chrono::duration<double>(t3 - t2).count();

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Dictionary preprocessing:  " << preprocessing_time << "s\n";
        std::cout << "Current query batch:       " << query_time << "s\n";
        std::cout << "Total autocorrect:         " << preprocessing_time + query_time << "s\n";
    }

    return (Result){suggestions, final_scores};
}

Results Autocorrector::top_k(const StrVec& queries_list, int k, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    if (k <= 0) {
        throw std::invalid_argument("k must be positive");
    }

    if (print_times) {
        save_dictionary();
    }

    std::vector<std::pair<std::string, std::string>> queries = load_queries(queries_list);

    t2 = std::chrono::steady_clock::now();

    std::vector<std::string> output;
    std::unordered_map<std::string, std::vector<std::string>> suggestions;
    std::unordered_map<std::string, std::vector<double>> final_scores;

    for (auto& [query_display, query] : queries) {
        std::vector<std::string> top_words;
        std::vector<double> top_scores;

        if (!is_valid(query)) {
            if (return_invalid_words) {
                top_words.push_back(query_display);
            }

            while ((int)top_words.size() < k) {
                top_words.push_back("");
            }

            top_scores.resize(k, 0.0);
        } else {
            std::vector<RankedCandidate> ranked = rank_candidates(query, use_keyboard, print_details);

            std::unordered_set<std::string> seen;

            for (auto& candidate : ranked) {
                std::string word = word_dict[candidate.idx];
                std::string suggestion = word;

                auto display_it = display_map.find(word);

                if (display_it != display_map.end()) {
                    suggestion = display_it->second;
                }

                if (seen.find(suggestion) != seen.end()) {
                    continue;
                }

                seen.insert(suggestion);
                top_words.push_back(suggestion);
                top_scores.push_back(candidate.score);

                if ((int)top_words.size() == k) {
                    break;
                }
            }

            if (top_words.empty() && return_invalid_words) {
                top_words.push_back(query_display);
                top_scores.push_back(0.0);
            }

            while ((int)top_words.size() < k) {
                top_words.push_back("");
                top_scores.push_back(0.0);
            }
        }

        if (print_details) {
            std::cout << "Selected top " << k << " for " << std::quoted(query_display) << ": ";

            bool first = true;

            for (int i = 0; i < k; ++i) {
                if (top_words[i].empty()) {
                    continue;
                }

                if (!first) {
                    std::cout << ", ";
                }

                std::cout << std::quoted(top_words[i]) << " (" << top_scores[i] << ")";
                first = false;
            }

            std::cout << "\n" << std::string(60, '-') << "\n";
        }

        suggestions[query_display] = top_words;
        final_scores[query_display] = top_scores;

        std::ostringstream joined;

        for (int i = 0; i < k; ++i) {
            if (i > 0) {
                joined << " ";
            }

            joined << top_words[i];
        }

        output.push_back(joined.str());
    }

    t3 = std::chrono::steady_clock::now();

    if (output_file != std::filesystem::path("None")) {
        std::ofstream out(output_file);

        if (!out) {
            throw std::runtime_error("Failed to open output file: " + output_file.string());
        }

        for (std::size_t i = 0; i < output.size(); ++i) {
            out << output[i];

            if (i + 1 < output.size()) {
                out << "\n";
            }
        }
    }

    if (print_times) {
        double query_time = std::chrono::duration<double>(t3 - t2).count();

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Dictionary preprocessing:  " << preprocessing_time << "s\n";
        std::cout << "Current query batch:       " << query_time << "s\n";
        std::cout << "Total top-" << k << ":               " << preprocessing_time + query_time << "s\n";
    }

    return (Results){suggestions, final_scores};
}

Results Autocorrector::top3(const StrVec& queries_list, std::filesystem::path output_file, bool use_keyboard, bool return_invalid_words, bool print_details, bool print_times) {
    return top_k(queries_list, 3, output_file, use_keyboard, return_invalid_words, print_details, print_times);
}

// ======== Autocorrector CLASS: PRIVATE ======== //
double Autocorrector::key_dist(char& a, char& b) {
    if (a == b) {
        return 0.0;
    }

    auto it_a = KEY_POS.find(a);
    auto it_b = KEY_POS.find(b);

    if (it_a == KEY_POS.end() || it_b == KEY_POS.end()) {
        return 1.0;
    }

    int dx = it_a->second.x - it_b->second.x;
    int dy = it_a->second.y - it_b->second.y;

    return std::sqrt(dx * dx + dy * dy);
}

double Autocorrector::word_dist(const std::string& a, const std::string& b) {
    int na = a.length();
    int nb = b.length();

    std::vector<double> previous(nb + 1);
    std::vector<double> current(nb + 1, 0.0);

    for (int j = 0; j <= nb; ++j) {
        previous[j] = (double)j;
    }

    for (int i = 1; i <= na; ++i) {
        current[0] = (double)i;

        char ai = a[i - 1];

        for (int j = 1; j <= nb; ++j) {
            char bj = b[j - 1];

            double substitution = previous[j - 1] + key_dist(ai, bj);
            double deletion = previous[j] + 1.0;
            double insertion = current[j - 1] + 1.0;

            current[j] = std::min({substitution, deletion, insertion});
        }

        std::swap(previous, current);
    }

    return previous[nb];
}

bool Autocorrector::is_valid(std::string& word) {
    return ::is_valid(word, letters);
}

std::vector<std::string> Autocorrector::StrVecToVec(StrVec sv) {
    if (auto str = std::get_if<std::string>(&sv)) {
        if (str->empty()) {
            return {};
        }

        return {*str};
    }

    return std::get<std::vector<std::string>>(sv);
}

HyperLogLogPlusPlus Autocorrector::build_query_sketch(const std::unordered_set<std::string>& qgrams) {
    HyperLogLogPlusPlus sketch(cfg);

    for (auto& gram : qgrams) {
        sketch.insert("feature:" + gram);
    }

    return sketch;
}

double Autocorrector::score_from_sizes(double intersection, double left_size) {
    if (left_size <= 0.0) {
        return 0.0;
    }

    double score = intersection / left_size;

    return std::min(std::max(score, 0.0), 1.0);
}

void Autocorrector::append_word(const std::string& word, const std::string& display) {
    std::size_t idx = word_dict.size();

    word_dict.push_back(word);
    display_map[word] = display;
    word_to_idx[word] = idx;

    std::string mutable_word = word;
    std::vector<std::string> raw_qgrams = extract_qgrams(mutable_word, q);
    std::unordered_set<std::string> qgrams(raw_qgrams.begin(), raw_qgrams.end());

    HyperLogLogPlusPlus word_sketch(cfg);

    for (auto& gram : qgrams) {
        qgram_word_indices[gram].push_back(idx);
        word_sketch.insert("feature:" + gram);
    }

    word_estimates.push_back(word_sketch.estimate());
    word_sketches.push_back(std::move(word_sketch));

    ++WORD_COUNT;
    ++ACTIVE_WORD_COUNT;
}

std::vector<std::pair<std::size_t, int>> Autocorrector::candidate_indices(const std::unordered_set<std::string>& query_qgrams) {
    std::unordered_map<std::size_t, int> overlap_counts;

    for (auto& gram : query_qgrams) {
        auto it = qgram_word_indices.find(gram);

        if (it == qgram_word_indices.end()) {
            continue;
        }

        for (std::size_t idx : it->second) {
            if (removed_words.find(word_dict[idx]) != removed_words.end()) {
                continue;
            }

            ++overlap_counts[idx];
        }
    }

    std::vector<std::pair<std::size_t, int>> candidates(
        overlap_counts.begin(),
        overlap_counts.end()
    );

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b){
            if (a.second != b.second) {
                return a.second > b.second;
            }

            return a.first < b.first;
        }
    );

    if ((int)candidates.size() > shortlist_size) {
        candidates.resize(shortlist_size);
    }

    return candidates;
}

bool Autocorrector::is_adjacent_transposition(const std::string& query, const std::string& candidate) {
    if (query.length() != candidate.length()) {
        return false;
    }

    std::vector<std::size_t> mismatches;

    for (std::size_t i = 0; i < query.length(); ++i) {
        if (query[i] != candidate[i]) {
            mismatches.push_back(i);
        }
    }

    if (mismatches.size() != 2) {
        return false;
    }

    std::size_t i = mismatches[0];
    std::size_t j = mismatches[1];

    return j == i + 1 && query[i] == candidate[j] && query[j] == candidate[i];
}

std::vector<RankedCandidate> Autocorrector::rank_candidates(const std::string& query, bool use_keyboard, bool print_details) {
    std::string mutable_query = query;
    std::vector<std::string> raw_qgrams = extract_qgrams(mutable_query, q);
    std::unordered_set<std::string> retrieval_qgrams(raw_qgrams.begin(), raw_qgrams.end());

    if (print_details) {
        std::cout << "Query " << std::quoted(query) << ": features=" << retrieval_qgrams.size() << "\n";
        std::cout << "Structural q-grams: ";

        std::vector<std::string> sorted_qgrams(retrieval_qgrams.begin(), retrieval_qgrams.end());
        std::sort(sorted_qgrams.begin(), sorted_qgrams.end());

        for (std::size_t i = 0; i < sorted_qgrams.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }

            std::cout << std::quoted(sorted_qgrams[i]);
        }

        std::cout << "\n";
    }

    HyperLogLogPlusPlus query_sketch = build_query_sketch(retrieval_qgrams);
    double query_estimate = query_sketch.estimate();

    std::vector<std::pair<std::size_t, int>> candidates = candidate_indices(retrieval_qgrams);
    std::size_t retrieved_count = candidates.size();

    std::unordered_map<std::size_t, int> candidate_map;

    for (auto& [idx, overlap] : candidates) {
        candidate_map[idx] = overlap;
    }

    // Adjacent-transposition rescue
    for (std::size_t i = 0; i + 1 < query.length(); ++i) {
        if (query[i] == query[i + 1]) {
            continue;
        }

        std::string swapped = query;
        std::swap(swapped[i], swapped[i + 1]);

        auto it = word_to_idx.find(swapped);

        if (it != word_to_idx.end() && removed_words.find(swapped) == removed_words.end()) {
            candidate_map.emplace(it->second, 0);
        }
    }

    if (print_details) {
        std::size_t rescued_count = candidate_map.size() - retrieved_count;

        std::cout << "Candidates: "
                  << retrieved_count << " retrieved, "
                  << rescued_count << " transposition-rescued, "
                  << candidate_map.size() << " total\n";
    }

    std::vector<PreliminaryCandidate> preliminary;

    preliminary.reserve(candidate_map.size());

    for (auto& [idx, exact_overlap] : candidate_map) {
        HyperLogLogPlusPlus& word_sketch = word_sketches[idx];
        double word_estimate = word_estimates[idx];

        double structural_union = query_sketch.union_estimate(word_sketch);
        double structural_intersection = std::max(0.0, query_estimate + word_estimate - structural_union);

        structural_intersection = std::min({
            structural_intersection,
            query_estimate,
            word_estimate
        });

        double structural_score = score_from_sizes(structural_intersection, query_estimate);

        const std::string& candidate = word_dict[idx];

        double length_ratio =
            (double)std::abs((int)candidate.length() - (int)query.length()) /
            (double)std::max<std::size_t>(query.length(), 1);

        double length_score = std::max(0.0, 1.0 - length_ratio * length_ratio);
        double exact_bonus = (query == candidate ? 1.0 : 0.0);

        double preliminary_score = structural_score * length_score + exact_bonus;

        preliminary.push_back({
            preliminary_score,
            idx,
            structural_score,
            exact_overlap,
            length_score,
            word_estimate,
            structural_union,
            structural_intersection
        });
    }

    std::sort(preliminary.begin(), preliminary.end(),
        [](const PreliminaryCandidate& a, const PreliminaryCandidate& b){
            if (a.preliminary_score != b.preliminary_score) {
                return a.preliminary_score > b.preliminary_score;
            }

            return a.idx < b.idx;
        }
    );

    std::size_t keyboard_limit = std::min<std::size_t>(
        keyboard_shortlist_size,
        preliminary.size()
    );

    std::vector<RankedCandidate> ranked;

    ranked.reserve(preliminary.size());

    for (std::size_t position = 0; position < preliminary.size(); ++position) {
        PreliminaryCandidate& item = preliminary[position];

        const std::string& candidate = word_dict[item.idx];

        double keyboard_score = 0.0;

        if (use_keyboard && position < keyboard_limit) {
            keyboard_score = 1.0 / (1.0 + word_dist(query, candidate));
        }

        double exact_bonus = (query == candidate ? 1.0 : 0.0);

        double transpose_bonus = (
            is_adjacent_transposition(query, candidate)
            ? transposition_bonus
            : 0.0
        );

        double score =
            item.structural_score * item.length_score +
            beta * keyboard_score +
            transpose_bonus +
            exact_bonus;

        ranked.push_back({
            score,
            item.idx,
            item.structural_score,
            item.exact_overlap,
            item.length_score,
            keyboard_score,
            transpose_bonus,
            exact_bonus,
            item.word_estimate,
            item.structural_union,
            item.structural_intersection
        });
    }

    std::sort(ranked.begin(), ranked.end(),
        [](const RankedCandidate& a, const RankedCandidate& b){
            if (a.score != b.score) {
                return a.score > b.score;
            }

            return a.idx < b.idx;
        }
    );

    if (print_details) {
        std::cout << "Top ranked candidates:\n";

        std::size_t limit = std::min<std::size_t>(5, ranked.size());

        for (std::size_t i = 0; i < limit; ++i) {
            const RankedCandidate& item = ranked[i];
            const std::string& candidate = word_dict[item.idx];

            std::cout << "  " << std::quoted(candidate) << ":\n";
            std::cout << "    retrieval overlap: " << item.exact_overlap << "\n";
            std::cout << "    HLL++ normal: query=" << query_estimate
                      << ", candidate=" << item.word_estimate
                      << ", union=" << item.structural_union
                      << ", intersection=" << item.structural_intersection << "\n";
            std::cout << "    structural: containment=" << item.structural_score << "\n";
            std::cout << "    modifiers: length=" << item.length_score
                      << ", keyboard=" << item.keyboard_score
                      << ", transpose_bonus=" << item.transposition_bonus
                      << ", exact_bonus=" << item.exact_bonus << "\n";
            std::cout << "    contributions: structural*length="
                      << item.structural_score * item.length_score
                      << ", beta*keyboard=" << beta * item.keyboard_score << "\n";
            std::cout << "    final score: " << item.score << "\n";
        }
    }

    return ranked;
}