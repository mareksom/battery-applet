#ifndef GTKMM_SLOT_LAMBDA_H_
#define GTKMM_SLOT_LAMBDA_H_

#include <sigc++/sigc++.h>
#include <type_traits>

namespace sigc {

template <typename Functor>
struct functor_trait<Functor, false> {
  typedef decltype (::sigc::mem_fun(
      std::declval<Functor&>(), &Functor::operator())) _intermediate;
  typedef typename _intermediate::result_type result_type;
  typedef Functor functor_type;
};

}  // namespace sigc

#endif  // GTKMM_SLOT_LAMBDA_H_
