#include "errors.h"
#include "helpers.h"
#include "passes.h"

namespace rego
{
  const inline auto AllOps =
    T(Add) / T(Subtract) / T(Multiply) / T(Divide) / T(Modulo) / T(Unify);

  // Finds unary expressions by looking for patterns in which
  // there is a stray Subtract token.
  PassDef unary()
  {
    return {
      In(Expr) * (Start * T(Subtract) * ArithInfixArg[Val]) >>
        [](Match& _) { return UnaryExpr << (ArithArg << _(Val)); },

      In(Expr) * (AllOps[Op] * T(Subtract) * ArithInfixArg[Val]) >>
        [](Match& _) {
          return Seq << _(Op) << (UnaryExpr << (ArithArg << _(Val)));
        },
    };
  }

  const inline auto Ops = T(Multiply) / T(Divide) / T(Modulo);

  // Processes multiply, divide, and modulo operations into ArithInfix nodes.
  // Also handles set intersection.
  PassDef multiply_divide()
  {
    return {
      In(Expr) * (ArithInfixArg[Lhs] * Ops[Op] * ArithInfixArg[Rhs]) >>
        [](Match& _) {
          return ArithInfix << (ArithArg << _(Lhs)) << _(Op)
                            << (ArithArg << _(Rhs));
        },

      T(Expr) << (T(Expr)[Expr] * End) >> [](Match& _) { return _(Expr); },

      In(Expr) * (BinInfixArg[Lhs] * T(And) * BinInfixArg[Rhs]) >>
        [](Match& _) {
          return BinInfix << (BinArg << _(Lhs)) << And << (BinArg << _(Rhs));
        },

      // errors

      In(Expr) * Ops[Op] >>
        [](Match& _) { return err(_(Op), "Invalid multiply/divide"); },

      In(Expr) * T(And)[And] >>
        [](Match& _) { return err(_(And), "Invalid and"); },
    };
  }

  // Transforms addition, subtraction, and unary negation into ArithInfix Nodes.
  // Also, handles set union and difference.
  PassDef add_subtract()
  {
    return {
      In(Expr) *
          (ArithInfixArg[Lhs] * (T(Add) / T(Subtract))[Op] *
           ArithInfixArg[Rhs]) >>
        [](Match& _) {
          return ArithInfix << (ArithArg << _(Lhs)) << _(Op)
                            << (ArithArg << _(Rhs));
        },

      In(Expr) * (ArithInfixArg * T(Subtract) * BinInfixArg[Rhs]) >>
        [](Match& _) {
          return err(
            _(Rhs), "operand 2 must be number but got set", EvalTypeError);
        },

      In(Expr) * (BinInfixArg * T(Subtract) * ArithInfixArg[Rhs]) >>
        [](Match& _) {
          return err(
            _(Rhs), "operand 2 must be set but got number", EvalTypeError);
        },

      In(Expr) * (T(Subtract) * ArithInfixArg[Val] * End) >>
        [](Match& _) { return UnaryExpr << (ArithArg << _(Val)); },

      In(Expr) * (T(UnaryExpr) << (T(UnaryExpr) << Any[Val])) >>
        [](Match& _) { return _(Val); },

      In(ArithArg) *
          (T(Expr)
           << ((T(RefTerm) / T(NumTerm) / T(ArithInfix) / T(UnaryExpr) /
                T(ExprCall))[Val] *
               End)) >>
        [](Match& _) { return _(Val); },

      In(Expr) *
          (BinInfixArg[Lhs] * (T(Subtract) / T(Or))[Op] * BinInfixArg[Rhs]) >>
        [](Match& _) {
          return BinInfix << (BinArg << _(Lhs)) << _(Op) << (BinArg << _(Rhs));
        },

      In(BinArg) *
          (T(Expr)
           << ((T(Ref) / T(RefTerm) / T(ExprCall) / T(Set) / T(SetCompr) /
                T(BinInfix))[Val] *
               End)) >>
        [](Match& _) { return _(Val); },

      // errors

      In(Expr) * (T(Add) / T(Subtract))[Op] >>
        [](Match& _) { return err(_(Op), "Invalid add/subtract"); },

      In(Expr) * T(Or)[Op] >>
        [](Match& _) { return err(_(Op), "Invalid set union"); },

      T(ArithArg)[ArithArg] << (Any * Any) >>
        [](Match& _) {
          return err(_(ArithArg), "Argument can only have one element");
        },

      T(BinArg)[BinArg] << (Any * Any) >>
        [](Match& _) {
          return err(_(BinArg), "Argument can only have one element");
        },
    };
  }

}