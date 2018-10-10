#ifndef OMEGA_H_LOOP_HPP
#define OMEGA_H_LOOP_HPP

#include <Omega_h_defines.hpp>
#include <Omega_h_stack.hpp>

#ifdef OMEGA_H_USE_KOKKOSCORE
#include <Omega_h_kokkos.hpp>
#else
#include <Omega_h_reduce.hpp>
#if defined(OMEGA_H_USE_CUDA)
#include <thrust/transform_scan.h>
#include <thrust/execution_policy.h>
#elif defined(OMEGA_H_USE_OPENMP)
#include <omp.h>
#endif
#endif

namespace Omega_h {

template <typename T>
void parallel_scan(LO n, T f, char const* name = "") {
#ifdef OMEGA_H_USE_KOKKOSCORE
  if (n > 0) Kokkos::parallel_scan(name, policy(n), f);
#else
  using VT = typename T::value_type;
  begin_code(name);
  VT update;
  f.init(update);
  for (LO i = 0; i < n; ++i) f(i, update, true);
  end_code();
#endif
}

#if defined(OMEGA_H_USE_CUDA)

template <typename InputIterator, typename OutputIterator, typename BinaryOp, typename UnaryOp>
OutputIterator transform_inclusive_scan(
    InputIterator first,
    InputIterator last,
    OutputIterator result,
    BinaryOp op,
    UnaryOp&& transform)
{
  return thrust::transform_inclusive_scan(
      thrust::device, first, last, result, native_op(transform), native_op(op));
}

#elif defined(OMEGA_H_USE_OPENMP)

template <typename InputIterator, typename OutputIterator, typename BinaryOp, typename UnaryOp>
OutputIterator transform_inclusive_scan(
    InputIterator first,
    InputIterator last,
    OutputIterator result,
    BinaryOp op,
    UnaryOp&& transform)
{
  constexpr int max_num_threads = 512;
  T thread_sums[max_num_threads];
  auto const n = last - first;
  Omega_h::entering_parallel = true;
  auto const transform_local = std::move(transform);
  Omega_h::entering_parallel = false;
#pragma omp parallel
  {
    int const num_threads = omp_get_num_threads();
    int const thread_num = omp_get_thread_num();
    auto const quotient = n / num_threads;
    auto const remainder = n % num_threads;
    auto const begin_i = (thread_num > remainder) ?
      (quotient * thread_num + remainder) :
      ((quotient + 1) * thread_num);
    auto const end_i = (thread_num >= remainder) ?
      (begin_i + quotient) : (begin_i + quotient + 1);
    auto thread_sum = transform_local(*first);
    for (auto i = begin_i + 1; i < end_i; ++i) {
      thread_sum = op(std::move(thread_sum), transform_local(first[i]));
    }
    thread_sums[thread_num] = std::move(thread_sum);
#pragma omp barrier
    thread_sum = thread_sums[0];
    for (int i = 1; i < thread_num; ++i) {
      thread_sum = op(std::move(thread_sum), thread_sums[i]);
    }
    for (auto i = begin_i; i < end_i; ++i) {
      thread_sum = op(std::move(thread_sum), transform_local(first[i]));
      result[i] = thread_sum;
    }
  }
  return result + n;
}

#else

template <typename InputIterator, typename OutputIterator, typename BinaryOp, typename UnaryOp>
OutputIterator transform_inclusive_scan(
    InputIterator first,
    InputIterator last,
    OutputIterator result,
    BinaryOp op,
    UnaryOp&& transform)
{
  Omega_h::entering_parallel = true;
  auto const transform_local = std::move(transform);
  Omega_h::entering_parallel = false;
  auto value = *first;
  ++first;
  for (; first != last; ++first) {
    value = op(std::move(value), transform_local(*first));
    *result = value;
    ++result;
  }
  return result;
}

#endif

}

#endif
