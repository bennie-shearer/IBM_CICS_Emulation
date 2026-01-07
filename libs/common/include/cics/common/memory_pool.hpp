// =============================================================================
// CICS Emulation - Memory Pool Utilities
// Version: 3.4.6
// =============================================================================

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <queue>
#include <type_traits>
#include <cassert>

namespace cics::memory {

/**
 * @brief Simple object pool for efficient object reuse
 * 
 * Reduces allocation overhead by reusing objects rather than
 * creating and destroying them repeatedly.
 * 
 * @tparam T The type of objects to pool
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief Construct a pool with optional initial capacity and max size
     * @param initial_size Number of objects to pre-allocate
     * @param max_size Maximum pool size (0 = unlimited)
     */
    explicit ObjectPool(size_t initial_size = 0, size_t max_size = 0)
        : max_size_(max_size) {
        for (size_t i = 0; i < initial_size; ++i) {
            pool_.push(create_object());
        }
    }

    /**
     * @brief Acquire an object from the pool
     * 
     * Returns a pooled object if available, otherwise creates a new one.
     * The returned shared_ptr returns the object to the pool when destroyed.
     */
    std::shared_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        T* obj = nullptr;
        if (!pool_.empty()) {
            obj = pool_.front();
            pool_.pop();
            ++active_count_;
        } else {
            obj = create_object();
            ++total_created_;
            ++active_count_;
        }

        // Return object to pool when shared_ptr is destroyed
        return std::shared_ptr<T>(obj, [this](T* ptr) {
            this->release(ptr);
        });
    }

    /**
     * @brief Get the number of available objects in the pool
     */
    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    /**
     * @brief Get the number of objects currently in use
     */
    size_t active() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_count_;
    }

    /**
     * @brief Get total objects created by this pool
     */
    size_t total_created() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_created_;
    }

    /**
     * @brief Clear the pool, destroying all pooled objects
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            delete pool_.front();
            pool_.pop();
        }
    }

    ~ObjectPool() {
        clear();
    }

    // Non-copyable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

private:
    T* create_object() {
        if constexpr (std::is_default_constructible_v<T>) {
            return new T();
        } else {
            static_assert(std::is_default_constructible_v<T>,
                         "ObjectPool requires default constructible type");
            return nullptr;
        }
    }

    void release(T* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        --active_count_;
        
        // Return to pool if under max size
        if (max_size_ == 0 || pool_.size() < max_size_) {
            // Reset object if possible
            if constexpr (requires(T& t) { t.reset(); }) {
                obj->reset();
            }
            pool_.push(obj);
        } else {
            delete obj;
        }
    }

    mutable std::mutex mutex_;
    std::queue<T*> pool_;
    size_t max_size_;
    size_t active_count_ = 0;
    size_t total_created_ = 0;
};

/**
 * @brief Fixed-size block allocator for uniform allocations
 * 
 * Efficient for allocating many objects of the same size.
 */
class BlockAllocator {
public:
    /**
     * @brief Create a block allocator
     * @param block_size Size of each block in bytes
     * @param blocks_per_chunk Number of blocks to allocate at once
     */
    explicit BlockAllocator(size_t block_size, size_t blocks_per_chunk = 64)
        : block_size_(std::max(block_size, sizeof(void*)))
        , blocks_per_chunk_(blocks_per_chunk) {}

    ~BlockAllocator() {
        for (auto chunk : chunks_) {
            std::free(chunk);
        }
    }

    /**
     * @brief Allocate a block
     */
    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!free_list_) {
            allocate_chunk();
        }

        void* block = free_list_;
        free_list_ = *static_cast<void**>(free_list_);
        ++allocated_count_;
        return block;
    }

    /**
     * @brief Deallocate a block
     */
    void deallocate(void* block) {
        if (!block) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        *static_cast<void**>(block) = free_list_;
        free_list_ = block;
        --allocated_count_;
    }

    /**
     * @brief Get block size
     */
    size_t block_size() const { return block_size_; }

    /**
     * @brief Get number of allocated blocks
     */
    size_t allocated_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocated_count_;
    }

    // Non-copyable
    BlockAllocator(const BlockAllocator&) = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;

private:
    void allocate_chunk() {
        size_t chunk_size = block_size_ * blocks_per_chunk_;
        void* chunk = std::malloc(chunk_size);
        if (!chunk) {
            throw std::bad_alloc();
        }
        chunks_.push_back(chunk);

        // Build free list
        char* ptr = static_cast<char*>(chunk);
        for (size_t i = 0; i < blocks_per_chunk_ - 1; ++i) {
            *reinterpret_cast<void**>(ptr) = ptr + block_size_;
            ptr += block_size_;
        }
        *reinterpret_cast<void**>(ptr) = free_list_;
        free_list_ = chunk;
    }

    size_t block_size_;
    size_t blocks_per_chunk_;
    void* free_list_ = nullptr;
    std::vector<void*> chunks_;
    size_t allocated_count_ = 0;
    mutable std::mutex mutex_;
};

/**
 * @brief Typed wrapper around BlockAllocator
 */
template<typename T>
class TypedBlockAllocator {
public:
    explicit TypedBlockAllocator(size_t blocks_per_chunk = 64)
        : allocator_(sizeof(T), blocks_per_chunk) {}

    /**
     * @brief Allocate and construct an object
     */
    template<typename... Args>
    T* allocate(Args&&... args) {
        void* ptr = allocator_.allocate();
        return new (ptr) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Destroy and deallocate an object
     */
    void deallocate(T* obj) {
        if (obj) {
            obj->~T();
            allocator_.deallocate(obj);
        }
    }

    size_t allocated_count() const {
        return allocator_.allocated_count();
    }

private:
    BlockAllocator allocator_;
};

/**
 * @brief RAII wrapper for pooled objects
 */
template<typename T>
class PooledPtr {
public:
    PooledPtr() = default;
    
    explicit PooledPtr(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_.get(); }
    T* get() const { return ptr_.get(); }
    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { ptr_.reset(); }

private:
    std::shared_ptr<T> ptr_;
};

/**
 * @brief Helper to create pooled objects
 */
template<typename T>
class PoolFactory {
public:
    static PooledPtr<T> create() {
        return PooledPtr<T>(pool().acquire());
    }

    static ObjectPool<T>& pool() {
        static ObjectPool<T> instance;
        return instance;
    }
};

} // namespace cics::memory
