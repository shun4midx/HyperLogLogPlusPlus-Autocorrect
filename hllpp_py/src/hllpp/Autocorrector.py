############################################
# Copyright (c) 2026 Shun/修海 (@shun4midx) #
# Project: HyperLogLogPlusPlus-Autocorrect #
# File Type: Python file                   #
# File: Autocorrector.py                   #
############################################

import math
import os
import time
import warnings
from dataclasses import dataclass
from typing import Dict, List, Sequence, Union

from .HyperLogLogPlusPlus import HyperLogLogPlusPlus, SketchConfig

SuggestionValue = Union[str, List[str]]
ScoreValue = Union[float, List[float]]

def extract_qgrams(word, q=2, fuzzier=None): # `fuzzier` is retained only for backwards compatibility
    # Depreciation notice
    if fuzzier is not None:
        warnings.warn("`fuzzier` is retained only for backwards compatibility and has no effect in the HLL++ structural algorithm.", DeprecationWarning, stacklevel=2)

    # Code
    if len(word) < q:
        return []

    padded = f" {word} "

    if q != 2:
        return {padded[i : i + q] for i in range(len(padded) - q + 1)}
    
    qgrams = []

    for i in range(len(padded) - 1):
        qgram = padded[i : i + 2]

        qgrams.append(qgram)
        qgrams.append(f"{qgram[1]}{qgram[0]}")

    return qgrams


def extract_reversed_qgrams(word, q=2): # extract locally reversed q-gram features and also space variants
    if len(word) < q:
        return []

    if q != 2:
        return [word[i : i + q][::-1] for i in range(len(word) - q + 1)]

    qgrams = []

    for i in range(len(word) - 1):
        left = word[i]
        right = word[i + 1]

        qgrams.append(f"{right}{left}")
        qgrams.append(f"{right} ")
        qgrams.append(f" {left}")

    return qgrams

# ======== REPACKAGING ======== #

def is_valid(word, letters=None):
    if letters is None:
        return True
    
    return all(character in letters for character in word.lower() )

def _read_source(src):
    """
    Read words from:
      - a list or tuple;
      - a file containing one word per line;
      - or a single literal word.
    """
    if isinstance(src, (list, tuple)):
        return [str(item) for item in src]

    if not isinstance(src, str):
        raise ValueError(f"`src` ({src!r}) must be a list/tuple, a file path, or a string")

    if os.path.isfile(src):
        with open(src, "r", encoding="utf-8") as file:
            return [line.strip() for line in file if line.strip()]

    # A string that looks like a path should not silently become one query.
    if (src.endswith(".txt") or os.path.sep in src or (os.path.altsep is not None and os.path.altsep in src)):
        raise FileNotFoundError(f"Input file not found: {os.path.abspath(src)}")

    # Otherwise, treat it as one literal word.
    return [src]

def load_words(src, letters=None):
    raw = _read_source(src)
    valid_raw = [word for word in raw if is_valid(word, letters)]

    words = [word.lower() for word in valid_raw]
    display = {word.lower(): word for word in valid_raw}

    return words, display

def load_queries(src):
    raw = _read_source(src)
    return [(word, word.lower()) for word in raw]

addon_files = ["texting"] # Files to addon 20k_database.txt

@dataclass
class Results:
    suggestions: Dict[str, SuggestionValue]
    scores: Dict[str, ScoreValue]

class Autocorrector:
    # The algorithm basically relies on padded q-grams, a separate loose reversed channel, adjacent-transposition rescue, and keyboard-aware edit distance.
    # Default non-custom imported keyboards here would disregarded special characters (such as commas, not things like é and ö). Please import your own if you need to. I consider number rows too.
    # Of course, Dvorak is not as intuitive. I replaced special characters with a whitespace for sake of consistency.
    # "a-z" only considers English letters. For French, for example, you can import valid_letters = ["a-z", "é", "É", "à", "À", "ê". "Ê". "è", "È"]
    def __init__(self, dictionary_list=os.path.join("test_files", "20k_database.txt"), valid_letters="a-z", keyboard="qwerty", *, alpha=None, beta=0.85, b=10, shortlist_size=100, keyboard_shortlist_size=75, transposition_bonus=0.35, reversal_weight=0.80):
        self.letters = self._build_valid_letter_set(valid_letters)
        self.keyboard = self._build_keyboard(keyboard)
        self.KEY_POS = self._build_key_positions(self.keyboard)
        self.KEY_COST = self._build_key_costs(self.KEY_POS)

        self.word_dict, self.display_map = (self._load_dictionary(dictionary_list))

        self.alpha = alpha
        if alpha is not None:
            warnings.warn("`alpha` is retained only for backwards compatibility and has no effect in the HLL++ structural algorithm.", DeprecationWarning, stacklevel=2)

        self.beta = float(beta)
        self.b = int(b)
        self.shortlist_size = int(shortlist_size)
        self.keyboard_shortlist_size = int(keyboard_shortlist_size)
        self.transposition_bonus = float(transposition_bonus)
        self.reversal_weight = float(reversal_weight)

        if self.shortlist_size <= 0:
            raise ValueError("`shortlist_size` must be positive")

        if self.keyboard_shortlist_size <= 0:
            raise ValueError("`keyboard_shortlist_size` must be positive")

        if not 0.0 <= self.reversal_weight <= 1.0:
            raise ValueError("`reversal_weight` must be between 0 and 1")

        self.removed_words = set()
        self.compact_threshold = 0.1

        self.save_dictionary()

    @staticmethod
    def _build_valid_letter_set(valid_letters):
        if valid_letters in (None, "", []):
            return None

        if isinstance(valid_letters, str):
            valid_letters = [valid_letters]
        elif not isinstance(valid_letters, list):
            raise ValueError(f"`valid_letters` ({valid_letters!r}) must be a string or a list")

        letters = []

        for letter in valid_letters:
            if letter == "a-z":
                letters.extend(chr(ord("a") + i) for i in range(26))
            elif letter == "0-9":
                letters.extend(chr(ord("0") + i) for i in range(10))
            elif (isinstance(letter, str) and len(letter) == 1 and letter != " "):
                letters.append(letter.lower())
            else:
                raise ValueError('''`valid_letters` must contain single non-space characters or the abbreviations "a-z" and "0-9"''')

        return set(letters)

    @staticmethod
    def _build_keyboard(keyboard):
        presets = {
            "qwerty": ["1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"],
            "azerty": ["1234567890", "azertyuiop", "qsdfghjklm", "wxcvbn"],
            "qwertz": ["1234567890", "qwertzuiopü", "asdfghjklöä", "yxcvbnm"],
            "dvorak": ["1234567890", "'  pyfgcrl", "aoeuidhtns", " qjkxbmwvz"],
            "colemak": ["1234567890", "qwfpgjluy", "arstdhneio", "zxcvbkm"],
        }

        if isinstance(keyboard, str):
            if keyboard not in presets:
                raise ValueError('''`keyboard` must be one of "qwerty", "azerty", "qwertz", "dvorak", or "colemak", or a custom list of rows''')
            return presets[keyboard]

        if not isinstance(keyboard, list) or not all(isinstance(row, str) for row in keyboard):
            raise ValueError("`keyboard` must be a preset name or a list of row strings")
        
        return keyboard

    @staticmethod
    def _build_key_positions(keyboard):
        positions = {}

        for row_index, row in enumerate(keyboard):
            for column_index, character in enumerate(row):
                positions[character] = (row_index, column_index)

        return positions

    @staticmethod
    def _build_key_costs(key_positions):
        costs = {}

        for a, (xa, ya) in key_positions.items():
            for b, (xb, yb) in key_positions.items():
                costs[(a, b)] = math.sqrt((xa - xb)**2 + (ya - yb)**2)
        return costs

    def _load_dictionary(self, dictionary_list):
        # 1) Already a Python sequence?
        if isinstance(dictionary_list, (list, tuple)):
            return load_words(dictionary_list, self.letters)

        # 2) File on disk?
        if dictionary_list in addon_files:
            base_dir = os.path.dirname(os.path.abspath(__file__))
            base_path = os.path.join(base_dir, "test_files", "20k_database.txt")
            addon_path = os.path.join(base_dir, "test_files", f"{dictionary_list}.txt")
            combined_words = (_read_source(base_path) + _read_source(addon_path))
            return load_words(combined_words, self.letters)

        if isinstance(dictionary_list, str) and os.path.isfile(dictionary_list):
            return load_words(os.path.abspath(dictionary_list), self.letters)

        # 3) String?
        if not isinstance(dictionary_list, str):
            raise ValueError("`dictionary_list` must be a list/tuple, a path, or a known addon")

        base_dir = os.path.dirname(os.path.abspath(__file__))
        dictionary_path = os.path.join(base_dir, dictionary_list)

        if not os.path.isfile(dictionary_path):
            raise FileNotFoundError(f"Dictionary file not found: {dictionary_path}")

        return load_words(dictionary_path, self.letters)

    def key_dist(self, a, b):
        if a == b:
            return 0.0
        return self.KEY_COST.get((a, b), 1.0)

    def word_dist(self, a, b): # Keyboard-aware Levenshtein
        na = len(a)
        nb = len(b)

        previous = [float(j) for j in range(nb + 1)]
        current = [0.0] * (nb + 1)

        for i in range(1, na + 1):
            current[0] = float(i)
            ai = a[i - 1]

            for j in range(1, nb + 1):
                bj = b[j - 1]

                substitution = previous[j - 1] + self.key_dist(ai, bj)
                deletion = previous[j] + 1.0
                insertion = current[j - 1] + 1.0
                current[j] = min(substitution, deletion, insertion)

            previous, current = current, previous

        return previous[nb]

    def is_valid(self, word):
        return is_valid(word, self.letters)

    def build_query_sketch(self, qgrams):
        sketch = HyperLogLogPlusPlus(self.cfg)

        for gram in set(qgrams):
            sketch.insert(f"feature:{gram}")

        return sketch

    @staticmethod
    def _score_from_sizes(intersection, left_size):
        if left_size <= 0.0:
            return 0.0
        score = intersection / left_size

        return min(max(score, 0.0), 1.0)

    def save_dictionary(self):
        self.t0 = time.perf_counter()
        self.q = 2
        self.cfg = SketchConfig(b=self.b, sparse=True)
        self.word_sketches = []
        self.word_estimates = []

        self.qgram_word_indices = {}

        # WORD_COUNT tracks physically stored entries, ACTIVE_WORD_COUNT excludes lazily removed entries. Between compactions, indices remain stable because word_dict and the parallel sketch arrays are append-only.
        self.WORD_COUNT = len(self.word_dict)
        self.ACTIVE_WORD_COUNT = self.WORD_COUNT - len(self.removed_words)
        self.word_to_idx = {word: idx for idx, word in enumerate(self.word_dict)}

        if self.WORD_COUNT == 0:
            raise ValueError("Dictionary cannot be empty")

        for idx, word in enumerate(self.word_dict):
            qgrams = set(extract_qgrams(word, self.q))
            word_sketch = HyperLogLogPlusPlus(self.cfg)

            for gram in qgrams:
                self.qgram_word_indices.setdefault(gram, []).append(idx)
                word_sketch.insert(f"feature:{gram}")

            self.word_sketches.append(word_sketch)
            self.word_estimates.append(word_sketch.estimate())

        self.preprocessing_time = time.perf_counter() - self.t0

    def _append_word(self, word, display):
        # Append one new word w/o rebuilding the dict. This is proportional to number of q-gram freatures generated from the word, i.e. O(L) for fixed q, where L = word len
        idx = len(self.word_dict)

        self.word_dict.append(word)
        self.display_map[word] = display
        self.word_to_idx[word] = idx

        qgrams = set(extract_qgrams(word, self.q))
        word_sketch = HyperLogLogPlusPlus(self.cfg)

        for gram in qgrams:
            self.qgram_word_indices.setdefault(gram, []).append(idx)
            word_sketch.insert(f"feature:{gram}")

        self.word_sketches.append(word_sketch)
        self.word_estimates.append(word_sketch.estimate())
        
        self.WORD_COUNT += 1
        self.ACTIVE_WORD_COUNT += 1

    def add_dictionary(self, to_be_added): # Overall cost is O(L), L = input len
        words, displays = load_words(to_be_added, self.letters)
        added = []
        restored = []

        for word in words:
            idx = self.word_to_idx.get(word)
            if idx is not None:
                if word in self.removed_words:
                    self.removed_words.remove(word)
                    self.ACTIVE_WORD_COUNT += 1
                    self.display_map[word] = displays.get(word, self.display_map.get(word, word))
                    restored.append(word)
                continue

            self._append_word(word, displays.get(word, word))
            added.append(word)

        return added + restored

    def remove_dictionary(self, to_be_removed): # Lazily remove using tombstones, normal removal is O(1), O(L) amortized
        words, _ = load_words(to_be_removed, self.letters)
        removed = []

        for word in words:
            if word in self.word_to_idx and word not in self.removed_words:
                self.removed_words.add(word)
                self.ACTIVE_WORD_COUNT -= 1
                removed.append(word)

        # Rebuild only after a const frac of tombstones, so O(NL) rebuild is spread across Theta(N) deletions
        if self.ACTIVE_WORD_COUNT > 0 and self.word_dict and len(self.removed_words) >= len(self.word_dict) * self.compact_threshold:
            self.word_dict = [word for word in self.word_dict if word not in self.removed_words]
            self.display_map = {word: self.display_map[word] for word in self.word_dict}
            self.removed_words.clear()
            self.save_dictionary()

        return removed

    def _candidate_indices(self, query_qgrams):
        overlap_counts = {}
        removed_words = self.removed_words
        word_dict = self.word_dict

        for gram in query_qgrams:
            for idx in self.qgram_word_indices.get(gram, ()):
                if word_dict[idx] in removed_words:
                    continue

                overlap_counts[idx] = overlap_counts.get(idx, 0) + 1

        candidates = list(overlap_counts.items())
        candidates.sort(key=lambda item: (-item[1], item[0]))
        return candidates[:self.shortlist_size]

    @staticmethod
    def _is_adjacent_transposition(query, candidate):
        if len(query) != len(candidate):
            return False

        mismatches = [i for i, (a, b) in enumerate(zip(query, candidate)) if a != b]
        if len(mismatches) != 2:
            return False
        i, j = mismatches

        return j == i + 1 and query[i] == candidate[j] and query[j] == candidate[i]

    def _rank_candidates(self, query, use_keyboard, print_details=False):
        retrieval_qgrams = set(extract_qgrams(query, self.q))
        scoring_qgrams = retrieval_qgrams
        reversed_scoring_qgrams = set(extract_reversed_qgrams(query, self.q))

        if print_details:
            print(f"Query {query!r}: features={len(scoring_qgrams)}, reversed_features={len(reversed_scoring_qgrams)}")
            print("Structural q-grams: " + ", ".join(repr(gram) for gram in sorted(scoring_qgrams)))
            print("Loose reversed q-grams: " + ", ".join(repr(gram) for gram in sorted(reversed_scoring_qgrams)))

        query_sketch = self.build_query_sketch(scoring_qgrams)
        query_estimate = query_sketch.estimate()

        reversed_query_sketch = self.build_query_sketch(reversed_scoring_qgrams)
        reversed_query_estimate = (reversed_query_sketch.estimate())

        candidate_indices = self._candidate_indices(retrieval_qgrams)
        retrieved_count = len(candidate_indices)
        candidate_map = dict(candidate_indices)

        for i in range(len(query) - 1):
            if query[i] == query[i + 1]:
                continue

            swapped = query[:i] + query[i + 1] + query[i] + query[i + 2:]
            
            idx = self.word_to_idx.get(swapped)
            if idx is not None and swapped not in self.removed_words:
                candidate_map.setdefault(idx, 0)

        candidate_indices = list(candidate_map.items())

        if print_details:
            rescued_count =len(candidate_indices) - retrieved_count
            print(f"Candidates: {retrieved_count} retrieved, {rescued_count} transposition-rescued, {len(candidate_indices)} total")

        preliminary = []

        for idx, exact_overlap in candidate_indices:
            word_sketch = self.word_sketches[idx]
            word_estimate = self.word_estimates[idx]

            structural_union = (query_sketch.union_estimate(word_sketch))
            structural_intersection = max(0.0, query_estimate + word_estimate - structural_union)
            structural_intersection = min(structural_intersection, query_estimate, word_estimate)

            normal_structural_score = self._score_from_sizes(intersection=structural_intersection, left_size=query_estimate)
            reversed_union = reversed_query_sketch.union_estimate(word_sketch)
            reversed_intersection = max(0.0, reversed_query_estimate + word_estimate - reversed_union)
            reversed_intersection = min(reversed_intersection, reversed_query_estimate, word_estimate)
            reversed_structural_score = self._score_from_sizes(intersection=reversed_intersection,left_size=reversed_query_estimate)

            structural_score = min(1.0, normal_structural_score + self.reversal_weight * max(0.0, reversed_structural_score - normal_structural_score))

            candidate = self.word_dict[idx]

            length_ratio = abs(len(candidate) - len(query)) / max(len(query), 1)
            length_score = max(0.0, 1.0 - length_ratio**2)
            exact_bonus = 1.0 if query == candidate else 0.0

            preliminary_score = structural_score * length_score + exact_bonus
            preliminary.append((preliminary_score, idx, structural_score, exact_overlap, length_score, normal_structural_score, reversed_structural_score, word_estimate, structural_union, structural_intersection, reversed_union, reversed_intersection))

        preliminary.sort(key=lambda item: (-item[0], item[1]))

        keyboard_limit = min(self.keyboard_shortlist_size, len(preliminary))

        ranked = []

        for position, (preliminary_score, idx, structural_score, exact_overlap, length_score, normal_structural_score, reversed_structural_score, word_estimate, structural_union, structural_intersection, reversed_union, reversed_intersection) in enumerate(preliminary):
            candidate = self.word_dict[idx]

            if use_keyboard and position < keyboard_limit:
                keyboard_score = 1.0 / (1.0 + self.word_dist(query, self.word_dict[idx]))
            else:
                keyboard_score = 0.0

            exact_bonus = 1.0 if query == self.word_dict[idx] else 0.0
            transposition_bonus = self.transposition_bonus if self._is_adjacent_transposition(query, candidate) else 0.0

            score = structural_score * length_score + self.beta * keyboard_score + transposition_bonus + exact_bonus

            ranked.append((score, idx, structural_score, exact_overlap, normal_structural_score, reversed_structural_score, length_score, keyboard_score, transposition_bonus, exact_bonus, word_estimate, structural_union, structural_intersection, reversed_union, reversed_intersection))

        ranked.sort(key=lambda item: (-item[0], item[1]))

        if print_details:
            print("Top ranked candidates:")

            for (score, idx, structural_score, exact_overlap, normal_structural_score, reversed_structural_score, length_score, keyboard_score, transposition_bonus, exact_bonus, word_estimate, structural_union, structural_intersection, reversed_union, reversed_intersection) in ranked[:5]:
                candidate = self.word_dict[idx]
                print(f"  {candidate!r}:")
                print(f"    retrieval overlap: {exact_overlap}")
                print(f"    HLL++ normal: query={query_estimate:.6f}, candidate={word_estimate:.6f}, union={structural_union:.6f}, intersection={structural_intersection:.6f}")
                print(f"    HLL++ reversed: query={reversed_query_estimate:.6f}, candidate={word_estimate:.6f}, union={reversed_union:.6f}, intersection={reversed_intersection:.6f}")
                print(f"    structural: normal={normal_structural_score:.6f}, reversed={reversed_structural_score:.6f}, blended={structural_score:.6f}")

                structural_contribution = structural_score * length_score
                keyboard_contribution = self.beta * keyboard_score
                print(f"    modifiers: length={length_score:.6f}, keyboard={keyboard_score:.6f}, transpose_bonus={transposition_bonus:.3f}, exact_bonus={exact_bonus:.1f}")
                print(f"    contributions: structural*length={structural_contribution:.6f}, beta*keyboard={keyboard_contribution:.6f}")
                print(f"    final score: {score:.6f}")

        return ranked

    def autocorrect(self, queries_list, output_file="None", use_keyboard=True, return_invalid_words=True, print_details=False, print_times=False):
        if print_times:
            self.save_dictionary()

        queries = load_queries(queries_list)

        self.t2 = time.perf_counter()

        output = []
        suggestions = {}
        final_scores = {}

        for query_display, query in queries:
            if not self.is_valid(query):
                replacement = query_display if return_invalid_words else ""
                suggestions[query_display] = replacement
                final_scores[query_display] = 0.0
                output.append(replacement)
                continue

            ranked = self._rank_candidates(query, use_keyboard=use_keyboard, print_details=print_details)

            if not ranked:
                replacement = query_display if return_invalid_words else ""
                suggestions[query_display] = replacement
                final_scores[query_display] = 0.0
                output.append(replacement)
                continue

            (best_score, best_idx, best_structural_score, best_exact_overlap, *_) = ranked[0]

            picked = self.word_dict[best_idx]
            displayed_picked = self.display_map.get(picked, picked)

            if print_details:
                print(f"Selected {displayed_picked!r} for {query_display!r}: final={best_score:.6f}, containment={best_structural_score:.6f}, retrieval_overlap={best_exact_overlap}")
                print("-" * 60)

            suggestions[query_display] = displayed_picked
            final_scores[query_display] = best_score
            output.append(displayed_picked)

        self.t3 = time.perf_counter()

        if output_file != "None":
            with open(output_file, "w", encoding="utf-8") as output_stream:
                output_stream.write("\n".join(output))

        if print_times:
            print(f"Dictionary preprocessing:  {self.preprocessing_time:.3f}s")
            print(f"Current query batch:       {self.t3 - self.t2:.3f}s")
            print(f"Total autocorrect:         {self.preprocessing_time + self.t3 - self.t2:.3f}s")

        return Results(suggestions=suggestions, scores=final_scores)

    def top_k(self, queries_list, k, output_file="None", use_keyboard=True, return_invalid_words=True, print_details=False, print_times=False):
        if not isinstance(k, int) or isinstance(k, bool):
            raise TypeError("`k` must be an integer")

        if k <= 0:
            raise ValueError("`k` must be positive")

        if print_times:
            self.save_dictionary()

        queries = load_queries(queries_list)

        self.t2 = time.perf_counter()

        output = []
        suggestions = {}
        final_scores = {}

        for query_display, query in queries:
            if not self.is_valid(query):
                if return_invalid_words:
                    top_words = [query_display]
                else:
                    top_words = []

                while len(top_words) < k:
                    top_words.append("")

                top_scores = [0.0] * k

                suggestions[query_display] = top_words
                final_scores[query_display] = top_scores
                output.append(" ".join(top_words))
                continue

            ranked = self._rank_candidates(query, use_keyboard=use_keyboard, print_details=print_details)

            seen = set()
            top_words = []
            top_scores = []

            for (score, idx, *_) in ranked:
                suggestion = self.display_map.get(self.word_dict[idx], self.word_dict[idx])

                if suggestion in seen:
                    continue

                seen.add(suggestion)
                top_words.append(suggestion)
                top_scores.append(score)

                if len(top_words) == k:
                    break

            if not top_words and return_invalid_words:
                top_words.append(query_display)
                top_scores.append(0.0)

            while len(top_words) < k:
                top_words.append("")
                top_scores.append(0.0)

            if print_details:
                displayed_results = [f"{word!r} ({score:.6f})" for word, score in zip(top_words, top_scores) if word]
                print(f"Selected top {k} for {query_display!r}: {', '.join(displayed_results)}")
                print("-" * 60)

            suggestions[query_display] = top_words
            final_scores[query_display] = top_scores
            output.append(" ".join(top_words))

        self.t3 = time.perf_counter()

        if output_file != "None":
            with open(output_file, "w", encoding="utf-8") as output_stream:
                output_stream.write("\n".join(output))

        if print_times:
            print(f"Dictionary preprocessing:  {self.preprocessing_time:.3f}s")
            print(f"Current query batch:       {self.t3 - self.t2:.3f}s")
            print(f"Total top-{k}:               {self.preprocessing_time + self.t3 - self.t2:.3f}s")

        return Results(suggestions=suggestions, scores=final_scores)
    
    def top3(self, queries_list, output_file="None", use_keyboard=True, return_invalid_words=True, print_details=False, print_times=False):
        return self.top_k(queries_list=queries_list, k=3, output_file=output_file, use_keyboard=use_keyboard, return_invalid_words=return_invalid_words, print_details=print_details, print_times=print_times)
    
# ======== SAMPLE USAGE ======== #
if __name__ == "__main__":
    ac = Autocorrector()

    # File
    ans1 = ac.autocorrect("test_files/typo_file.txt", "outputs/class_suggestions.txt")
    print(ans1.suggestions)
    print(ans1.scores)

    ans2 = ac.top3("test_files/typo_file.txt", "outputs/class_suggestions.txt")

    # Or even top 5
    ans2_top5 = ac.top_k("test_files/typo_file.txt", 5, "outputs/class_suggestions.txt")

    # Optionally, you can not want it to output it into a file, then:
    # Individual strings
    ans3 = ac.autocorrect("hillo")
    ans4 = ac.top3("hillo")
    ans4_top5 = ac.top_k("hillo", 5)

    # Arrays
    ans5 = ac.autocorrect(["tsetign", "hillo", "goobye", "haedhpoesn"])
    ans6 = ac.top3(["tsetign", "hillo", "goobye", "haedhpoesn"])

    # You can even have a custom dictionary!
    dictionary = ["apple", "banana", "grape", "orange"]
    custom_ac = Autocorrector(dictionary)

    ans7 = custom_ac.autocorrect(["applle", "banana", "banan", "orenge", "grap", "pineapple"])
    ans8 = custom_ac.top3(["applle", "banana", "banan", "orenge", "grap", "pineapple"])

    print(ans7.suggestions)
    print(ans8.suggestions)