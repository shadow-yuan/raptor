/*
    Copyright (c) 2011 The LevelDB Authors. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Google Inc. nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __RAPTOR_UTIL_TESTUTIL_H__
#define __RAPTOR_UTIL_TESTUTIL_H__

#include <stdio.h>
#include <stdlib.h>

#include <sstream>

namespace raptor {
namespace test {

// Run some of the tests registered by the TEST() macro.
//
// Returns 0 if all tests pass.
// Dies or returns a non-zero value if some test fails.
int RunAllTests();

class Tester {
public:
    Tester(const char* f, int l)
        : _ok(true), _file(f), _line(l) {
    }

    ~Tester() {
        if (!_ok) {
            fprintf(stderr, "%s:%d:%s\n", _file, _line, _ss.str().c_str());
            exit(1);
        }
    }

    Tester& Is(bool b, const char* msg) {
        if (!b) {
            _ss << "Assertion failure " << msg;
            _ok = false;
        }
        return *this;
    }

#define BINARY_OPERATION(name, op)                           \
    template<typename X, typename Y>                         \
    Tester& name(const X& x, const Y& y) {                   \
        if (!(x op y)) {                                     \
            _ss << " failed: " << x << (" " #op " ") << y;   \
            _ok = false;                                     \
        }                                                    \
        return *this;                                        \
    }

    BINARY_OPERATION(IsEq, ==)
    BINARY_OPERATION(IsNe, !=)
    BINARY_OPERATION(IsGe, >=)
    BINARY_OPERATION(IsGt, >)
    BINARY_OPERATION(IsLe, <=)
    BINARY_OPERATION(IsLt, <)

#undef BINARY_OPERATION

    template<typename V>
    Tester& operator << (const V& value) {
        if (!_ok) {
            _ss << " " << value;
        }
        return *this;
    }

private:
    bool _ok;
    const char* _file;
    int _line;
    std::stringstream _ss;
};

#define ASSERT_TRUE(c) ::raptor::test::Tester(__FILE__, __LINE__).Is((c), #c)
#define ASSERT_EQ(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsEq((a),(b))
#define ASSERT_NE(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsNe((a),(b))
#define ASSERT_GE(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsGe((a),(b))
#define ASSERT_GT(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsGt((a),(b))
#define ASSERT_LE(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsLe((a),(b))
#define ASSERT_LT(a,b) ::raptor::test::Tester(__FILE__, __LINE__).IsLt((a),(b))

#define TCONCAT(a, b) TCONCAT1(a, b)
#define TCONCAT1(a, b) a##b

#define TEST(base, name)                                                       \
class TCONCAT(_Test_, name) : public base {                                    \
 public:                                                                       \
  void _Run();                                                                 \
  static void _RunIt() {                                                       \
    TCONCAT(_Test_, name) t;                                                   \
    t._Run();                                                                  \
  }                                                                            \
};                                                                             \
bool TCONCAT(_Test_ignored_, name) =                                           \
  ::raptor::test::RegisterTest(#base, #name, &TCONCAT(_Test_, name)::_RunIt); \
void TCONCAT(_Test_, name)::_Run()

// Register the specified test.  Typically not used directly, but
// invoked via the macro expansion of TEST.
bool RegisterTest(const char* base, const char* name, void (*func)());

}  // namespace test
}  // namespace raptor

#endif  // __RAPTOR_UTIL_TESTUTIL_H__
