# HyperLogLog++ (HLLPP) Autocorrect
<a href="https://github.com/shun4midx/FQ-HyperLogLog-Autocorrect/tree/main/hllpp_cpp"><img src="https://img.shields.io/badge/c++-%23f34b7d.svg?style=for-the-badge&logo=c%2B%2B"><a href="https://github.com/shun4midx/FQ-HyperLogLog-Autocorrect/tree/main/hllpp_py"><img src="https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=fff&style=for-the-badge">

## NOTE: Specific details on how to install and run the programs in Python and C++ are in the `hllpp_py` and `hllpp_cpp` folders separately, and more information about the actual algorithm will be in `algo_description/description.pdf`

A HyperLogLog++-based autocorrection library developed from my [earlier work on FQ-HLL Autocorrection](https://github.com/shun4midx/FQ-HyperLogLog-Autocorrect), which used conventional HyperLogLog sketches.

This implementation uses the higher-precision sparse representation introduced by HLL++ for small q-gram sets, then converts sketches to conventional dense HLL registers when the sparse representation exceeds its promotion threshold. The empirical bias-correction tables used by the complete general-purpose HLL++ estimator are not currently included. Typical autocorrection workloads use very small q-gram sets and therefore remain in sparse mode, where those dense-estimator corrections are generally not used.

## Contents
- [Context](#context)
- [Results](#results)
- [Remark on Keyboards](#remark-on-keyboards)
- [Anecdotal Comparison with FQ-HLL](#anecdotal-qualitative-comparison-with-fq-hll-autocorrection)
- [Notes](#notes)
- [Plans](#plans-for-the-repo)
- [Current Usages](#current-repos-using-this-hllpp-library)
- [License](#license)

## Context
**For theoretical value, there is nothing in my algorithm that uses any information about the English language or the QWERTY keyboard, to do any of the corrections. It is a NON-ML algorithm too.**

The HLLPP Autocorrection algorithm is a continuation of the [FQ-HLL Autocorrection](https://github.com/shun4midx/FQ-HyperLogLog-Autocorrect) algorithm. Although, as the name suggests, FQ-HLL uses a modified form of HLL to perform autocorrection, and HLLPP uses HLL++ under sparse contexts to perform autocorrection, they are fundamentally structured quite differently, from estimation down to scoring, and are **NOT simply the same algorithm** with FQ-HLL replaced with HLL++.

HLLPP was also created to fix some minor anecdotal qualitative stylistic flaws with FQ-HLL's prioritization ranking suggestions, with more detail written [later in the README](https://github.com/shun4midx/HyperLogLogPlusPlus-Autocorrect#anecdotal-qualitative-comparison-with-fq-hll-autocorrection). Above all, it is made to be faster and more accurate than FQ-HLL.

## Results
### Setting Description (same as in FQ-HLL)
I've referenced [a list of common typos in datasets from Peter Norvig's classic spelling corrector](https://www.kaggle.com/datasets/bittlingmayer/spelling/data), which I should call the `typo_file.txt`, and used HLLPP with two different sets of base "dictionary words". 

The other set was comprised of the [most commonly used 20000 English words](https://github.com/first20hours/google-10000-english/blob/master/20k.txt) on top, and the original words from `database.txt` on the bottom, since I needed to include all possible answers in the dictionary. This is called `20k_database.txt`. It was previously named `20k_shun4midx.txt` in FQ-HLL, but it is the same file.

Here, for the most objective measure, I counted "accuracy" as simply if the word matches what the typo originally was intended to correct to. For example, if "mant" was supposed to correct to "want" in the list, even if my code outputted "many", I'd still count it wrong. Inspired by how autocorrection on mobile devices offer top 3 suggestions, I have also implemented a `top3` function. For `top3` results, a result is "correct" if any one of the three suggested results is "correct".

### Quantitative Results
Here is a comparison of accuracy for HLLPP versus FQ-HLL (for non-keyboard-aware mode). On average, results are 8 to 10 percentage points more accurate.
| Measurement   | `database.txt`      | `20k_database.txt`  |
| ------------- | ------------------- | ------------------- |
| HLLPP (Auto)  | 92~93%              | 68~69%              |
| HLLPP (Top3)  | 96~97%              | 85~86%              |
| FQ-HLL (Auto) | 87~88%              | 59~60%              |
| FQ-HLL (Top3) | 93~94%              | 75~76%              |

### Python
I also implemented the standard Levenshtein + BK-Tree autocorrection algorithm with **edit distance <= 2** (Since otherwise it would be more than five times slower than HLLPP) in `hllpp_py/tests/bk_test.py`, and it performs slower but also at a lower accuracy, at around **75~76%** and **43~44%** respectively. I even increased the **edit distance to be <= 3**, and allowed the program to be slower. Even then, its accuracy only achieves around **89~90%** and **46~47%** respectively, undoubtedly it uses more memory too. The accuracy doesn't increase much after edit distance is greater than 3.

Even for **`SymSpell`** in `hllpp_py/tests/symspell_test.py`, I increased to **edit distance <= 5**, and even then its accuracy was only around **89~90%** and **46~47%** respectively.

In `Python`, here is a rough total runtime of each algorithm to finish all queries:
| Method        | `database.txt`      | `20k_database.txt`  |
| ------------- | ------------------- | ------------------- |
| HLLPP         | 0.347s              | 5.061s              |
| FQ-HLL        | 0.223s              | 9.955s              |
| BK (ED <= 2)  | 2.126s              | 46.028s             |
| BK (ED <= 3)  | 3.816s              | 92.812s             |
| SymSpell      | 0.515s + 0.996s     | 16.994s + 29.355s   |

*(SymSpell times are Build + Query time)*

### C++
In `C++`, due to time constraints and the fact that Python demonstrated well enough the efficiency and relative accuracy of HLLPP, only HLLPP and FQ-HLL have been demonstrated here, and `top3` results are as shown. Execution details are in the README file of `hllpp_cpp`, most importantly, remember to use the `-O2` flag. The accuracy was unchanged.

| Method        | `database.txt`      | `20k_database.txt`  |
| ------------- | ------------------- | ------------------- |
| HLLPP (Auto)  | 0.076s              | 1.646s              |
| HLLPP (Top3)  | 0.082s              | 1.609s              |
| FQ-HLL (Auto) | 0.071s              | 2.649s              |
| FQ-HLL (Top3) | 0.084s              | 3.716s              |

### Conclusion
Given the relatively small memory usage yet huge accuracy and it being locally run, HLLPP is something worth considering for autocorrection algorithms.

## Remark on Keyboards
As a side note, I made the QWERTY keyboard (including AZERTY, QWERTZ, Colemak, Dvorak, or any other custom keyboard layout) as toggleable parameters to influence my HLLPP, since I am coding with [Ducky](https://github.com/ducky4life) to create an [HLLPP Android keyboard](https://github.com/shun4midx/HLLPP-Keyboard). In this case, runtime slowed down by roughly 1 second for the entire `20k_database.txt` file, but with an accuracy of **71~72%** and **88~89%**, for the autocorrection and top 3 results respectively. For FQ-HLL, it only achieved an accuracy of **64~65%** and **80~81%**, respectively. However, the main takeaway of this repository is how strong HLLPP is without the knowledge of a keyboard layout, which is why I make it something that can be turned off, and most results would be dedicated to that.

Specific details in how these keyboards can be accessed in the programming languages are available in `hllpp_cpp/README.md` and `hllpp_py/README.md` separately.

## Anecdotal Qualitative Comparison with FQ-HLL Autocorrection
FQ-HLL has its flaws that came with the naive implementation of dyslexia-friendly autocorrection based on a "dyslexic impression" of a word via q-grams, without caring about order, and also it being **too** fuzzy with its suggestions, due to the fuzzing of each q-gram, sometimes suggesting words like "information" if there is low accuracy signal for any word.

Namely, one specific problem was FQ-HLL loved "reflecting" at the boundary of words, since it viewed both q-grams in the correct and reverse order quite heavily. For example, the word "varely" would look like a typo of the word "barely", but FQ-HLL would reflect the "ar" to "ra" and combine it in its suggestion to suggest "rarely". Similarly, FQ-HLL upon seeing "habe", would reflect "ab" to "ba" and suggest "babe" instead of "have". At this time, I thought this was a needed tradeoff if I wanted to preserve dyslexia-friendly q-gram reading, but it was a noticeable problem when used as a main keyboard.

HLLPP resolves both problems by firstly, padding a typo with spaces like " \<typo\> ", then extracting the q-grams with the spaces at the boundaries. This avoids the problem of "reflecting" at the boundaries. Secondly, HLLPP separates forward-order and reverse-order q-gram signals in its calculations. "Fuzzy" q-grams like "ab" being also "a " and " b" are now only present when looking at the word in reverse order, otherwise there is no fuzziness to the q-grams when reading in the right order. When doing so, HLLPP is less prone to producing overly fuzzy suggestions, but it still seems to preserve quite a bit of dyslexia-friendly typing, yet also suggests more intended results at higher confidence levels than before. For example, "varely" would suggest "barely" and "habe" would suggest "have".

Clearly, HLLPP does have some imperfections. For example "ita" wouldn't rank the suggestion "its" very highly. However, overall, it seems to resolve the problems FQ-HLL had and provide anecdotally more intended results.

## Notes
 - This library does not collect personal data.
 
### Dyslexia
Personally, I've always had an interest in autocorrect because I'm dyslexic and often unintentionally scramble or reverse letters when I read. Here are my thoughts about this algorithm based on my dyslexia.
 - Reasoning would be more detailed in the `algo_description/description.pdf` file, but I find this algorithm's autocorrection suggestions are sometimes more intuitive (e.g. "klof" -> "folk") to my dyslexia than other Levenshtein distance-based autocorrection models.
 - As a side note, as a dyslexic person, I naturally process words similar to how the HLLPP algorithm processes words, and that was my intuition in terms of how to create this algorithm in the first place.

## Plans for the Repo
 - ✅ Develop a Python library importable via `pip install`
 - ✅ Include a C++ library that is importable via CMake, since as most of you know, I love C++.
 - Formally document the logic behind the algorithm via a LaTeX file (or its PDF directly).
 - If possible, quantitatively and empirically document the comparison between FQ-HLL and HLLPP autocorrection

## Current Repos using this HLLPP Library
<a href="https://github.com/shun4midx/HyperLogLogPlusPlus-Autocorrect/tree/main/hllpp_cpp"><img src="https://img.shields.io/badge/c++-%23f34b7d.svg?style=for-the-badge&logo=c%2B%2B">
 - `HLLPP Keyboard` (To be released to the public soon): [An Android mobile keyboard](https://github.com/shun4midx/HLLPP-Keyboard) that integrates this HLLPP autocorrect library. It serves as a semi-official real-world use case for the algorithm alongside this specific library, and I am beyond honored to be a part of its development with [Ducky](https://github.com/ducky4life).

 <a href="https://github.com/shun4midx/HyperLogLogPlusPlus-Autocorrect/tree/main/hllpp_py"><img src="https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=fff&style=for-the-badge">

 - `Web autocorrector`: An [autocorrector website](https://web-autocorrector.vercel.app/) which uses HLLPP autocorrect to deal with inputs
 - `klofr`: A [discord.py bot interface](https://github.com/ducky4life/klofr) for HLLPP (and [FQ-HLL](https://github.com/shun4midx/FQ-HyperLogLog-Autocorrect)) that autocorrects every word in each message

## License
HLLPP is licensed under the Apache-2.0 License, reference `LICENSE` for more information.