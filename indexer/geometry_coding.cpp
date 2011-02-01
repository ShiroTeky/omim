#include "geometry_coding.hpp"

#include "../../geometry/distance.hpp"

#include "../base/assert.hpp"
#include "../base/stl_add.hpp"

#include "../std/complex.hpp"
#include "../std/vector.hpp"


namespace
{
  template <typename T>
  inline m2::PointU ClampPoint(m2::PointU const & maxPoint, m2::Point<T> const & point)
  {
    return m2::PointU(my::clamp(static_cast<uint32_t>(point.x), 0, maxPoint.x),
                      my::clamp(static_cast<uint32_t>(point.y), 0, maxPoint.y));
  }
}

m2::PointU PredictPointInPolyline(m2::PointU const & maxPoint,
                                  m2::PointU const & p1,
                                  m2::PointU const & p2)
{
  // return ClampPoint(maxPoint, m2::PointI64(p1) + m2::PointI64(p1) - m2::PointI64(p2));
  // return ClampPoint(maxPoint, m2::PointI64(p1) + (m2::PointI64(p1) - m2::PointI64(p2)) / 2);
  return ClampPoint(maxPoint, m2::PointD(p1) + (m2::PointD(p1) - m2::PointD(p2)) / 2);
}

m2::PointU PredictPointInPolyline(m2::PointU const & maxPoint,
                                  m2::PointU const & p1,
                                  m2::PointU const & p2,
                                  m2::PointU const & p3)
{
  CHECK_NOT_EQUAL(p2, p3, ());

  complex<double> const c1(p1.x, p1.y);
  complex<double> const c2(p2.x, p2.y);
  complex<double> const c3(p3.x, p3.y);
  complex<double> const d = (c1 - c2) / (c2 - c3);
  complex<double> const c0 = c1 + (c1 - c2) * polar(0.5, 0.5 * arg(d));

  /*
  complex<double> const c1(p1.x, p1.y);
  complex<double> const c2(p2.x, p2.y);
  complex<double> const c3(p3.x, p3.y);
  complex<double> const d = (c1 - c2) / (c2 - c3);
  complex<double> const c01 = c1 + (c1 - c2) * polar(0.5, arg(d));
  complex<double> const c02 = c1 + (c1 - c2) * complex<double>(0.5, 0.0);
  complex<double> const c0 = (c01 + c02) * complex<double>(0.5, 0.0);
  */

  return ClampPoint(maxPoint, m2::PointD(c0.real(), c0.imag()));
}

namespace geo_coding
{
  bool TestDecoding(InPointsT const & points,
                    m2::PointU const & basePoint,
                    m2::PointU const & maxPoint,
                    DeltasT const & deltas,
                    void (* fnDecode)(DeltasT const & deltas,
                                      m2::PointU const & basePoint,
                                      m2::PointU const & maxPoint,
                                      OutPointsT & points))
  {
    vector<m2::PointU> decoded;
    decoded.reserve(points.size());
    fnDecode(deltas, basePoint, maxPoint, decoded);
    ASSERT_EQUAL(points, decoded, (basePoint, maxPoint));
    return true;
  }

void EncodePolylinePrev1(InPointsT const & points,
                         m2::PointU const & basePoint,
                         m2::PointU const & /*maxPoint*/,
                         DeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodeDelta(points[0], basePoint));
    for (size_t i = 1; i < count; ++i)
      deltas.push_back(EncodeDelta(points[i], points[i-1]));
  }

  ASSERT(TestDecoding(points, basePoint, m2::PointU(), deltas, &DecodePolylinePrev1), ());
}

void DecodePolylinePrev1(DeltasT const & deltas,
                         m2::PointU const & basePoint,
                         m2::PointU const & /*maxPoint*/,
                         OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    points.push_back(DecodeDelta(deltas[0], basePoint));
    for (size_t i = 1; i < count; ++i)
      points.push_back(DecodeDelta(deltas[i], points.back()));
  }
}

void EncodePolylinePrev2(InPointsT const & points,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         DeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodeDelta(points[0], basePoint));
    if (count > 1)
    {
      deltas.push_back(EncodeDelta(points[1], points[0]));
      for (size_t i = 2; i < count; ++i)
        deltas.push_back(EncodeDelta(points[i],
                                     PredictPointInPolyline(maxPoint, points[i-1], points[i-2])));
    }
  }

  ASSERT(TestDecoding(points, basePoint, maxPoint, deltas, &DecodePolylinePrev2), ());
}

void DecodePolylinePrev2(DeltasT const & deltas,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    points.push_back(DecodeDelta(deltas[0], basePoint));
    if (count > 1)
    {
      points.push_back(DecodeDelta(deltas[1], points.back()));
      for (size_t i = 2; i < count; ++i)
      {
        size_t const n = points.size();
        points.push_back(DecodeDelta(deltas[i],
                                     PredictPointInPolyline(maxPoint, points[n-1], points[n-2])));
      }
    }
  }
}

void EncodePolylinePrev3(InPointsT const & points,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         DeltasT & deltas)
{
  ASSERT_LESS_OR_EQUAL(basePoint.x, maxPoint.x, (basePoint, maxPoint));
  ASSERT_LESS_OR_EQUAL(basePoint.y, maxPoint.y, (basePoint, maxPoint));

  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodeDelta(points[0], basePoint));
    if (count > 1)
    {
      deltas.push_back(EncodeDelta(points[1], points[0]));
      if (count > 2)
      {
        m2::PointU const prediction = PredictPointInPolyline(maxPoint, points[1], points[0]);
        deltas.push_back(EncodeDelta(points[2], prediction));
        for (size_t i = 3; i < count; ++i)
        {
          m2::PointU const prediction =
              PredictPointInPolyline(maxPoint, points[i-1], points[i-2], points[i-3]);
          deltas.push_back(EncodeDelta(points[i], prediction));
        }
      }
    }
  }

  ASSERT(TestDecoding(points, basePoint, maxPoint, deltas, &DecodePolylinePrev3), ());
}

void DecodePolylinePrev3(DeltasT const & deltas,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         OutPointsT & points)
{
  ASSERT_LESS_OR_EQUAL(basePoint.x, maxPoint.x, (basePoint, maxPoint));
  ASSERT_LESS_OR_EQUAL(basePoint.y, maxPoint.y, (basePoint, maxPoint));

  size_t const count = deltas.size();
  if (count> 0)
  {
    points.push_back(DecodeDelta(deltas[0], basePoint));
    if (count > 1)
    {
      m2::PointU const pt0 = points.back();
      points.push_back(DecodeDelta(deltas[1], pt0));
      if (count > 2)
      {
        points.push_back(DecodeDelta(deltas[2],
                                     PredictPointInPolyline(maxPoint, points.back(), pt0)));
        for (size_t i = 3; i < count; ++i)
        {
          size_t const n = points.size();
          m2::PointU const prediction =
              PredictPointInPolyline(maxPoint, points[n-1], points[n-2], points[n-3]);
          points.push_back(DecodeDelta(deltas[i], prediction));
        }
      }
    }
  }
}


m2::PointU PredictPointInTriangle(m2::PointU const & maxPoint,
                                  m2::PointU const & p1,
                                  m2::PointU const & p2,
                                  m2::PointU const & p3)
{
  // parallelogramm prediction
  return ClampPoint(maxPoint, p2 + p3 - p1);
}

void EncodeTriangleStrip(InPointsT const & points,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         DeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    ASSERT_GREATER(count, 2, ());

    deltas.push_back(EncodeDelta(points[0], basePoint));
    deltas.push_back(EncodeDelta(points[1], points[0]));
    deltas.push_back(EncodeDelta(points[2], points[1]));

    for (size_t i = 3; i < count; ++i)
    {
      m2::PointU const prediction =
          PredictPointInTriangle(maxPoint, points[i-1], points[i-2], points[i-3]);
      deltas.push_back(EncodeDelta(points[i], prediction));
    }
  }
}

void DecodeTriangleStrip(DeltasT const & deltas,
                         m2::PointU const & basePoint,
                         m2::PointU const & maxPoint,
                         OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    ASSERT_GREATER(count, 2, ());

    points.push_back(DecodeDelta(deltas[0], basePoint));
    points.push_back(DecodeDelta(deltas[1], points.back()));
    points.push_back(DecodeDelta(deltas[2], points.back()));

    for (size_t i = 3; i < count; ++i)
    {
      size_t const n = points.size();
      m2::PointU const prediction =
          PredictPointInTriangle(maxPoint, points[n-1], points[n-2], points[n-3]);
      points.push_back(DecodeDelta(deltas[i], prediction));
    }
  }
}

}
