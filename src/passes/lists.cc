#include "errors.h"
#include "helpers.h"
#include "passes.h"

namespace
{
  using namespace rego;

  bool contains_or(const Node& group)
  {
    for (Node child : *group)
    {
      if (child->type() == Or)
      {
        return true;
      }
    }

    return false;
  }

  bool contains(const Node& node, const std::set<Token>& tokens)
  {
    if (node->type() == Error)
    {
      return false;
    }

    if (tokens.contains(node->type()))
    {
      return true;
    }

    for (Node child : *node)
    {
      if (contains(child, tokens))
      {
        return true;
      }
    }

    return false;
  }

  bool is_top_level(const Node& node)
  {
    return node->parent()->parent()->type() == Policy;
  }
}

namespace rego
{
  // Processes lists into more semantically meaningful structures.
  PassDef lists()
  {
    return {
      In(Data) * (T(Brace) << T(List)[List]) >>
        [](Match& _) { return ObjectItemSeq << *_[List]; },

      In(Data) * (T(Brace) << End) >>
        ([](Match&) -> Node { return ObjectItemSeq; }),

      In(Group) * (T(Var)[Var] * T(If) * T(Brace)[Brace]) >>
        [](Match& _) {
          return Seq << _(Var) << If << (UnifyBody << *_[Brace]);
        },

      In(Group) *
          (Start * T(Var)[Var]([](auto& n) { return is_top_level(*n.first); }) *
           T(Brace)[Brace]) >>
        [](Match& _) { return Seq << _(Var) << (UnifyBody << *_[Brace]); },

      In(Group) *
          (Start * T(Var)[Var]([](auto& n) { return is_top_level(*n.first); }) *
           (T(Assign) / T(Unify))[Assign] * ExprToken[Head] *
           ExprToken++[Tail] * T(Brace)[Brace]) >>
        [](Match& _) {
          return Seq << _(Var) << _(Assign) << _(Head) << _[Tail]
                     << (UnifyBody << *_[Brace]);
        },

      In(Group) *
          ((T(Square) / T(Brace))[Compr] << (T(Group)[Group](
             [](auto& n) { return contains_or(*n.first); }))) >>
        [](Match& _) {
          // array/set comprehension
          Node group = NodeDef::create(Group);
          Node item = NodeDef::create(Undefined);
          for (auto& node : *_(Group))
          {
            if (node->type() == Or)
            {
              item = group;
              group = NodeDef::create(Group);
            }
            else
            {
              group->push_back(node);
            }
          }

          Node unifybody = NodeDef::create(UnifyBody);
          if (group->size() > 0)
          {
            unifybody->push_back(group);
          }
          for (auto it = _(Compr)->begin() + 1; it != _(Compr)->end(); ++it)
          {
            unifybody->push_back(*it);
          }

          if (_(Compr)->type() == Brace)
          {
            return SetCompr << item << unifybody;
          }

          return ArrayCompr << item << unifybody;
        },

      In(Group) *
          (T(Brace)[Compr]
           << ((T(List) << ((T(ObjectItem)
                             << (T(Group)[Key] * T(Group)[Val]([](auto& n) {
                                   return contains_or(*n.first);
                                 })))) *
                  End) *
               T(Group)++[UnifyBody] * End)) >>
        [](Match& _) {
          // object comprehension
          Node group = NodeDef::create(Group);
          Node val;
          for (auto& node : *_(Val))
          {
            if (node->type() == Or)
            {
              val = group;
              group = NodeDef::create(Group);
            }
            else
            {
              group->push_back(node);
            }
          }

          Node unifybody = NodeDef::create(UnifyBody);
          if (group->size() > 0)
          {
            unifybody->push_back(group);
          }

          unifybody << _[UnifyBody];

          return ObjectCompr << _(Key) << val << unifybody;
        },

      In(Group) * (T(Brace) << End) >> [](Match&) -> Node { return Object; },

      In(Group) *
          (T(Brace)
           << (T(List)
               << (T(ObjectItem)[Head] * T(ObjectItem)++[Tail] * End))) >>
        [](Match& _) { return Object << _(Head) << _[Tail]; },

      In(Group) * (T(Brace) << (T(List)[List] * End)) >>
        [](Match& _) { return Set << *_[List]; },

      In(Group) *
          T(Brace)[Brace]([](auto& n) { return is_top_level(*n.first); }) >>
        [](Match& _) { return UnifyBody << *_[Brace]; },

      In(Group) * (T(Square) << (T(List)[List] * End)) >>
        [](Match& _) { return Array << *_[List]; },

      In(Group, ImportRef) * (T(Square) << T(Group)[Group]) >>
        [](Match& _) { return Array << _(Group); },

      T(List)
          << ((T(Group) << (T(Every) * T(Var)[Key])) *
              (T(Group) << (T(Var)[Val] * Any[Head] * Any++[Tail]))) >>
        [](Match& _) {
          return Group
            << (ExprEvery << (VarSeq << (Group << _(Key)) << (Group << _(Val)))
                          << (EverySeq << (Group << _(Head) << _[Tail])));
        },

      In(Group) * (T(Every) * T(Var)[Var] * Any++[Tail]) >>
        [](Match& _) {
          return ExprEvery << (VarSeq << (Group << _(Var)))
                           << (EverySeq << (Group << _[Tail]));
        },

      In(Group) * T(UnifyBody)[UnifyBody]([](auto& n) {
        return is_in(*n.first, {ExprEvery});
      }) >>
        [](Match& _) { return Lift << ExprEvery << _(UnifyBody); },

      In(Group) * (T(Brace) << (T(Group)[Head] * T(Group)++[Tail] * End)) >>
        [](Match& _) { return UnifyBody << _(Head) << _[Tail]; },

      T(List)
          << ((T(Group)
               << (MembershipToken++[Lhs] * (T(Assign) / T(Unify))[Assign] *
                   MembershipToken++[Idx] * End)) *
              (T(Group) << (MembershipToken++[Item] * T(IsIn) *
                            MembershipToken++[ItemSeq] * T(IsIn) *
                            MembershipToken++[ItemSeq1]) *
                 End)) >>
        [](Match& _) {
          return Group << _[Lhs] << _(Assign)
                       << (Paren
                           << (Group << _[Idx] << Comma << _[Item] << IsIn
                                     << _[ItemSeq]))
                       << IsIn << _[ItemSeq1];
        },

      T(List)
          << ((T(Group) << (MembershipToken++[Idx] * End)) *
              (T(Group) << (MembershipToken++[Item] * T(IsIn) *
                            MembershipToken++[ItemSeq] * T(IsIn) *
                            MembershipToken++[ItemSeq1]) *
                 End)) >>
        [](Match& _) {
          return Group << (Paren
                           << (Group << _[Idx] << Comma << _[Item] << IsIn
                                     << _[ItemSeq]))
                       << IsIn << _[ItemSeq1];
        },

      T(List)
          << ((T(Group)
               << (MembershipToken++[Lhs] * (T(Assign) / T(Unify))[Assign] *
                   MembershipToken++[Idx] * End)) *
              (T(Group)
               << (MembershipToken++[Item] * T(IsIn)[IsIn] *
                   MembershipToken++[ItemSeq] * End))) >>
        [](Match& _) {
          return Group << _[Lhs] << _(Assign) << _[Idx] << Comma << _[Item]
                       << IsIn << _[ItemSeq];
        },

      T(List)
          << ((T(Group) << (MembershipToken++[Idx] * End)) *
              (T(Group)
               << (MembershipToken++[Item] * T(IsIn)[IsIn] *
                   MembershipToken++[ItemSeq] * End))) >>
        [](Match& _) {
          return Group << _[Idx] << Comma << _[Item] << IsIn << _[ItemSeq];
        },

      In(UnifyBody) * T(List)[List] >> [](Match& _) { return Seq << *_[List]; },

      In(Group) * (T(Some) << (T(List)[List] * End)) >>
        [](Match& _) {
          Node list = _(List);
          if (list->size() == 0)
          {
            return err(list, "some must contain at least one variable");
          }

          Node maybe_contains_in;
          if (list->size() == 1)
          {
            maybe_contains_in = list->front();
          }
          else
          {
            maybe_contains_in = list->at(1);
          }

          auto in_pos = maybe_contains_in->begin();
          for (; in_pos != maybe_contains_in->end(); ++in_pos)
          {
            Node n = *in_pos;
            if (n->type() == IsIn)
            {
              break;
            }
          }

          if (in_pos != maybe_contains_in->end())
          {
            Node item = NodeDef::create(Group);
            item->insert(item->end(), maybe_contains_in->begin(), in_pos);
            Node itemseq = NodeDef::create(Group);
            itemseq->insert(itemseq->end(), in_pos, maybe_contains_in->end());
            if (list->size() > 2)
            {
              // some i, x in c with ...
              Node seq = Seq
                << (SomeDecl << (VarSeq << list->front() << item) << itemseq);
              for (auto it = list->begin() + 2; it != list->end(); ++it)
              {
                Node group = *it;
                seq->insert(seq->end(), group->begin(), group->end());
              }
              return seq;
            }
            else if (list->size() == 2)
            {
              // some i, x in c
              return SomeDecl << (VarSeq << list->front() << item) << itemseq;
            }
            else
            {
              // some x in c
              return SomeDecl << (VarSeq << item) << itemseq;
            }
          }

          // some x, y, z, ...
          return SomeDecl << (VarSeq << *_[List]) << (Group << Undefined);
        },

      In(Group) * (T(Some) << T(Group)[Group]) >>
        [](Match& _) {
          Node group = _(Group);

          auto in_pos = group->begin();
          for (; in_pos != group->end(); ++in_pos)
          {
            Node n = *in_pos;
            if (n->type() == IsIn)
            {
              break;
            }
          }

          if (in_pos != group->end())
          {
            Node item = NodeDef::create(Group);
            item->insert(item->end(), group->begin(), in_pos);
            Node itemseq = NodeDef::create(Group);
            itemseq->insert(itemseq->end(), in_pos, group->end());
            // some x in c
            return SomeDecl << (VarSeq << item) << itemseq;
          }

          // some x
          return SomeDecl << (VarSeq << group) << (Group << Undefined);
        },

      In(Group) * T(EmptySet) >> ([](Match&) -> Node { return Set; }),

      In(Group) * (T(Square) << End) >> [](Match&) -> Node { return Array; },

      // errors

      In(Data) * T(Brace)[Brace] >>
        [](Match& _) { return err(_(Brace), "Invalid data body"); },

      In(Group) * T(Brace)[Brace] >>
        [](Match& _) { return err(_(Brace), "Invalid object"); },

      In(Group) * T(Square)[Square] >>
        [](Match& _) { return err(_(Square), "Invalid array"); },

      In(Group) * T(Some)[Some] >>
        [](Match& _) { return err(_(Some), "Invalid some declaration"); },

      In(SomeDecl) * T(ObjectItem)[ObjectItem] >>
        [](Match& _) {
          return err(_(ObjectItem), "Invalid object item in some-decl");
        },

      In(VarSeq) * T(ObjectItem)[ObjectItem] >>
        [](Match& _) {
          return err(_(ObjectItem), "Invalid object item in var-seq");
        },

      In(Some) * (T(List) * Any[Rhs]) >>
        [](Match& _) {
          return err(_(Rhs), "Invalid second node in some declaration");
        },

      In(ObjectItemSeq, Object) * T(Group)[Group] >>
        [](Match& _) { return err(_(Group), "Invalid object key/value"); },

      In(Array, Set, List) * T(ObjectItem)[ObjectItem] >>
        [](Match& _) { return err(_(ObjectItem), "Invalid item"); },

      In(Group) * T(Group)[Group] >>
        [](Match& _) { return err(_(Group), "Syntax error"); },

      In(Group) * T(Every)[Every] >>
        [](Match& _) { return err(_(Every), "Invalid every"); },

      In(ImportRef) * T(Square)[Square] >>
        [](Match& _) { return err(_(Square), "Invalid reference"); },

      In(UnifyBody) * T(List)[List] >>
        [](Match& _) { return err(_(List), "Invalid block"); },

      In(EverySeq) * (T(Group)[Group] << End) >>
        [](Match& _) { return err(_(Group), "Invalid every sequence"); },

      In(ExprEvery) * (T(VarSeq) * T(EverySeq)[EverySeq]([](auto& n) {
                         return !contains(*n.first, {UnifyBody, Brace});
                       })) >>
        [](Match& _) { return err(_(EverySeq), "Missing body of every"); },

      In(ArrayCompr, SetCompr, ObjectCompr) * (T(Group)[Group] << End) >>
        [](Match& _) { return err(_(Group), "Invalid comprehension"); },

      In(UnifyBody) * T(ObjectItem)[ObjectItem] >>
        [](Match& _) { return err(_(ObjectItem), "Invalid key/value pair"); },
    };
  }
}