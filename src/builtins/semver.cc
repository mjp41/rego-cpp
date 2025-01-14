#include "errors.h"
#include "helpers.h"
#include "register.h"

namespace
{
  using namespace rego;

  struct SemVer
  {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    std::string metadata;
  };

  bool operator<(const SemVer& lhs, const SemVer& rhs)
  {
    if (lhs.major < rhs.major)
    {
      return true;
    }

    if (lhs.minor < rhs.minor)
    {
      return true;
    }

    if (lhs.patch < rhs.patch)
    {
      return true;
    }

    if (lhs.prerelease < rhs.prerelease)
    {
      return true;
    }

    if (lhs.metadata < rhs.metadata)
    {
      return true;
    }

    return false;
  }

  std::optional<SemVer> parse_semver(const std::string& s)
  {
    SemVer result;
    std::size_t start = 0;
    std::size_t end = s.find('.');

    if (end == s.npos)
    {
      // No minor or patch version
      return std::nullopt;
    }

    std::string major = s.substr(start, end);
    if (major.empty())
    {
      return std::nullopt;
    }
    try
    {
      result.major = std::stoi(major);
    }
    catch (std::invalid_argument&)
    {
      return std::nullopt;
    }

    start = end + 1;
    end = s.find('.', start);

    if (end == s.npos)
    {
      // no patch version
      return std::nullopt;
    }

    std::string minor = s.substr(start, end);
    if (minor.empty())
    {
      return std::nullopt;
    }
    try
    {
      result.minor = std::stoi(minor);
    }
    catch (std::invalid_argument&)
    {
      return std::nullopt;
    }

    start = end + 1;
    end = s.find('-', start);
    std::string patch = s.substr(start, end);
    if (patch.empty())
    {
      return std::nullopt;
    }
    try
    {
      result.patch = std::stoi(patch);
    }
    catch (std::invalid_argument&)
    {
      return std::nullopt;
    }

    start = end + 1;
    end = s.find('+', start);
    result.prerelease = s.substr(start, end);

    if (end == s.npos)
    {
      return result;
    }

    result.metadata = s.substr(end + 1);
    return result;
  }

  Node compare(const Nodes& args)
  {
    Node a =
      unwrap_arg(args, UnwrapOpt(0).func("semver.compare").type(JSONString));
    if (a->type() == Error)
    {
      return a;
    }
    Node b =
      unwrap_arg(args, UnwrapOpt(1).func("semver.compare").type(JSONString));
    if (b->type() == Error)
    {
      return b;
    }

    std::string a_str = get_string(a);
    auto a_semver = parse_semver(a_str);
    if (!a_semver.has_value())
    {
      return err(
        args[0],
        "semver.compare: operand 1: string \"" + a_str +
          "\" is not a valid SemVer",
        EvalBuiltInError);
    }

    std::string b_str = get_string(b);
    auto b_semver = parse_semver(b_str);
    if (!b_semver.has_value())
    {
      return err(
        args[1],
        "semver.compare: operand 2: string \"" + b_str +
          "\" is not a valid SemVer",
        EvalBuiltInError);
    }

    if (*a_semver < *b_semver)
    {
      return JSONInt ^ "-1";
    }
    if (*b_semver < *a_semver)
    {
      return JSONInt ^ "1";
    }
    return JSONInt ^ "0";
  }

  Node is_valid(const Nodes& args)
  {
    auto vsn = unwrap(args[0], {JSONString});
    if (!vsn.success)
    {
      return JSONFalse ^ "false";
    }

    std::string vsn_str = get_string(vsn.node);
    auto semver = parse_semver(vsn_str);
    if (semver.has_value())
    {
      return JSONTrue ^ "true";
    }

    return JSONFalse ^ "false";
  }
}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> semver()
    {
      return {
        BuiltInDef::create(Location("semver.compare"), 2, compare),
        BuiltInDef::create(Location("semver.is_valid"), 1, is_valid)};
    }
  }
}