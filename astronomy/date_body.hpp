﻿
#pragma once

#include "astronomy/date.hpp"

#include <array>
#include <cstdint>

#include "astronomy/epoch.hpp"
#include "geometry/named_quantities.hpp"
#include "glog/logging.h"
#include "quantities/si.hpp"

// A macro to allow glog checking within C++11 constexpr code.  If |condition|
// is true, evaluates to |expression|.  Otherwise, results in a CHECK failure at
// runtime and a compilation error due to a call to non-constexpr code at
// compile time.
#define CHECKING(condition, expression) \
  ((condition) ? (expression) : (CHECK(condition), (expression)))

namespace principia {
namespace astronomy {
namespace internal_date {

using quantities::si::Day;
using quantities::si::Second;

// The following is horrendous in part because written in C++11, thus
// functionally; it should perhaps be reworked once MSVC offers proper C++14
// constexpr.

// Calendar classes.

class Date {
 public:
  static constexpr Date YYYYMMDD(std::int64_t const digits);
  static constexpr Date YYYYwwD(std::int64_t const digits);
  static constexpr Date YYYYDDD(std::int64_t const digits);

  static constexpr Date Calendar(int const year,
                                 int const month,
                                 int const day);
  static constexpr Date Ordinal(int const year, int const day);
  static constexpr Date Week(int const year, int const week, int const day);

  constexpr int year() const;
  constexpr int month() const;
  constexpr int day() const;

  constexpr int ordinal() const;

  constexpr Date next_day() const;

 private:
  constexpr Date(int const year,
                 int const month,
                 int const day);

  int const year_;
  int const month_;
  int const day_;
};

class Time {
 public:
  static constexpr Time hhmmss_ns(int const hhmmss, int ns);

  constexpr int hour() const;
  constexpr int minute() const;
  constexpr int second() const;
  constexpr int nanosecond() const;

  constexpr bool is_leap_second() const;
  // Whether |*this| is 24:00:00.
  constexpr bool is_end_of_day() const;

 private:
  constexpr Time(int const hour,
                 int const minute,
                 int const second,
                 int const nanosecond);

  // Checks that this represents a valid time of day as per ISO 8601, thus
  // that the components are in the normal range, or that the object represents
  // a time in a leap second, or that it represents the end of the day.
  constexpr Time const& checked() const;

  int const hour_;
  int const minute_;
  int const second_;
  int const nanosecond_;
};

class DateTime {
 public:
  constexpr Date const& date() const;
  constexpr Time const& time() const;

  // If |time()| is 24:00:00, returns an equivalent DateTime where midnight is
  // expressed as 00:00:00 on the next day; otherwise, returns |*this|.
  constexpr DateTime normalized_end_of_day() const;

 private:
  constexpr DateTime(Date const date, Time const time);

  // Checks that |time| does not represent a leap second unless |date| is the
  // last day of June, December, March, or September.
  constexpr DateTime const& checked() const;

  Date const date_;
  Time const time_;

  friend constexpr DateTime operator""_DateTime(char const* str,
                                                std::size_t size);
};

// Arithmetico-calendrical utility functions.

constexpr std::array<int, 12> non_leap_year_month_lengths{
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

constexpr bool is_gregorian_leap_year(int const year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

constexpr int gregorian_year_length(int const year) {
  return is_gregorian_leap_year(year) ? 366 : 365;
}

constexpr int month_length(int year, int month) {
  return (is_gregorian_leap_year(year) && month == 2)
             ? 29
             : non_leap_year_month_lengths[month - 1];
}

// The result of |mod| has the same sign as |divisor| (same convention as Ada,
// ISO Prolog, Haskell, etc.), whereas the result of |dividend % divisor| has
// the same sign as |dividend| (like |rem| in the aforementioned languages).
constexpr int mod(int const dividend, int const divisor) {
  return ((dividend % divisor) + divisor) % divisor;
}

// Similar to Mathematica's |Mod|: the result is congruent to |dividend| modulo
// |divisor|, and lies in [offset, divisor + offset[ if divisor > 0, and in
// ]divisor + offset, offset] otherwise.
constexpr int mod(int const dividend, int const divisor, int const offset) {
  return mod(dividend - offset, divisor) + offset;
}

// Result in [1, 7], 1 is Monday.
constexpr int day_of_week_on_january_1st(int const year) {
  // Gauss's formula, see
  // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Gauss.27s_algorithm.
  return mod(1 + 5 * ((year - 1) % 4) +
                 4 * ((year - 1) % 100) +
                 6 * ((year - 1) % 400),
             7,
             1);
}

constexpr int number_of_weeks_in_year(int const year) {
  return day_of_week_on_january_1st(year) == 4 ||
                 (is_gregorian_leap_year(year) &&
                  day_of_week_on_january_1st(year) == 3)
             ? 53
             : 52;
}

// Returns the ordinal in |year| of the first day of the first week of |year|.
// The result is in [-2, 4], with values in [-2, 0] meaning that the first week
// of |year| starts in |year - 1|.
// A result in [-2, 1] means that the first day of |year| is in the first week
// of |year|; otherwise, it is in the last week of |year - 1|.
constexpr int ordinal_of_w_01_1(int const year) {
  return mod(2 - day_of_week_on_january_1st(year), 7, -2);
}

// The signed number of days from 2000-01-01 to the first day of |year|.
constexpr int days_from_2000_01_01_at_start_of_year(int const year) {
  return year - 2000 > 0
             ? 1 + (year - 2000) * 365 +
               (year - 2000 - 1) / 4 -
               (year - 2000 - 1) / 100 +
               (year - 2000 - 1) / 400
             : (year - 2000) * 365 +
               (year - 2000) / 4 -
               (year - 2000) / 100 +
               (year - 2000) / 400;
}

// Returns the number formed by taking |end - begin| increasingly significant
// digits, starting from the digit of the (10 ** |begin|)s.
constexpr std::int64_t digit_range(std::int64_t const digits,
                                   int const begin,
                                   int const end) {
  return CHECKING(
      digits >= 0 && begin >= 0 && begin <= end,
      (begin == end)
          ? 0
          : ((begin == 0)
                 ? digit_range(digits / 10, 0, end - 1) * 10 + digits % 10
                 : digit_range(digits / 10, begin - 1, end - 1)));
}

// Returns x * 10 ** count.
constexpr std::int64_t append_0s(std::int64_t const x, int const count) {
  return CHECKING(count >= 0, count == 0 ? x : append_0s(x * 10, count - 1));
}

// Returns |date| advanced by the specified number of |days|. The result must be
// in the same year.
constexpr Date add_days_within_year(Date const& date, int const days) {
  return CHECKING(
      days >= 0,
      days == 0
          ? date
          : (date.day() + days > month_length(date.year(), date.month())
                 ? CHECKING(
                       date.month() <= 11,
                       add_days_within_year(
                           Date::Calendar(date.year(), date.month() + 1, 1),
                           days - month_length(date.year(), date.month()) +
                               date.day() - 1))
                 : Date::Calendar(date.year(),
                                  date.month(),
                                  date.day() + days)));
}

// The |day|th day of some |year|.  The resulting date need not be in |year|.
constexpr Date arbitrary_ordinal(int const year, int const day) {
  return day < 1 ? arbitrary_ordinal(year - 1,
                                     gregorian_year_length(year - 1) + day)
                 : day > gregorian_year_length(year)
                       ? arbitrary_ordinal(year + 1,
                                           day - gregorian_year_length(year))
                       : Date::Ordinal(year, day);
}

// Implementation of class |Date|.

constexpr Date Date::YYYYMMDD(std::int64_t const digits) {
  return CHECKING(digits >= 0 && digits <= 9999'99'99,
                  Date::Calendar(digit_range(digits, 4, 8),
                                 digit_range(digits, 2, 4),
                                 digit_range(digits, 0, 2)));
}

constexpr Date Date::YYYYDDD(std::int64_t const digits) {
  return CHECKING(
      digits >= 0 && digits <= 9999'999,
      Date::Ordinal(digit_range(digits, 3, 7), digit_range(digits, 0, 3)));
}

constexpr Date Date::YYYYwwD(std::int64_t const digits) {
  return CHECKING(digits >= 0 && digits <= 9999'99'9,
                  Date::Week(digit_range(digits, 3, 7),
                             digit_range(digits, 1, 3),
                             digit_range(digits, 0, 1)));
}

constexpr Date Date::Calendar(int const year, int const month, int const day) {
  return CHECKING(year >= 1583 && year <= 9999 && month >= 1 && month <= 12 &&
                  day >= 1 && day <= month_length(year, month),
                  Date(year, month, day));
}

constexpr Date Date::Ordinal(int const year, int const day) {
  return CHECKING(
      day >= 1 && day <= gregorian_year_length(year),
      add_days_within_year(Date::Calendar(year, 1, 1), day - 1));
}

constexpr Date Date::Week(int const year, int const week, int const day) {
  return CHECKING(
      week >= 1 && week <= number_of_weeks_in_year(year) &&
      day >= 1 && day <= 7,
      arbitrary_ordinal(year,
                        (week - 1) * 7 + day - 1 + ordinal_of_w_01_1(year)));
}

constexpr int Date::year() const {
  return year_;
}

constexpr int Date::month() const {
  return month_;
}

constexpr int Date::day() const {
  return day_;
}

constexpr int Date::ordinal() const {
  return day_ > 1 ? (day_ - 1) + Date(year_, month_, 1).ordinal()
                  : month_ > 1
                        ? month_length(year_, month_ - 1) +
                              Date(year_, month_ - 1, 1).ordinal()
                        : 1;
}

constexpr Date Date::next_day() const {
  return day_ == month_length(year_, month_)
             ? month_ == 12 ? Date(year_ + 1, 1, 1) : Date(year_, month_ + 1, 1)
             : Date(year_, month_, day_ + 1);
}

constexpr Date::Date(int const year,
                     int const month,
                     int const day)
      : year_(year),
        month_(month),
        day_(day) {}

// Implementation of class Time.

constexpr Time Time::hhmmss_ns(int const hhmmss, int ns) {
  return CHECKING(hhmmss >= 0 && hhmmss <= 99'99'99,
                  Time(digit_range(hhmmss, 4, 6),
                       digit_range(hhmmss, 2, 4),
                       digit_range(hhmmss, 0, 2),
                       ns).checked());
}

constexpr int Time::hour() const {
  return hour_;
}

constexpr int Time::minute() const {
  return minute_;
}

constexpr int Time::second() const {
  return second_;
}

constexpr int Time::nanosecond() const {
  return nanosecond_;
}

constexpr bool Time::is_leap_second() const {
  return second_ == 60;
}

constexpr bool Time::is_end_of_day() const {
  return hour_ == 24;
}

constexpr Time::Time(int const hour,
                     int const minute,
                     int const second,
                     int const nanosecond)
    : hour_(hour),
      minute_(minute),
      second_(second),
      nanosecond_(nanosecond) {}

constexpr Time const& Time::checked() const {
  return CHECKING(
      (hour_ == 24 && minute_ == 0 && second_ == 0 && nanosecond_ == 0) ||
          ((nanosecond_ >= 0 && nanosecond_ <= 999'999'999) &&
           ((hour_ == 23 && minute_ == 59 && second_ == 60) ||
            (hour_ >= 0 && hour_ <= 23 && minute_ >= 0 && minute_ <= 59 &&
             second_ >= 0 && second_ <= 59))),
      *this);
}

// Implementation of class DateTime.

constexpr Date const& DateTime::date() const {
  return date_;
}

constexpr Time const& DateTime::time() const {
  return time_;
}

constexpr DateTime DateTime::normalized_end_of_day() const {
  return time_.is_end_of_day()
             ? DateTime(date_.next_day(), Time::hhmmss_ns(00'00'00, 0))
             : *this;
}

constexpr DateTime::DateTime(Date const date, Time const time)
    : date_(date),
      time_(time) {}

constexpr DateTime const& DateTime::checked() const {
  return CHECKING(
      !time_.is_leap_second() ||
          (date_.day() == month_length(date_.year(), date_.month()) &&
           (date_.month() == 6 || date_.month() == 12 || date_.month() == 3 ||
            date_.month() == 9)),
      *this);
}

// Parsing utilities.

constexpr bool contains(char const* str, std::size_t size, char const c) {
  return size > 0 && (str[0] == c || contains(str + 1, size - 1, c));
}

constexpr int index_of(char const* str, std::size_t size, char const c) {
  return CHECKING(size > 0,
                  str[0] == c ? 0 : (index_of(str + 1, size - 1, c) + 1));
}

class CStringIterator {
 public:
  constexpr CStringIterator(char const* str, std::size_t size);

  constexpr bool at_end() const;
  constexpr CStringIterator next() const;
  constexpr int index() const;
  constexpr char const& operator*() const;

 private:
  constexpr CStringIterator(char const* str, char const* end, char const* it);

  char const* const str_;
  char const* const end_;
  char const* const it_;
};

constexpr CStringIterator::CStringIterator(char const* str, std::size_t size)
    : str_(str),
      end_(str + size),
      it_(str) {}

constexpr bool CStringIterator::at_end() const {
  return it_ == end_;
}

constexpr CStringIterator CStringIterator::next() const {
  return CHECKING(!at_end(), CStringIterator(str_, end_, it_ + 1));
}

constexpr int CStringIterator::index() const {
  return it_ - str_;
}

constexpr char const& CStringIterator::operator*() const {
  return CHECKING(!at_end(), *it_);
}

constexpr CStringIterator::CStringIterator(char const* str,
                                           char const* end,
                                           char const* it)
    : str_(str),
      end_(end),
      it_(it) {}

// Date parsing.

// A |DateParser| contains information about a string necessary to interpret it
// as a date representation.
class DateParser {
 public:
  // Returns a |Date| corresponding to the representation |str|.
  // Fails unless |str| is a date representation of one of the following forms:
  // [YYYY-MM-DD], [YYYYMMDD], [YYYY-Www-D], [YYYYWwwD], [YYYY-DDD], [YYYYDDD].
  static constexpr Date Parse(char const* str, std::size_t size);

 private:
  constexpr DateParser(std::int64_t const digits,
                       int const digit_count,
                       int const hyphens,
                       int const first_hyphen_index,
                       int const second_hyphen_index,
                       bool const has_w,
                       int const w_index);

  // Returns a |DateParser| describing the given string. Fails if the string
  // does not exclusively consist of:
  //   - any number of decimal digits;
  //   - at most two hyphens;
  //   - at most one 'W'.
  static constexpr DateParser ReadToEnd(char const* str, std::size_t size);
  static constexpr DateParser ReadToEnd(CStringIterator const str,
                                        std::int64_t const digits,
                                        int const digit_count,
                                        int const colons,
                                        int const first_hyphen_index,
                                        int const second_hyphen_index,
                                        bool const has_w,
                                        int const w_index);

  // Returns a |Date| corresponding to the string that |*this| describes.
  // Fails if the format is invalid or the string represents an invalid date.
  constexpr Date ToDate() const;

  // The number formed by all digits in the string.
  std::int64_t const digits_;
  // The number of digits.
  int const digit_count_;
  // The number of hyphens.
  int const hyphens_;
  // The index of the first hyphen.
  int const first_hyphen_index_;
  // The index of the second hyphen.
  int const second_hyphen_index_;
  // Whether the string contains a W.
  bool const has_w_;
  // The index of the W.
  int const w_index_;
};

constexpr Date DateParser::Parse(char const* str, std::size_t size) {
  return ReadToEnd(str, size).ToDate();
}

constexpr DateParser::DateParser(std::int64_t const digits,
                                 int const digit_count,
                                 int const hyphens,
                                 int const first_hyphen_index,
                                 int const second_hyphen_index,
                                 bool const has_w,
                                 int const w_index)
    : digits_(digits),
      digit_count_(digit_count),
      hyphens_(hyphens),
      first_hyphen_index_(first_hyphen_index),
      second_hyphen_index_(second_hyphen_index),
      has_w_(has_w),
      w_index_(w_index) {}

constexpr DateParser DateParser::ReadToEnd(char const* str, std::size_t size) {
  return ReadToEnd(CStringIterator(str, size),
                   /*digits=*/0,
                   /*digit_count=*/0,
                   /*hyphens=*/0,
                   /*first_hyphen_index=*/-1,
                   /*second_hyphen_index=*/-1,
                   /*has_w=*/false,
                   /*w_index=*/-1);
}

constexpr DateParser DateParser::ReadToEnd(CStringIterator const str,
                                           std::int64_t const digits,
                                           int const digit_count,
                                           int const hyphens,
                                           int const first_hyphen_index,
                                           int const second_hyphen_index,
                                           bool const has_w,
                                           int const w_index) {
  return str.at_end()
             ? DateParser{digits,
                          digit_count,
                          hyphens,
                          first_hyphen_index,
                          second_hyphen_index,
                          has_w,
                          w_index}
             : *str == '-'
                   ? CHECKING(
                         hyphens < 2,
                         hyphens == 0
                             ? ReadToEnd(str.next(),
                                         digits,
                                         digit_count,
                                         hyphens + 1,
                                         /*first_hyphen_index=*/str.index(),
                                         second_hyphen_index,
                                         has_w,
                                         w_index)
                             : ReadToEnd(str.next(),
                                         digits,
                                         digit_count,
                                         hyphens + 1,
                                         first_hyphen_index,
                                         /*second_hyphen_index=*/str.index(),
                                         has_w,
                                         w_index))
                   : *str == 'W' ? CHECKING(!has_w,
                                            ReadToEnd(str.next(),
                                                      digits,
                                                      digit_count,
                                                      hyphens,
                                                      first_hyphen_index,
                                                      second_hyphen_index,
                                                      /*has_w=*/true,
                                                      /*w_index=*/str.index()))
                                 : CHECKING(*str >= '0' && *str <= '9',
                                            ReadToEnd(str.next(),
                                                      digits * 10 + *str - '0',
                                                      digit_count + 1,
                                                      hyphens,
                                                      first_hyphen_index,
                                                      second_hyphen_index,
                                                      has_w,
                                                      w_index));
}

constexpr Date DateParser::ToDate() const {
  return digit_count_ == 8
             ? CHECKING(hyphens_ == 0 ||
                            (hyphens_ == 2 && first_hyphen_index_ == 4 &&
                             second_hyphen_index_ == 7),
                        Date::YYYYMMDD(digits_))
             : CHECKING(
                   digit_count_ == 7,
                   has_w_
                       ? CHECKING(
                             (hyphens_ == 0 && w_index_ == 4) ||
                                 (hyphens_ == 2 && first_hyphen_index_ == 4 &&
                                  w_index_ == 5 && second_hyphen_index_ == 8),
                             Date::YYYYwwD(digits_))
                       : CHECKING(hyphens_ == 0 || (hyphens_ == 1 &&
                                                    first_hyphen_index_ == 4),
                                  Date::YYYYDDD(digits_)));
}

constexpr Date operator""_Date(char const* str, std::size_t size) {
  return DateParser::Parse(str, size);
}

// Time parsing.

// A |TimeParser| contains information about a string necessary to interpret it
// as a time representation.
class TimeParser {
 public:
  // Returns a |Time| corresponding to the representation |str|.
  // Fails unless |str| is a valid time representation of one of the following
  // forms: [hh:mm:ss], [hhmmss], [hh:mm:ss.ss̲], [hh:mm:ss,ss̲], [hhmmss.ss̲],
  // [hhmmss,ss̲], with at most nine digits after the decimal mark.
  static constexpr Time Parse(char const* str, std::size_t size);

 private:
  constexpr TimeParser(std::int64_t const digits,
                       int const digit_count,
                       int const colons,
                       int const first_colon_index,
                       int const second_colon_index,
                       bool const has_decimal_mark,
                       int const decimal_mark_index);

  // Returns a |TimeParser| describing the given string. Fails if the string
  // does not exclusively consist of:
  // Fails if the string does not exclusively consist of:
  //   - any number of decimal digits;
  //   - at most two colons;
  //   - at most one decimal mark ('.' or ',').
  static constexpr TimeParser ReadToEnd(char const* str, std::size_t size);
  static constexpr TimeParser ReadToEnd(CStringIterator const str,
                                        std::int64_t const digits,
                                        int const digit_count,
                                        int const colons,
                                        int const first_colon_index,
                                        int const second_colon_index,
                                        bool const has_decimal_mark,
                                        int const decimal_mark_index);

  // Returns a |Time| corresponding to the string that |*this| describes.
  // Fails if the format is invalid or the string represents an invalid time.
  constexpr Time ToTime() const;

  // The number formed by all digits in the string.
  std::int64_t const digits_;
  // The number of digits.
  int const digit_count_;
  // The number of colons.
  int const colons_;
  // The index of the first colon.
  int const first_colon_index_;
  // The index of the second colon.
  int const second_colon_index_;
  // Whether the string contains a decimal mark.
  bool const has_decimal_mark_;
  // The index of the decimal mark.
  int const decimal_mark_index_;
};

constexpr Time TimeParser::Parse(char const* str, std::size_t size) {
  return ReadToEnd(str, size).ToTime();
}

constexpr TimeParser::TimeParser(std::int64_t const digits,
                                 int const digit_count,
                                 int const colons,
                                 int const first_colon_index,
                                 int const second_colon_index,
                                 bool const has_decimal_mark,
                                 int const decimal_mark_index)
    : digits_(digits),
      digit_count_(digit_count),
      colons_(colons),
      first_colon_index_(first_colon_index),
      second_colon_index_(second_colon_index),
      has_decimal_mark_(has_decimal_mark),
      decimal_mark_index_(decimal_mark_index) {}

constexpr TimeParser TimeParser::ReadToEnd(char const* str, std::size_t size) {
  return ReadToEnd(CStringIterator(str, size),
                   /*digits=*/0,
                   /*digit_count*/ 0,
                   /*colons=*/0,
                   /*first_colon_index=*/-1,
                   /*second_colon_index=*/-1,
                   /*has_decimal_mark=*/false,
                   /*decimal_mark_index=*/-1);
}

constexpr TimeParser TimeParser::ReadToEnd(CStringIterator const str,
                                           std::int64_t const digits,
                                           int const digit_count,
                                           int const colons,
                                           int const first_colon_index,
                                           int const second_colon_index,
                                           bool const has_decimal_mark,
                                           int const decimal_mark_index) {
  return str.at_end()
             ? TimeParser(digits,
                          digit_count,
                          colons,
                          first_colon_index,
                          second_colon_index,
                          has_decimal_mark,
                          decimal_mark_index)
             : *str == ':'
                   ? CHECKING(
                         colons < 2,
                         colons == 0
                             ? ReadToEnd(str.next(),
                                         digits,
                                         digit_count,
                                         colons + 1,
                                         /*first_colon_index=*/str.index(),
                                         second_colon_index,
                                         has_decimal_mark,
                                         decimal_mark_index)
                             : ReadToEnd(str.next(),
                                         digits,
                                         digit_count,
                                         colons + 1,
                                         first_colon_index,
                                         /*second_colon_index=*/str.index(),
                                         has_decimal_mark,
                                         decimal_mark_index))
                   : *str == ',' || *str == '.'
                         ? CHECKING(
                               !has_decimal_mark,
                               ReadToEnd(str.next(),
                                         digits,
                                         digit_count,
                                         colons,
                                         first_colon_index,
                                         second_colon_index,
                                         /*has_decimal_mark=*/true,
                                         /*decimal_mark_index=*/str.index()))
                         : CHECKING(*str >= '0' && *str <= '9',
                                    ReadToEnd(str.next(),
                                              digits * 10 + *str - '0',
                                              digit_count + 1,
                                              colons,
                                              first_colon_index,
                                              second_colon_index,
                                              has_decimal_mark,
                                              decimal_mark_index));
}

constexpr Time TimeParser::ToTime() const {
  return CHECKING(
      digit_count_ >= 6 &&
          (colons_ == 0 || (colons_ == 2 && first_colon_index_ == 2 &&
                            second_colon_index_ == 5)) &&
          ((digit_count_ == 6 && !has_decimal_mark_) ||
           (has_decimal_mark_ &&
            ((colons_ == 0 && decimal_mark_index_ == 6) ||
             (colons_ != 0 && decimal_mark_index_ == 8)))) &&
          digit_count_ <= 15,
      Time::hhmmss_ns(digit_range(digits_, digit_count_ - 6, digit_count_),
                      append_0s(digit_range(digits_, 0, digit_count_ - 6),
                                9 - (digit_count_ - 6))));
}

constexpr Time operator""_Time(char const* str, std::size_t size) {
  return TimeParser::Parse(str, size);
}

// DateTime parsing.

constexpr DateTime operator""_DateTime(char const* str, std::size_t size) {
  // Given correctness of the date and time parts of the string, this check
  // ensures that either both are in basic format or both are in extended
  // format.
  return CHECKING(
      contains(str, size, '-') == contains(str, size, ':'),
      DateTime(
          operator""_Date(str, index_of(str, size, 'T')),
          operator""_Time(str + index_of(str, size, 'T') + 1,
                          size - (index_of(str, size, 'T') + 1))).checked());
}

// Conversion to |Instant|, continuous time scales.

constexpr Instant DateTimeAsTT(DateTime const& date_time) {
  return CHECKING(!date_time.time().is_leap_second(),
                  J2000 +
                  date_time.time().nanosecond() / 1e9 * Second +
                  (date_time.time().second() +
                   date_time.time().minute() * 60 +
                   (date_time.time().hour() - 12) * 60 * 60) * Second +
                  (days_from_2000_01_01_at_start_of_year(
                       date_time.date().year()) +
                   date_time.date().ordinal() - 1) * Day);
}

// Allows leap seconds, which are interpreted as the first second of the
// following TAI day; used in the implementation of both TAI and UTC.
constexpr Instant DateTimeAsTAIUnchecked(DateTime const& date_time) {
  return J2000 + (date_time.time().nanosecond() + 184'000'000) / 1e9 * Second +
         ((date_time.time().second() - 28) +
          (date_time.time().minute() - 59) * 60 +
          (date_time.time().hour() - 11) * 60 * 60) * Second +
         (days_from_2000_01_01_at_start_of_year(date_time.date().year()) +
          date_time.date().ordinal() - 1) * Day;
}

constexpr Instant DateTimeAsTAI(DateTime const& date_time) {
  return CHECKING(!date_time.time().is_leap_second(),
                  DateTimeAsTAIUnchecked(date_time));
}

// Leap second handling and conversion to UTC.

constexpr std::array<int, (2016 - 1972) * 2 + 1> leap_seconds = {{
    +1, +1,  // 1972
    +0, +1,  // 1973
    +0, +1,  // 1974
    +0, +1,  // 1975
    +0, +1,  // 1976
    +0, +1,  // 1977
    +0, +1,  // 1978
    +0, +1,  // 1979
    +0, +0,  // 1980
    +1, +0,  // 1981
    +1, +0,  // 1982
    +1, +0,  // 1983
    +0, +0,  // 1984
    +1, +0,  // 1985
    +0, +0,  // 1986
    +0, +1,  // 1987
    +0, +0,  // 1988
    +0, +1,  // 1989
    +0, +1,  // 1990
    +0, +0,  // 1991
    +1, +0,  // 1992
    +1, +0,  // 1993
    +1, +0,  // 1994
    +0, +1,  // 1995
    +0, +0,  // 1996
    +1, +0,  // 1997
    +0, +1,  // 1998
    +0, +0,  // 1999
    +0, +0,  // 2000
    +0, +0,  // 2001
    +0, +0,  // 2002
    +0, +0,  // 2003
    +0, +0,  // 2004
    +0, +1,  // 2005
    +0, +0,  // 2006
    +0, +0,  // 2007
    +0, +1,  // 2008
    +0, +0,  // 2009
    +0, +0,  // 2010
    +0, +0,  // 2011
    +1, +0,  // 2012
    +0, +0,  // 2013
    +0, +0,  // 2014
    +1, +0,  // 2015
    +0,      // 2016
}};

// Returns UTC - TAI on the given UTC day (similar to Bulletin C).
constexpr quantities::Time UTC_TAI(Date const& utc_date) {
  return utc_date.month() == 1 && utc_date.day() == 1
             ? utc_date.year() == 1972
                   ? -10 * Second
                   : -leap_seconds[(utc_date.year() - 1973) * 2] * Second +
                     -leap_seconds[(utc_date.year() - 1973) * 2 + 1] * Second +
                     UTC_TAI(Date::Calendar(utc_date.year() - 1, 1, 1))
             : (utc_date.month() > 6
                    ? -leap_seconds[(utc_date.year() - 1972) * 2] * Second
                    : 0 * Second) +
               UTC_TAI(Date::Calendar(utc_date.year(), 1, 1));
}

// NOTE(egg): no check for invalid UTC in case of negative leap seconds.
constexpr Instant DateTimeAsUTC(DateTime const& date_time) {
  return date_time.time().is_end_of_day()
             ? DateTimeAsUTC(date_time.normalized_end_of_day())
             : CHECKING(
                   !date_time.time().is_leap_second() ||
                   (date_time.date().month() == 6 &&
                    leap_seconds[(date_time.date().year() - 1972) * 2] == +1) ||
                   (date_time.date().month() == 12 &&
                    leap_seconds[(date_time.date().year() - 1972) * 2 + 1] ==
                        +1),
                   DateTimeAsTAIUnchecked(date_time) -
                       UTC_TAI(date_time.date()));
}

// |Instant| date literals.

#if (PRINCIPIA_COMPILER_CLANG || PRINCIPIA_COMPILER_CLANG_CL) && WE_LIKE_N3599
template<typename C, C... str>
constexpr std::array<C, sizeof...(str)> unpack_as_array() {
  return std::array<C, sizeof...(str)>{{str...}};
}

template<typename T>
constexpr T const& as_const_ref(T const& t) {
  return t;
}

template<typename C, std::size_t size>
constexpr C const* c_str(std::array<C, size> const& array) {
  // In C++17 this could be |data()|.  For the moment this does the job.
  return &as_const_ref(array)[0];
}

// NOTE(egg): In the following three functions, the |constexpr| intermediate
// variable forces failures to occur at compile time and not as glog |CHECK|
// failures at evaluation.

template<typename C, C... str>
constexpr Instant operator""_TAI() {
  constexpr auto result = DateTimeAsTAI(
      operator""_DateTime(c_str(unpack_as_array<C, str...>()), sizeof...(str)));
  return result;
}

template<typename C, C... str>
constexpr Instant operator""_TT() {
  constexpr auto result = DateTimeAsTT(
      operator""_DateTime(c_str(unpack_as_array<C, str...>()), sizeof...(str)));
  return result;
}

template<typename C, C... str>
constexpr Instant operator""_UTC() {
  constexpr auto result = DateTimeAsUTC(
      operator""_DateTime(c_str(unpack_as_array<C, str...>()), sizeof...(str)));
  return result;
}
#else
constexpr Instant operator""_TAI(char const* str, std::size_t size) {
  return DateTimeAsTAI(operator""_DateTime(str, size));
}

constexpr Instant operator""_TT(char const* str, std::size_t size) {
  return DateTimeAsTT(operator""_DateTime(str, size));
}

constexpr Instant operator""_UTC(char const* str, std::size_t size) {
  return DateTimeAsUTC(operator""_DateTime(str, size));
}
#endif

}  // namespace internal_date
}  // namespace astronomy
}  // namespace principia