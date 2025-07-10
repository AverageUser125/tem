#pragma once
#include <type_traits>

#define DEFINE_BITFLAGS(Name)                                                                                   \
	Value value;                                                                                                       \
                                                                                                                       \
  public:                                                                                                              \
	using U = std::underlying_type_t<Value>;                                                                           \
                                                                                                                       \
	inline Name() : value() {                                                                                       \
	}                                                                                                                  \
	inline Name(Value aFlag) : value(aFlag) {                                                                       \
	}                                                                                                                  \
	inline operator Value() const {                                                                                 \
		return value;                                                                                                  \
	}                                                                                                                  \
	inline Name& operator=(Value flag) {                                                                            \
		value = flag;                                                                                                  \
		return *this;                                                                                                  \
	}                                                                                                                  \
	inline Name& operator=(int flag) {                                                                              \
		value = static_cast<Value>(flag);                                                                              \
		return *this;                                                                                                  \
	}                                                                                                                  \
	inline Name& operator=(Name flag) {                                                                             \
		value = flag.value;                                                                                            \
		return *this;                                                                                                  \
	}                                                                                                                  \
	explicit operator bool() const = delete;                                                                           \
	inline bool operator==(Name a) const {                                                                          \
		return value == a.value;                                                                                       \
	}                                                                                                                  \
	inline bool operator!=(Name a) const {                                                                          \
		return value != a.value;                                                                                       \
	}                                                                                                                  \
	inline Name operator|(Name other) const {                                                                       \
		return Name(static_cast<Value>(static_cast<U>(value) | static_cast<U>(other.value)));                          \
	}                                                                                                                  \
	inline Name operator&(Name other) const {                                                                       \
		return Name(static_cast<Value>(static_cast<U>(value) & static_cast<U>(other.value)));                          \
	}                                                                                                                  \
	inline Name& operator|=(Name other) {                                                                           \
		value = static_cast<Value>(static_cast<U>(value) | static_cast<U>(other.value));                               \
		return *this;                                                                                                  \
	}                                                                                                                  \
	inline bool has(Name flag) const {                                                                              \
		return (static_cast<U>(value) & static_cast<U>(flag.value)) != 0;                                              \
	}                                                                                                                  \
	inline void set(Name flag) {                                                                                    \
		value = static_cast<Value>(static_cast<U>(value) | static_cast<U>(flag.value));                                \
	}                                                                                                                  \
	inline void clear(Name flag) {                                                                                  \
		value = static_cast<Value>(static_cast<U>(value) & ~static_cast<U>(flag.value));                               \
	}                                                                                                                  \
	inline void reset() {                                                                                           \
		value = {};                                                                                                \
	}
