﻿
#pragma once

#include "geometry/frame.hpp"

#include <string>

#include "base/fingerprint2011.hpp"
#include "geometry/named_quantities.hpp"
#include "google/protobuf/descriptor.h"

namespace principia {
namespace geometry {
namespace internal_frame {

using base::Fingerprint2011;

// Utility for fingerprinting.
inline uint32_t Fingerprint(std::string const& s) {
  return Fingerprint2011(s.c_str(), s.size()) & 0xFFFFFFFF;
}

template<typename FrameTag, FrameTag frame_tag, bool frame_is_inertial>
void Frame<FrameTag, frame_tag, frame_is_inertial>::WriteToMessage(
    not_null<serialization::Frame*> const message) {
  std::string const& tag_type_full_name =
      google::protobuf::GetEnumDescriptor<Tag>()->full_name();

  message->set_tag_type_fingerprint(Fingerprint(tag_type_full_name));
  message->set_tag(tag);
  message->set_is_inertial(is_inertial);
}

template<typename FrameTag, FrameTag frame_tag, bool frame_is_inertial>
void Frame<FrameTag, frame_tag, frame_is_inertial>::ReadFromMessage(
    serialization::Frame const& message) {
  std::string const& tag_type_full_name =
      google::protobuf::GetEnumDescriptor<Tag>()->full_name();

  CHECK_EQ(Fingerprint(tag_type_full_name), message.tag_type_fingerprint())
      << tag_type_full_name;
  CHECK_EQ(tag, message.tag());
  CHECK_EQ(is_inertial, message.is_inertial());
}

// Default-initialized to {0, 0, 0}.
template<typename FrameTag, FrameTag frame_tag, bool frame_is_inertial>
Position<Frame<FrameTag, frame_tag, frame_is_inertial>> const
Frame<FrameTag, frame_tag, frame_is_inertial>::origin;
template<typename FrameTag, FrameTag frame_tag, bool frame_is_inertial>
FrameTag const Frame<FrameTag, frame_tag, frame_is_inertial>::tag;
template<typename FrameTag, FrameTag frame_tag, bool frame_is_inertial>
bool const Frame<FrameTag, frame_tag, frame_is_inertial>::is_inertial;

inline void ReadFrameFromMessage(
    serialization::Frame const& message,
    google::protobuf::EnumValueDescriptor const*& enum_value_descriptor,
    bool& is_inertial) {
  // Look at the enumeration types nested in serialization::Frame for one that
  // matches our fingerprint.
  const google::protobuf::Descriptor* frame_descriptor =
      serialization::Frame::descriptor();
  enum_value_descriptor = nullptr;
  for (int i = 0; i < frame_descriptor->enum_type_count(); ++i) {
    const google::protobuf::EnumDescriptor* enum_type_descriptor =
        frame_descriptor->enum_type(i);
    std::string const& enum_type_full_name = enum_type_descriptor->full_name();
    if (Fingerprint(enum_type_full_name) == message.tag_type_fingerprint()) {
      enum_value_descriptor =
          enum_type_descriptor->FindValueByNumber(message.tag());
      break;
    }
  }
  CHECK_NOTNULL(enum_value_descriptor);
  is_inertial = message.is_inertial();
}

}  // namespace internal_frame
}  // namespace geometry
}  // namespace principia
