﻿
#pragma once

#include <string>

#include "geometry/r3_element.hpp"

namespace principia {
namespace geometry {
namespace internal_rp2_element {

// An |RP2Element| is an element of ℝP², the real projective plane.  |Scalar|
// must be a 1-dimensional vector space over ℝ, typically represented by
// |Quantity| or |double|.
template<typename Scalar>
class RP2Element {
 public:
  // Constructs a point from a set of homogeneous coordinates.  The
  // coordinates may not all be zero.
  explicit RP2Element(Scalar const& x, Scalar const& y, double z);

  // Returns true if and only if the point is at infinity.
  bool is_at_infinity() const;

  // Returns the Euclidean (inhomogeneous) coordinates of the point.  May be
  // infinities.
  Scalar const x() const;
  Scalar const y() const;

 private:
  Scalar x_;
  Scalar y_;
  double z_;

  template<typename S>
  friend bool operator==(RP2Element<S> const& left,
                         RP2Element<S> const& right);
  template<typename S>
  friend bool operator!=(RP2Element<S> const& left,
                         RP2Element<S> const& right);
  template<typename Scalar>
  friend std::string DebugString(RP2Element<Scalar> const& rp2_element);
};

// TODO(phl): Improve these operators using |std::fma| so that they define a
// proper equivalence.
template<typename Scalar>
bool operator==(RP2Element<Scalar> const& left,
                RP2Element<Scalar> const& right);
template<typename Scalar>
bool operator!=(RP2Element<Scalar> const& left,
                RP2Element<Scalar> const& right);

template<typename Scalar>
std::string DebugString(RP2Element<Scalar> const& rp2_element);

template<typename Scalar>
std::ostream& operator<<(std::ostream& os,
                         RP2Element<Scalar> const& rp2_element);

}  // namespace internal_rp2_element
}  // namespace geometry
}  // namespace principia

#include "geometry/rp2_element_body.hpp"
