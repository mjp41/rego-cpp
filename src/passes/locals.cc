#include "builtins.h"
#include "errors.h"
#include "helpers.h"
#include "passes.h"

#include <map>

namespace
{
  using namespace rego;
  using Scope = std::map<std::string, Node>;

  void add_locals(
    Node unifybody, std::vector<Scope>& scopes, const BuiltIns& builtins);

  void find_free_vars(
    const Node& node, std::vector<Scope>& scopes, const BuiltIns& builtins)
  {
    std::set<Token> exclude_parents = {
      RefArgDot, RuleRef, VarSeq, ArrayCompr, SetCompr, ObjectCompr, WithSeq};
    if (exclude_parents.contains(node->type()))
    {
      return;
    }

    if (node->type() == Local)
    {
      Node var = node / Var;
      std::string name = std::string(var->location().view());
      for (auto& scope : scopes)
      {
        if (scope.contains(name))
        {
          scope[name] = Undefined;
        }
      }
      return;
    }

    if (node->type() == Var)
    {
      if (builtins.is_builtin(node->location()))
      {
        return;
      }

      std::string name = std::string(node->location().view());
      if (name == "data")
      {
        // reserved
        return;
      }

      for (auto& scope : scopes)
      {
        if (scope.contains(name))
        {
          return;
        }
      }

      Nodes defs = node->lookup();
      if (defs.size() > 0)
      {
        return;
      }

      scopes.back().insert({name, Local << node->clone() << Undefined});
    }
    else if (node->type() == UnifyBody)
    {
      add_locals(node, scopes, builtins);
    }
    else
    {
      for (auto& child : *node)
      {
        find_free_vars(child, scopes, builtins);
      }
    }
  }

  void add_locals(
    Node unifybody, std::vector<Scope>& scopes, const BuiltIns& builtins)
  {
    if (unifybody->type() == NestedBody)
    {
      unifybody = unifybody / Val;
    }

    if (unifybody->type() != UnifyBody)
    {
      return;
    }

    scopes.push_back({});
    for (const auto& child : *unifybody)
    {
      find_free_vars(child, scopes, builtins);
    }

    for (const auto& [name, local] : scopes.back())
    {
      if (local->type() == Local)
      {
        unifybody->push_front(local);
      }
    }

    scopes.pop_back();
  }

  int preprocess_body(Node node, const BuiltIns& builtins)
  {
    std::vector<Scope> scopes;
    add_locals(node / Body, scopes, builtins);
    return 0;
  }

  int preprocess_value(Node node, const BuiltIns& builtins)
  {
    std::vector<Scope> scopes;
    add_locals(node / Val, scopes, builtins);
    return 0;
  }
}

namespace rego
{
  // Discovers undeclared local variables from rule bodies and comprehensions
  // inserts Local nodes for them at the appropriate scope.
  PassDef body_locals(const BuiltIns& builtins)
  {
    PassDef locals = {dir::bottomup | dir::once};

    locals.pre(
      RuleComp, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      RuleFunc, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      RuleObj, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      RuleSet, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      ArrayCompr, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      SetCompr, [builtins](Node n) { return preprocess_body(n, builtins); });
    locals.pre(
      ObjectCompr, [builtins](Node n) { return preprocess_body(n, builtins); });

    return locals;
  }

  // Discovers undeclared local variables from rule values and inserts Local
  // nodes for them at the appropriate scope.
  PassDef value_locals(const BuiltIns& builtins)
  {
    PassDef locals = {dir::bottomup | dir::once};

    locals.pre(
      RuleComp, [builtins](Node n) { return preprocess_value(n, builtins); });
    locals.pre(
      RuleFunc, [builtins](Node n) { return preprocess_value(n, builtins); });
    locals.pre(
      RuleObj, [builtins](Node n) { return preprocess_value(n, builtins); });
    locals.pre(
      RuleSet, [builtins](Node n) { return preprocess_value(n, builtins); });

    return locals;
  }
}