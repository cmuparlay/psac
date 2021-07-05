
#ifndef PARLAY_PARALLEL_H_
#define PARLAY_PARALLEL_H_

#include <cstddef>

#include "scheduler.h"

namespace parlay {

extern inline fork_join_scheduler& get_default_scheduler() {
  static fork_join_scheduler fj;
  return fj;
}

inline size_t num_workers() {
  return get_default_scheduler().num_workers();
}

inline void set_num_workers(size_t num_workers) {
  get_default_scheduler().set_num_workers(num_workers);
}

inline size_t worker_id() {
  return get_default_scheduler().worker_id();
}

template <class F>
inline void parallel_for(size_t start, size_t end, F f,
                         size_t granularity = 0,
                         bool conservative = false) {
  if (end > start)
    get_default_scheduler().parfor(start, end, f, granularity, conservative);
}

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool conservative = false) {
  return get_default_scheduler().pardo(left, right, conservative);
}

}  // namespace parlay

#endif  // PARLAY_PARALELL_H_
