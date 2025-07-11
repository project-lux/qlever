// Copyright 2014 - 2024, University of Freiburg
// Chair of Algorithms and Data Structures
// Authors: Björn Buchhold <buchhold@cs.uni-freiburg.de> [2014 - 2017]
//          Johannes Kalmbach <kalmbach@cs.uni-freiburg.de>

#ifndef QLEVER_SRC_PARSER_PARSEDQUERY_H
#define QLEVER_SRC_PARSER_PARSEDQUERY_H

#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/ResultType.h"
#include "engine/sparqlExpressions/SparqlExpressionPimpl.h"
#include "index/ScanSpecification.h"
#include "parser/Alias.h"
#include "parser/ConstructClause.h"
#include "parser/DatasetClauses.h"
#include "parser/GraphPattern.h"
#include "parser/GraphPatternOperation.h"
#include "parser/PropertyPath.h"
#include "parser/SelectClause.h"
#include "parser/TripleComponent.h"
#include "parser/UpdateClause.h"
#include "parser/data/GroupKey.h"
#include "parser/data/LimitOffsetClause.h"
#include "parser/data/OrderKey.h"
#include "parser/data/SolutionModifiers.h"
#include "parser/data/SparqlFilter.h"
#include "parser/data/Types.h"
#include "util/Algorithm.h"
#include "util/Exception.h"
#include "util/Generator.h"
#include "util/HashMap.h"
#include "util/OverloadCallOperator.h"
#include "util/ParseException.h"
#include "util/StringUtils.h"

using std::string;
using std::vector;

// Data container for prefixes
class SparqlPrefix {
 public:
  SparqlPrefix(string prefix, string uri)
      : _prefix(std::move(prefix)), _uri(std::move(uri)) {}

  string _prefix;
  string _uri;

  [[nodiscard]] string asString() const;

  bool operator==(const SparqlPrefix&) const = default;
};

// Forward declaration
namespace parsedQuery {
struct GraphPatternOperation;
}

// A parsed SPARQL query. To be extended.
class ParsedQuery {
 public:
  using GraphPattern = parsedQuery::GraphPattern;

  using SelectClause = parsedQuery::SelectClause;

  using ConstructClause = parsedQuery::ConstructClause;

  using UpdateClause = parsedQuery::UpdateClause;

  using DatasetClauses = parsedQuery::DatasetClauses;

  // ASK queries have no further context in the header, so we use an empty
  // struct
  struct AskClause : public parsedQuery::ClauseBase {};

  ParsedQuery() = default;

  GraphPattern _rootGraphPattern;
  vector<SparqlFilter> _havingClauses;
  // The number of additional internal variables that were added by the
  // implementation of ORDER BY as BIND+ORDER BY.
  int64_t numInternalVariables_ = 0;
  vector<VariableOrderKey> _orderBy;
  IsInternalSort _isInternalSort = IsInternalSort::False;
  vector<Variable> _groupByVariables;
  LimitOffsetClause _limitOffset{};
  string _originalString;
  std::optional<parsedQuery::Values> postQueryValuesClause_ = std::nullopt;

  // Contains warnings about queries that are valid according to the SPARQL
  // standard, but are probably semantically wrong.
  std::vector<std::string> warnings_;

  using HeaderClause =
      std::variant<SelectClause, ConstructClause, UpdateClause, AskClause>;
  // Use explicit default initialization for `SelectClause` because its
  // constructor is private.
  HeaderClause _clause{SelectClause{}};

  // The IRIs from the FROM and FROM NAMED clauses.
  DatasetClauses datasetClauses_;

  [[nodiscard]] bool hasSelectClause() const {
    return std::holds_alternative<SelectClause>(_clause);
  }

  [[nodiscard]] bool hasConstructClause() const {
    return std::holds_alternative<ConstructClause>(_clause);
  }

  [[nodiscard]] bool hasUpdateClause() const {
    return std::holds_alternative<UpdateClause>(_clause);
  }

  bool hasAskClause() const {
    return std::holds_alternative<AskClause>(_clause);
  }

  [[nodiscard]] decltype(auto) selectClause() const {
    return std::get<SelectClause>(_clause);
  }

  [[nodiscard]] decltype(auto) constructClause() const {
    return std::get<ConstructClause>(_clause);
  }

  [[nodiscard]] decltype(auto) updateClause() const {
    return std::get<UpdateClause>(_clause);
  }

  [[nodiscard]] decltype(auto) selectClause() {
    return std::get<SelectClause>(_clause);
  }

  [[nodiscard]] decltype(auto) constructClause() {
    return std::get<ConstructClause>(_clause);
  }

  [[nodiscard]] decltype(auto) updateClause() {
    return std::get<UpdateClause>(_clause);
  }

  // Add a variable, that was found in the query body.
  void registerVariableVisibleInQueryBody(const Variable& variable);

  // Add variables, that were found in the query body.
  void registerVariablesVisibleInQueryBody(const vector<Variable>& variables);

  // Return all the warnings that have been added via `addWarning()` or
  // `addWarningOrThrow`.
  const std::vector<std::string>& warnings() const { return warnings_; }

  // Add a warning to the query. The warning becomes part of the return value of
  // the `warnings()` function above.
  void addWarning(std::string warning) {
    warnings_.push_back(std::move(warning));
  }

  // If unbound variables that are used in a query are supposed to throw because
  // the corresponding `RuntimeParameter` is set, then throw. Else add a
  // warning.
  void addWarningOrThrow(std::string warning);

  // Returns all variables that are visible in the Query Body.
  const std::vector<Variable>& getVisibleVariables() const;

  auto& children() { return _rootGraphPattern._graphPatterns; }
  [[nodiscard]] const auto& children() const {
    return _rootGraphPattern._graphPatterns;
  }

 private:
  // Add a BIND clause to the body of the query. The second argument determines
  // whether the `targetVariable` will be part of the visible variables that are
  // selected by `SELECT *`. For an example usage see `addInternalBind` and
  // `addSolutionModifiers`.
  void addBind(sparqlExpression::SparqlExpressionPimpl expression,
               Variable targetVariable, bool targetIsVisible);

  // Generates an internal BIND that binds the given expression. The BIND is
  // added to the query as a child. The variable that the expression is bound to
  // is returned.
  Variable addInternalBind(sparqlExpression::SparqlExpressionPimpl expression);

  // Add an internal AS clause to the SELECT clause that computes the given
  // expression. This is needed by the `addSolutionModifiers` function to
  // implement aggregating expressions in the ORDER BY and HAVING clauses of
  // queries with a GROUP BY
  Variable addInternalAlias(sparqlExpression::SparqlExpressionPimpl expression);

  // If the `variable` is neither visible in the query body nor contained in the
  // `additionalVisibleVariables`, add a warning or throw an exception (see
  // `addWarningOrThrow`) that uses the `locationDescription` inside the
  // message.
  void checkVariableIsVisible(
      const Variable& variable, const std::string& locationDescription,
      const ad_utility::HashSet<Variable>& additionalVisibleVariables = {},
      std::string_view otherPossibleLocationDescription = "");

  // Similar to `checkVariableIsVisible` above, but performs the check for each
  // of the variables that are used inside the `expression`.
  void checkUsedVariablesAreVisible(
      const sparqlExpression::SparqlExpressionPimpl& expression,
      const std::string& locationDescription,
      const ad_utility::HashSet<Variable>& additionalVisibleVariables = {},
      std::string_view otherPossibleLocationDescription = "");

  // Add the `groupKeys` (either variables or expressions) to the query and
  // check whether all the variables are visible inside the query body.
  void addGroupByClause(std::vector<GroupKey> groupKeys);

  // Add the `havingClause` to the query. The argument `isGroupBy` denotes
  // whether the query performs a GROUP BY. If it is set to false, then an
  // exception is thrown (HAVING without GROUP BY is not allowed). The function
  // also throws if one of the variables that is used in the `havingClause` is
  // neither grouped nor aggregate by the expression it is contained in.
  void addHavingClause(std::vector<SparqlFilter> havingClause, bool isGroupBy);

  // Add the `orderClause` to the query. Throw an exception if the `orderClause`
  // is not valid. This might happen if it uses variables that are not visible
  // or (in case of a GROUP BY) not grouped or aggregated.
  void addOrderByClause(OrderClause orderClause, bool isGroupBy,
                        std::string_view noteForImplicitGroupBy);

 public:
  // Return the next internal variable. Used e.g. by `addInternalBind` and
  // `addInternalAlias`
  Variable getNewInternalVariable();

  // Turn a blank node `_:someBlankNode` into an internal variable
  // `?<prefixForInternalVariables>_someBlankNode`. This is required by the
  // SPARQL parser, because blank nodes in the bodies of SPARQL queries behave
  // like variables.
  static Variable blankNodeToInternalVariable(std::string_view blankNode);

  // Add the `modifiers` (like GROUP BY, HAVING, ORDER BY) to the query. Throw
  // an `InvalidQueryException` if the modifiers are invalid. This might happen
  // if one of the modifiers uses a variable that is either not visible in the
  // query before it is used, or if it uses a variable that is not properly
  // grouped or aggregated in the presence of a GROUP BY clause.
  void addSolutionModifiers(SolutionModifiers modifiers);

  // If this is a SELECT query, return all the selected aliases. Return an empty
  // vector for construct clauses.
  [[nodiscard]] const std::vector<Alias>& getAliases() const;
};

#endif  // QLEVER_SRC_PARSER_PARSEDQUERY_H
