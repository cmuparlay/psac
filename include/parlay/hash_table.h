// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include "utilities.h"

namespace parlay {

  // A "history independent" hash table that supports insertion, and searching
  // It is described in the paper
  //   Guy E. Blelloch, Daniel Golovin
  //   Strongly History-Independent Hashing with Applications
  //   FOCS 2007: 272-282
  // At any quiescent point (when no operations are actively updating the
  //   structure) the state will depend only on the keys it contains and not
  //   on the history of the insertion order.
  // Insertions can happen in parallel, but they cannot overlap with searches
  // Searches can happen in parallel
  // Deletions must happen sequentially
  template <class HASH>
  struct Table {
    using eType = typename HASH::eType;
    using kType = typename HASH::kType;
    size_t m;
    eType empty;
    HASH hashStruct;
    eType* TA;
    using index = long;

    static void clear(eType* A, size_t n, eType v) {
      parallel_for(0, n, [&](size_t i) {
        assign_uninitialized(A[i], v);    
      });
    }

    struct notEmptyF {
      eType e; notEmptyF(eType _e) : e(_e) {}
      int operator() (eType a) {return e != a;}};

    __attribute__((no_sanitize("integer")))
    index hashToRange(index h) {return (int) h % (uint) m;}

    index firstIndex(kType v) {return hashToRange(hashStruct.hash(v));}
    index incrementIndex(index h) {return (h + 1 == (long) m) ? 0 : h+1;}
    index decrementIndex(index h) {return (h == 0) ? m-1 : h-1;}
    bool lessIndex(index a, index b) {return (a < b) ? (2*(b-a) < m) : (2*(a-b) > m);}
    bool lessEqIndex(index a, index b) {return a==b || lessIndex(a,b);}

  eType* new_array_no_init(size_t n) {
    return (eType*) ::operator new(n * sizeof(eType));
  }

  void delete_array(eType* ptr) {
    ::operator delete(ptr);
  }

  public:
    // Size is the maximum number of values the hash table will hold.
    // Overfilling the table could put it into an infinite loop.
    Table(size_t size, HASH hashF, float load = 1.5) :
      m(((size_t) 100.0 + load * size)),
      empty(hashF.empty()),
      hashStruct(hashF),
      TA(new_array_no_init(m)) {
      clear(TA, m, empty); }

    ~Table() { delete_array(TA); };

    // Apply the given function to every element
    // currently in the hashtable
    template<typename Function>
    void for_all(Function f) {
      parallel_for(0, m, [&](auto i) {
        if (TA[i] != empty) {
          f(TA[i]);
        }
      });
    }

    // prioritized linear probing
    //   a new key will bump an existing key up if it has a higher priority
    //   an equal key will replace an old key if replaceQ(new,old) is true
    // returns 0 if not inserted (i.e. equal and replaceQ false) and 1 otherwise
    bool insert(eType v) {
      index i = firstIndex(hashStruct.getKey(v));
      while (true) {
	eType c = TA[i];
	if (c == empty) {
	  if (hashStruct.cas(&TA[i],c,v)) return true;
	} else {
	  int cmp = hashStruct.cmp(hashStruct.getKey(v),hashStruct.getKey(c));
	  if (cmp == 0) {
	    if (!hashStruct.replaceQ(v,c)) return false;
	    else if (hashStruct.cas(&TA[i],c,v)) return true;
	  } else if (cmp < 0)
	    i = incrementIndex(i);
	  else if (hashStruct.cas(&TA[i],c,v)) {
	    v = c;
	    i = incrementIndex(i);
	  }
	}
      }
    }

    // prioritized linear probing
    //   a new key will bump an existing key up if it has a higher priority
    //   an equal key will replace an old key if replaceQ(new,old) is true
    // returns 0 if not inserted (i.e. equal and replaceQ false) and 1 otherwise
    bool update(eType v) {
      index i = firstIndex(hashStruct.getKey(v));
      while (true) {
	eType c = TA[i];
	if (c == empty) {
	  if (hashStruct.cas(&TA[i],c,v)) return true;
	} else {
	  int cmp = hashStruct.cmp(hashStruct.getKey(v),hashStruct.getKey(c));
	  if (cmp == 0) {
	    if (!hashStruct.replaceQ(v,c)) return false;
	    else {
	      eType new_val = hashStruct.update(c,v);
	      if (hashStruct.cas(&TA[i],c,new_val)) return true;
	    }
	  } else if (cmp < 0)
	    i = incrementIndex(i);
	  else if (hashStruct.cas(&TA[i],c,v)) {
	    v = c;
	    i = incrementIndex(i);
	  }
	}
      }
    }

    eType deleteVal(kType v) {
      eType val = empty;

      index i = firstIndex(v);
      int cmp;

      // find first element less than or equal to v in priority order
      index j = i;
      eType c = TA[j];

      if (c == empty) return empty;

      // find first location with priority less or equal to v's priority
      while ((cmp = (c==empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c))) < 0) {
        j = incrementIndex(j);
        c = TA[j];
      }
      while (true) {
        // Invariants:
        //   v is the key that needs to be deleted
        //   j is our current index into TA
        //   if v appears in TA, then at least one copy must appear at or before j
        //   c = TA[j] at some previous time (could now be changed)
        //   i = h(v)
        //   cmp = compare v to key of c (positive if greater, 0 equal, negative less)
        if (cmp != 0) {
          // v does not match key of c, need to move down one and exit if
          // moving before h(v)
          if (j == i) return val;
          j = decrementIndex(j);
          c = TA[j];
          cmp = (c == empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c));
        } else { // found v at location j (at least at some prior time)
          if (val == empty) val = c;

          // Find next available element to fill location j.
          // This is a little tricky since we need to skip over elements for
          // which the hash index is greater than j, and need to account for
          // things being moved downwards by others as we search.
          // Makes use of the fact that values in a cell can only decrease
          // during a delete phase as elements are moved from the right to left.
          index jj = incrementIndex(j);
          eType x = TA[jj];
          while (x != empty && lessIndex(j, firstIndex(hashStruct.getKey(x)))) {
            jj = incrementIndex(jj);
            x = TA[jj];
          }
          index jjj = decrementIndex(jj);
          while (jjj != j) {
            eType y = TA[jjj];
            if (y == empty || !lessIndex(j, firstIndex(hashStruct.getKey(y)))) {
              x = y;
              jj = jjj;
            }
            jjj = decrementIndex(jjj);
          }

          // try to copy the the replacement element into j
          if (hashStruct.cas(&TA[j],c,x)) {
            // swap was successful
            // if the replacement element was empty, we are done
            if (x == empty) return val;

            // Otherwise there are now two copies of the replacement element x
            // delete one copy (probably the original) by starting to look at jj.
            // Note that others can come along in the meantime and delete
            // one or both of them, but that is fine.
            v = hashStruct.getKey(x);
            j = jj;
            i = firstIndex(v);
          }
          c = TA[j];
          cmp = (c == empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c));
        }
      }
    }

    // Returns the value if an equal value is found in the table
    // otherwise returns the "empty" element.
    // due to prioritization, can quit early if v is greater than cell
    eType find(kType v) {
      index h = firstIndex(v);
      eType c = TA[h];
      while (true) {
	if (c == empty) return empty;
	int cmp = hashStruct.cmp(v,hashStruct.getKey(c));
	if (cmp >= 0) {
	  if (cmp > 0) return empty;
	  else return c;
	}
	h = incrementIndex(h);
	c = TA[h];
      }
    }

    index findIndex(kType v) {
      index h = firstIndex(v);
      eType c = TA[h];
      while (true) {
	if (c == empty) return -1;
	int cmp = hashStruct.cmp(v,hashStruct.getKey(c));
	if (cmp >= 0) {
	  if (cmp > 0) return -1;
	  else return h;
	}
	h = incrementIndex(h);
	c = TA[h];
      }
    }
  };

}

#pragma clang diagnostic pop

