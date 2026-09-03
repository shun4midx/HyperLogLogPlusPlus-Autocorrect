/********************************************
 * Copyright (c) 2026 Shun/修海 (@shun4midx) *
 * Project: HyperLogLogPlusPlus-Autocorrect *
 * File Type: C++ file                      *
 * File: hllpp_test.cpp                     *
 ****************************************** */

#include <HLLPP/HLLPP.h>
#include <iostream>
#include <string>

int main() {
    // ======== 20k_shun4midx.txt ======== //
    AutocorrectorCfg cfg;
    cfg.valid_letters = "";

    Autocorrector ac = Autocorrector(cfg);

    // Test files
    Result autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/20k_autocorrect_suggestions.txt", false, false, false, true); // Don't use keyboard, print details and print times
           autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/qwerty_20k_autocorrect_suggestions.txt", true, false, false, true); // Use keyboard, don't print details but print times
    // // std::unordered_map<std::string, std::string> sug = autocor.suggestions;
    // // std::unordered_map<std::string, double> score = autocor.scores;

    Results top3_ans = ac.top3("test_files/typo_file.txt", "outputs/20k_top3_suggestions.txt", false, false, false, true); // Don't use keyboard, print details and print times
            top3_ans = ac.top3("test_files/typo_file.txt", "outputs/qwerty_20k_top3_suggestions.txt", true, false, false, true); // Use keyboard, don't print details but print times
    // std::unordered_map<std::string, std::string> sug = top3_ans.suggestions;
    // std::unordered_map<std::string, double> score = top3_ans.scores;

    /*
    Runtimes (-O2, default):
    Dictionary preprocessing:  0.175s
    Current query batch:       1.471s
    Total autocorrect:         1.646s

    Dictionary preprocessing:  0.179s
    Current query batch:       1.530s
    Total autocorrect:         1.710s
    
    Dictionary preprocessing:  0.171s
    Current query batch:       1.438s
    Total top-3:               1.609s
    
    Dictionary preprocessing:  0.187s
    Current query batch:       1.573s
    Total top-3:               1.760s
    */

    compare_files("outputs/20k_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");
    compare_files("outputs/qwerty_20k_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");

    compare3_files("outputs/20k_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");
    compare3_files("outputs/qwerty_20k_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");

    /*
    Accuracy:
    68~69%
    71~72%
    85~86%
    88~89%
    */

    // ======== database.txt ======== //
    cfg.dictionary_list = "test_files/database.txt";
    ac = Autocorrector(cfg);

    // Test files
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/database_autocorrect_suggestions.txt", false, false, false, true); // Don't use keyboard, print details and print times
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/qwerty_database_autocorrect_suggestions.txt", true, false, false, true); // Use keyboard, don't print details but print times
    // std::unordered_map<std::string, std::string> sug = autocor.suggestions;
    // std::unordered_map<std::string, double> score = autocor.scores;

    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/database_top3_suggestions.txt", false, false, false, true); // Don't use keyboard, print details and print times
    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/qwerty_database_top3_suggestions.txt", true, false, false, true); // Use keyboard, don't print details but print times
    // std::unordered_map<std::string, std::string> sug = top3_ans.suggestions;
    // std::unordered_map<std::string, double> score = top3_ans.scores;

    /*
    Runtimes (-O2, default):
    Dictionary preprocessing:  0.005s
    Current query batch:       0.071s
    Total autocorrect:         0.076s

    Dictionary preprocessing:  0.004s
    Current query batch:       0.155s
    Total autocorrect:         0.159s
    
    Dictionary preprocessing:  0.005s
    Current query batch:       0.077s
    Total top-3:               0.082s
    
    Dictionary preprocessing:  0.004s
    Current query batch:       0.181s
    Total top-3:               0.185s
    */

    compare_files("outputs/database_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");
    compare3_files("outputs/database_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt");

    /*
    Accuracy (non keyboard):
    92~93%
    96~97%
    */

    return 0;
}