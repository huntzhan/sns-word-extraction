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

const string kInputFilename = "input.txt";
const string kOutputFilename = "output.txt";

ifstream fin(kInputFilename);
ofstream fout(kOutputFilename);

using KeyType = uint16_t;

#define DEFINE_ACCESSOR_AND_MUTATOR(type, name) \
  type name() {                                 \
    return name ## _;                           \
  }                                             \
  void set_ ## name(type val) {                 \
    name ## _ = val;                            \
  }                                             \


class State {
 public:
  using TransType = unordered_map<KeyType, State *>;

  DEFINE_ACCESSOR_AND_MUTATOR(int, maxlen)
  DEFINE_ACCESSOR_AND_MUTATOR(int, minlen)
  DEFINE_ACCESSOR_AND_MUTATOR(int, first_endpos)
  DEFINE_ACCESSOR_AND_MUTATOR(State *, link)
  DEFINE_ACCESSOR_AND_MUTATOR(bool, accept)
  DEFINE_ACCESSOR_AND_MUTATOR(TransType, trans)
  DEFINE_ACCESSOR_AND_MUTATOR(vector<State *>, reversed_links)
  DEFINE_ACCESSOR_AND_MUTATOR(int, appearance)
  DEFINE_ACCESSOR_AND_MUTATOR(bool, split)

  bool has_trans(KeyType c) const {
    return trans_.count(c) > 0;
  }
  State *trans(KeyType c) const {
    return trans_.at(c);
  }
  void set_trans(KeyType c, State *v) {
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
  unordered_map<KeyType, State *> trans_;

  bool accept_ = false;
};


State *AddSymbolToSAM(State *start, State *last, KeyType c) {
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


vector<KeyType> Decode(const string &text) {
  vector<KeyType> utf16text;
  utf8::utf8to16(text.begin(), text.end(), back_inserter(utf16text));
  return utf16text;
}


State *CreateSAM(const string &RT) {
  auto T = Decode(RT);

  auto start = new State;
  auto last = start;

  for (KeyType c : T) {
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


double QueryPossibility(const string &word, State *start, int total) {
  auto state = start;
  for (KeyType c : Decode(word)) {
    if (!state->has_trans(c)) {
      return -1.0;
    }
    state = state->trans(c);
  }
  return word.size() * static_cast<double>(state->appearance()) / total;
}


int main() {
  string T;
  fin >> T;

  auto start = CreateSAM(T);

  cout << QueryPossibility("\xe4\xb8\xad\xe6\x96\x87", start, T.size()) << endl;
}
