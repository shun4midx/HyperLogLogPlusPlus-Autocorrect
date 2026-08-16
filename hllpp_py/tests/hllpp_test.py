############################################
# Copyright (c) 2026 Shun/修海 (@shun4midx) #
# Project: HyperLogLogPlusPlus-Autocorrect #
# File Type: Python file                   #
# File: hllpp_test.py                      #
############################################

from hllpp import Autocorrector, compare_files, compare3_files

if __name__ == "__main__":
    # ======== 20k_database.txt ======== #
    ac = Autocorrector(valid_letters = "")

    # Test files
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/20k_autocorrect_suggestions.txt", use_keyboard=False, return_invalid_words=False, print_details=False, print_times=True)
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/qwerty_20k_autocorrect_suggestions.txt", use_keyboard=True, return_invalid_words=False, print_details=False, print_times=True)
    print(autocor.suggestions)
    print(autocor.scores)

    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/20k_top3_suggestions.txt", use_keyboard=False, return_invalid_words=False, print_details=False, print_times=True)
    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/qwerty_20k_top3_suggestions.txt", use_keyboard=True, return_invalid_words=False, print_details=False, print_times=True)
    print(top3_ans.suggestions)
    print(top3_ans.scores)

    """
    Runtimes:
    Dictionary preprocessing:  0.986s
    Current query batch:       4.074s
    Total autocorrect:         5.061s

    Dictionary preprocessing:  0.966s
    Current query batch:       4.965s
    Total autocorrect:         5.932s

    Dictionary preprocessing:  0.968s
    Current query batch:       3.909s
    Total top-3:               4.877s

    Dictionary preprocessing:  0.990s
    Current query batch:       5.085s
    Total top-3:               6.075s
    """

    # Run files
    compare_files("outputs/20k_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")
    compare_files("outputs/qwerty_20k_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")

    compare3_files("outputs/20k_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")
    compare3_files("outputs/qwerty_20k_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")

    """
    Accuracy:
    68~69%
    71~72%
    85~86%
    88~89%
    """


    # ======== database.txt ======== #
    ac = Autocorrector("test_files/database.txt", valid_letters = "") # It is able to search within the src folder first, before searching in the user's folder

    # Test files
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/database_autocorrect_suggestions.txt", use_keyboard=False, return_invalid_words=False, print_details=False, print_times=True)
    autocor = ac.autocorrect("test_files/typo_file.txt", "outputs/qwerty_database_autocorrect_suggestions.txt", use_keyboard=True, return_invalid_words=False, print_details=False, print_times=True)
    print(autocor.suggestions)
    print(autocor.scores)

    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/database_top3_suggestions.txt", use_keyboard=False, return_invalid_words=False, print_details=False, print_times=True)
    top3_ans = ac.top3("test_files/typo_file.txt", "outputs/qwerty_database_top3_suggestions.txt", use_keyboard=True, return_invalid_words=False, print_details=False, print_times=True)
    print(top3_ans.suggestions)
    print(top3_ans.scores)

    """
    Runtimes:
    Dictionary preprocessing:  0.024s
    Current query batch:       0.322s
    Total autocorrect:         0.347s

    Dictionary preprocessing:  0.024s
    Current query batch:       1.437s
    Total autocorrect:         1.461s

    Dictionary preprocessing:  0.024s
    Current query batch:       0.326s
    Total top-3:               0.350s

    Dictionary preprocessing:  0.025s
    Current query batch:       1.394s
    Total top-3:               1.418s
    """

    # Run files
    compare_files("outputs/database_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")
    compare3_files("outputs/database_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")

    """
    Accuracy (non keyboard):
    92~93%
    96~97%
    """