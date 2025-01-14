configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

set( SOURCES
bigint.cc
interpreter.cc
lang.cc
parse.cc
args.cc
unifier.cc
value.cc
value_map.cc
resolver.cc
variable.cc
wf.cc
builtins.cc
helpers.cc
rego_c.cc
encoding.cc
passes/input_data.cc
passes/modules.cc
passes/lists.cc
passes/imports.cc
passes/keywords.cc
passes/membership.cc
passes/if_else.cc
passes/rules.cc
passes/build_calls.cc
passes/build_refs.cc
passes/structure.cc
passes/strings.cc
passes/merge_data.cc
passes/lift_refheads.cc
passes/symbols.cc
passes/replace_argvals.cc
passes/lift_query.cc
passes/constants.cc
passes/enumerate.cc
passes/locals.cc
passes/compr.cc
passes/absolute_refs.cc
passes/merge_modules.cc
passes/skips.cc
passes/arithmetic.cc
passes/comparison.cc
passes/assign.cc
passes/skip_refs.cc
passes/simple_refs.cc
passes/init.cc
passes/rulebody.cc
passes/lift_to_rule.cc
passes/functions.cc
passes/unify.cc
passes/query.cc
builtins/aggregates.cc
builtins/arrays.cc
builtins/bits.cc
builtins/casts.cc
builtins/encoding.cc
builtins/numbers.cc
builtins/objects.cc
builtins/regex.cc
builtins/semver.cc
builtins/sets.cc
builtins/strings.cc
builtins/time.cc
builtins/types.cc
builtins/units.cc
builtins/utf8.cc
# third-party
builtins/base64/base64.cpp
)

add_library(rego STATIC ${SOURCES})

add_library(regocpp::rego ALIAS rego)

if (REGOCPP_SPECULATIVE_LOAD_HARDENING)
  target_compile_options(rego PRIVATE -mllvm -x86-speculative-load-hardening)
endif()

target_link_libraries(rego
  PUBLIC
    trieste::trieste
)

target_precompile_headers(rego
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rego/errors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rego/helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rego/passes.h
)


if (MSVC)
  target_compile_options(rego PUBLIC "/Zc:__cplusplus")
  target_compile_definitions(rego PUBLIC "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
endif()

target_include_directories( rego
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/rego>
)
