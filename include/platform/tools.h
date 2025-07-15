#pragma once
#define NOMINMAX
#include <string.h>
#include <cstring>

void assertFuncProduction(const char* expression, const char* file_name, unsigned const line_number,
						  const char* comment = "---");

void assertFuncInternal(const char* expression, const char* file_name, unsigned const line_number,
						const char* comment = "---");

#if PRODUCTION_BUILD == 0

#define permaAssert(expression)                                                                                        \
	(void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, (unsigned)(__LINE__)), 0))

#define permaAssertComment(expression, comment)                                                                        \
	(void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, (unsigned)(__LINE__), comment), 1))

#else

#define permaAssert(expression)                                                                                        \
	(void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, (unsigned)(__LINE__)), 0))

#define permaAssertComment(expression, comment)                                                                        \
	(void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, (unsigned)(__LINE__), comment), 1))

#endif

#include <functional>

//raii stuff, it will basically call the function that you pass to it be called at scope end, usage: defer(func());
struct DeferImpl {
  public:
	explicit DeferImpl(std::function<void()> func) : func_(std::move(func)) {
	}

	~DeferImpl() {
		func_();
	}

	std::function<void()> func_;
};

#define CONCATENATE_DEFER(x, y) x##y
#define MAKE_UNIQUE_VAR_DEFER(x, y) CONCATENATE_DEFER(x, y)
#define MAKE_FUNCTION_DEFER(func) [&]() { func; }
#define defer(func) DeferImpl MAKE_UNIQUE_VAR_DEFER(_defer_, __COUNTER__)(MAKE_FUNCTION_DEFER(func))


#if PRODUCTION_BUILD == 0
#define permaAssertDevelopement permaAssert
#define permaAssertCommentDevelopement permaAssertComment
#else
#define permaAssertDevelopement
#define permaAssertCommentDevelopement
#endif