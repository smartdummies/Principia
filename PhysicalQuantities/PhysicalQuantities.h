// PhysicalQuantities.h

#pragma once

namespace PhysicalQuantities {
template<int LengthExponent, int MassExponent, int TimeExponent,
         int CurrentExponent, int TemperatureExponent, int AmountExponent,
         int LuminousIntensityExponent, int WindingExponent,
         int WrappingExponent>
struct Dimensions;
template<typename D> class Quantity;
typedef Dimensions<0, 0, 0, 0, 0, 0, 0, 0, 0> NoDimensions;
#pragma region Base quantities
typedef Quantity<NoDimensions> Dimensionless;
typedef Quantity<Dimensions<1, 0, 0, 0, 0, 0, 0, 0, 0>> Length;
typedef Quantity<Dimensions<0, 1, 0, 0, 0, 0, 0, 0, 0>> Mass;
typedef Quantity<Dimensions<0, 0, 1, 0, 0, 0, 0, 0, 0>> Time;
typedef Quantity<Dimensions<0, 0, 0, 1, 0, 0, 0, 0, 0>> Current;
typedef Quantity<Dimensions<0, 0, 0, 0, 1, 0, 0, 0, 0>> Temperature;
typedef Quantity<Dimensions<0, 0, 0, 0, 0, 1, 0, 0, 0>> Amount;
typedef Quantity<Dimensions<0, 0, 0, 0, 0, 0, 1, 0, 0>> LuminousIntensity;
// Nonstandard; winding is a dimensionless quantity counting cycles, in order to
// strongly type the distinction between Hz = cycle/s and rad/s; wrapping is a
// quantity counting globes, in order to strongly type the distinction between
// 1 lm = 1 cd*sr/globe = 1 cd/globe and 1 cd (or between 1 W/globe and 1 W/sr).
typedef Quantity<Dimensions<0, 0, 0, 0, 0, 0, 0, 1, 0>> Winding;
typedef Quantity<Dimensions<0, 0, 0, 0, 0, 0, 0, 0, 1>> Wrapping;
#pragma endregion
template<typename Left, typename Right> struct ProductGenerator;
template<typename Left, typename Right> struct QuotientGenerator;
template<typename Left, typename Right>
using Quotient = typename QuotientGenerator<Left, Right>::ResultType;
template<typename Left, typename Right>
using Product = typename ProductGenerator<Left, Right>::ResultType;
template<typename Q> using Inverse = Quotient<Dimensionless, Q>;

Length              Metres(double const);
Mass                Kilograms(double const);
Time                Seconds(double const);
Current             Amperes(double const);
Temperature         Kelvins(double const);
Amount              Moles(double const);
LuminousIntensity   Candelas(double const);
Winding             Cycles(double const);
Wrapping            Globes(double const);

template<typename D>
class Quantity {
 public:
  Quantity() = default;
  typedef typename D Dimensions;
  friend Quantity<D> operator*(Dimensionless const& left,
                               Quantity<D> const& right) {
    return Quantity<D>(left.Value() * right.magnitude_);
  };
 private:
  explicit Quantity(double const magnitude) : magnitude_(magnitude) {}
  double   magnitude_;

  friend Length            Metres(double const);
  friend Mass              Kilograms(double const);
  friend Time              Seconds(double const);
  friend Current           Amperes(double const);
  friend Temperature       Kelvins(double const);
  friend Amount            Moles(double const);
  friend LuminousIntensity Candelas(double const);
  friend Winding           Cycles(double const);
  friend Wrapping          Globes(double const);
  template<typename D>
  friend Quantity<D> operator+(Quantity<D> const&);
  template<typename D> 
  friend Quantity<D> operator-(Quantity<D> const&);
  template<typename D>
  friend Quantity<D> operator+(Quantity<D> const&, Quantity<D> const&);
  template<typename D> 
  friend Quantity<D> operator-(Quantity<D> const&, Quantity<D> const&);
  template<typename DLeft, typename DRight>
  friend Product<typename Quantity<DLeft>,
                 typename Quantity<DRight>> operator*(Quantity<DLeft> const&,
                                                      Quantity<DRight> const&);
  template<typename DLeft, typename DRight>
  friend Quotient<typename Quantity<DLeft>,
                  typename Quantity<DRight>> operator/(Quantity<DLeft> const&, 
                                                       Quantity<DRight> const&);
};

template<>
class Quantity<NoDimensions> {
public:
  typedef NoDimensions Dimensions;
  Quantity() = delete;
  Quantity(double const magnitude) : magnitude_(magnitude) {}
  double Value() const;
private:
  double magnitude_;

  template<typename D>
  friend Quantity<D> operator+(Quantity<D> const&);
  template<typename D>
  friend Quantity<D> operator-(Quantity<D> const&);
  template<typename D>
  friend Quantity<D> operator+(Quantity<D> const&, Quantity<D> const&);
  template<typename D>
  friend Quantity<D> operator-(Quantity<D> const&, Quantity<D> const&);
  template<typename DLeft, typename DRight>
  friend Product<typename Quantity<DLeft>,
    typename Quantity<DRight>> operator*(Quantity<DLeft> const&,
    Quantity<DRight> const&);
  template<typename DLeft, typename DRight>
  friend Quotient<typename Quantity<DLeft>,
    typename Quantity<DRight>> operator/(Quantity<DLeft> const&,
    Quantity<DRight> const&);
};

template<typename D>
void operator+=(Quantity<D>&, Quantity<D> const&);
template<typename D>
inline void operator-=(Quantity<D>&, Quantity<D> const&);
template<typename D>
inline void operator*=(Quantity<D>&, Dimensionless const&);
template<typename D>
inline void operator/=(Quantity<D>&, Dimensionless const&);

template<typename Q>
class Unit {
public:
  explicit Unit(Q const& value) : value_(value) {};
  template<typename Q> friend Q operator*(Dimensionless const&, Unit<Q> const&);
  template<typename Q_Left, typename Q_Right>
  friend Unit<Product<Q_Left, Q_Right>> operator*(Unit<Q_Left> const&,
                                                  Unit<Q_Right> const&);
  template<typename Q_Left, typename Q_Right>
  friend Unit<Quotient<Q_Left, Q_Right>> operator/(Unit<Q_Left> const&,
                                                   Unit<Q_Right> const&);
private:
  Q const value_;
};
#include "PhysicalQuantities-body-inl.h"
}
