//
//  mapped_structs.hpp
//  
// Contains implementations of classic data structures for use in a memory-mapped area.
//

#ifndef BDSG_MAPPED_STRUCTS_HPP_INCLUDED
#define BDSG_MAPPED_STRUCTS_HPP_INCLUDED

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <climits>
#include <iostream>
#include <functional>
#include <limits>

#include <map>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

// Find the endian conversion functions.
// See: <https://stackoverflow.com/a/58418801>
#ifdef __APPLE__
    // See <https://gist.github.com/yinyin/2027912>
    #include <libkern/OSByteOrder.h> // BINDER_IGNORE

    #define htobe16(x) OSSwapHostToBigInt16(x)
    #define be16toh(x) OSSwapBigToHostInt16(x)

    #define htobe32(x) OSSwapHostToBigInt32(x)
    #define be32toh(x) OSSwapBigToHostInt32(x)

    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
#else
    #include <endian.h> // BINDER_IGNORE
#endif

// Since the library already depends on SDSL we might as well use their fast
// bit twiddling functions.
#include <sdsl/bits.hpp>
// And we need to stock our non-mmapped backend with an int vector.
#include <sdsl/int_vector.hpp>


namespace bdsg {
    
using namespace std;

/**
 * Wrapper that stores a number in a defined endian-ness.
 */
template<typename T>
class big_endian {
public:
    big_endian();
    big_endian(const T& value);
    operator T () const;
    big_endian<T>& operator=(const T& x);
    big_endian<T>& operator+=(const T& other);
    big_endian<T>& operator-=(const T& other);
    big_endian<T>& operator++();
    big_endian<T>& operator--();
    T operator++(int);
    T operator--(int);
protected:
    char storage[sizeof(T)];
};

/**
 * Type enum value for selecting data structures that use the STL and SDSL.
 */
struct STLBackend {
};

/**
 * Template to choose the appropriate bit-packed integer vector for a backend.
 * Exposes the resulting type at ::type.
 */
template<typename Backend>
struct IntVectorFor {
};

/**
 * Template to choose the appropriate vector for a backend. Exposes the
 * resulting template at ::type.
 */
template<typename Backend>
struct VectorFor {
};

// Implementations for the data structures for STLBackend

template<>
struct VectorFor<STLBackend> {
    template<typename Item>
    using type = std::vector<Item>;
};

template<>
struct IntVectorFor<STLBackend> {
    using type = sdsl::int_vector<>;
};

/**
 * You Only Map Once: mapped memory with safe allocation by objects in the mapped memory.
 *
 * YOMO provides an interconnected system for file-backed memory mapping and
 * allocation, so that objects whose *this is in a mapped memory segment can
 * safely allocate more memory backed by the file the object exists in.
 * Allocating more memory will not unmap memory already allocated.
 *
 * Note that you need to follow some rules in the objects you use:
 *
 * 1. Always template on an allocator and pointer type, or use yomo::Allocator
 * and yomo::Pointer.
 *
 * 2. Don't use yomo::Allocator off of the stack, only from object storage.
 *
 * 3. Don't have a vtable. Don't inherit from anything with virtual methods, or
 * have virtual methods yourself. Note that this means you can't actually
 * inherit from libhandlegraph::HandleGraph!
 */
namespace yomo {

/**
 * Pointer to an object of type T, which lives at an address mapped by a
 * yomo::Allocator (or otherwise in a yomo::Manager's chain), and is itself
 * stored at an address mapped by a yomo::Allocator (or otherwise in the same
 * yomo::Manager chain).
 *
 * Can also work outside of any chain, which lets us make things of types that
 * use this as local variables, at the cost of not being able to fail fast
 * whenever we make a mistake and wander out of a chain.
 *
 * We interpret constness as applying to the value of the pointer and not the
 * pointed-to object. If you want a pointer to a const object, you need a
 * Pointer<const T>.
 */
template<typename T>
class Pointer {
public:
    /// Be constructable.
    /// Constructs as a pointer that equals nullptr.
    Pointer() = default;
    
    Pointer(T* destination);
    
    // Be a good pointer
    operator bool () const;
    // We use this template instead of T& to let us have a Pointer<void>.
    std::add_lvalue_reference<T> operator*() const;
    T* operator->() const;
    operator T* () const;
    Pointer<T>& operator=(T* addr);
    T* operator+(size_t items) const;
    
    // Expose the address of the pointed-to object not through manual operator
    // calling.
    T* get() const;
    
protected:
    /// Stores the destination position in the chain, or max size_t for null.
    big_endian<size_t> position = std::numeric_limits<size_t>::max();
};

/**
 * Global manager of mapped memory segments. Talked to by pointers in memory
 * segments to figure out where they actually point to.
 *
 * The manager manages one or more "chains", each potentially corresponding to
 * a file. The chains are made up of mapped memory "segments", and each segment
 * can be mapped at a different base address.
 *
 * When a file is initially mapped, it is mapped as a single segment.
 * Additional segments may be mapped later to fulfill allocations from
 * yomo::Allocator<T> instances stored in the chain.
 */
class Manager {

public:

    using chainid_t = intptr_t;
    static const chainid_t NO_CHAIN = 0;

    /**
     * Create a chain not backed by any file. The given prefix data will be
     * placed before the chain allocator data structures.
     */
    static chainid_t create_chain(const std::string& prefix = "");
    
    /**
     * Create a chain by mapping all of the given open file. The file must
     * begin with the given prefix, if specified, or an error will occur.
     *
     * Modifications to the chain will affect the file, and it will grow as
     * necessary.
     *
     * The Manager will not take ownership of the file descriptor.
     *
     * If the file is nonempty, data after the length of the passed prefix must
     * contain the chain allocator data structures. If it is empty, the prefix
     * and the chain allocator data structures will be written to it.
     */
    static chainid_t create_chain(int fd, const std::string& prefix = "");
    
    /**
     * Create a chain by calling the given function until it returns an empty
     * string, and concatenating all the results.
     *
     * The result must begin with the given prefix, if specified, or an error
     * will occur.
     */
    static chainid_t create_chain(const std::function<std::string(void)>& iterator, const std::string& prefix = "");
    
    /**
     * Return a chain which has the same stored data as the given chain, but
     * for which modification of the chain will not modify any backing file on
     * disk. The chain returned may be the same chain as the given chain.
     *
     * Not thread safe with concurrent modificatons to the source chain.
     */
    static chainid_t get_dissociated_chain(chainid_t chain);
    
    /**
     * Return a chain which has the same stored data as the given chain, but
     * for which modification of the chain will modify the open file with the
     * given file descriptor. The chain returned may be the same chain as the
     * given chain.
     *
     * The Manager will not take ownership of the file descriptor.
     *
     * Not thread safe with concurrent modificatons to the source chain.
     */
    static chainid_t get_associated_chain(chainid_t chain, int fd);
    
    /**
     * Destroy the given chain and unmap all of its memory, and close any
     * associated file.
     */
    static void destroy_chain(chainid_t chain);
    
    /**
     * Get the chain that contains the given address, or NO_CHAIN if the
     * address is outside all current chains.
     */
    static chainid_t get_chain(const void* address);
    
    /**
     * Get the address of the given byte from the start of the chain.
     * If a length is provided, throws if the given length of bytes from
     * position are not contiguous in memory.
     * For NO_CHAIN just uses an offset from address 0 in memory.
     */
    static void* get_address_in_chain(chainid_t chain, size_t position, size_t length = 0);
    
    /**
     * Get the position of the given byte in the chain it is in, along with the
     * identifier for that chain.
     * If a length is provided, throws if the given length of bytes from
     * position are not contiguous in memory.
     */
    static std::pair<chainid_t, size_t> get_chain_and_position(const void* address, size_t length = 0);
    
    /**
     * Find the address of the given position in the chain that the given address exists in.
     * For NO_CHAIN just uses an offset from address 0 in memory.
     */
    static void* get_address_in_same_chain(const void* here, size_t position);
    
    /**
     * Find the position of the given address in the chain that here exists in.
     * For NO_CHAIN just uses an offset from address 0 in memory.
     */
    static size_t get_position_in_same_chain(const void* here, const void* address);
    
    /**
     * Allocate the given number of bytes from the given chain.
     * For NO_CHAIN just allocates with malloc().
     */
    static void* allocate_from(chainid_t chain, size_t bytes);
    
    /**
     * Allocate the given number of bytes from the chain containing the given
     * address.
     * For NO_CHAIN just allocates with malloc().
     */
    static void* allocate_from_same_chain(void* here, size_t bytes);
    
    /**
     * Free the given allocated block in the chain to which it belongs.
     * For NO_CHAIN just frees with free().
     */
    static void deallocate(void* address);
    
    /**
     * Find the mapped address of the first thing allocated in the chain, given
     * that it was allocated with the given size.
     *
     * That first allocated thing must exist and not be deallocated.
     *
     * Must not be called for NO_CHAIN.
     */
    static void* find_first_allocation(chainid_t chain, size_t bytes);
    
    /**
     * Scan all memory regions in the given chain. Calls the iteratee with each
     * region's start address and length, in order.
     */
    static void scan_chain(chainid_t chain, const std::function<void(const void*, size_t)>& iteratee);
    
    /**
     * Dump information about free and allocated memory.
     * Not thread safe!
     */
    static void dump(chainid_t chain);  
    
protected:
    
    
    struct LinkRecord;
    
    /**
     * This occurs inside the chains and represents the header of some free or
     * allocated memory.
     */
    struct AllocatorBlock {
        /// Previous block. Only used when block is free. Null if allocated.
        Pointer<AllocatorBlock> prev;
        /// Next block. Only used when block is free. Null if allocated.
        Pointer<AllocatorBlock> next;
        /// Size fo the block in bytes, not counting this header. Used for free
        /// and allocated blocks.
        big_endian<size_t> size;
        
        /// Get the address of the first byte of memory we manage.
        void* get_user_data() const;
        /// Get the address of the block managing the data starting at the given byte.
        static AllocatorBlock* get_from_data(void* user_data);
        
        /// Split the block, keeping first_bytes bytes and giving the rest to a new
        /// subsequent block, which is wired up and returned. Assumes the block is
        /// free.
        AllocatorBlock* split(size_t first_bytes);
        
        /// Remove this block from the free list. Returns the blocks before and
        /// after it, which is has wired together. If this was the first or last
        /// block (or both), the appropriate return value will be null.
        std::pair<AllocatorBlock*, AllocatorBlock*> detach();
        
        /// Attach this block to the free list, between the given blocks, which may
        /// be null.
        void attach(AllocatorBlock* left, AllocatorBlock* right);
        
        /// Defragment and coalesce adjacent free blocks in the contiguous run this
        /// block is part of, if any. Returns the first and last blocks in the run;
        /// the last block's header will be in the free space of the first block,
        /// unless the last block is the first block.
        std::pair<AllocatorBlock*, AllocatorBlock*> coalesce();
        
    protected:
        
        /// Return true if this block comes immediately before the other one, with
        /// no space between them.
        bool immediately_before(const AllocatorBlock* other) const;
    };

    /**
     * This occurs at the start of a chain, after any prefix, and lets us find the free list.
     *
     * Since the location of the first allocation is dependent on the size of
     * the first mapping in the chain, we also keep track of the address of the
     * first allocation. It's not allowed to be deallocated or Bad Things will
     * happen.
     */
    struct AllocatorHeader {
        /// Where is the first free block of memory?
        Pointer<AllocatorBlock> first_free;
        /// Where is the last free block of memory?
        Pointer<AllocatorBlock> last_free;
        /// Where is the first object allocated?
        Pointer<void> first_allocated;
    };
    
    
    /**
     * For each chain, stores each mapping's start address by chain offset position.
     * Each entry is useful for bound queries.
     */
    static unordered_map<chainid_t, map<size_t, intptr_t>> chain_space_index;
    
    /**
     * For each memory start address, what mapping does it start?
     * Useful for bound queries.
     */
    static map<intptr_t, LinkRecord> address_space_index;
    
    /**
     * Mutex for a readers-writer lock on the indexes, for allocating.
     */
    static shared_timed_mutex mutex;
    
    /**
     * How big should a link be to start with.
     */
    static constexpr size_t BASE_SIZE = 1024;
    
    /**
     * Of that, how many bytes should we allow the prefix to possibly use?
     * The allocator has to be able to keep its header within the rest of the first link.
     */
    static constexpr size_t MAX_PREFIX_SIZE = 16;
    
    /**
     * How much should we expand each new link by?
     */
    static constexpr size_t SCALE_FACTOR = 2;
   
    /**
     * Create a chain with one link and no allocator setup.
     * Block will either be the entire size of an existing file, or the given starting size.
     * Returns the chain ID and a flag for if there was data in an open file to read.
     */
    static std::pair<chainid_t, bool> open_chain(int fd = 0, size_t start_size = BASE_SIZE);
    
    /**
     * Extend the given chain to the given new total size.
     */
    static void extend_chain_to(chainid_t chain, size_t new_total_size);
    
    /**
     * Get the address of the given byte from the start of the chain, and the
     * number of contiguous bytes after it. The number of bytes is always 1 or
     * more if the address is not past the end of the chain.
     */
    static std::pair<void*, size_t> get_address_and_length_in_chain(chainid_t chain, size_t position);
    
    /**
     * Add a link into a chain. The caller must hold a write lock on the manager data structures.
     * The number of bytes must be nonzero.
     */
    static LinkRecord& add_link(LinkRecord& head, size_t new_bytes);
    
    /**
     * Create a new chain, using the given file if set, and copy data from the
     * given existing chain.
     */
    static chainid_t copy_chain(chainid_t chain, int fd = 0);
   
    /**
     * Set up the allocator data structures in the first link, assuming they
     * aren't present. Put them at the given offset, and carve them out of the
     * given amoutn of remaining space in the link. 
     */
    static void set_up_allocator_at(chainid_t chain, size_t offset, size_t space);
    
    /**
     * Connect to the allocator data structures in the first link, assuming they are present.
     */
    static void connect_allocator_at(chainid_t chain, size_t offset);
    
    /**
     * Find the allocator header in a chain that has had its allocator connected.
     */
    static AllocatorHeader* find_allocator_header(chainid_t chain);
    
    /**
     * Run the given callback with the allocator header for the given chain.
     * The allocator will be locked; only one function will be run on a given
     * chain's allocator at a time.
     */
    static void with_allocator_header(chainid_t chain,
                                      const std::function<void(AllocatorHeader*)>& callback);

};

/**
 * Allocator that allocates via the yomo::Manager from the chain in which it
 * itself occurs.
 * Still deals in normal pointers, which are safe to use when not storing them
 * in the allocated memory. Pointers in the allocated memory should be
 * yomo::Pointer<T>.
 */
template<typename T>
class Allocator {

public:
    // TODO: If we want to template these like this, we need to make them able
    // to work from the stack and be the types that allocate and deallocate
    // use, if we really want to be standard-compliant.
    using pointer = Pointer<T>;
    using const_pointer = Pointer<const T>;
    using size_type = size_t;
    using difference_type = size_t;
    using value_type = T;
    
    
    Allocator() = default;
    Allocator(const Allocator& other) = default;
    Allocator(Allocator&& other) = default;
    Allocator& operator=(const Allocator& other) = default;
    Allocator& operator=(Allocator&& other) = default;
    
    /**
     * Copy an allocator, but change the allocated type.
     */
    template<typename U>
    Allocator(const Allocator<U>& alloc);
    
    /**
     * A template we can use to re-instantiate this allocator for a different
     * type.
     */
    template<typename U>
    struct rebind {
        using other = Allocator<U>; 
    };
    
    /**
     * Return true if the two allocators can allocate and deallocate each
     * others' memory, and false otherwise.
     */
    bool operator==(const Allocator& other) const;
    
    /**
     * Return false if the two allocators can allocate and deallocate each
     * others' memory, and true otherwise.
     */
    bool operator!=(const Allocator& other) const;
    
    /**
     * Allocate the given number of items. Ought to be near the hint.
     */
    T* allocate(size_type n, const T* hint = nullptr);
    
    /**
     * Deallocate the given number of items. Must be the same number as were allocated.
     */
    void deallocate(T* p, size_type n);
    
    /**
     * Work out the maximum size that can be allocated.
     */
    size_t max_size() const;
    
private:
    
    /**
     * Determine the chain that an allocator will allocate from.
     */
    Manager::chainid_t get_chain() const;
    

};

/**
 * Interface between normally allocated objects and chain-allocated objects.
 * Pointer to an object that is allocated at the beginning of a chain, and
 * which should allocate, if it allocates, from the chain.
 * Itself lives outside the chain. Can be null.
 *
 * T must use yomo::Pointer as its pointer type and yomo::Allocator as its
 * allocator type.
 */
template<typename T>
class UniqueMappedPointer {
public:
    UniqueMappedPointer() = default;
    UniqueMappedPointer(const UniqueMappedPointer& other) = delete;
    UniqueMappedPointer(UniqueMappedPointer&& other) = default;
    UniqueMappedPointer& operator=(const UniqueMappedPointer& other) = delete;
    UniqueMappedPointer& operator=(UniqueMappedPointer&& other) = default;
    
    operator bool () const;
    T& operator*();
    const T& operator*() const;
    T* operator->();
    const T* operator->() const;
    operator T*();
    operator const T*() const;
    
    /**
     * Get the memory address of the referenced object, or nullptr.
     */
    T* get();
    const T* get() const;

protected:

    /**
     * Make a new constructed T in memory, preceeded by the given
     * prefix. Forward other arguments to the constructor.
     */
    template <typename... Args>
    void construct_internal(const std::string& prefix, Args&&... constructor_args);

public:

    /**
     * Make a new default-constructed T in memory, with an empty preceeding
     * prefix.
     */
    void construct();

    /**
     * Make a new default-constructed T in memory, preceeded by the given
     * prefix.
     */
    void construct(const std::string& prefix);

    /**
     * Make a new constructed T in memory, preceeded by the given
     * prefix. Forward other arguments to the constructor.
     *
     * If you want to use arguments you must pass a prefix, although it may be
     * empty. Otherwise we get into trouble with templates replacing overloads.
     */
    template <typename... Args>
    void construct(const std::string& prefix, Args&&... constructor_args);
    
    /**
     * Point to the already-constructed T saved to the file at fd by a previous
     * save() call. The file must begin with the given prefix, or an error will
     * occur.
     */
    void load(int fd, const std::string& prefix);
    
    /**
     * Load into memory and point to the already-constructed T saved to the
     * given stream by a previous save() call. The stream must begin with the
     * given prefix, or an error will occur.
     */
    void load(std::istream& in, const std::string& prefix);
    
    /**
     * Load into memory and point to the already-constructed T emitted in
     * blocks by the given function. The data must begin with the given prefix,
     * or an error will occur.
     */
    void load(const std::function<std::string(void)>& iterator, const std::string& prefix);
    
    /** 
     * Load into memory and point to the already-constructed T saved to the
     * given stream by a previous save() call. The stream is expected to have
     * had the prefix read from it already, but the prefix must still be
     * provided.
     */
    void load_after_prefix(std::istream& in, const std::string& prefix);
    
    /**
     * Break any write-back association with a backing file and move the object
     * to non-file-backed memory.
     *
     * TODO: Allow private COW mappings by adding features to MIO, to avoid a copy.
     */
    void dissociate();
    
    /**
     * Move the stored item and all associated memory into memory mapped in the
     * given file. The pointer must not be null. No move constructors are
     * called.
     */
    void save(int fd);
    
    /**
     * Save the stored item to the given stream. The pointer must not be null.
     */
    void save(std::ostream& out) const;
    
    /**
     * Save the stored item as blocks of data shown to the given function. The
     * pointer must not be null.
     */
    void save(const std::function<void(const void*, size_t)>& iteratee) const;
    
    /**
     * Save the stored item to the given stream. The pointer must not be null.
     * The stream is expected to have already had the prefix written to it.
     */
    void save_after_prefix(std::ostream& out, const std::string& prefix) const;
    
    /**
     * Free any associated memory and become empty.
     */
    void reset();
private:
    Manager::chainid_t chain = Manager::NO_CHAIN;
};

};

/**
 * A vector of primitive items, with guaranteed ABI compatibility across
 * compilers and platforms.
 */
template<typename T, typename Alloc = std::allocator<T>>
class CompatVector {
public:

    CompatVector() = default;
    
    // Because we contain a pointer, we need a destructor and copy and move
    // constructors and assignment operators.
    ~CompatVector();
    
    CompatVector(const CompatVector& other);
    CompatVector(CompatVector&& other);
    
    /// Allow copying within the same allocator
    CompatVector& operator=(const CompatVector& other);
    
    /// Allow copying across allocators.
    template<typename OtherAlloc>
    CompatVector& operator=(const CompatVector<T, OtherAlloc>& other);
    
    /// Alow moving within the same allocator.
    CompatVector& operator=(CompatVector&& other);
    
    size_t size() const;
    size_t capacity() const;
    void resize(size_t new_size);
    void reserve(size_t new_reserved_length);
    void shrink_to_fit();
    
    T& back();
    const T& back() const;
    
    void pop_back();
    
    template<typename... Args>
    void emplace_back(Args&&... args);
    
    /**
     * Empty out the vector and free any allocated memory.
     */
    void clear();
    T& at(size_t index);
    const T& at(size_t index) const;
    
    T& operator[](size_t index);
    const T& operator[](size_t index) const;
    
    using value_type = T;
    
    // We have iterators but they aren't safe to serialize.
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;
    
    // Compatibility with SDSL-lite serialization
    
    /**
     * Serialize the data to the given stream.
     */
    void serialize(std::ostream& out) const;
    
    /**
     * Load the data from the given stream.
     */
    void load(std::istream& in);
    
protected:
    // We keep the allocator in ourselves, so if working in a chain we allocate
    // in the right chain.
    Alloc alloc;
    big_endian<size_t> length = 0;
    big_endian<size_t> reserved_length = 0;
    typename std::allocator_traits<Alloc>::pointer first = nullptr;
    
    static const int RESIZE_FACTOR = 2;
};

/**
 * Vector mapped in from a file.
 */
template<typename T>
using MappedVector = CompatVector<T, yomo::Allocator<T>>; 

/**
 * An int vector that is mostly API-compatible with SDSL's int vectors, but
 * which can exist in a memory mapping and uses the given allocator and its
 * pointers.
 *
 * Does *NOT* automatically adjust its width; you must call width() or repack()
 * to set the appropriate bit width for your values *before* you write them.
 * Consider a PagedVector if this seems hard, or if your values come in runs of
 * differing widths. Width defaults to 1 bit.
 */
template<typename Alloc = std::allocator<uint64_t>>
class CompatIntVector {
public:
    
    CompatIntVector() = default;
    
    /// Allow copy construction
    CompatIntVector(const CompatIntVector& other);
    /// Allow move construction
    CompatIntVector(CompatIntVector&& other);
    
    // To allow copy across backends, we need to be friends with other
    // instantiations of us.
    template<typename OtherAlloc> friend class CompatIntVector;
    
    /// Allow copy construction across allocators
    template<typename OtherAlloc>
    CompatIntVector(const CompatIntVector<OtherAlloc>& other);
    
    /// Allow copy assignment across allocators
    template<typename OtherAlloc>
    CompatIntVector& operator=(const CompatIntVector<OtherAlloc>& other);
    
    /// Allow copy construction from anything with the right methods
    template<typename OtherType>
    CompatIntVector(const OtherType& other);
    
    /// Allow copy assignment from anything with the right methods
    template<typename OtherType>
    CompatIntVector& operator=(const OtherType& other);
    
    /// Allow copy assignment
    CompatIntVector& operator=(const CompatIntVector& other);
    
    /// Allow move assignment
    CompatIntVector& operator=(CompatIntVector&& other);
    
    /**
     * Return the number of integers in the vector.
     */
    size_t size() const;
    
    /**
     * Resize the vector to hold the given number of elements.
     */
    void resize(size_t new_size);
    
    /**
     * Return the number of elements the vector can hold without reallocating.
     */
    size_t capacity() const;
    
    /**
     * Pre-allocate space to hold the given number of elements.
     */
    void reserve(size_t new_reserved_length);
    
    /**
     * Drop everything from the vector.
     */
    void clear();

    /**
     * Return the width in bits of the entries.
     */
    size_t width() const;
    /**
     * Set the width in bits of the entries, causing existing data to be
     * reinterpreted.
     *
     * Throws if any stored entry cannot be represented with the new width.
     */
    void width(size_t new_width);
    /**
     * Repack existing data, re-encoding it with the given width, and
     * simultaneously resize to the new given number of elements.
     *
     * Throws if any stored entry cannot be represented with the new width.
     */
    void repack(size_t new_width, size_t new_size);
    
    /**
     * Actual accessor method that sets the value at a position.
     * Does not check if value actually fits.
     * Uses the given width override instead of the stored width to write the value, if set.
     *
     * Throws if the value will not fit in the relevant number of bits.
     */
    void pack(size_t index, uint64_t value, size_t width_override = 0);
    
    /**
     * Actual accessor method that gets the value at a position.
     * Uses the given width override instead of the stored width to write the value, if set.
     */
    uint64_t unpack(size_t index, size_t width_override = 0) const;
    
    /**
     * Proxy that acts as a mutable reference to an entry in the vector.
     */
    class Proxy {
    protected:
        // What vector are we operating on?
        CompatIntVector& parent;
        // What location do we refer to?
        size_t index;
    public:
        // Must be movable via constructor for by-value return
        Proxy(Proxy&& other) = default;
        
        // But generally not movable or copyable
        Proxy(const Proxy& other) = delete;
        Proxy& operator=(const Proxy& other) = delete;
        Proxy& operator=(Proxy&& other) = delete;
        
        /// Make a proxy for the entry at the given position in the given int
        /// vector
        Proxy(CompatIntVector& parent, size_t index);
        
        /// Get the value the proxy refers to
        operator uint64_t () const;
        
        /// Set the value the proxy refers to
        Proxy& operator=(uint64_t new_value);
    };
    
    using reference = Proxy;
    
    /**
     * Proxy that acts as an immutable reference to an entry in the vector.
     */
    class ConstProxy {
    protected:
        // What vector are we operating on?
        const CompatIntVector& parent;
        // What location do we refer to?
        size_t index;
    public:
        // Must be movable via constructor for by-value return
        ConstProxy(ConstProxy&& other) = default;
        
        // But generally not movable or copyable
        ConstProxy(const ConstProxy& other) = delete;
        ConstProxy& operator=(const ConstProxy& other) = delete;
        ConstProxy& operator=(ConstProxy&& other) = delete;
        
        /// Make a proxy for the entry at the given position in the given int
        /// vector
        ConstProxy(const CompatIntVector& parent, size_t index);
        
        /// Get the value the proxy refers to
        operator uint64_t () const;
    };
    
    using const_reference = ConstProxy;
    
    /**
     * Get a proxy reference to read or write the given index.
     * Checks bounds, but not value width.
     */
    Proxy at(size_t index);
    /**
     * Get a proxy reference to read the given index.
     * Checks bounds.
     */
    ConstProxy at(size_t index) const;
    
    /**
     * Get a proxy reference to read or write the given index.
     */
    Proxy operator[](size_t index);
    /**
     * Get a proxy reference to read the given index.
     */
    ConstProxy operator[](size_t index) const;
    
    // Compatibility with SDSL-lite serialization
    
    /**
     * Serialize the data to the given stream.
     */
    void serialize(std::ostream& out) const;
    
    /**
     * Load the data from the given stream.
     */
    void load(std::istream& in);
    
protected:
    /// How many items are stored?
    big_endian<size_t> length = 0;
    /// How many bits are used to represent each item?
    big_endian<size_t> bit_width = 1;
    
    /// We store our actual data in this vector, which manages memory for us.
    CompatVector<uint64_t, Alloc> data;
    
};

/**
 * Int vector mapped in from a file.
 */
using MappedIntVector = CompatIntVector<yomo::Allocator<uint64_t>>; 

/**
 * Type enum value for selecting data structures that use memory-mappable data
 * structures but with the standard allocator.
 */
struct CompatBackend {
};

/**
 * Type enum value for selecting data structures that use YOMO memory mapping
 * and the YOMO allocator.
 *
 * They can safely exist outside of mapped memory, but are probably slower
 * there since YOMO's internal tables still need to be consulted when following
 * pointers.
 */
struct MappedBackend {
};

// Implementations for the data structures for MappedBackend

template<>
struct VectorFor<MappedBackend> {
    template<typename Item>
    using type = MappedVector<Item>;
};

template<>
struct IntVectorFor<MappedBackend> {
    using type = MappedIntVector;
};




// Implementations for the data structures for CompatBackend

template<>
struct VectorFor<CompatBackend> {
    template<typename Item>
    using type = CompatVector<Item>;
};

template<>
struct IntVectorFor<CompatBackend> {
    using type = CompatIntVector<>;
};

///////////////////////////////////////////////////////////////////////////////////


// Implementations

template<typename T>
big_endian<T>::big_endian() {
    for (auto& byte : storage) {
        byte = 0;
    }
#ifdef debug_big_endian
    std::cerr << "Zeroed " << sizeof(T) << " bytes for a big_endian at " << (intptr_t) this << std::endl;
#endif
    assert(*this == 0);
}

template<typename T>
big_endian<T>::big_endian(const T& value) {
    *this = value;
}

template<typename T>
big_endian<T>::operator T () const {
    
    // We assume an IEEE floating point representation and the same endian-ness
    // as the integer type of the same width.
    
    // I had no luck partially specializing the conversion operator for all
    // integral types of a given width, so we switch and call only the
    // conversion that will work.
    // TODO: manually write all the specializations?
    // Note that signed types report 1 fewer bits (i.e. 63)
    switch (CHAR_BIT * sizeof(T)) {
    case 64:
        {
            uint64_t scratch = be64toh(*(reinterpret_cast<const uint64_t*>(storage)));
            return *reinterpret_cast<T*>(&scratch);
        }
        break;
    case 32:
        {
            uint32_t scratch = be32toh(*(reinterpret_cast<const uint32_t*>(storage)));
            return *reinterpret_cast<T*>(&scratch);
        }
        break;
    case 16:
        {
            uint16_t scratch = be16toh(*(reinterpret_cast<const uint16_t*>(storage)));
            return *reinterpret_cast<T*>(&scratch);
        }
        break;
    default:
        throw runtime_error("Unimplemented bit width: " + std::to_string(CHAR_BIT * sizeof(T)));
    }
}

template<typename T>
big_endian<T>& big_endian<T>::operator=(const T& x) {

    // We assume an IEEE floating point representation and the same endian-ness
    // as the integer type of the same width.
    
    // We make sure to reinterpret the input as an int type in case it is floating point.

    switch (CHAR_BIT * sizeof(T)) {
    case 64:
        *((uint64_t*)storage) = htobe64(reinterpret_cast<const uint64_t&>(x));
        break;
    case 32:
        *((uint32_t*)storage) = htobe32(reinterpret_cast<const uint32_t&>(x));
        break;
    case 16:
        *((uint16_t*)storage) = htobe16(reinterpret_cast<const uint16_t&>(x));
        break;
    default:
        throw runtime_error("Unimplemented bit width: " + std::to_string(CHAR_BIT * sizeof(T)));
    }
    
#ifdef debug_big_endian
    std::cerr << "Set a big_endian at " << (intptr_t) this << " to " << x << std::endl;
    std::cerr << "Contents:";
    for (auto& b : storage) {
        std::cerr << " " << (int)b;
    }
    std::cerr << std::endl;
    std::cerr << "Reads back as " << (T)*this << std::endl;
#endif
    
    return *this;
}

template<typename T>
big_endian<T>& big_endian<T>::operator+=(const T& other) {
    *this = ((T)*this) + other;
    return *this;
}

template<typename T>
big_endian<T>& big_endian<T>::operator-=(const T& other) {
    *this = ((T)*this) - other;
    return *this;
}

template<typename T>
big_endian<T>& big_endian<T>::operator++() {
    *this += 1;
    return *this;
}

template<typename T>
big_endian<T>& big_endian<T>::operator--() {
    *this -= 1;
    return *this;
}

template<typename T>
T big_endian<T>::operator++(int) {
    T old = *this;
    ++*this;
    return old;
}

template<typename T>
T big_endian<T>::operator--(int) {
    T old = *this;
    --*this;
    return old;
}

////////////////////

template<typename T, typename Alloc>
CompatVector<T, Alloc>::~CompatVector() {
    if (first) {
        // We have some memory allocated to us.
        
        for (size_t i = 0; i < length; i++) {
            // Destroy everything in it.
            (first + i)->~T();
        }
        
        // Get rid of it.
        alloc.deallocate(first, reserved_length);
    }
}

template<typename T, typename Alloc>
CompatVector<T, Alloc>::CompatVector(const CompatVector& other) {
#ifdef debug_compat_vector
    std::cerr << "Copy-constructing a vector of size " << other.size() << " from " << (intptr_t)&other << " to " << (intptr_t)this << std::endl;
#endif
    if (other.size() != 0) {
        first = alloc.allocate(other.size());
        reserved_length = other.size();
        length = other.size();
        
        for (size_t i = 0; i < other.size(); i++) {
            // Copy each item
            new (first + i) T(other.at(i));
        }
    }
#ifdef debug_compat_vector
    std::cerr << "Result is of size " << size() << std::endl;
#endif
}

template<typename T, typename Alloc>
CompatVector<T, Alloc>::CompatVector(CompatVector&& other) {

#ifdef debug_compat_vector
    std::cerr << "Move-constructing a vector of size " << other.size() << " from " << (intptr_t)&other << " to " << (intptr_t)this << std::endl;
#endif

    // We need to make sure that we and the other vector are in the same memory
    // arena (i.e. same chain or lack of chain), if applicable.
    
    if (alloc == other.alloc) {
        // We can accept whatever they have allocated.
        
        // Take everything out of other
        length = other.length;
        reserved_length = other.reserved_length;
        first = other.first;
    
        // And say they have no items or memory.
        other.length = 0;
        other.reserved_length = 0;
        other.first = nullptr;
    } else {
        // Fall back on copy assignment
        CompatVector& to_copy = other;
        *this = to_copy;
    }
#ifdef debug_compat_vector
    std::cerr << "Result is of size " << size() << std::endl;
#endif
}

template<typename T, typename Alloc>
CompatVector<T, Alloc>& CompatVector<T, Alloc>::operator=(const CompatVector& other) {
#ifdef debug_compat_vector
    std::cerr << "Copy-assigning a vector of size " << other.size() << " from " << (intptr_t)&other << " to " << (intptr_t)this << std::endl;
#endif
    if ((void*)this != (void*)&other) {
        // Get rid of our memory
        clear();
        // Get some new memory
        reserve(other.size());
        
        for (size_t i = 0; i < other.size(); i++) {
            // Copy construct each thing.
            new (first + i) T(other[i]);
        }
        
        length = other.size();
    }
#ifdef debug_compat_vector
    std::cerr << "Result is of size " << size() << std::endl;
#endif
    return *this;
}

template<typename T, typename Alloc>
template<typename OtherAlloc>
CompatVector<T, Alloc>& CompatVector<T, Alloc>::operator=(const CompatVector<T, OtherAlloc>& other) {
#ifdef debug_compat_vector
    std::cerr << "Copy-assigning a vector of size " << other.size() << " from " << (intptr_t)&other << " to " << (intptr_t)this << std::endl;
#endif
    if ((void*)this != (void*)&other) {
        // Get rid of our memory
        clear();
        // Get some new memory
        reserve(other.size());
        
        for (size_t i = 0; i < other.size(); i++) {
            // Copy construct each thing.
            new (first + i) T(other[i]);
        }
        
        length = other.size();
    }
#ifdef debug_compat_vector
    std::cerr << "Result is of size " << size() << std::endl;
#endif
    return *this;
}

template<typename T, typename Alloc>
CompatVector<T, Alloc>& CompatVector<T, Alloc>::operator=(CompatVector&& other) {
#ifdef debug_compat_vector
    std::cerr << "Move-assigning a vector of size " << other.size() << " from " << (intptr_t)&other << " to " << (intptr_t)this << std::endl;
#endif
    if ((void*)this != (void*)&other) {
        if (alloc == other.alloc) {
            // We can safely use their memory.
            
            // Get rid of our memory.
            clear();
        
            // Steal their memory
            length = other.length;
            reserved_length = other.reserved_length;
            first = other.first;
            
            // And say they have no items or memory.
            other.length = 0;
            other.reserved_length = 0;
            other.first = nullptr;
        } else {
            // Fall back on copy assignment
            CompatVector& to_copy = other;
            *this = to_copy;
        }
    }
#ifdef debug_compat_vector
    std::cerr << "Result is of size " << size() << std::endl;
#endif
    return *this;
}

template<typename T, typename Alloc>
size_t CompatVector<T, Alloc>::size() const {
    return length;
}

template<typename T, typename Alloc>
size_t CompatVector<T, Alloc>::capacity() const {
    return reserved_length;
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::reserve(size_t new_reserved_length) {
    // Find where the data is. Note that this may be null.
    T* old_first = first;
    // And how much there is
    size_t old_reserved_length = reserved_length;
    
#ifdef debug_compat_vector
    std::cerr << "Reserving vector at " << (intptr_t)this
        << " with " << old_reserved_length << " spaces at "
        << (intptr_t) old_first << " to have " << new_reserved_length  << " spaces" << std::endl;
#endif

    if (new_reserved_length > old_reserved_length) {
        // Allocate space for the new data, and get the position in the context
        T* new_first  = alloc.allocate(new_reserved_length);
        
        // Record the new reserved length
        reserved_length = new_reserved_length;
        
#ifdef debug_compat_vector
        std::cerr << "Vector data moving from " << (intptr_t) old_first
            << " to " << (intptr_t) new_first << std::endl;
#endif
        for (size_t i = 0; i < size(); i++) {
            // Move over the preserved values.
            new (new_first + i) T(std::move(*(old_first + i)));
        }
        
        for (size_t i = 0; i < size(); i++) {
            // Destroy all the original values
            (old_first + i)->~T();
        }
        
        if (old_first) {
            // Free old memory if we moved
            alloc.deallocate(old_first, old_reserved_length);
        }
        first = new_first;
    }
    
    // If it isn't growing, ignore it.
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::resize(size_t new_size) {
    // Find where the data is. Note that this may be null.
    T* old_first = first;
    // And how much there is
    size_t old_reserved_length = reserved_length;
    
#ifdef debug_compat_vector
    std::cerr << "Resizing vector at " << (intptr_t)this
        << " with " << size() << " items at "
        << (intptr_t) old_first << " to size " << new_size << std::endl;
#endif

    if (new_size == size()) {
        // Nothing to do!
        return;
    }

    if (new_size > reserved_length) {
        // We will need some more memory, and our data might move.
        // Make sure repeated calls to be bigger reallocate only a sensible number of times.
        reserve(std::max(new_size, size() * RESIZE_FACTOR));
    }
   
    // TODO: throw away excess memory when shrinking
   
    if (new_size < size()) {
        // We shrank, and we had at least one item.
#ifdef debug_compat_vector
        std::cerr << "Removing items between " << new_size << " and " << size() << std::endl;
#endif
        for (size_t i = new_size; i < size(); i++) {
            // For anything trimmed off, just run destructors.
            (first + i)->~T();
        }
    } else {
#ifdef debug_compat_vector
        std::cerr << "Adding items between " << size() << " and " << new_size << std::endl;
#endif
        for (size_t i = size(); i < new_size; i++) {
            // For anything aded on, just run constructor.
            // This will use value initialization
            // <https://stackoverflow.com/a/2418195> and zero everything if a
            // default constructor is not defined.
            new (first + i) T();
        }
    }
    
    // Record the new length
    length = new_size;
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::shrink_to_fit() {
    // TODO: actually reallocate smaller.
}

template<typename T, typename Alloc>
T& CompatVector<T, Alloc>::back() {
    return (*this)[size() - 1];
}

template<typename T, typename Alloc>
const T& CompatVector<T, Alloc>::back() const {
    return (*this)[size() - 1];
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::pop_back() {
    resize(size() - 1);
}

template<typename T, typename Alloc>
template<typename... Args>
void CompatVector<T, Alloc>::emplace_back(Args&&... args) {
    if (reserved_length <= length) {
        // Make sure we're big enough
        reserve(std::max((size_t) 1, reserved_length * RESIZE_FACTOR));
    }
    
    // Make the new item in place
    new (first + length) T(std::forward<Args>(args)...);
    // Record that we have it.
    length = length + 1;
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::clear() {
    // Deconstruct anything we have.
    resize(0);
    if (first) {
        // We have some memory allocated to us.
        // Get rid of it.
        alloc.deallocate(first, reserved_length);
        // And remember it is gone
        first = nullptr;
    }
    reserved_length = 0;
}

template<typename T, typename Alloc>
T& CompatVector<T, Alloc>::at(size_t index) {
    if (index >= size()) {
        // TODO: throw the right type here.
        throw std::runtime_error("Cannot get " + std::to_string(index) +
                                 " in vector of length " + std::to_string(size()));
    }
    
    return (*this)[index];
}

template<typename T, typename Alloc>
auto CompatVector<T, Alloc>::begin() -> iterator {
    return &(*this)[0];
}

template<typename T, typename Alloc>
auto CompatVector<T, Alloc>::begin() const -> const_iterator {
    return &(*this)[0];
}

template<typename T, typename Alloc>
auto CompatVector<T, Alloc>::end() -> iterator {
    return &(*this)[size()];
}

template<typename T, typename Alloc>
auto CompatVector<T, Alloc>::end() const -> const_iterator {
    return &(*this)[size()];
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::serialize(std::ostream& out) const {
    
    // Dump the length
    out.write((const char*)&length, sizeof(length));
    // And all the set items
    out.write((const char*)(T*)first, length * sizeof(T));
}

template<typename T, typename Alloc>
void CompatVector<T, Alloc>::load(std::istream& in) {
    // Start empty
    this->clear();

    // Determine the length
    decltype(length) new_len;
    in.read((char*)&new_len, sizeof(new_len));
    
    // Make sure we have that space
    resize(new_len);
    
    // Fill in the items
    in.read((char*)(T*)first, new_len * sizeof(T));
}


template<typename T, typename Alloc>
const T& CompatVector<T, Alloc>::at(size_t index) const {
    // Just run non-const at and constify result
    return const_cast<CompatVector<T, Alloc>*>(this)->at(index);
}


template<typename T, typename Alloc>
T& CompatVector<T, Alloc>::operator[](size_t index) {
    return *(first + index);
}
 

template<typename T, typename Alloc>
const T& CompatVector<T, Alloc>::operator[](size_t index) const {
    // Just run non-const at and constify result
    return (*const_cast<CompatVector<T, Alloc>*>(this))[index];
}



template<typename Alloc>
CompatIntVector<Alloc>::CompatIntVector(const CompatIntVector& other) :
    length(other.length), bit_width(other.bit_width), data(other.data) {
    // Nothing to do!
}

template<typename Alloc>
CompatIntVector<Alloc>::CompatIntVector(CompatIntVector&& other) :
    length(other.length), bit_width(other.bit_width), data(std::move(other.data)) {
    
    // Say they have no items or memory.
    other.clear();
}

template<typename Alloc>
template<typename OtherAlloc>
CompatIntVector<Alloc>::CompatIntVector(const CompatIntVector<OtherAlloc>& other) : length(other.length), bit_width(other.bit_width), data(other.data) {
    // Nothing to do!
}


template<typename Alloc>
template<typename OtherAlloc>
CompatIntVector<Alloc>& CompatIntVector<Alloc>::operator=(const CompatIntVector<OtherAlloc>& other) {
    // Copy their contents
    length = other.length;
    bit_width = other.bit_width;
    data = other.data;
    
    return *this;
}

template<typename Alloc>
template<typename OtherType>
CompatIntVector<Alloc>::CompatIntVector(const OtherType& other) {
    // Fall back on copy assignment
    *this = other;
}


template<typename Alloc>
template<typename OtherType>
CompatIntVector<Alloc>& CompatIntVector<Alloc>::operator=(const OtherType& other) {
    clear();
    width(other.width());
    resize(other.size());
    for (size_t i = 0; i < size(); i++) {
        // Copy all the entries, repacking each.
        // TODO: accelerate somehow if we can get direct access to bit-packed data.
        (*this)[i] = other[i];
    }
    return *this;
}

template<typename Alloc>
CompatIntVector<Alloc>& CompatIntVector<Alloc>::operator=(const CompatIntVector& other) {
    // Copy their contents
    length = other.length;
    bit_width = other.bit_width;
    data = other.data;
    
    return *this;
}

template<typename Alloc>
CompatIntVector<Alloc>& CompatIntVector<Alloc>::operator=(CompatIntVector&& other) {
    // Steal their contents
    length = other.length;
    bit_width = other.bit_width;
    data = std::move(other.data);
    
    // Say they have no items or memory.
    other.clear();
    
    return *this;
}

template<typename Alloc>
size_t CompatIntVector<Alloc>::size() const {
    return length;
}

template<typename Alloc>
void CompatIntVector<Alloc>::resize(size_t new_size) {
    // Work how many slots we need in the backing vector for this, rounding up.
    size_t item_slots = (new_size * width() + (std::numeric_limits<size_t>::digits - 1)) /
        std::numeric_limits<uint64_t>::digits;
    // Make sure we have that many slots
    data.resize(item_slots);
    // And say how big we are
    length = new_size;
}

template<typename Alloc>
size_t CompatIntVector<Alloc>::capacity() const {
    if (width() == 0) {
        // This is sort of nonsense.
        return 0;
    } else {
        // Work out how many items could be packed into the bits we have reserved.
        return data.capacity() * std::numeric_limits<uint64_t>::digits / width();
    }
}

template<typename Alloc>
void CompatIntVector<Alloc>::reserve(size_t new_reserved_length) {
    // Work how many slots we need in the backing vector for this, rounding up.
    size_t item_slots = (new_reserved_length * width() + (std::numeric_limits<uint64_t>::digits - 1)) /
        std::numeric_limits<uint64_t>::digits;
    // Reserve space in the backing vector
    data.reserve(item_slots);
}

template<typename Alloc>
void CompatIntVector<Alloc>::clear() {
    // Reset to our initial state
    length = 0;
    bit_width = 1;
    data.clear();
}

template<typename Alloc>
size_t CompatIntVector<Alloc>::width() const {
    return bit_width;
}

template<typename Alloc>
void CompatIntVector<Alloc>::width(size_t new_width) {
    // Save the new bit width, causing reinterpretation.
    bit_width = new_width;
    // TODO: make sure length won't take us out of bounds now?
    // TODO: prevent a width of 0?
}

template<typename Alloc>
void CompatIntVector<Alloc>::repack(size_t new_width, size_t new_size) {
    // TODO: Actually combine these operations to save a pass and an allocation
    resize(new_size);
    
    size_t old_width = bit_width;
    
    if (new_width == old_width) {
        // Nothing to do!
        return;
    }
    
    assert(new_width > 0);
    
    // Work out how many slots we need in backing storage.
    size_t new_entries = (length * new_width + (std::numeric_limits<uint64_t>::digits - 1)) /
        std::numeric_limits<uint64_t>::digits;
    
    if (new_width > old_width) {
        // We can expand in place
        data.resize(new_entries);
        
        // Starting at the end, to avoid overwriting
        size_t i = length;
        while (i > 0) {
            i--;
            // Copy all the data to the new size
            pack(i, unpack(i, old_width), new_width);
        }
    } else {
        // Must be shrinking
        
        // Starting at the beginning, to avoid overwriting
        for (size_t i = 0; i < length; i++) {
            // Copy all the data to the new size
            pack(i, unpack(i, old_width), new_width);
        }
        
        // Now shrink down
        data.resize(new_entries);
    }
    
    // And save the new width
    width(new_width);
}

template<typename Alloc>
void CompatIntVector<Alloc>::pack(size_t index, uint64_t value, size_t width_override) {
    // Decide how wide to encode the items as
    size_t effective_width = width_override ? width_override : (size_t) bit_width;
    
    if (effective_width < std::numeric_limits<uint64_t>::digits && value) {
        // It is possible we have been given a number that we cannot represent.
        // Use the compiler's count leading zeroes function, which hopefully it has.
        size_t needed_bits = std::numeric_limits<uint64_t>::digits - __builtin_clzll(value);
        if (needed_bits > effective_width) {
            // The value will not fit.
            throw std::invalid_argument("Need " + std::to_string(needed_bits) +
                                        " bits to represent value " + std::to_string(value) +
                                        " but only have " + std::to_string(effective_width)); 
        }
    }
    
    // Find the bit index we start at
    size_t start_bit = index * effective_width;
    // And break into a slot number
    size_t start_slot = start_bit / std::numeric_limits<uint64_t>::digits;
    // And a start bit in that slot
    size_t start_slot_bit_offset = start_bit % std::numeric_limits<uint64_t>::digits;
    // And then save
#ifdef debug_bit_packing
    std::cerr << "Write " << value
        << " of width " << effective_width
        << " at bit " << start_slot_bit_offset
        << " in slot " << start_slot << endl; 
#endif
    sdsl::bits::write_int(&data[start_slot], value, start_slot_bit_offset, effective_width);
}

template<typename Alloc>
uint64_t CompatIntVector<Alloc>::unpack(size_t index, size_t width_override) const {
    // Decide how wide to interpret the items as
    size_t effective_width = width_override ? width_override : (size_t) bit_width;
    // Find the bit index we start at
    size_t start_bit = index * effective_width;
    // And break into a slot number
    size_t start_slot = start_bit / std::numeric_limits<uint64_t>::digits;
    // And a start bit in that slot
    size_t start_slot_bit_offset = start_bit % std::numeric_limits<uint64_t>::digits;
    // And then load
#ifdef debug_bit_packing
    std::cerr << "Read value of width " << effective_width
        << " at bit " << start_slot_bit_offset
        << " in slot " << start_slot << ": ";
#endif
    uint64_t value = sdsl::bits::read_int(&data[start_slot], start_slot_bit_offset, effective_width);
#ifdef debug_bit_packing
    std::cerr << value << std::endl;
#endif
    return value;
}

template<typename Alloc>
CompatIntVector<Alloc>::Proxy::Proxy(CompatIntVector& parent, size_t index) : parent(parent), index(index) {
    // Nothing to do!
}

template<typename Alloc>
CompatIntVector<Alloc>::Proxy::operator uint64_t () const {
    return parent.unpack(index);
}

template<typename Alloc>
auto CompatIntVector<Alloc>::Proxy::operator=(uint64_t new_value) -> Proxy& {
    parent.pack(index, new_value);
    return *this;
}

template<typename Alloc>
CompatIntVector<Alloc>::ConstProxy::ConstProxy(const CompatIntVector& parent, size_t index) : parent(parent), index(index) {
    // Nothing to do!
}

template<typename Alloc>
CompatIntVector<Alloc>::ConstProxy::operator uint64_t () const {
    return parent.unpack(index);
}

template<typename Alloc>
auto CompatIntVector<Alloc>::at(size_t index) -> Proxy {
    if (index > size()) {
        throw std::out_of_range("Accessing index " + std::to_string(index) +
            " in integer vector of length " + std::to_string(size()));
    }
    return Proxy(*this, index);
}

template<typename Alloc>
auto CompatIntVector<Alloc>::at(size_t index) const -> ConstProxy {
    if (index > size()) {
        throw std::out_of_range("Accessing index " + std::to_string(index) +
            " in integer vector of length " + std::to_string(size()));
    }
    return ConstProxy(*this, index);
}

template<typename Alloc>
auto CompatIntVector<Alloc>::operator[](size_t index) -> Proxy {
    return Proxy(*this, index);
}

template<typename Alloc>
auto CompatIntVector<Alloc>::operator[](size_t index) const -> ConstProxy {
    return ConstProxy(*this, index);
}

template<typename Alloc>
void CompatIntVector<Alloc>::serialize(std::ostream& out) const {
    // Dump the length
    out.write((const char*)&length, sizeof(length));
    // Dump the width
    out.write((const char*)&bit_width, sizeof(length));
    // Write the data
    data.serialize(out);
}

template<typename Alloc>
void CompatIntVector<Alloc>::load(std::istream& in) {
    // Read the length
    in.read((char*)&length, sizeof(length));
    // Read the width
    in.read((char*)&bit_width, sizeof(bit_width));
    // Read the data
    data.load(in);
}

namespace yomo {

template<typename T>
Pointer<T>::Pointer(T* destination) {
    *this = destination;
}

template<typename T>
Pointer<T>::operator bool () const {
    return position != std::numeric_limits<size_t>::max();
}

template<typename T>
std::add_lvalue_reference<T> Pointer<T>::operator*() const {
    return *get();
}

template<typename T>
T* Pointer<T>::operator->() const {
    return get();
}

template<typename T>
Pointer<T>::operator T* () const {
    return get();
}

template<typename T>
Pointer<T>& Pointer<T>::operator=(T* addr) {
    if (addr == nullptr) {
        // Adopt our special null value
        position = std::numeric_limits<size_t>::max();
    } else {
        // Get the position, requiring that it is in the same chain as us.
        position = Manager::get_position_in_same_chain(this, addr);
    }
    return *this;
}

template<typename T>
T* Pointer<T>::operator+(size_t items) const {
    return get() + items;
}

template<typename T>
T* Pointer<T>::get() const {
    if (position == std::numeric_limits<size_t>::max()) {
        return nullptr;
    } else {
        return (T*) Manager::get_address_in_same_chain((const void*) this, position);
    }
}



template<typename T>
template<typename U>
Allocator<T>::Allocator(const Allocator<U>& alloc) {
    // Nothing to do!
}

template<typename T>
bool Allocator<T>::operator==(const Allocator& other) const {
    // TODO: Technically anybody can deallocate anybody else's memory since it
    // always deallocates from the chain it is in, not the one we are in. But
    // we say we're equal if we are using the same chain for allocations.
    return get_chain() == other.get_chain();
}

template<typename T>
bool Allocator<T>::operator!=(const Allocator& other) const {
    return !(*this == other);
}

template<typename T>
auto Allocator<T>::allocate(size_type n, const T* hint) -> T* {
    return (T*) Manager::allocate_from(get_chain(), n * sizeof(T));
}

template<typename T>
void Allocator<T>::deallocate(T* p, size_type n) {
    Manager::deallocate((void*) p);
}

template<typename T>
size_t Allocator<T>::max_size() const {
    // TODO: this probably won't really fit in memory, but other than that
    // there's no reason we can't allocate something this big.
    return numeric_limits<size_t>::max();
}

template<typename T>
Manager::chainid_t Allocator<T>::get_chain() const {
    return Manager::get_chain((void*) this);
}

template<typename T>
UniqueMappedPointer<T>::operator bool () const {
    return chain != Manager::NO_CHAIN;
}

template<typename T>
T& UniqueMappedPointer<T>::operator*() {
    return *get();
}

template<typename T>
const T& UniqueMappedPointer<T>::operator*() const {
    return *get();
}

template<typename T>
T* UniqueMappedPointer<T>::operator->() {
    return get();
}

template<typename T>
const T* UniqueMappedPointer<T>::operator->() const {
    return get();
}

template<typename T>
UniqueMappedPointer<T>::operator T*() {
    return get();
}

template<typename T>
UniqueMappedPointer<T>::operator const T*() const {
    return get();
}

template<typename T>
T* UniqueMappedPointer<T>::get() {
    if (chain == Manager::NO_CHAIN) {
        return nullptr;
    } else {
        return (T*) Manager::find_first_allocation(chain, sizeof(T));
    }
}

template<typename T>
const T* UniqueMappedPointer<T>::get() const {
    if (chain == Manager::NO_CHAIN) {
        return nullptr;
    } else {
        return (T*) Manager::find_first_allocation(chain, sizeof(T));
    }
}

template<typename T>
template<typename... Args>
void UniqueMappedPointer<T>::construct_internal(const std::string& prefix, Args&&... constructor_args) {
    // Drop any existing chain.
    reset();
    
    // Make a new chain
    chain = Manager::create_chain(prefix);
    // Allocate space in the cahin for the item.
    // Can't use the Allocator because we don't have a place in the chain to
    // store one.
    T* item = (T*) Manager::allocate_from(chain, sizeof(T));
    
    // Run the constructor.
    new (item) T(std::forward<Args>(constructor_args)...);
}

template<typename T>
void UniqueMappedPointer<T>::construct() {
    // Use an empty prefix.
    construct_internal("");
}

template<typename T>
void UniqueMappedPointer<T>::construct(const std::string& prefix) {
    // Use the provided prefix.
    construct_internal(prefix);
}

template<typename T>
template<typename... Args>
void UniqueMappedPointer<T>::construct(const std::string& prefix, Args&&... constructor_args) {
    // Pass along args and use the provided prefix.
    construct_internal(prefix, std::forward<Args>(constructor_args)...);
}

template<typename T>
void UniqueMappedPointer<T>::load(int fd, const std::string& prefix) {
    // Drop any existing chain.
    reset();

    chain = Manager::create_chain(fd, prefix);
}

template<typename T>
void UniqueMappedPointer<T>::load(std::istream& in, const std::string& prefix) {
    if (!prefix.empty()) {
        // First read and check the prefix
        std::string prefix_buffer(prefix.size());
        in.read(&prefix_buffer.at(0), prefix_buffer.size());
        if (!in || prefix_buffer != prefix) {
            // Hit EOF or got the wrong thing
            throw std::runtime_error("Expected prefix not found in input. Check file type.");
        }
    }
    // Then read everything after the prefix.
    load_after_prefix(in, prefix);
}

template<typename T>
void UniqueMappedPointer<T>::load(const std::function<std::string(void)>& iterator, const std::string& prefix) {
    // Drop any existing chain.
    reset();
    
    // Just pass through to the Manager
    chain = Manager::create_chain(iterator, prefix);
}

template<typename T>
void UniqueMappedPointer<T>::load_after_prefix(std::istream& in, const std::string& prefix) {
    // Drop any existing chain.
    reset();
    
    const size_t MAX_CHUNK_SIZE = 4096;
    
    // Fill up this buffer with chunks of a certian size
    std::string buffer;
    
    chain = Manager::create_chain([&]() {
        if (buffer.empty() && !prefix.empty()) {
            // Inject the prefix on the first call
            buffer = prefix;
        } else {
            // Other calls read data, until the last call shows an empty buffer
            // for EOF.
            buffer.resize(MAX_CHUNK_SIZE);
            // Grab a chunk
            in.read(&buffer.at(0), MAX_CHUNK_SIZE);
            if (!in) {
                // Didn't read all the characters, so shrink down (maybe to 0)
                buffer.resize(in.gcount());
            }
        }
        // Copy the buffer over to the caller.
        // TODO: can we save a copy here?
        return buffer;
    }, prefix);
}

template<typename T>
void UniqueMappedPointer<T>::dissociate() {
    if (chain == Manager::NO_CHAIN) {
        throw runtime_error("Cannot dissociate a null object");
    }
    // Copy the chain
    Manager::chainid_t new_chain = Manager::get_dissociated_chain(chain);
    // Get rid of the old chain
    Manager::destroy_chain(chain);
    // Adopt the new chain
    chain = new_chain;
}

template<typename T>
void UniqueMappedPointer<T>::save(int fd) {
    if (chain == Manager::NO_CHAIN) {
        throw runtime_error("Cannot save a null object");
    }
    // Copy the chain
    Manager::chainid_t new_chain = Manager::get_associated_chain(chain, fd);
    // Get rid of the old chain
    Manager::destroy_chain(chain);
    // Adopt the new chain
    chain = new_chain;
}

template<typename T>
void UniqueMappedPointer<T>::save(std::ostream& out) const {
    Manager::scan_chain(chain, [&](const void* start, size_t length) {
        // Go through all the data in the chain
        // And save it to the stream.
        out.write((const char*) start, length); 
    });
}

template<typename T>
void UniqueMappedPointer<T>::save(const std::function<void(const void*, size_t)>& iteratee) const {
    // Just pass through to the Manager.
    Manager::scan_chain(chain, iteratee);
}

template<typename T>
void UniqueMappedPointer<T>::save_after_prefix(std::ostream& out, const std::string& prefix) const {
    // We need to drop as many characters from the chain as are in the prefix.
    size_t dropped = 0;
     
    Manager::scan_chain(chain, [&](const void* start, size_t length) {
        // Go through all the data in the chain
        const char* start_char = (const char*) start;
        
        // Adjust to skip any part of the prefix that's still here.
        size_t to_drop = std::min(length, prefix.size() - dropped);
        start_char += to_drop;
        length -= to_drop;
        dropped += to_drop;
        
        // And save it to the stream.
        out.write(start_char, length); 
    });
}

template<typename T>
void UniqueMappedPointer<T>::reset() {
    if (chain != Manager::NO_CHAIN) {
        Manager::destroy_chain(chain);
        chain = Manager::NO_CHAIN;
    }
}

}


}

#endif
