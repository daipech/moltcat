/**
 * @file molt_plugin.hpp
 * @brief MoltCat plugin base interfaces
 * @note Defines stable interfaces across DLL boundaries, avoiding STL ABI issues
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace moltcat {

// ============================================================
// Forward declarations
// ============================================================

template<typename T>
class ArrayList;
class IString;
class IBuffer;

// ============================================================
// Basic type definitions
// ============================================================

/**
 * @brief Plugin object handle
 *
 * Used for passing object references across DLL boundaries,
 * avoiding C++ object slicing issues.
 */
using PluginHandle = void*;
using ConstPluginHandle = const void*;

/**
 * @brief Plugin version structure
 */
struct PluginVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    const char* pre_release;   // Can be nullptr
    const char* build_metadata; // Can be nullptr

    // Convenience constructor
    constexpr PluginVersion(uint16_t maj = 0, uint16_t min = 0, uint16_t pat = 0,
                           const char* pre = nullptr, const char* build = nullptr) noexcept
        : major(maj), minor(min), patch(pat), pre_release(pre), build_metadata(build) {}

    // Version comparison
    constexpr auto operator<=>(const PluginVersion& other) const noexcept -> std::strong_ordering {
        if (auto cmp = major <=> other.major; cmp != std::strong_ordering::equal) return cmp;
        if (auto cmp = minor <=> other.minor; cmp != std::strong_ordering::equal) return cmp;
        if (auto cmp = patch <=> other.patch; cmp != std::strong_ordering::equal) return cmp;
        return std::strong_ordering::equal;
    }

    constexpr bool operator==(const PluginVersion& other) const noexcept {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

// ============================================================
// String interface (IString)
// ============================================================

/**
 * @brief Cross-DLL boundary string interface
 *
 * Plugin developers: Use this interface instead of std::string to ensure ABI compatibility.
 *
 * @note
 * - All string operations return new IString instances
 * - Caller is responsible for destroying returned strings
 * - String content is null-terminated
 *
 * @example
 * @code
 * // Create from C string
 * IString* str = IString::create("Hello");
 *
 * // Get length and content
 * size_t len = str->length();
 * const char* data = str->c_str();
 *
 * // Append
 * IString* result = str->append(" World");
 *
 * // Clean up after use
 * str->destroy();
 * result->destroy();
 * @endcode
 */
class IString {
public:
    /**
     * @brief Create string instance
     * @param str C string (can be nullptr for empty string)
     * @return New string instance, caller must destroy
     */
    static auto create(const char* str = nullptr) -> IString*;

    /**
     * @brief Create string with specified length
     * @param data String data
     * @param length Length (excluding null terminator)
     * @return New string instance
     */
    static auto create(const char* data, size_t length) -> IString*;

    /**
     * @brief Create formatted string
     * @param format printf-style format string
     * @param ... Variable arguments
     * @return New string instance
     */
    static auto format(const char* format, ...) -> IString*;

    /**
     * @brief Destroy string instance
     */
    virtual void destroy() noexcept = 0;

    /**
     * @brief Get string length
     * @return String length (excluding null terminator)
     */
    virtual auto length() const noexcept -> size_t = 0;

    /**
     * @brief Get C string pointer
     * @return Null-terminated string pointer, valid until string is destroyed
     */
    virtual auto c_str() const noexcept -> const char* = 0;

    /**
     * @brief Check if string is empty
     * @return Returns true if string is empty
     */
    virtual auto empty() const noexcept -> bool = 0;

    /**
     * @brief Append string
     * @param other String to append
     * @return New string instance, caller must destroy
     */
    virtual auto append(const char* other) -> IString* = 0;
    virtual auto append(const IString* other) -> IString* = 0;

    /**
     * @brief Get substring
     * @param pos Start position
     * @param count Length (npos means to end)
     * @return New string instance
     */
    static constexpr size_t npos = static_cast<size_t>(-1);
    virtual auto substring(size_t pos, size_t count = npos) const -> IString* = 0;

    /**
     * @brief Find substring
     * @param str String to find
     * @param pos Start position
     * @return Position found, returns npos if not found
     */
    virtual auto find(const char* str, size_t pos = 0) const noexcept -> size_t = 0;

    /**
     * @brief Compare strings
     * @param other String to compare
     * @return <0: less, 0: equal, >0: greater
     */
    virtual auto compare(const char* other) const noexcept -> int = 0;

    /**
     * @brief Clone string
     * @return New string instance with same content
     */
    virtual auto clone() const -> IString* = 0;

protected:
    virtual ~IString() = default;
};

// ============================================================
// String RAII wrapper class
// ============================================================

/**
 * @brief RAII wrapper for IString
 *
 * Automatically manages IString lifecycle, suitable for internal plugin use.
 *
 * @example
 * @code
 * String str = String::create("Hello");
 * String result = str->append(" World");
 * // Automatically destroyed
 * @endcode
 */
class String {
public:
    // Default constructor: empty string
    String() noexcept : str_(IString::create()) {}

    // Construct from C string
    explicit String(const char* s) : str_(IString::create(s)) {}

    // Construct from IString pointer (takes ownership)
    explicit String(IString* s) noexcept : str_(s) {}

    // Copy constructor
    String(const String& other) : str_(other.str_ ? other.str_->clone() : nullptr) {}

    // Move constructor
    String(String&& other) noexcept : str_(other.str_) {
        other.str_ = nullptr;
    }

    // Destructor
    ~String() {
        if (str_) {
            str_->destroy();
            str_ = nullptr;
        }
    }

    // Assignment
    auto operator=(const String& other) -> String& {
        if (this != &other) {
            if (str_) str_->destroy();
            str_ = other.str_ ? other.str_->clone() : nullptr;
        }
        return *this;
    }

    auto operator=(String&& other) noexcept -> String& {
        if (this != &other) {
            if (str_) str_->destroy();
            str_ = other.str_;
            other.str_ = nullptr;
        }
        return *this;
    }

    // Assign from C string
    auto operator=(const char* s) -> String& {
        if (str_) str_->destroy();
        str_ = IString::create(s);
        return *this;
    }

    // Access underlying pointer
    auto get() const noexcept -> IString* { return str_; }
    auto operator->() const noexcept -> IString* { return str_; }
    explicit operator bool() const noexcept { return str_ != nullptr; }

    // Get C string
    auto c_str() const noexcept -> const char* {
        return str_ ? str_->c_str() : "";
    }

    // Get length
    auto length() const noexcept -> size_t {
        return str_ ? str_->length() : 0;
    }

    // Check if empty
    auto empty() const noexcept -> bool {
        return !str_ || str_->empty();
    }

    // Static factory methods
    static auto create(const char* s = nullptr) -> String {
        return String(IString::create(s));
    }

    static auto format(const char* fmt, ...) -> String {
        // TODO: Implement variadic arguments
        return String(IString::create(fmt));
    }

private:
    IString* str_;
};

// ============================================================
// List interface (IList)
// ============================================================

/**
 * @brief Generic list interface base class
 *
 * Plugin developers: Use this interface instead of std::vector to ensure ABI compatibility.
 *
 * @tparam T Element type (must be POD or pointer type)
 *
 * @example
 * @code
 * // Create string list
 * ArrayList<IString*>* list = ArrayList<IString>::create();
 *
 * // Add elements
 * list->add(IString::create("Hello"));
 *
 * // Get element
 * IString* item = list->get(0);
 *
 * // Cleanup
 * for (size_t i = 0; i < list->size(); ++i) {
 *     list->get(i)->destroy();
 * }
 * list->destroy();
 * @endcode
 */
template<typename T>
class IList {
public:
    /**
     * @brief Create empty list
     * @return New list instance, caller must destroy
     */
    static auto create() -> IList<T>*;

    /**
     * @brief Create list with specified capacity
     * @param capacity Initial capacity
     * @return New list instance
     */
    static auto create_with_capacity(size_t capacity) -> IList<T>*;

    /**
     * @brief Destroy list instance
     * @note Does not destroy elements in the list, manual cleanup required
     */
    virtual void destroy() noexcept = 0;

    /**
     * @brief Get element count
     */
    virtual auto size() const noexcept -> size_t = 0;

    /**
     * @brief Check if list is empty
     */
    virtual auto empty() const noexcept -> bool = 0;

    /**
     * @brief Get element at specified position
     * @param index Index (must be within valid range)
     * @return Element pointer
     */
    virtual auto get(size_t index) const -> T = 0;

    /**
     * @brief Add element to end of list
     * @param item Element to add
     * @return Returns true on success
     */
    virtual auto add(T item) -> bool = 0;

    /**
     * @brief Set element at specified position
     * @param index Index
     * @param item New element
     * @return Returns true on success
     */
    virtual auto set(size_t index, T item) -> bool = 0;

    /**
     * @brief Remove element at specified position
     * @param index Index
     * @return Removed element
     */
    virtual auto remove(size_t index) -> T = 0;

    /**
     * @brief Clear list
     * @note Does not destroy elements, manual cleanup required
     */
    virtual void clear() noexcept = 0;

    /**
     * @brief Get list capacity
     */
    virtual auto capacity() const noexcept -> size_t = 0;

    /**
     * @brief Reserve capacity
     * @param capacity Minimum capacity
     */
    virtual void reserve(size_t capacity) = 0;

    /**
     * @brief Clone list (shallow copy)
     * @return New list instance with same element pointers
     */
    virtual auto clone() const -> IList<T>* = 0;

protected:
    virtual ~IList() = default;
};

/**
 * @brief String list type alias
 */
using StringList = IList<IString*>;

/**
 * @brief Byte list type alias
 */
using ByteList = IList<uint8_t>;

// ============================================================
// List RAII wrapper class
// ============================================================

/**
 * @brief RAII wrapper for IList
 *
 * @tparam T Element type
 *
 * @example
 * @code
 * ArrayList<IString*> list;
 * list.add(IString::create("Hello"));
 * list.add(IString::create("World"));
 * // Automatically destroys list (note: elements require manual cleanup)
 * @endcode
 */
template<typename T>
class ArrayList {
public:
    ArrayList() noexcept : list_(IList<T>::create()) {}

    explicit ArrayList(IList<T>* l) noexcept : list_(l) {}

    explicit ArrayList(size_t capacity) : list_(IList<T>::create_with_capacity(capacity)) {}

    ~ArrayList() {
        if (list_) {
            list_->destroy();
            list_ = nullptr;
        }
    }

    // Disable copy
    ArrayList(const ArrayList&) = delete;
    auto operator=(const ArrayList&) = delete;

    // Move
    ArrayList(ArrayList&& other) noexcept : list_(other.list_) {
        other.list_ = nullptr;
    }

    auto operator=(ArrayList&& other) noexcept -> ArrayList& {
        if (this != &other) {
            if (list_) list_->destroy();
            list_ = other.list_;
            other.list_ = nullptr;
        }
        return *this;
    }

    // Access
    auto get() const noexcept -> IList<T>* { return list_; }
    auto operator->() const noexcept -> IList<T>* { return list_; }
    explicit operator bool() const noexcept { return list_ != nullptr; }

    // Convenience methods
    auto size() const noexcept -> size_t { return list_ ? list_->size() : 0; }
    auto empty() const noexcept -> bool { return !list_ || list_->empty(); }
    auto get(size_t index) const -> T { return list_->get(index); }
    auto add(T item) -> bool { return list_ ? list_->add(item) : false; }
    auto set(size_t index, T item) -> bool { return list_ ? list_->set(index, item) : false; }
    auto remove(size_t index) -> T { return list_ ? list_->remove(index) : T{}; }
    void clear() { if (list_) list_->clear(); }
    auto capacity() const noexcept -> size_t { return list_ ? list_->capacity() : 0; }
    void reserve(size_t capacity) { if (list_) list_->reserve(capacity); }

private:
    IList<T>* list_;
};

// ============================================================
// Buffer interface (IBuffer)
// ============================================================

/**
 * @brief Byte buffer interface
 *
 * Used for passing binary data across DLL boundaries.
 */
class IBuffer {
public:
    /**
     * @brief Create empty buffer
     */
    static auto create() -> IBuffer*;

    /**
     * @brief Create buffer with specified size
     * @param size Buffer size
     */
    static auto create(size_t size) -> IBuffer*;

    /**
     * @brief Create buffer from data
     * @param data Data pointer
     * @param size Data size
     */
    static auto create(const void* data, size_t size) -> IBuffer*;

    /**
     * @brief Destroy buffer
     */
    virtual void destroy() noexcept = 0;

    /**
     * @brief Get data pointer
     */
    virtual auto data() noexcept -> uint8_t* = 0;
    virtual auto data() const noexcept -> const uint8_t* = 0;

    /**
     * @brief Get buffer size
     */
    virtual auto size() const noexcept -> size_t = 0;

    /**
     * @brief Resize buffer
     */
    virtual void resize(size_t new_size) = 0;

    /**
     * @brief Check if empty
     */
    virtual auto empty() const noexcept -> bool = 0;

    /**
     * @brief Append data
     */
    virtual auto append(const void* data, size_t size) -> bool = 0;

    /**
     * @brief Clear buffer
     */
    virtual void clear() noexcept = 0;

    /**
     * @brief Clone buffer
     */
    virtual auto clone() const -> IBuffer* = 0;

protected:
    virtual ~IBuffer() = default;
};

// ============================================================
// Buffer RAII wrapper class
// ============================================================

/**
 * @brief RAII wrapper for IBuffer
 */
class Buffer {
public:
    Buffer() noexcept : buf_(IBuffer::create()) {}

    explicit Buffer(size_t size) : buf_(IBuffer::create(size)) {}

    Buffer(const void* data, size_t size) : buf_(IBuffer::create(data, size)) {}

    explicit Buffer(IBuffer* b) noexcept : buf_(b) {}

    ~Buffer() {
        if (buf_) {
            buf_->destroy();
            buf_ = nullptr;
        }
    }

    Buffer(const Buffer& other) : buf_(other.buf_ ? other.buf_->clone() : nullptr) {}

    Buffer(Buffer&& other) noexcept : buf_(other.buf_) {
        other.buf_ = nullptr;
    }

    auto operator=(const Buffer& other) -> Buffer& {
        if (this != &other) {
            if (buf_) buf_->destroy();
            buf_ = other.buf_ ? other.buf_->clone() : nullptr;
        }
        return *this;
    }

    auto operator=(Buffer&& other) noexcept -> Buffer& {
        if (this != &other) {
            if (buf_) buf_->destroy();
            buf_ = other.buf_;
            other.buf_ = nullptr;
        }
        return *this;
    }

    auto get() const noexcept -> IBuffer* { return buf_; }
    auto operator->() const noexcept -> IBuffer* { return buf_; }
    explicit operator bool() const noexcept { return buf_ != nullptr; }

    auto data() const noexcept -> const uint8_t* { return buf_ ? buf_->data() : nullptr; }
    auto data() noexcept -> uint8_t* { return buf_ ? buf_->data() : nullptr; }
    auto size() const noexcept -> size_t { return buf_ ? buf_->size() : 0; }
    auto empty() const noexcept -> bool { return !buf_ || buf_->empty(); }

private:
    IBuffer* buf_;
};

// ============================================================
// Helper functions
// ============================================================

/**
 * @brief Convert string literal to String
 */
inline auto operator"" _s(const char* str, size_t) -> String {
    return String::create(str);
}

/**
 * @brief Compare two IStrings
 */
inline auto operator==(const IString* lhs, const IString* rhs) noexcept -> bool {
    if (lhs == rhs) return true;
    if (!lhs || !rhs) return false;
    return lhs->compare(rhs->c_str()) == 0;
}

inline auto operator!=(const IString* lhs, const IString* rhs) noexcept -> bool {
    return !(lhs == rhs);
}

inline auto operator==(const IString* lhs, const char* rhs) noexcept -> bool {
    return lhs && lhs->compare(rhs) == 0;
}

inline auto operator==(const char* lhs, const IString* rhs) noexcept -> bool {
    return rhs && rhs->compare(lhs) == 0;
}

} // namespace moltcat
