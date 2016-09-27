// {{{ $VIMCODER$ <-----------------------------------------------------
// vim:filetype=cpp:foldmethod=marker:foldmarker={{{,}}}

#include <algorithm>
#include <array>
#include <bitset>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "include/utf8/utf8.h"

using namespace std;

// }}}

using CharType = uint16_t;
using WordType = vector<CharType>;

#define DEFINE_ACCESSOR_AND_MUTATOR(type, name) \
  type name() {                                 \
    return name ## _;                           \
  }                                             \
  void set_ ## name(type val) {                 \
    name ## _ = val;                            \
  }                                             \


class State {
 public:
  using TransType = unordered_map<CharType, State *>;

  DEFINE_ACCESSOR_AND_MUTATOR(int, maxlen)
  DEFINE_ACCESSOR_AND_MUTATOR(int, minlen)
  DEFINE_ACCESSOR_AND_MUTATOR(int, first_endpos)
  DEFINE_ACCESSOR_AND_MUTATOR(State *, link)
  DEFINE_ACCESSOR_AND_MUTATOR(bool, accept)
  DEFINE_ACCESSOR_AND_MUTATOR(TransType, trans)
  DEFINE_ACCESSOR_AND_MUTATOR(vector<State *>, reversed_links)
  DEFINE_ACCESSOR_AND_MUTATOR(int, appearance)
  DEFINE_ACCESSOR_AND_MUTATOR(bool, split)

  bool has_trans(CharType c) const {
    return trans_.count(c) > 0;
  }
  State *trans(CharType c) const {
    return trans_.at(c);
  }
  void set_trans(CharType c, State *v) {
    trans_[c] = v;
  }

  void add_reversed_link(State *v) {
    reversed_links_.push_back(v);
  }

 private:
  int maxlen_ = 0;
  int minlen_ = 0;
  int first_endpos_ = -1;
  int appearance_ = 1;
  bool split_ = false;

  State *link_ = nullptr;
  vector<State *> reversed_links_;
  unordered_map<CharType, State *> trans_;

  bool accept_ = false;
};


State *AddSymbolToSAM(State *start, State *last, CharType c) {
  auto cur = new State;
  cur->set_maxlen(last->maxlen() + 1);
  cur->set_first_endpos(last->first_endpos() + 1);

  auto p = last;
  while (p && !p->has_trans(c)) {
    p->set_trans(c, cur);
    p = p->link();
  }

  if (p == nullptr) {
    cur->set_link(start);
    cur->set_minlen(1);
    return cur;
  }

  auto q = p->trans(c);
  if (p->maxlen() + 1 == q->maxlen()) {
    cur->set_link(q);
    cur->set_minlen(q->maxlen() + 1);
  } else {
    auto sq = new State;
    sq->set_split(true);
    sq->set_maxlen(p->maxlen() + 1);
    sq->set_trans(q->trans());
    sq->set_first_endpos(q->first_endpos());

    while (p && p->has_trans(c) && p->trans(c) == q) {
      p->set_trans(c, sq);
      p = p->link();
    }

    sq->set_link(q->link());
    sq->set_minlen(sq->link()->maxlen() + 1);

    q->set_link(sq);
    q->set_minlen(sq->maxlen() + 1);

    cur->set_link(sq);
    cur->set_minlen(sq->maxlen() + 1);
  }

  return cur;
}


void SetReversedLink(State *u, unordered_set<State *> &searched) {
  if (searched.count(u) > 0) {
    return;
  }
  searched.insert(u);

  auto v = u->link();
  if (v) {
    v->add_reversed_link(u);
  }

  for (auto &tran : u->trans()) {
    SetReversedLink(tran.second, searched);
  }
}


int SetAppearance(State *u) {
  u->set_appearance(u->split() ? 0 : 1);

  int count = u->appearance();
  for (auto child : u->reversed_links()) {
    count += SetAppearance(child);
  }
  u->set_appearance(count);
  return count;
}


WordType Decode(const string &text) {
  WordType utf16text;
  utf8::utf8to16(text.begin(), text.end(), back_inserter(utf16text));
  return utf16text;
}


State *CreateSAM(const WordType &T) {
  auto start = new State;
  auto last = start;

  for (CharType c : T) {
    last = AddSymbolToSAM(start, last, c);
  }

  while (last != start) {
    last->set_accept(true);
    last = last->link();
  }

  unordered_set<State *> searched;
  SetReversedLink(start, searched);
  SetAppearance(start);

  return start;
}


double QueryPossibility(WordType::const_iterator iter_begin,
                        WordType::const_iterator iter_end,
                        State *start, int total) {
  auto state = start;
  for (auto iter = iter_begin; iter != iter_end; ++iter) {
    CharType c = *iter;
    if (!state->has_trans(c)) {
      return -1.0;
    }
    state = state->trans(c);
  }

  int size = iter_end - iter_begin;
  return size * static_cast<double>(state->appearance()) / total;
}

double QueryPossibility(const string &word, State *start, int total) {
  auto utf16word = Decode(word);
  return QueryPossibility(utf16word.begin(), utf16word.end(), start, total);
}


void CollectWordCandidates(State *state, const int depth,
                           WordType &word,
                           vector<WordType> &candidates) {
  if (depth < 0) {
    return;
  }
  candidates.push_back(word);

  for (auto &tran : state->trans()) {
    word.push_back(tran.first);
    CollectWordCandidates(tran.second, depth - 1, word, candidates);
    word.pop_back();
  }
}


// word.size() > 1
double CohesionValue(const WordType &word, State *start, int total) {
  double div = -1.0;
  for (auto iter_mid = word.begin() + 1;
       iter_mid != word.end(); ++iter_mid) {
    double product = QueryPossibility(word.begin(), iter_mid, start, total)
                     * QueryPossibility(iter_mid, word.end(), start, total);
    div = max(div, product);
  }

  double base = QueryPossibility(word.begin(), word.end(), start, total);
  return base / div;
}


double EntropyFormula(const vector<double> &possibilities) {
  double entropy = 0.0;
  for (double p : possibilities) {
    entropy += p * log2(p);
  }
  return -entropy;
}


vector<CharType> FindAdjacentChars(const WordType &word, State *start) {
  auto state = start;
  for (CharType c : word) {
    state = state->trans(c);
  }
  vector<CharType> adjacent_chars;
  for (auto tran : state->trans()) {
    adjacent_chars.push_back(tran.first);
  }
  return adjacent_chars;
}


double EntropyValue(const WordType &word, State *start, const int total) {
  auto adjacent_chars = FindAdjacentChars(word, start);
  vector<double> possibilities;
  for (auto iter = adjacent_chars.begin();
       iter != adjacent_chars.end(); ++iter) {
    possibilities.push_back(
        QueryPossibility(iter, iter + 1, start, total));
  }
  return EntropyFormula(possibilities);
}


double LeftRightEntropyValue(WordType &word,
                             State *start, State *rstart,
                             const int total) {
  double right = EntropyValue(word, start, total);
  reverse(word.begin(), word.end());
  double left = EntropyValue(word, rstart, total);
  reverse(word.begin(), word.end());

  return min(left, right);
}


int WordAppearance(const WordType &word, State *start) {
  auto state = start;
  for (CharType c : word) {
    state = state->trans(c);
  }
  return state->appearance();
}


vector<WordType> FilterCandidates(vector<WordType> &candidates,
                                  const double kCohesion,
                                  const double kEntropy,
                                  const int kAppearance,
                                  State *start, State *rstart,
                                  const int total) {
  set<pair<int, WordType>> appearance_word;
  for (auto &word : candidates) {
    if (word.size() <= 1) {
      continue;
    }

    int appearance = WordAppearance(word, start);
    if (appearance < kAppearance) {
      continue;
    }

    double cohesion = CohesionValue(word, start, total);
    if (cohesion < kCohesion) {
      continue;
    }

    double entropy = LeftRightEntropyValue(word, start, rstart, total);
    if (entropy < kEntropy) {
      continue;
    }
    // good word.
    appearance_word.insert({appearance, word});
  }

  vector<WordType> ret;
  for (auto pair : appearance_word) {
    ret.push_back(pair.second);
  }
  reverse(ret.begin(), ret.end());
  return ret;
}


// INPUT: utf-8 encoded file.
// DEPTH: the maximum word-length of candidates.
// COHESION: the lower bound of CohesionValue.
// ENTROPY: the lower bound of LeftRightEntropyValue.
// APPEARANCE: the lower bound of appearance.
int main(int argc, char **argv) {
  if (argc != 6) {
    return 1;
  }

  const string kInputFileName(argv[1]);
  const int kDepth(stoi(argv[2]));
  const double kCohesion(stod(argv[3]));
  const double kEntropy(stod(argv[4]));
  const int kAppearance(stoi(argv[5]));

  ifstream fin(kInputFileName);
  string utf8content;
  fin >> utf8content;
  auto T = Decode(utf8content);
  int word_size = T.size();

  auto start = CreateSAM(T);
  reverse(T.begin(), T.end());
  auto rstart = CreateSAM(T);

  WordType word;
  vector<WordType> candidates;
  CollectWordCandidates(start, kDepth, word, candidates);

  auto extracted_words = FilterCandidates(
      candidates, kCohesion, kEntropy, kAppearance, start, rstart, word_size);
  for (int i = 0; i < min(30, static_cast<int>(extracted_words.size())); ++i) {
    auto &utf16word = extracted_words[i];
    string utf8word;
    utf8::utf16to8(utf16word.begin(), utf16word.end(), back_inserter(utf8word));
    cout << utf8word << endl;
  }
}
