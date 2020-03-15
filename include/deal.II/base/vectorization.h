// ---------------------------------------------------------------------
//
// Copyright (C) 2011 - 2019 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


#ifndef dealii_vectorization_h
#define dealii_vectorization_h

#include <deal.II/base/config.h>

#include <deal.II/base/exceptions.h>
#include <deal.II/base/template_constraints.h>

#include <array>
#include <cmath>

// Note:
// The flag DEAL_II_COMPILER_VECTORIZATION_LEVEL is essentially constructed
// according to the following scheme (on x86-based architectures)
// #ifdef __AVX512F__
// #define DEAL_II_COMPILER_VECTORIZATION_LEVEL 3
// #elif defined (__AVX__)
// #define DEAL_II_COMPILER_VECTORIZATION_LEVEL 2
// #elif defined (__SSE2__)
// #define DEAL_II_COMPILER_VECTORIZATION_LEVEL 1
// #else
// #define DEAL_II_COMPILER_VECTORIZATION_LEVEL 0
// #endif
// In addition to checking the flags __AVX__ and __SSE2__, a CMake test,
// 'check_01_cpu_features.cmake', ensures that these feature are not only
// present in the compilation unit but also working properly.

#if DEAL_II_COMPILER_VECTORIZATION_LEVEL > 0

#  if DEAL_II_COMPILER_VECTORIZATION_LEVEL >= 2 && defined(__SSE2__) && \
    !defined(__AVX__)
#    error \
      "Mismatch in vectorization capabilities: AVX was detected during configuration of deal.II and switched on, but it is apparently not available for the file you are trying to compile at the moment. Check compilation flags controlling the instruction set, such as -march=native."
#  endif
#  if DEAL_II_COMPILER_VECTORIZATION_LEVEL >= 3 && defined(__SSE2__) && \
    !defined(__AVX512F__)
#    error \
      "Mismatch in vectorization capabilities: AVX-512F was detected during configuration of deal.II and switched on, but it is apparently not available for the file you are trying to compile at the moment. Check compilation flags controlling the instruction set, such as -march=native."
#  endif

#  if defined(_MSC_VER)
#    include <intrin.h>
#  elif defined(__ALTIVEC__)
#    include <altivec.h>

// altivec.h defines vector, pixel, bool, but we do not use them, so undefine
// them before they make trouble
#    undef vector
#    undef pixel
#    undef bool
#  else
#    include <x86intrin.h>
#  endif

#endif


DEAL_II_NAMESPACE_OPEN


// Enable the EnableIfScalar type trait for VectorizedArray<Number> such
// that it can be used as a Number type in Tensor<rank,dim,Number>, etc.

template <typename Number, int width>
struct EnableIfScalar<VectorizedArray<Number, width>>
{
  using type = VectorizedArray<typename EnableIfScalar<Number>::type, width>;
};



/**
 * An iterator for VectorizedArray.
 *
 * @author Peter Munch, 2019
 */
template <typename T>
class VectorizedArrayIterator
{
public:
  /**
   * Constructor.
   *
   * @param data The actual VectorizedArray.
   * @param lane A pointer to the current lane.
   */
  VectorizedArrayIterator(T &data, const unsigned int lane)
    : data(data)
    , lane(lane)
  {}

  /**
   * Compare for inequality.
   */
  bool
  operator!=(const VectorizedArrayIterator<T> &other) const
  {
    return this->lane != other.lane;
  }

  /**
   * Dereferencing operator (const version): returns the value of the current
   * lane.
   */
  const typename T::value_type &operator*() const
  {
    return data[lane];
  }


  /**
   * Dereferencing operator (non-@p const version): returns the value of the
   * current lane.
   */
  template <typename U = T>
  typename std::enable_if<!std::is_same<U, const U>::value,
                          typename T::value_type>::type &
  operator*()
  {
    return data[lane];
  }

  /**
   * Prefix <tt>++</tt> operator: <tt>++iterator</tt>. This operator advances
   * the iterator to the next lane and returns a reference to
   * <tt>*this</tt>.
   */
  VectorizedArrayIterator<T> &
  operator++()
  {
    lane++;
    return *this;
  }

private:
  /**
   * Reference to the actual VectorizedArray.
   */
  T &data;

  /**
   * Pointer to the current lane.
   */
  unsigned int lane;
};



/**
 * A base class for the various VectorizedArray template specializations,
 * containing common functionalities.
 *
 * @tparam T Type of the actual vectorized array. We are using the
 *   Couriously Recurring Template Pattern (see
 *   https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) in this
 *   class to avoid having to resort to `virtual` member functions.
 *
 * @author Peter Munch, 2019
 */
template <typename T>
class VectorizedArrayBase
{
public:
  /**
   * Return the number of elements in the array stored in the variable
   * n_array_elements of VectorizedArray.
   */
  static constexpr unsigned int
  size()
  {
    return T::n_array_elements;
  }

  /**
   * @return An iterator pointing to the beginning of the underlying data.
   */
  VectorizedArrayIterator<T>
  begin()
  {
    return VectorizedArrayIterator<T>(static_cast<T &>(*this), 0);
  }

  /**
   * @return An iterator pointing to the beginning of the underlying data (`const`
   * version).
   */
  VectorizedArrayIterator<const T>
  begin() const
  {
    return VectorizedArrayIterator<const T>(static_cast<const T &>(*this), 0);
  }

  /**
   * @return An iterator pointing to the end of the underlying data.
   */
  VectorizedArrayIterator<T>
  end()
  {
    return VectorizedArrayIterator<T>(static_cast<T &>(*this),
                                      T::n_array_elements);
  }

  /**
   * @return An iterator pointing to the end of the underlying data (`const`
   * version).
   */
  VectorizedArrayIterator<const T>
  end() const
  {
    return VectorizedArrayIterator<const T>(static_cast<const T &>(*this),
                                            T::n_array_elements);
  }
};



/**
 * This generic class defines a unified interface to a vectorized data type.
 * For general template arguments, this class simply corresponds to the
 * template argument. For example, VectorizedArray<long double> is nothing
 * else but a wrapper around <tt>long double</tt> with exactly one data field
 * of type <tt>long double</tt> and overloaded arithmetic operations. This
 * means that <tt>VectorizedArray<ComplicatedType></tt> has a similar layout
 * as ComplicatedType, provided that ComplicatedType defines basic arithmetic
 * operations. For floats and doubles, an array of numbers are packed together
 * with the goal to be processed in a single-instruction/multiple-data (SIMD)
 * fashion. In the SIMD context, the elements of such a short vector are often
 * called lanes. The number of elements packed together, i.e., the number of
 * lanes, depends on the computer system and compiler flags that are used for
 * compilation of deal.II. The fundamental idea of these packed data types is
 * to use one single CPU instruction to perform arithmetic operations on the
 * whole array using the processor's vector (SIMD) units. Most computer
 * systems by 2010 standards will use an array of two doubles or four floats,
 * respectively (this corresponds to the SSE/SSE2 data sets) when compiling
 * deal.II on 64-bit operating systems. On Intel Sandy Bridge processors and
 * newer or AMD Bulldozer processors and newer, four doubles or eight floats
 * are used when deal.II is configured using gcc with \--with-cpu=native
 * or \--with-cpu=corei7-avx. On compilations with AVX-512 support (e.g.,
 * Intel Skylake Server from 2017), eight doubles or sixteen floats are used.
 *
 * This behavior of this class is made similar to the basic data types double
 * and float. The definition of a vectorized array does not initialize the
 * data field but rather leaves it undefined, as is the case for double and
 * float. However, when calling something like `VectorizedArray<double> a =
 * VectorizedArray<double>()` or `VectorizedArray<double> a = 0.`, it sets all
 * numbers in this field to zero. This class is of standard layout type
 * according to the C++11 standard, which means that there is an equivalent C
 * representation and the class can e.g. be safely copied with std::memcpy.
 * (See also https://en.cppreference.com/w/cpp/named_req/StandardLayoutType.)
 * The standard layout is also necessary for ensuring correct alignment of
 * data with address boundaries when collected in a vector (i.e., when the
 * first element in a vector is properly aligned, all subsequent elements will
 * be correctly aligned, too).
 *
 * Note that for proper functioning of this class, certain data alignment
 * rules must be respected. This is because the computer expects the starting
 * address of a VectorizedArray<double> field at specific addresses in memory
 * (usually, the address of the vectorized array should be a multiple of the
 * length of the array in bytes). Otherwise, a segmentation fault or a severe
 * loss of performance might occur. When creating a single data field on the
 * stack like `VectorizedArray<double> a = 5.;`, the compiler will take care
 * of data alignment automatically. However, when allocating a long vector of
 * VectorizedArray<double> data, one needs to respect these rules. Use the
 * class AlignedVector or data containers based on AlignedVector (such as
 * Table) for this purpose. It is a class very similar to std::vector
 * otherwise but always makes sure that data is correctly aligned.
 *
 * The user can explicitly control the width of a particular instruction set
 * architecture (ISA) extension by specifying the number of lanes via the second
 * template parameter of this wrapper class. For example on Intel Skylake
 * Server, you have the following options for the data type double:
 *  - VectorizedArray<double, 1> // no vectorization (auto-optimization)
 *  - VectorizedArray<double, 2> // SSE2
 *  - VectorizedArray<double, 4> // AVX
 *  - VectorizedArray<double, 8> // AVX-512 (default)
 *
 * and for Intel Sandy Bridge, Haswell, Broadwell, AMD Bulldozer and Zen/Ryzen:
 *  - VectorizedArray<double, 1> // no vectorization (auto-optimization)
 *  - VectorizedArray<double, 2> // SSE2
 *  - VectorizedArray<double, 4> // AVX (default)
 *
 * and for processors with AltiVec support:
 *  - VectorizedArray<double, 1>
 *  - VectorizedArray<double, 2>
 *
 * for older x86 processors or in case no processor-specific compilation flags
 * were added (i.e., without `-D CMAKE_CXX_FLAGS=-march=native` or similar
 * flags):
 *  - VectorizedArray<double, 1> // no vectorization (auto-optimization)
 *  - VectorizedArray<double, 2> // SSE2
 *
 * Similar considerations also apply to the data type `float`.
 *
 * Wrongly selecting the width, e.g., width=3 or width=8 on a processor which
 * does not support AVX-512 leads to a static assert.
 *
 * @tparam Number underlying data type
 * @tparam width  vector length (optional; if not set, the maximal width of the
 *                architecture is used)
 *
 * @author Katharina Kormann, Martin Kronbichler, Peter Munch, 2010, 2011, 2019
 */
template <typename Number, int width>
class VectorizedArray
  : public VectorizedArrayBase<VectorizedArray<Number, width>>
{
public:
  /**
   * This gives the type of the array elements.
   */
  using value_type = Number;

  /**
   * This gives the number of elements collected in this class. In the general
   * case, there is only one element. Specializations use SIMD intrinsics and
   * can work on multiple elements at the same time.
   */
  static const unsigned int n_array_elements = width;

  /**
   * Default empty constructor, leaving the data in an uninitialized state
   * similar to float/double.
   */
  VectorizedArray() = default;

  /**
   * Construct an array with the given scalar broadcast to all lanes.
   */
  VectorizedArray(const Number scalar)
  {
    this->operator=(scalar);
  }

  /**
   * Copy constructor.
   */
  VectorizedArray(const VectorizedArray &other)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] = other.data[v];
  }

  /**
   * This function assigns a scalar to this class.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator=(const Number scalar)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] = scalar;
    return *this;
  }

  /**
   * Copy assignment operator.
   */
  VectorizedArray &
  operator=(const VectorizedArray &other)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] = other.data[v];
    return *this;
  }

  /**
   * Access operator (only valid with component 0 in the base class without
   * specialization).
   */
  DEAL_II_ALWAYS_INLINE
  Number &operator[](const unsigned int comp)
  {
    AssertIndexRange(comp, n_array_elements);
    return data[comp];
  }

  /**
   * Constant access operator (only valid with component 0 in the base class
   * without specialization).
   */
  DEAL_II_ALWAYS_INLINE
  const Number &operator[](const unsigned int comp) const
  {
    AssertIndexRange(comp, n_array_elements);
    return data[comp];
  }

  /**
   * Addition
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator+=(const VectorizedArray &vec)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] += vec.data[v];
    return *this;
  }

  /**
   * Subtraction
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator-=(const VectorizedArray &vec)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] -= vec.data[v];
    return *this;
  }

  /**
   * Multiplication
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator*=(const VectorizedArray &vec)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] *= vec.data[v];
    return *this;
  }

  /**
   * Division
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator/=(const VectorizedArray &vec)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] /= vec.data[v];
    return *this;
  }

  /**
   * Load @p n_array_elements from memory into the calling class, starting at
   * the given address. The pointer `ptr` needs not be aligned by the amount
   * of bytes in the vectorized array, as opposed to casting a double address
   * to VectorizedArray<double>*.
   */
  DEAL_II_ALWAYS_INLINE
  void
  load(const Number *ptr)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] = ptr[v];
  }

  /**
   * Write the content of the calling class into memory in form of @p
   * n_array_elements to the given address. The pointer `ptr` needs not be
   * aligned by the amount of bytes in the vectorized array, as opposed to
   * casting a double address to VectorizedArray<double>*.
   */
  DEAL_II_ALWAYS_INLINE
  void
  store(Number *ptr) const
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      ptr[v] = data[v];
  }

  /**
   * Write the content of the calling class into memory in form of
   * @p n_array_elements to the given address using non-temporal stores that
   * bypass the processor's caches, using @p _mm_stream_pd store intrinsics on
   * supported CPUs. The destination of the store @p ptr must be aligned by
   * the amount of bytes in the vectorized array.
   *
   * This store operation can be faster than usual store operations in case
   * the store is streaming because it avoids the read-for-ownership transfer
   * typically invoked in standard stores. This approximately works as follows
   * (see the literature on computer architecture for details): When an
   * algorithm stores some results to a memory address, a processor typically
   * wants to move it into some of its caches as it expects the data to be
   * re-used again at some point. Since caches are organized in lines of sizes
   * either 64 byte or 128 byte but writes are usually smaller, a processor
   * must first load in the destination cache line upon a write because only
   * part of the cache line is overwritten initially. If a series of stores
   * write data in a chunk bigger than any of its caches could handle, the
   * data finally has to be moved out from the caches to main memory. But
   * since all addressed have first been read, this doubles the load on main
   * memory, which can incur a performance penalty. Furthermore, the
   * organization of caches in a multicore context also requires reading an
   * address before something can be written to cache to that address, see
   * e.g. the <a href="https://en.wikipedia.org/wiki/MESI_protocol">Wikipedia
   * article on the MESI protocol</a> for details. The instruction underlying
   * this function call signals to the processor that these two prerequisites
   * on a store are relaxed: Firstly, one expects the whole cache line to be
   * overwritten (meaning that the memory subsystem makes sure that
   * consecutive stores that together span a cache line are merged, and
   * appropriately handling the case where only part of a cache line is
   * written), so there is no need to first read the "remainder" of the cache
   * line. Secondly, the data behind that particular memory will not be
   * subject to cache coherency protocol as it will be in main memory both
   * when the same processor wants to access it again as well as any other
   * processors in a multicore chip. Due to this particular setup, any
   * subsequent access to the data written by this function will need to query
   * main memory, which is slower than an access from a cache both
   * latency-wise and throughput-wise. Thus, this command should only be used
   * for storing large arrays that will collectively not fit into caches, as
   * performance will be degraded otherwise. For a typical use case, see also
   * <a href="https://blogs.fau.de/hager/archives/2103">this blog article</a>.
   *
   * Note that streaming stores are only available in the specialized SSE/AVX
   * classes of VectorizedArray of type @p double or @p float, not in the
   * generic base class.
   */
  DEAL_II_ALWAYS_INLINE
  void
  streaming_store(Number *ptr) const
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      ptr[v] = data[v];
  }

  /**
   * Load @p n_array_elements from memory into the calling class, starting at
   * the given address and with given offsets, each entry from the offset
   * providing one element of the vectorized array.
   *
   * This operation corresponds to the following code (but uses a more
   * efficient implementation in case the hardware allows for that):
   * @code
   * for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
   *   this->operator[](v) = base_ptr[offsets[v]];
   * @endcode
   */
  DEAL_II_ALWAYS_INLINE
  void
  gather(const Number *base_ptr, const unsigned int *offsets)
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      data[v] = base_ptr[offsets[v]];
  }

  /**
   * Write the content of the calling class into memory in form of @p
   * n_array_elements to the given address and the given offsets, filling the
   * elements of the vectorized array into each offset.
   *
   * This operation corresponds to the following code (but uses a more
   * efficient implementation in case the hardware allows for that):
   * @code
   * for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
   *   base_ptr[offsets[v]] = this->operator[](v);
   * @endcode
   */
  DEAL_II_ALWAYS_INLINE
  void
  scatter(const unsigned int *offsets, Number *base_ptr) const
  {
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      base_ptr[offsets[v]] = data[v];
  }

  /**
   * Actual data field. To be consistent with the standard layout type and to
   * enable interaction with external SIMD functionality, this member is
   * declared public.
   */
  Number data[width];

private:
  /**
   * Return the square root of this field. Not for use in user code. Use
   * sqrt(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_sqrt() const
  {
    VectorizedArray res;
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      res.data[v] = std::sqrt(data[v]);
    return res;
  }

  /**
   * Return the absolute value of this field. Not for use in user code. Use
   * abs(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_abs() const
  {
    VectorizedArray res;
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      res.data[v] = std::fabs(data[v]);
    return res;
  }

  /**
   * Return the component-wise maximum of this field and another one. Not for
   * use in user code. Use max(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_max(const VectorizedArray &other) const
  {
    VectorizedArray res;
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      res.data[v] = std::max(data[v], other.data[v]);
    return res;
  }

  /**
   * Return the component-wise minimum of this field and another one. Not for
   * use in user code. Use min(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_min(const VectorizedArray &other) const
  {
    VectorizedArray res;
    DEAL_II_OPENMP_SIMD_PRAGMA
    for (unsigned int v=0; v<n_array_elements; ++v)
      res.data[v] = std::min(data[v], other.data[v]);
    return res;
  }

  // Make a few functions friends.
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::sqrt(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::abs(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::max(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::min(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
};



// We need to have a separate declaration for static const members
template <typename Number, int width>
const unsigned int VectorizedArray<Number, width>::n_array_elements;



/**
 * @name Packing and unpacking of a VectorizedArray
 */
//@{


/**
 * Create a vectorized array that sets all entries in the array to the given
 * scalar, i.e., broadcasts the scalar to all array elements.
 *
 * @relatesalso VectorizedArray
 */
template <
  typename Number,
  int width = internal::VectorizedArrayWidthSpecifier<Number>::max_width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             make_vectorized_array(const Number &u)
{
  VectorizedArray<Number, width> result = u;
  return result;
}



/**
 * Create a vectorized array of given type and broadcast the scalar value
 * to all array elements.
 *
 *  @relatesalso VectorizedArray
 */
template <typename VectorizedArrayType>
inline DEAL_II_ALWAYS_INLINE VectorizedArrayType
                             make_vectorized_array(const typename VectorizedArrayType::value_type &u)
{
  static_assert(
    std::is_same<VectorizedArrayType,
                 VectorizedArray<typename VectorizedArrayType::value_type,
                                 VectorizedArrayType::n_array_elements>>::value,
    "VectorizedArrayType is not a VectorizedArray.");

  VectorizedArrayType result = u;
  return result;
}



/**
 * Load @p n_array_elements from memory into the the VectorizedArray @p out,
 * starting at the given addresses and with given offset, each entry from the
 * offset providing one element of the vectorized array.
 *
 * This operation corresponds to the following code:
 * @code
 * for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
 *   out.data[v] = ptrs[v][offset];
 * @endcode
 */
template <typename Number, int width, std::size_t width_>
inline DEAL_II_ALWAYS_INLINE void
gather(VectorizedArray<Number, width> &    out,
       const std::array<Number *, width_> &ptrs,
       const unsigned int                  offset)
{
  static_assert(width == width_, "Length of input vector do not match!");

  for (unsigned int v = 0; v < width_; v++)
    out.data[v] = ptrs[v][offset];
}



/**
 * This method loads VectorizedArray::n_array_elements data streams from the
 * given array @p in. The offsets to the input array are given by the array @p
 * offsets. From each stream, n_entries are read. The data is then transposed
 * and stored it into an array of VectorizedArray type. The output array @p
 * out is expected to be an array of size @p n_entries. This method operates
 * on plain arrays, so no checks for valid data access are made. It is the
 * user's responsibility to ensure that the given arrays are valid according
 * to the access layout below.
 *
 * This operation corresponds to a transformation of an array-of-struct
 * (input) into a struct-of-array (output) according to the following formula:
 *
 * @code
 * for (unsigned int i=0; i<n_entries; ++i)
 *   for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
 *     out[i][v] = in[offsets[v]+i];
 * @endcode
 *
 * A more optimized version of this code will be used for supported types.
 *
 * This is the inverse operation to vectorized_transpose_and_store().
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE void
vectorized_load_and_transpose(const unsigned int              n_entries,
                              const Number *                  in,
                              const unsigned int *            offsets,
                              VectorizedArray<Number, width> *out)
{
  for (unsigned int i = 0; i < n_entries; ++i)
    {
      DEAL_II_OPENMP_SIMD_PRAGMA
      for (unsigned int v = 0;
           v < VectorizedArray<Number, width>::n_array_elements;
           ++v)
        out[i][v] = in[offsets[v] + i];
    }
}


/**
 * The same as above with the difference that an array of pointers are
 * passed in as input argument @p in.
 *
 * In analogy to the function above, one can consider that
 * `in+offset[v]` is precomputed and passed as input argument.
 *
 * However, this function can also be used if some function returns an array
 * of pointers and no assumption can be made that they belong to the same array,
 * i.e., they can have their origin in different memory allocations.
 */
template <typename Number, int width, std::size_t width_>
inline DEAL_II_ALWAYS_INLINE void
vectorized_load_and_transpose(const unsigned int                  n_entries,
                              const std::array<Number *, width_> &in,
                              VectorizedArray<Number, width> *    out)
{
  static_assert(width == width_, "Length of input vector do not match!");

  for (unsigned int i = 0; i < n_entries; ++i)
    {
      DEAL_II_OPENMP_SIMD_PRAGMA
      for (unsigned int v = 0;
           v < VectorizedArray<Number, width>::n_array_elements;
           ++v)
        out[i][v] = in[v][i];
    }
}



/**
 * This method stores the vectorized arrays in transposed form into the given
 * output array @p out with the given offsets @p offsets. This operation
 * corresponds to a transformation of a struct-of-array (input) into an array-
 * of-struct (output). This method operates on plain array, so no checks for
 * valid data access are made. It is the user's responsibility to ensure that
 * the given arrays are valid according to the access layout below.
 *
 * This method assumes that the specified offsets do not overlap. Otherwise,
 * the behavior is undefined in the vectorized case. It is the user's
 * responsibility to make sure that the access does not overlap and avoid
 * undefined behavior.
 *
 * The argument @p add_into selects where the entries should only be written
 * into the output arrays or the result should be added into the existing
 * entries in the output. For <code>add_into == false</code>, the following
 * code is assumed:
 *
 * @code
 * for (unsigned int i=0; i<n_entries; ++i)
 *   for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
 *     out[offsets[v]+i] = in[i][v];
 * @endcode
 *
 * For <code>add_into == true</code>, the code implements the following
 * action:
 * @code
 * for (unsigned int i=0; i<n_entries; ++i)
 *   for (unsigned int v=0; v<VectorizedArray<Number>::n_array_elements; ++v)
 *     out[offsets[v]+i] += in[i][v];
 * @endcode
 *
 * A more optimized version of this code will be used for supported types.
 *
 * This is the inverse operation to vectorized_load_and_transpose().
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE void
vectorized_transpose_and_store(const bool                            add_into,
                               const unsigned int                    n_entries,
                               const VectorizedArray<Number, width> *in,
                               const unsigned int *                  offsets,
                               Number *                              out)
{
  if (add_into)
    for (unsigned int i = 0; i < n_entries; ++i)
      {
        DEAL_II_OPENMP_SIMD_PRAGMA
        for (unsigned int v = 0;
             v < VectorizedArray<Number, width>::n_array_elements;
             ++v)
          out[offsets[v] + i] += in[i][v];
      }
  else
    for (unsigned int i = 0; i < n_entries; ++i)
      {
        DEAL_II_OPENMP_SIMD_PRAGMA
        for (unsigned int v = 0;
             v < VectorizedArray<Number, width>::n_array_elements;
             ++v)
          out[offsets[v] + i] = in[i][v];
      }
}


/**
 * The same as above with the difference that an array of pointers are
 * passed in as input argument @p out.
 *
 * In analogy to the function above, one can consider that
 * `out+offset[v]` is precomputed and passed as input argument.
 *
 * However, this function can also be used if some function returns an array
 * of pointers and no assumption can be made that they belong to the same array,
 * i.e., they can have their origin in different memory allocations.
 */
template <typename Number, int width, std::size_t width_>
inline DEAL_II_ALWAYS_INLINE void
vectorized_transpose_and_store(const bool                            add_into,
                               const unsigned int                    n_entries,
                               const VectorizedArray<Number, width> *in,
                               std::array<Number *, width_> &        out)
{
  static_assert(width == width_, "Length of input vector do not match!");

  if (add_into)
    for (unsigned int i = 0; i < n_entries; ++i)
      for (unsigned int v = 0;
           v < VectorizedArray<Number, width>::n_array_elements;
           ++v)
        out[v][i] += in[i][v];
  else
    for (unsigned int i = 0; i < n_entries; ++i)
      for (unsigned int v = 0;
           v < VectorizedArray<Number, width>::n_array_elements;
           ++v)
        out[v][i] = in[i][v];
}


//@}


#  if DEAL_II_COMPILER_VECTORIZATION_LEVEL >= 1 && defined(__ALTIVEC__) && \
    defined(__VSX__)

template <>
class VectorizedArray<double, 2>
  : public VectorizedArrayBase<VectorizedArray<double, 2>>
{
public:
  /**
   * This gives the type of the array elements.
   */
  using value_type = double;

  /**
   * This gives the number of vectors collected in this class.
   */
  static const unsigned int n_array_elements = 2;

  /**
   * Default empty constructor, leaving the data in an uninitialized state
   * similar to float/double.
   */
  VectorizedArray() = default;

  /**
   * Construct an array with the given scalar broadcast to all lanes.
   */
  VectorizedArray(const double scalar)
  {
    this->operator=(scalar);
  }

  /**
   * This function assigns a scalar to this class.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator=(const double x)
  {
    data = vec_splats(x);

    // Some compilers believe that vec_splats sets 'x', but that's not true.
    // They then warn about setting a variable and not using it. Suppress the
    // warning by "using" the variable:
    (void)x;
    return *this;
  }

  /**
   * Access operator. The component must be either 0 or 1.
   */
  DEAL_II_ALWAYS_INLINE
  double &operator[](const unsigned int comp)
  {
    AssertIndexRange(comp, 2);
    return *(reinterpret_cast<double *>(&data) + comp);
  }

  /**
   * Constant access operator.
   */
  DEAL_II_ALWAYS_INLINE
  const double &operator[](const unsigned int comp) const
  {
    AssertIndexRange(comp, 2);
    return *(reinterpret_cast<const double *>(&data) + comp);
  }

  /**
   * Addition.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator+=(const VectorizedArray &vec)
  {
    data = vec_add(data, vec.data);
    return *this;
  }

  /**
   * Subtraction.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator-=(const VectorizedArray &vec)
  {
    data = vec_sub(data, vec.data);
    return *this;
  }

  /**
   * Multiplication.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator*=(const VectorizedArray &vec)
  {
    data = vec_mul(data, vec.data);
    return *this;
  }

  /**
   * Division.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator/=(const VectorizedArray &vec)
  {
    data = vec_div(data, vec.data);
    return *this;
  }

  /**
   * Load @p n_array_elements from memory into the calling class, starting at
   * the given address.
   */
  DEAL_II_ALWAYS_INLINE
  void
  load(const double *ptr)
  {
    data = vec_vsx_ld(0, ptr);
  }

  /**
   * Write the content of the calling class into memory in form of @p
   * n_array_elements to the given address.
   */
  DEAL_II_ALWAYS_INLINE
  void
  store(double *ptr) const
  {
    vec_vsx_st(data, 0, ptr);
  }

  /** @copydoc VectorizedArray<Number>::streaming_store()
   */
  DEAL_II_ALWAYS_INLINE
  void
  streaming_store(double *ptr) const
  {
    store(ptr);
  }

  /** @copydoc VectorizedArray<Number>::gather()
   */
  DEAL_II_ALWAYS_INLINE
  void
  gather(const double *base_ptr, const unsigned int *offsets)
  {
    for (unsigned int i = 0; i < 2; ++i)
      *(reinterpret_cast<double *>(&data) + i) = base_ptr[offsets[i]];
  }

  /** @copydoc VectorizedArray<Number>::scatter
   */
  DEAL_II_ALWAYS_INLINE
  void
  scatter(const unsigned int *offsets, double *base_ptr) const
  {
    for (unsigned int i = 0; i < 2; ++i)
      base_ptr[offsets[i]] = *(reinterpret_cast<const double *>(&data) + i);
  }

  /**
   * Actual data field. To be consistent with the standard layout type and to
   * enable interaction with external SIMD functionality, this member is
   * declared public.
   */
  __vector double data;

private:
  /**
   * Return the square root of this field. Not for use in user code. Use
   * sqrt(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_sqrt() const
  {
    VectorizedArray res;
    res.data = vec_sqrt(data);
    return res;
  }

  /**
   * Return the absolute value of this field. Not for use in user code. Use
   * abs(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_abs() const
  {
    VectorizedArray res;
    res.data = vec_abs(data);
    return res;
  }

  /**
   * Return the component-wise maximum of this field and another one. Not for
   * use in user code. Use max(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_max(const VectorizedArray &other) const
  {
    VectorizedArray res;
    res.data = vec_max(data, other.data);
    return res;
  }

  /**
   * Return the component-wise minimum of this field and another one. Not for
   * use in user code. Use min(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_min(const VectorizedArray &other) const
  {
    VectorizedArray res;
    res.data = vec_min(data, other.data);
    return res;
  }

  // Make a few functions friends.
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::sqrt(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::abs(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::max(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::min(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
};



template <>
class VectorizedArray<float, 4>
  : public VectorizedArrayBase<VectorizedArray<float, 4>>
{
public:
  /**
   * This gives the type of the array elements.
   */
  using value_type = float;

  /**
   * This gives the number of vectors collected in this class.
   */
  static const unsigned int n_array_elements = 4;

  /**
   * Default empty constructor, leaving the data in an uninitialized state
   * similar to float/double.
   */
  VectorizedArray() = default;

  /**
   * Construct an array with the given scalar broadcast to all lanes.
   */
  VectorizedArray(const float scalar)
  {
    this->operator=(scalar);
  }

  /**
   * This function assigns a scalar to this class.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator=(const float x)
  {
    data = vec_splats(x);

    // Some compilers believe that vec_splats sets 'x', but that's not true.
    // They then warn about setting a variable and not using it. Suppress the
    // warning by "using" the variable:
    (void)x;
    return *this;
  }

  /**
   * Access operator. The component must be between 0 and 3.
   */
  DEAL_II_ALWAYS_INLINE
  float &operator[](const unsigned int comp)
  {
    AssertIndexRange(comp, 4);
    return *(reinterpret_cast<float *>(&data) + comp);
  }

  /**
   * Constant access operator.
   */
  DEAL_II_ALWAYS_INLINE
  const float &operator[](const unsigned int comp) const
  {
    AssertIndexRange(comp, 4);
    return *(reinterpret_cast<const float *>(&data) + comp);
  }

  /**
   * Addition.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator+=(const VectorizedArray &vec)
  {
    data = vec_add(data, vec.data);
    return *this;
  }

  /**
   * Subtraction.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator-=(const VectorizedArray &vec)
  {
    data = vec_sub(data, vec.data);
    return *this;
  }

  /**
   * Multiplication.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator*=(const VectorizedArray &vec)
  {
    data = vec_mul(data, vec.data);
    return *this;
  }

  /**
   * Division.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray &
  operator/=(const VectorizedArray &vec)
  {
    data = vec_div(data, vec.data);
    return *this;
  }

  /**
   * Load @p n_array_elements from memory into the calling class, starting at
   * the given address.
   */
  DEAL_II_ALWAYS_INLINE
  void
  load(const float *ptr)
  {
    data = vec_vsx_ld(0, ptr);
  }

  /**
   * Write the content of the calling class into memory in form of @p
   * n_array_elements to the given address.
   */
  DEAL_II_ALWAYS_INLINE
  void
  store(float *ptr) const
  {
    vec_vsx_st(data, 0, ptr);
  }

  /** @copydoc VectorizedArray<Number>::streaming_store()
   */
  DEAL_II_ALWAYS_INLINE
  void
  streaming_store(float *ptr) const
  {
    store(ptr);
  }

  /** @copydoc VectorizedArray<Number>::gather()
   */
  DEAL_II_ALWAYS_INLINE
  void
  gather(const float *base_ptr, const unsigned int *offsets)
  {
    for (unsigned int i = 0; i < 4; ++i)
      *(reinterpret_cast<float *>(&data) + i) = base_ptr[offsets[i]];
  }

  /** @copydoc VectorizedArray<Number>::scatter
   */
  DEAL_II_ALWAYS_INLINE
  void
  scatter(const unsigned int *offsets, float *base_ptr) const
  {
    for (unsigned int i = 0; i < 4; ++i)
      base_ptr[offsets[i]] = *(reinterpret_cast<const float *>(&data) + i);
  }

  /**
   * Actual data field. To be consistent with the standard layout type and to
   * enable interaction with external SIMD functionality, this member is
   * declared public.
   */
  __vector float data;

private:
  /**
   * Return the square root of this field. Not for use in user code. Use
   * sqrt(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_sqrt() const
  {
    VectorizedArray res;
    res.data = vec_sqrt(data);
    return res;
  }

  /**
   * Return the absolute value of this field. Not for use in user code. Use
   * abs(x) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_abs() const
  {
    VectorizedArray res;
    res.data = vec_abs(data);
    return res;
  }

  /**
   * Return the component-wise maximum of this field and another one. Not for
   * use in user code. Use max(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_max(const VectorizedArray &other) const
  {
    VectorizedArray res;
    res.data = vec_max(data, other.data);
    return res;
  }

  /**
   * Return the component-wise minimum of this field and another one. Not for
   * use in user code. Use min(x,y) instead.
   */
  DEAL_II_ALWAYS_INLINE
  VectorizedArray
  get_min(const VectorizedArray &other) const
  {
    VectorizedArray res;
    res.data = vec_min(data, other.data);
    return res;
  }

  // Make a few functions friends.
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::sqrt(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::abs(const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::max(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
  template <typename Number2, int width2>
  friend VectorizedArray<Number2, width2>
  std::min(const VectorizedArray<Number2, width2> &,
           const VectorizedArray<Number2, width2> &);
};

#  endif // if DEAL_II_VECTORIZATION_LEVEL >=1 && defined(__ALTIVEC__) &&
         // defined(__VSX__)


/**
 * @name Arithmetic operations with VectorizedArray
 */
//@{

/**
 * Relational operator == for VectorizedArray
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE bool
operator==(const VectorizedArray<Number, width> &lhs,
           const VectorizedArray<Number, width> &rhs)
{
  for (unsigned int i = 0; i < VectorizedArray<Number, width>::n_array_elements;
       ++i)
    if (lhs[i] != rhs[i])
      return false;

  return true;
}


/**
 * Addition of two vectorized arrays with operator +.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator+(const VectorizedArray<Number, width> &u,
          const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp += v;
}

/**
 * Subtraction of two vectorized arrays with operator -.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator-(const VectorizedArray<Number, width> &u,
          const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp -= v;
}

/**
 * Multiplication of two vectorized arrays with operator *.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator*(const VectorizedArray<Number, width> &u,
          const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp *= v;
}

/**
 * Division of two vectorized arrays with operator /.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator/(const VectorizedArray<Number, width> &u,
          const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp /= v;
}

/**
 * Addition of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator+(const Number &u, const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp += v;
}

/**
 * Addition of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array in case the scalar
 * is a double (needed in order to be able to write simple code with constants
 * that are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator+(const double u, const VectorizedArray<float, width> &v)
{
  VectorizedArray<float, width> tmp = u;
  return tmp += v;
}

/**
 * Addition of a vectorized array and a scalar (expanded to a vectorized array
 * with @p n_array_elements equal entries).
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator+(const VectorizedArray<Number, width> &v, const Number &u)
{
  return u + v;
}

/**
 * Addition of a vectorized array and a scalar (expanded to a vectorized array
 * with @p n_array_elements equal entries) in case the scalar is a double
 * (needed in order to be able to write simple code with constants that are
 * usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator+(const VectorizedArray<float, width> &v, const double u)
{
  return u + v;
}

/**
 * Subtraction of a vectorized array from a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries).
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator-(const Number &u, const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp -= v;
}

/**
 * Subtraction of a vectorized array from a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries) in case the scalar is a
 * double (needed in order to be able to write simple code with constants that
 * are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator-(const double u, const VectorizedArray<float, width> &v)
{
  VectorizedArray<float, width> tmp = static_cast<float>(u);
  return tmp -= v;
}

/**
 * Subtraction of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) from a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator-(const VectorizedArray<Number, width> &v, const Number &u)
{
  VectorizedArray<Number, width> tmp = u;
  return v - tmp;
}

/**
 * Subtraction of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) from a vectorized array in case the scalar
 * is a double (needed in order to be able to write simple code with constants
 * that are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator-(const VectorizedArray<float, width> &v, const double u)
{
  VectorizedArray<float, width> tmp = static_cast<float>(u);
  return v - tmp;
}

/**
 * Multiplication of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator*(const Number &u, const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp *= v;
}

/**
 * Multiplication of a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array in case the scalar
 * is a double (needed in order to be able to write simple code with constants
 * that are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator*(const double u, const VectorizedArray<float, width> &v)
{
  VectorizedArray<float, width> tmp = static_cast<float>(u);
  return tmp *= v;
}

/**
 * Multiplication of a vectorized array and a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries).
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator*(const VectorizedArray<Number, width> &v, const Number &u)
{
  return u * v;
}

/**
 * Multiplication of a vectorized array and a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries) in case the scalar is a
 * double (needed in order to be able to write simple code with constants that
 * are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator*(const VectorizedArray<float, width> &v, const double u)
{
  return u * v;
}

/**
 * Quotient between a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator/(const Number &u, const VectorizedArray<Number, width> &v)
{
  VectorizedArray<Number, width> tmp = u;
  return tmp /= v;
}

/**
 * Quotient between a scalar (expanded to a vectorized array with @p
 * n_array_elements equal entries) and a vectorized array in case the scalar
 * is a double (needed in order to be able to write simple code with constants
 * that are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator/(const double u, const VectorizedArray<float, width> &v)
{
  VectorizedArray<float, width> tmp = static_cast<float>(u);
  return tmp /= v;
}

/**
 * Quotient between a vectorized array and a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries).
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator/(const VectorizedArray<Number, width> &v, const Number &u)
{
  VectorizedArray<Number, width> tmp = u;
  return v / tmp;
}

/**
 * Quotient between a vectorized array and a scalar (expanded to a vectorized
 * array with @p n_array_elements equal entries) in case the scalar is a
 * double (needed in order to be able to write simple code with constants that
 * are usually double numbers).
 *
 * @relatesalso VectorizedArray
 */
template <int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<float, width>
                             operator/(const VectorizedArray<float, width> &v, const double u)
{
  VectorizedArray<float, width> tmp = static_cast<float>(u);
  return v / tmp;
}

/**
 * Unary operator + on a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator+(const VectorizedArray<Number, width> &u)
{
  return u;
}

/**
 * Unary operator - on a vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline DEAL_II_ALWAYS_INLINE VectorizedArray<Number, width>
                             operator-(const VectorizedArray<Number, width> &u)
{
  // to get a negative sign, subtract the input from zero (could also
  // multiply by -1, but this one is slightly simpler)
  return VectorizedArray<Number, width>() - u;
}

/**
 * Output operator for vectorized array.
 *
 * @relatesalso VectorizedArray
 */
template <typename Number, int width>
inline std::ostream &
operator<<(std::ostream &out, const VectorizedArray<Number, width> &p)
{
  constexpr unsigned int n = VectorizedArray<Number, width>::n_array_elements;
  for (unsigned int i = 0; i < n - 1; ++i)
    out << p[i] << ' ';
  out << p[n - 1];

  return out;
}

//@}

/**
 * @name Ternary operations on VectorizedArray
 */
//@{


/**
 * enum class encoding binary operations for a component-wise comparison of
 * VectorizedArray data types.
 *
 * @note In case of SIMD vecorization (sse, avx, av512) we select the
 * corresponding ordered, non-signalling (<code>OQ</code>) variants.
 */
enum class SIMDComparison : int
{
#if DEAL_II_COMPILER_VECTORIZATION_LEVEL >= 2 && defined(__AVX__)
  equal                 = _CMP_EQ_OQ,
  not_equal             = _CMP_NEQ_OQ,
  less_than             = _CMP_LT_OQ,
  less_than_or_equal    = _CMP_LE_OQ,
  greater_than          = _CMP_GT_OQ,
  greater_than_or_equal = _CMP_GE_OQ
#else
  equal,
  not_equal,
  less_than,
  less_than_or_equal,
  greater_than,
  greater_than_or_equal
#endif
};


/**
 * Computes the vectorized equivalent of the following ternary operation:
 * @code
 *   (left OP right) ? true_value : false_value
 * @endcode
 * where <code>OP</code> is a binary operator (such as <code>=</code>,
 * <code>!=</code>, <code><</code>, <code><=</code>, <code>></code>, and
 * <code>>=</code>).
 *
 * Such a computational idiom is useful as an alternative to branching
 * whenever the control flow itself would depend on (computed) data. For
 * example, in case of a scalar data type the statement
 * <code>(left < right) ? true_value : false_value</code>
 * could have been also implementd using an <code>if</code>-statement:
 * @code
 * if (left < right)
 *     result = true_value;
 * else
 *     result = false_value;
 * @endcode
 * This, however, is fundamentally impossible in case of vectorization
 * because different decisions will be necessary on different vector entries
 * (lanes) and
 * the first variant (based on a ternary operator) has to be used instead:
 * @code
 *   result = compare_and_apply_mask<SIMDComparison::less_than>
 *     (left, right, true_value, false_value);
 * @endcode
 * Some more illustrative examples (that are less efficient than the
 * dedicated <code>std::max</code> and <code>std::abs</code> overloads):
 * @code
 *   VectorizedArray<double> left;
 *   VectorizedArray<double> right;
 *
 *   // std::max
 *   const auto maximum = compare_and_apply_mask<SIMDComparison::greater_than>
 *     (left, right, left, right);
 *
 *   // std::abs
 *   const auto absolute = compare_and_apply_mask<SIMDComparison::less_than>
 *     (left, VectorizedArray<double>(0.), -left, left);
 * @endcode
 *
 * More precisely, this function first computes a (boolean) mask that is
 * the result of a binary operator <code>OP</code> applied to all elements
 * of the VectorizedArray arguments @p left and @p right. The mask is then
 * used to either select the corresponding component of @p true_value (if
 * the binary operation equates to true), or @p false_value. The binary
 * operator is encoded via the SIMDComparison template argument
 * @p predicate.
 *
 * In order to ease with generic programming approaches, the function
 * provides overloads for all VectorizedArray<Number> variants as well as
 * generic POD types such as double and float.
 *
 * @note For this function to work the binary operation has to be encoded
 * via a SIMDComparison template argument @p predicate. Depending on it
 * appropriate low-level machine instructions are generated replacing the
 * call to compare_and_apply_mask. This also explains why @p predicate is a
 * compile-time constant template parameter and not a constant function
 * argument. In order to be able to emit the correct low-level instruction,
 * the compiler has to know the comparison at compile time.
 */
template <SIMDComparison predicate, typename Number>
DEAL_II_ALWAYS_INLINE inline Number
compare_and_apply_mask(const Number &left,
                       const Number &right,
                       const Number &true_value,
                       const Number &false_value)
{
  bool mask;
  switch (predicate)
    {
      case SIMDComparison::equal:
        mask = (left == right);
        break;
      case SIMDComparison::not_equal:
        mask = (left != right);
        break;
      case SIMDComparison::less_than:
        mask = (left < right);
        break;
      case SIMDComparison::less_than_or_equal:
        mask = (left <= right);
        break;
      case SIMDComparison::greater_than:
        mask = (left > right);
        break;
      case SIMDComparison::greater_than_or_equal:
        mask = (left >= right);
        break;
    }

  return mask ? true_value : false_value;
}


/**
 * Specialization of above function for the non-vectorized
 * VectorizedArray<Number, 1> variant.
 */
template <SIMDComparison predicate, typename Number>
DEAL_II_ALWAYS_INLINE inline VectorizedArray<Number, 1>
compare_and_apply_mask(const VectorizedArray<Number, 1> &left,
                       const VectorizedArray<Number, 1> &right,
                       const VectorizedArray<Number, 1> &true_value,
                       const VectorizedArray<Number, 1> &false_value)
{
  VectorizedArray<Number, 1> result;
  result.data = compare_and_apply_mask<predicate, Number>(left.data,
                                                          right.data,
                                                          true_value.data,
                                                          false_value.data);
  return result;
}

//@}


DEAL_II_NAMESPACE_CLOSE

/**
 * Implementation of functions from cmath on VectorizedArray. These functions
 * do not reside in the dealii namespace in order to ensure a similar
 * interface as for the respective functions in cmath. Instead, call them
 * using std::sin.
 */
namespace std
{
  /**
   * Compute the sine of a vectorized data field. The result is returned as
   * vectorized array in the form <tt>{sin(x[0]), sin(x[1]), ...,
   * sin(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  sin(const ::dealii::VectorizedArray<Number, width> &x)
  {
    // put values in an array and later read in that array with an unaligned
    // read. This should save some instructions as compared to directly
    // setting the individual elements and also circumvents a compiler
    // optimization bug in gcc-4.6 with SSE2 (see also deal.II developers list
    // from April 2014, topic "matrix_free/step-48 Test").
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::sin(x[i]);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the cosine of a vectorized data field. The result is returned as
   * vectorized array in the form <tt>{cos(x[0]), cos(x[1]), ...,
   * cos(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  cos(const ::dealii::VectorizedArray<Number, width> &x)
  {
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::cos(x[i]);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the tangent of a vectorized data field. The result is returned
   * as vectorized array in the form <tt>{tan(x[0]), tan(x[1]), ...,
   * tan(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  tan(const ::dealii::VectorizedArray<Number, width> &x)
  {
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::tan(x[i]);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the exponential of a vectorized data field. The result is
   * returned as vectorized array in the form <tt>{exp(x[0]), exp(x[1]), ...,
   * exp(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  exp(const ::dealii::VectorizedArray<Number, width> &x)
  {
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::exp(x[i]);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the natural logarithm of a vectorized data field. The result is
   * returned as vectorized array in the form <tt>{log(x[0]), log(x[1]), ...,
   * log(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  log(const ::dealii::VectorizedArray<Number, width> &x)
  {
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::log(x[i]);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the square root of a vectorized data field. The result is
   * returned as vectorized array in the form <tt>{sqrt(x[0]), sqrt(x[1]),
   * ..., sqrt(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  sqrt(const ::dealii::VectorizedArray<Number, width> &x)
  {
    return x.get_sqrt();
  }



  /**
   * Raises the given number @p x to the power @p p for a vectorized data
   * field. The result is returned as vectorized array in the form
   * <tt>{pow(x[0],p), pow(x[1],p), ..., pow(x[n_array_elements-1],p)}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  pow(const ::dealii::VectorizedArray<Number, width> &x, const Number p)
  {
    Number values[::dealii::VectorizedArray<Number, width>::n_array_elements];
    for (unsigned int i = 0;
         i < dealii::VectorizedArray<Number, width>::n_array_elements;
         ++i)
      values[i] = std::pow(x[i], p);
    ::dealii::VectorizedArray<Number, width> out;
    out.load(&values[0]);
    return out;
  }



  /**
   * Compute the absolute value (modulus) of a vectorized data field. The
   * result is returned as vectorized array in the form <tt>{abs(x[0]),
   * abs(x[1]), ..., abs(x[n_array_elements-1])}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  abs(const ::dealii::VectorizedArray<Number, width> &x)
  {
    return x.get_abs();
  }



  /**
   * Compute the componentwise maximum of two vectorized data fields. The
   * result is returned as vectorized array in the form <tt>{max(x[0],y[0]),
   * max(x[1],y[1]), ...}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  max(const ::dealii::VectorizedArray<Number, width> &x,
      const ::dealii::VectorizedArray<Number, width> &y)
  {
    return x.get_max(y);
  }



  /**
   * Compute the componentwise minimum of two vectorized data fields. The
   * result is returned as vectorized array in the form <tt>{min(x[0],y[0]),
   * min(x[1],y[1]), ...}</tt>.
   *
   * @relatesalso VectorizedArray
   */
  template <typename Number, int width>
  inline ::dealii::VectorizedArray<Number, width>
  min(const ::dealii::VectorizedArray<Number, width> &x,
      const ::dealii::VectorizedArray<Number, width> &y)
  {
    return x.get_min(y);
  }

} // namespace std

#endif
