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
    Dictionary preprocessing:  0.962s
    Current query batch:       4.799s
    Total autocorrect:         5.761s

    Dictionary preprocessing:  0.960s
    Current query batch:       6.011s
    Total autocorrect:         6.971s

    Dictionary preprocessing:  0.959s
    Current query batch:       4.041s
    Total top-3:               4.999s

    Dictionary preprocessing:  1.461s
    Current query batch:       5.951s
    Total top-3:               7.412s
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
    84~85%
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
    Dictionary preprocessing:  0.023s
    Current query batch:       0.494s
    Total autocorrect:         0.518s

    Dictionary preprocessing:  0.024s
    Current query batch:       1.579s
    Total autocorrect:         1.603s

    Dictionary preprocessing:  0.024s
    Current query batch:       0.496s
    Total top-3:               0.521s

    Dictionary preprocessing:  0.024s
    Current query batch:       1.547s
    Total top-3:               1.571s
    """

    # Run files
    compare_files("outputs/database_autocorrect_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")
    compare3_files("outputs/database_top3_suggestions.txt", "test_files/typo_file.txt", "test_files/output_compare.txt")

    """
    Accuracy (non keyboard):
    92~93%
    96~97%
    """