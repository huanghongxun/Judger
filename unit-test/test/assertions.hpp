#pragma once

#include <nlohmann/json.hpp>
#include <type_traits>
#include "gtest/gtest.h"

template <typename T>
inline std::string DUMP_STRING(const T &obj) {
    return std::to_string(obj);
}

template <>
inline std::string DUMP_STRING(const nlohmann::json &obj) {
    return obj.dump(2);
}

inline const std::string &DUMP_STRING(const std::string &obj) {
    return obj;
}

template <std::size_t N>
inline std::string DUMP_STRING(const char (&obj)[N]) {
    return JudgeSystem::string(obj);
}

template <bool equal, typename Ltype, typename Rtype>
std::string formatOutput(const char *lhs_expression,
                         const char *rhs_expression, const Ltype &lhs,
                         const Rtype &rhs) {
    JudgeSystem::string lhs_value = DUMP_STRING(lhs);
    JudgeSystem::string rhs_value = DUMP_STRING(rhs);
    std::stringstream ss;
    ss << std::endl
       << "      Expected: " << std::endl
       << lhs_expression << std::endl;
    if (lhs_expression != lhs_value) {
        ss << "      Which is: " << std::endl
           << lhs_value << std::endl;
    }
    if (equal) {
        ss << "To be equal to: " << std::endl
           << rhs_expression << std::endl;
    } else {
        ss << "Not to be equal to: " << std::endl
           << rhs_expression << std::endl;
    }
    if (rhs_expression != rhs_value) {
        ss << "      Which is: " << std::endl
           << rhs_value << std::endl;
    }
    ss << "      Differece: " << std::endl
       << nlohmann::json::diff(lhs, rhs).dump(2);
    return ss.str();
}

inline ::testing::AssertionResult jsonEqual(const char *lhs_expression,
                                            const char *rhs_expression,
                                            const nlohmann::json &lhs,
                                            const nlohmann::json &rhs) {
    if (lhs == rhs) {
        return ::testing::AssertionSuccess();
    } else {
        return ::testing::AssertionFailure()
               << formatOutput<true>(lhs_expression, rhs_expression, lhs, rhs);
    }
}

inline ::testing::AssertionResult jsonNotEqual(const char *lhs_expression,
                                               const char *rhs_expression,
                                               const nlohmann::json &lhs,
                                               const nlohmann::json &rhs) {
    if (lhs != rhs) {
        return ::testing::AssertionSuccess();
    } else {
        return ::testing::AssertionFailure()
               << formatOutput<false>(lhs_expression, rhs_expression, lhs, rhs);
    }
}

#define EXPECT_JSON_EQ(obj1, obj2) \
    EXPECT_TRUE(jsonEqual(#obj1, #obj2, obj1, obj2))

#define EXPECT_JSON_NE(obj1, obj2) \
    EXPECT_TRUE(jsonNotEqual(#obj1, #obj2, obj1, obj2))

#define ASSERT_JSON_EQ(obj1, obj2) \
    ASSERT_TRUE(jsonEqual(#obj1, #obj2, obj1, obj2))

#define ASSERT_JSON_NE(obj1, obj2) \
    ASSERT_TRUE(jsonNotEqual(#obj1, #obj2, obj1, obj2))
