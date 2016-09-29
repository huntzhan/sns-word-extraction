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

#define DEFINE_ACCESSOR_AND_MUTATOR(type, name) \
  type name() {                                 \
    return name ## _;                           \
  }                                             \
  void set_ ## name(type val) {                 \
    name ## _ = val;                            \
  }                                             \


class State;

using CharType = uint16_t;
using WordType = vector<CharType>;

using StatePtr = State *;
using TransType = unordered_map<CharType, StatePtr>;


class State {
 public:

  DEFINE_ACCESSOR_AND_MUTATOR(bool, empty)

  DEFINE_ACCESSOR_AND_MUTATOR(int, maxlen)
  DEFINE_ACCESSOR_AND_MUTATOR(int, minlen)
  // DEFINE_ACCESSOR_AND_MUTATOR(int, first_endpos)
  DEFINE_ACCESSOR_AND_MUTATOR(StatePtr, link)
  // DEFINE_ACCESSOR_AND_MUTATOR(bool, accept)
  DEFINE_ACCESSOR_AND_MUTATOR(TransType, trans)
  // DEFINE_ACCESSOR_AND_MUTATOR(vector<StatePtr>, reversed_links)
  DEFINE_ACCESSOR_AND_MUTATOR(int, appearance)
  DEFINE_ACCESSOR_AND_MUTATOR(bool, split)

  bool has_trans(CharType c) const {
    return trans_.count(c) > 0;
  }
  StatePtr trans(CharType c) const {
    return trans_.at(c);
  }
  void set_trans(CharType c, StatePtr v) {
    trans_[c] = v;
  }
  void free_trans() {
    trans_.clear();
  }

 private:
  bool empty_ = true;

  int maxlen_ = 0;
  int minlen_ = 0;
  // int first_endpos_ = -1;
  int appearance_ = 1;
  bool split_ = false;

  StatePtr link_ = nullptr;
  // vector<StatePtr> reversed_links_;
  unordered_map<CharType, StatePtr> trans_;

  // bool accept_ = false;
};


struct StateManager {

  static StatePtr CreateState() {
    while (free_index_ < upper_bound_ && !state_space_[free_index_].empty()) {
      ++free_index_;
    }
    if (free_index_ == upper_bound_) {
      // all consumed.
      throw runtime_error("Too Much State.");
    }
    state_space_[free_index_].set_empty(false);
    return &state_space_[free_index_];
  }

  static void AllocateMemory(const int size) {
    free_index_ = 0;
    upper_bound_ = size;
    state_space_ = state_allocator_.allocate(size);
    for (int i = 0; i < size; ++i) {
      new(&state_space_[i]) State;
    }
  }

  static void DeallocateMemory() {
    state_allocator_.deallocate(state_space_, upper_bound_);
  }

  static void ResetFreeIndex() {
    free_index_ = 0;
  }

  static StatePtr state_space_;
  static int free_index_;
  static int upper_bound_;

  static allocator<State> state_allocator_;
};


StatePtr StateManager::state_space_ = nullptr;
int StateManager::free_index_ = 0;
int StateManager::upper_bound_ = 0;
allocator<State> StateManager::state_allocator_{};


StatePtr AddSymbolToSAM(StatePtr start, StatePtr last, CharType c) {
  auto cur = StateManager::CreateState();
  cur->set_maxlen(last->maxlen() + 1);
  // cur->set_first_endpos(last->first_endpos() + 1);

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
    auto sq = StateManager::CreateState();
    sq->set_split(true);
    sq->set_maxlen(p->maxlen() + 1);
    sq->set_trans(q->trans());
    // sq->set_first_endpos(q->first_endpos());

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


struct StateHash {
    size_t operator()(const StatePtr val) const {
        static const size_t shift =
            static_cast<size_t>(log2(1 + sizeof(State)));
        return reinterpret_cast<size_t>(val) >> shift;
    }
};


void SetReversedLink(
      StatePtr start,
      unordered_map<StatePtr, vector<StatePtr>, StateHash> &reversed_links,
      unordered_set<StatePtr, StateHash> &searched) {

  queue<StatePtr> q;
  q.push(start);

  while (!q.empty()) {
    auto u = q.front();
    q.pop();

    if (u && searched.count(u) > 0) {
      continue;
    }
    searched.insert(u);

    auto v = u->link();
    if (v) {
      reversed_links[v].push_back(u);
    }

    for (auto &tran : u->trans()) {
      q.push(tran.second);
    }
  }
}


int SetAppearance(
      StatePtr u,
      unordered_map<StatePtr, vector<StatePtr>, StateHash> &reversed_links) {
  int count = u->split() ? 0 : 1;

  if (reversed_links.count(u) > 0) {
    for (auto child : reversed_links[u]) {
      count += SetAppearance(child, reversed_links);
    }
  }
  u->set_appearance(count);
  return count;
}


void FreeState(StatePtr node, unordered_set<StatePtr> &skip_nodes) {
  queue<StatePtr> q;
  q.push(node);

  while (!q.empty()) {
    auto u = q.front();
    q.pop();
    if (skip_nodes.count(u) > 0) {
      continue;
    }
    for (auto &tran : u->trans()) {
      q.push(tran.second);
    }
    u->free_trans();
    u->set_link(nullptr);
    u->set_empty(true);
  }
}


void FreeUnnecessaryStates(StatePtr start, const int kDepth) {
  queue<StatePtr> q;
  unordered_set<StatePtr> skip_nodes;

  q.push(start);
  int level = 0;
  int level_size = 1;

  while (level <= kDepth && !q.empty()) {
    level_size = q.size();

    for (int i = 0; i < level_size; ++i) {
      auto node = q.front();
      q.pop();
      skip_nodes.insert(node);
      for (auto &tran : node->trans()) {
        q.push(tran.second);
      }
    }
    
    ++level;
  }

  while (!q.empty()) {
    auto free_root = q.front();
    q.pop();
    // it's possibile!
    if (skip_nodes.count(free_root) > 0) {
      continue;
    }
    for (auto &tran : free_root->trans()) {
      FreeState(tran.second, skip_nodes);
    }
    free_root->free_trans();
  }
}


StatePtr CreateSAM(const WordType &T, const int kDepth) {
  auto start = StateManager::CreateState();
  auto last = start;

  for (CharType c : T) {
    last = AddSymbolToSAM(start, last, c);
  }

  while (last != start) {
    // last->set_accept(true);
    last = last->link();
  }

  unordered_map<StatePtr, vector<StatePtr>, StateHash> reversed_links;
  unordered_set<StatePtr, StateHash> searched;

  SetReversedLink(start, reversed_links, searched);
  SetAppearance(start, reversed_links);

  FreeUnnecessaryStates(start, kDepth);
  StateManager::ResetFreeIndex();
  return start;
}


WordType Decode(const string &text) {
  WordType utf16text;
  utf8::utf8to16(text.begin(), text.end(), back_inserter(utf16text));
  return utf16text;
}


double QueryPossibility(WordType::const_iterator iter_begin,
                        WordType::const_iterator iter_end,
                        StatePtr start, int total) {
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

double QueryPossibility(const string &word, StatePtr start, int total) {
  auto utf16word = Decode(word);
  return QueryPossibility(utf16word.begin(), utf16word.end(), start, total);
}


void CollectWordCandidates(StatePtr state, const int depth,
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
double CohesionValue(const WordType &word, StatePtr start, int total) {
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


vector<CharType> FindAdjacentChars(const WordType &word, StatePtr start) {
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


double EntropyValue(const WordType &word, StatePtr start, const int total) {
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
                             StatePtr start, StatePtr rstart,
                             const int total) {
  double right = EntropyValue(word, start, total);
  reverse(word.begin(), word.end());
  double left = EntropyValue(word, rstart, total);
  reverse(word.begin(), word.end());

  return min(left, right);
}


int WordAppearance(const WordType &word, StatePtr start) {
  auto state = start;
  for (CharType c : word) {
    state = state->trans(c);
  }
  return state->appearance();
}


struct WordInfo {

  WordInfo(const WordType &utf16word,
           int appearance,
           double cohesion,
           double entropy)
      : appearance_(appearance), cohesion_(cohesion), entropy_(entropy) {
    // encoding.
    utf8::utf16to8(utf16word.begin(), utf16word.end(), back_inserter(text_));
  }

  bool operator<(const WordInfo &other) const {
    return appearance_ > other.appearance_;
  }

  string ToStr() const {
    string ret = text_;
    ret.push_back(' ');

    ret.append(to_string(appearance_));
    ret.push_back(' ');

    ret.append(to_string(cohesion_));
    ret.push_back(' ');

    ret.append(to_string(entropy_));

    return ret;
  }
  
  int appearance_ = -1;
  double cohesion_ = -1.0;
  double entropy_ = -1.0;
  string text_ = "";
};


set<WordInfo> FilterCandidates(vector<WordType> &candidates,
                                  const double kCohesion,
                                  const double kEntropy,
                                  const int kAppearance,
                                  StatePtr start, StatePtr rstart,
                                  const int total) {
  set<WordInfo> ret_words;
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
    ret_words.insert(WordInfo(word, appearance, cohesion, entropy));
  }
  return ret_words;
}


// INPUT-MATERIAL: utf-8 encoded file.
// INPUT-STOPWORDS: utf-8 encoded file.
// OUTPUT: utf-8 encoded file.
// DEPTH: the maximum word-length of candidates.
// COHESION: the lower bound of CohesionValue.
// ENTROPY: the lower bound of LeftRightEntropyValue.
// APPEARANCE: the lower bound of appearance.
int main(int argc, char **argv) {
  if (argc != 8) {
    return 1;
  }

  const string kInputMaterialFileName(argv[1]);
  const string kInputStopwordsFileName(argv[2]);

  const string kOutputFileName(argv[3]);

  const int kDepth(stoi(argv[4]));
  const double kCohesion(stod(argv[5]));
  const double kEntropy(stod(argv[6]));
  const int kAppearance(stoi(argv[7]));

  ifstream fin(kInputMaterialFileName);
  string utf8content;
  fin >> utf8content;
  auto T = Decode(utf8content);
  int word_size = T.size();

  StateManager::AllocateMemory(3 * word_size);

  unordered_set<string> stopwords;
  if (!kInputStopwordsFileName.empty()) {
    ifstream fin(kInputStopwordsFileName);
    string word;
    while (fin) {
      fin >> word;
      stopwords.insert(word);
    }
  }

  auto start = CreateSAM(T, kDepth);
  reverse(T.begin(), T.end());
  auto rstart = CreateSAM(T, kDepth);

  WordType word;
  vector<WordType> candidates;
  CollectWordCandidates(start, kDepth, word, candidates);

  auto extracted_words = FilterCandidates(
      candidates, kCohesion, kEntropy, kAppearance, start, rstart, word_size);

  ofstream fout(kOutputFileName);
  for (auto &word : extracted_words) {
    if (stopwords.count(word.text_) > 0) {
      continue;
    } else {
      fout << word.ToStr() << endl;
    }
  }

  StateManager::DeallocateMemory();
  return 0;
}
