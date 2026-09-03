/********************************************
 * Copyright (c) 2026 Shun/修海 (@shun4midx) *
 * Project: HyperLogLogPlusPlus-Autocorrect *
 * File Type: C++ Header file               *
 * File: Autocorrector.h                    *
 ****************************************** */

// ======== INCLUDE ======== //
#pragma once
#include "HyperLogLogPlusPlus.h"
#include <iostream>
#include <unordered_map>
#include <variant>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <chrono>
#include <initializer_list>

// ======== DEFINE ======== //
using StrVec = std::variant<std::string, std::vector<std::string>>;
static const std::vector<std::string> addon_files = {"texting"};

// ======== STRUCT ======== //
typedef struct AutocorrectorCfg {
    StrVec dictionary_list = std::filesystem::path{"test_files"} / "20k_database.txt";
    StrVec valid_letters = "a-z";
    StrVec keyboard = "qwerty";
    double alpha = 0.2; // Retained only for backwards compatibility
    double beta = 0.85;
    int b = 10;
    int shortlist_size = 100;
    int keyboard_shortlist_size = 75;
    double transposition_bonus = 0.35;
} AutocorrectorCfg;

typedef struct WordData {
    std::vector<std::string> words;
    std::unordered_map<std::string, std::string> display;
} WordData;

typedef struct MultiResults {
    std::unordered_map<std::string, std::vector<std::string>> suggestions;
    std::unordered_map<std::string, std::vector<double>> scores;
} Results;

typedef struct SingleResult {
    std::unordered_map<std::string, std::string> suggestions;
    std::unordered_map<std::string, double> scores;
} Result;

struct Coord {
    int x;
    int y;
};

struct RankedCandidate {
    double score;
    std::size_t idx;
    double structural_score;
    int exact_overlap;
    double length_score;
    double keyboard_score;
    double transposition_bonus;
    double exact_bonus;
    double word_estimate;
    double structural_union;
    double structural_intersection;
};

struct PreliminaryCandidate {
    double preliminary_score;
    std::size_t idx;
    double structural_score;
    int exact_overlap;
    double length_score;
    double word_estimate;
    double structural_union;
    double structural_intersection;
};

// ======== FUNCTION PROTOTYPES ======== //
std::vector<std::string> extract_qgrams(std::string& word, int q = 2, bool fuzzier = false);

bool is_valid(std::string& word, std::unordered_set<char> letters = {});
WordData load_words(std::vector<std::string>& arr, std::unordered_set<char> letters = {});
WordData load_words(std::string& str, std::unordered_set<char> letters = {}); // Either is a file path or a single string input
WordData load_words(StrVec sv, std::unordered_set<char> letters = {});

std::vector<std::pair<std::string, std::string>> load_queries(std::vector<std::string>& arr);
std::vector<std::pair<std::string, std::string>> load_queries(std::string& str); // Either is a file path or a single string input
std::vector<std::pair<std::string, std::string>> load_queries(StrVec sv);

// ======== CLASS ======== //
class Autocorrector {
public:
    explicit Autocorrector(AutocorrectorCfg& cfg);
    Autocorrector& operator=(const Autocorrector& ac) = default;
    explicit Autocorrector(
        StrVec _dictionary_list = std::filesystem::path{"test_files"} / std::filesystem::path{"20k_database.txt"},
        StrVec _valid_letters = "a-z",
        StrVec _keyboard = "qwerty",
        double _alpha = 0.2,
        double _beta = 0.85,
        int _b = 10,
        int _shortlist_size = 100,
        int _keyboard_shortlist_size = 75,
        double _transposition_bonus = 0.35
    );

    void save_dictionary();

    std::vector<std::string> add_dictionary(StrVec to_be_added);
    std::vector<std::string> remove_dictionary(StrVec to_be_removed);

    Result autocorrect(const std::initializer_list<std::string> queries_list, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);
    Results top_k(const std::initializer_list<std::string> queries_list, int k, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);
    Results top3(const std::initializer_list<std::string> queries_list, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);

    Result autocorrect(const StrVec& queries_list, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);
    Results top_k(const StrVec& queries_list, int k, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);
    Results top3(const StrVec& queries_list, std::filesystem::path output_file = "None", bool use_keyboard = true, bool return_invalid_words = true, bool print_details = false, bool print_times = false);

private:
    // ~~~~~~~~ VARIABLES ~~~~~~~~ //
    std::unordered_set<char> letters;
    std::vector<std::string> keyboard;
    std::unordered_map<char, struct Coord> KEY_POS;

    std::vector<std::string> word_dict;
    std::unordered_map<std::string, std::string> display_map;
    std::unordered_map<std::string, std::size_t> word_to_idx;

    double alpha; // Retained only for backwards compatibility
    double beta;
    int b;
    int shortlist_size;
    int keyboard_shortlist_size;
    double transposition_bonus;

    std::unordered_set<std::string> removed_words;
    double compact_threshold = 0.1;

    std::chrono::steady_clock::time_point t0, t2, t3;
    double preprocessing_time;

    int q;
    SketchConfig cfg;

    std::vector<HyperLogLogPlusPlus> word_sketches;
    std::vector<double> word_estimates;
    std::unordered_map<std::string, std::vector<std::size_t>> qgram_word_indices;

    std::size_t WORD_COUNT;
    std::size_t ACTIVE_WORD_COUNT;

    // ~~~~~~~~ FUNCTIONS ~~~~~~~~ //
    double key_dist(char& a, char& b);
    double word_dist(const std::string& a, const std::string& b);
    bool is_valid(std::string& word);
    std::vector<std::string> StrVecToVec(StrVec sv);

    HyperLogLogPlusPlus build_query_sketch(const std::unordered_set<std::string>& qgrams);
    static double score_from_sizes(double intersection, double left_size);

    void append_word(const std::string& word, const std::string& display);

    std::vector<std::pair<std::size_t, int>> candidate_indices(const std::unordered_set<std::string>& query_qgrams);

    static bool is_adjacent_transposition(const std::string& query, const std::string& candidate);

    std::vector<RankedCandidate> rank_candidates(const std::string& query, bool use_keyboard, bool print_details = false);
};