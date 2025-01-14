#include "errors.h"
#include "helpers.h"
#include "passes.h"

namespace
{
  using namespace rego;

  const inline auto RefHeadToken = T(Var) / T(Array) / T(Object) / T(Set) /
    T(ArrayCompr) / T(ObjectCompr) / T(SetCompr) / T(ExprCall);
}

namespace rego
{
  // Builts RefTerm and RuleRef nodes up from their tokens.
  PassDef build_refs()
  {
    return {
      In(Group) * (RefHeadToken[RefHead] * T(Dot) * T(Var)[Rhs]) >>
        [](Match& _) {
          return Ref << (RefHead << _(RefHead))
                     << (RefArgSeq << (RefArgDot << _(Rhs)));
        },

      In(Group) * (RefHeadToken[RefHead] * T(Array)[Array]) >>
        [](Match& _) {
          return Ref << (RefHead << _(RefHead))
                     << (RefArgSeq << (RefArgBrack << *_[Array]));
        },

      In(RuleRef) * (T(Var)[RefHead] * T(Dot) * T(Var)[Rhs]) >>
        [](Match& _) {
          return Ref << (RefHead << _(RefHead))
                     << (RefArgSeq << (RefArgDot << _(Rhs)));
        },

      In(RuleRef) *
          (T(Var)[RefHead] *
           (T(Array) << ((T(Group)[Arg] << StringToken) * End))) >>
        [](Match& _) {
          return Ref << (RefHead << _(RefHead))
                     << (RefArgSeq << (RefArgBrack << _(Arg)));
        },

      In(Group, RuleRef) *
          ((T(Ref) << (T(RefHead)[RefHead] * T(RefArgSeq)[RefArgSeq])) *
           T(Dot) * T(Var)[Rhs]) >>
        [](Match& _) {
          return Ref << _(RefHead)
                     << (RefArgSeq << *_[RefArgSeq] << (RefArgDot << _(Rhs)));
        },

      In(Group) *
          ((T(Ref) << (T(RefHead)[RefHead] * T(RefArgSeq)[RefArgSeq])) *
           T(Array)[Array]) >>
        [](Match& _) {
          return Ref << _(RefHead)
                     << (RefArgSeq << *_[RefArgSeq]
                                   << (RefArgBrack << *_[Array]));
        },

      In(RuleRef) *
          ((T(Ref) << (T(RefHead)[RefHead] * T(RefArgSeq)[RefArgSeq])) *
           (T(Array) << ((T(Group)[Arg] << StringToken) * End))) >>
        [](Match& _) {
          return Ref << _(RefHead)
                     << (RefArgSeq << *_[RefArgSeq] << (RefArgBrack << _(Arg)));
        },

      // errors
      In(RefArgBrack) * (T(Group) * T(Group)[Group]) >>
        [](Match& _) {
          return err(
            _(Group), "Multi-dimensional array references are not supported");
        },

      In(RefArgSeq) * (T(RefArgBrack)[RefArgBrack] << End) >>
        [](Match& _) {
          return err(_(RefArgBrack), "Must provide an index argument");
        },
    };
  }
}