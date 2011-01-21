#pragma once
#include "../base/base.hpp"
#include "../base/stl_add.hpp"
#include "../base/logging.hpp"

#include "../std/iterator.hpp"
#include "../std/algorithm.hpp"
#include "../std/utility.hpp"
#include "../std/vector.hpp"

// Polyline simplification algorithms.
//
// SimplifyXXX() should be used to simplify polyline for a given epsilon.
// (!) They do not include the last point to the simplification, the calling side should do it.

namespace impl
{

///@name This functions take input range NOT like STL do: [first, last].
//@{
template <typename DistanceF, typename IterT>
pair<double, IterT> MaxDistance(IterT first, IterT last)
{
  pair<double, IterT> res(0.0, last);
  if (distance(first, last) <= 1)
    return res;

  DistanceF distanceF(*first, *last);
  for (IterT i = first + 1; i != last; ++i)
  {
    double const dist = distanceF(*i);
    if (dist > res.first)
    {
      res.first = dist;
      res.second = i;
    }
  }
  return res;
}

// Actual SimplifyDP implementation.
template <typename DistanceF, typename IterT, typename OutT>
void SimplifyDP(IterT first, IterT last, double epsilon, OutT & out)
{
  pair<double, IterT> maxDist = impl::MaxDistance<DistanceF>(first, last);
  if (maxDist.second == last || maxDist.first < epsilon)
  {
    out(*last);
  }
  else
  {
    SimplifyDP<DistanceF>(first, maxDist.second, epsilon, out);
    SimplifyDP<DistanceF>(maxDist.second, last, epsilon, out);
  }
}
//@}

struct SimplifyOptimalRes
{
  SimplifyOptimalRes() : m_PointCount(-1) {}
  SimplifyOptimalRes(int32_t nextPoint, uint32_t pointCount)
    : m_NextPoint(nextPoint), m_PointCount(pointCount) {}

  int32_t m_NextPoint;
  uint32_t m_PointCount;
};

}

// Douglas-Peucker algorithm for STL-like range [first, last).
// Iteratively includes the point with max distance form the current simplification.
// Average O(n log n), worst case O(n^2).
template <typename DistanceF, typename IterT, typename OutT>
void SimplifyDP(IterT first, IterT last, double epsilon, OutT out)
{
  if (first != last)
  {
    out(*first);
    impl::SimplifyDP<DistanceF>(first, last-1, epsilon, out);
  }
}

// Dynamic programming near-optimal simplification.
// Uses O(n) additional memory.
// Worst case O(n^3) performance, average O(n*k^2), where k is kMaxFalseLookAhead - parameter,
// which limits the number of points to try, that produce error > epsilon.
// Essentially, it's a trade-off between optimality and performance.
// Values around 20 - 200 are reasonable.
template <typename DistanceF, typename IterT, typename OutT>
void SimplifyNearOptimal(int kMaxFalseLookAhead, IterT first, IterT last, double epsilon, OutT out)
{
  int32_t const n = last - first + 1;
  if (n <= 2)
  {
    out(*first);
    return;
  }

  vector<impl::SimplifyOptimalRes> F(n);
  F[n - 1] = impl::SimplifyOptimalRes(n, 1);
  for (int32_t i = n - 2; i >= 0; --i)
  {
    for (int32_t falseCount = 0, j = i + 1; j < n && falseCount < kMaxFalseLookAhead; ++j)
    {
      uint32_t const newPointCount = F[j].m_PointCount + 1;
      if (newPointCount < F[i].m_PointCount)
      {
          if (impl::MaxDistance<DistanceF>(first + i, first + j).first < epsilon)
          {
            F[i].m_NextPoint = j;
            F[i].m_PointCount = newPointCount;
          }
          else
          {
            ++falseCount;
          }
      }
    }
  }

  for (int32_t i = 0; i < n - 1; i = F[i].m_NextPoint)
    out(*(first + i));
}


// Additional points filter to use in simplification.
// SimplifyDP can produce points that define degenerate triangle.
template <class DistanceF, class PointT>
class AccumulateSkipSmallTrg
{
  vector<PointT> & m_vec;
  double m_eps;

public:
  AccumulateSkipSmallTrg(vector<PointT> & vec, double eps)
    : m_vec(vec), m_eps(eps)
  {
  }

  void operator() (PointT const & p) const
  {
    // remove points while they make linear triangle with p
    size_t count;
    while ((count = m_vec.size()) >= 2)
    {
      if (DistanceF(m_vec[count-2], p)(m_vec[count-1]) < m_eps)
        m_vec.pop_back();
      else
        break;
    }

    m_vec.push_back(p);
  }
};
