#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "search_server.h"

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& items);

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T>& items);

template <typename T, typename U>
std::ostream& operator<<(std::ostream& out, const std::map<T, U>& items);

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
                unsigned line, const std::string& hint);

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint);

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


void TestAddDocumentContent();
void TestCountAllDocuments();
void TestExcludeStopWordsFromAddedDocumentContent();
void TestExcludingDocumentWithNegativeWordFromQueryResult();
void TestMatchDocument();
void TestRelevanceCalculation();
void TestRelevanceSorting();
void TestRatingDocuments();
void TestStatusFilter();
void TestPredicateFilter();

template <typename T>
void RunTestImpl(T func, const std::string& func_name);

#define RUN_TEST(func) RunTestImpl((func), #func)

void TestSearchServer();
