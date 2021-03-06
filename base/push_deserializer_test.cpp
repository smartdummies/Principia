﻿
#include "base/push_deserializer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <list>
#include <string>
#include <thread>
#include <vector>

#include "base/array.hpp"
#include "base/not_null.hpp"
#include "base/pull_serializer.hpp"
#include "gmock/gmock.h"
#include "serialization/physics.pb.h"

namespace principia {
namespace base {
namespace internal_push_deserializer {

using base::not_null;
using serialization::DiscreteTrajectory;
using serialization::Pair;
using serialization::Point;
using serialization::Quantity;
using ::std::placeholders::_1;
using ::testing::ElementsAreArray;

namespace {
int const deserializer_chunk_size = 99;
int const runs_per_test = 1000;
int const serializer_chunk_size = 99;
int const number_of_chunks = 3;
const char start[] = "START";
}  // namespace

class PushDeserializerTest : public ::testing::Test {
 protected:
  PushDeserializerTest()
      : pull_serializer_(std::make_unique<PullSerializer>(serializer_chunk_size,
                                                          number_of_chunks)),
        push_deserializer_(
            std::make_unique<PushDeserializer>(deserializer_chunk_size,
                                               number_of_chunks)),
        stream_(std::bind(&PushDeserializerTest::OnEmpty,
                          this,
                          std::ref(strings_))) {}

  static not_null<std::unique_ptr<DiscreteTrajectory const>> BuildTrajectory() {
    not_null<std::unique_ptr<DiscreteTrajectory>> result =
        make_not_null_unique<DiscreteTrajectory>();
    // Build a biggish protobuf for serialization.
    for (int i = 0; i < 100; ++i) {
      DiscreteTrajectory::InstantaneousDegreesOfFreedom* idof =
          result->add_timeline();
      Point* instant = idof->mutable_instant();
      Quantity* scalar = instant->mutable_scalar();
      scalar->set_dimensions(3);
      scalar->set_magnitude(3 * i);
      Pair* dof = idof->mutable_degrees_of_freedom();
      Pair::Element* t1 = dof->mutable_t1();
      Point* point1 = t1->mutable_point();
      Quantity* scalar1 = point1->mutable_scalar();
      scalar1->set_dimensions(1);
      scalar1->set_magnitude(i);
      Pair::Element* t2 = dof->mutable_t2();
      Point* point2 = t2->mutable_point();
      Quantity* scalar2 = point2->mutable_scalar();
      scalar2->set_dimensions(2);
      scalar2->set_magnitude(2 * i);
    }
    return std::move(result);
  }

  static void CheckSerialization(google::protobuf::Message const& message) {
    std::string const actual_serialized = message.SerializeAsString();
    std::string const expected_serialized =
        BuildTrajectory()->SerializeAsString();
    EXPECT_EQ(actual_serialized, expected_serialized);
  }


  static void Stomp(Bytes const& bytes) {
    std::memset(bytes.data, 0xCD, static_cast<std::size_t>(bytes.size));
  }

  // Returns the first string in the list.  Note that the very first string is
  // always discarded.
  Bytes OnEmpty(std::list<std::string>& strings) {
    strings.pop_front();
    CHECK(!strings.empty());
    std::string& front = strings.front();
    return Bytes(reinterpret_cast<std::uint8_t*>(&front[0]),
                 static_cast<std::int64_t>(front.size()));
  }

  std::unique_ptr<PullSerializer> pull_serializer_;
  std::unique_ptr<PushDeserializer> push_deserializer_;
  DelegatingArrayInputStream stream_;
  std::list<std::string> strings_;
};

using PushDeserializerDeathTest = PushDeserializerTest;

TEST_F(PushDeserializerTest, Stream) {
  void const* data;
  int size;

  strings_ = {start, "abc"};
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(3, size);
  EXPECT_EQ("abc", std::string(static_cast<char const*>(data), size));
  EXPECT_EQ(3, stream_.ByteCount());

  strings_ = {start, ""};
  EXPECT_FALSE(stream_.Next(&data, &size));
  EXPECT_EQ(3, stream_.ByteCount());

  strings_ = {start, "abc", "xyzt"};
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(3, size);
  EXPECT_EQ("abc", std::string(static_cast<char const*>(data), size));
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(4, size);
  EXPECT_EQ("xyzt", std::string(static_cast<char const*>(data), size));
  EXPECT_EQ(10, stream_.ByteCount());

  strings_ = {start, "abc", "xyzt", "uvw", ""};
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(3, size);
  EXPECT_TRUE(stream_.Skip(2));
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(2, size);
  EXPECT_EQ("zt", std::string(static_cast<char const*>(data), size));
  EXPECT_FALSE(stream_.Skip(5));
  EXPECT_EQ(20, stream_.ByteCount());

  strings_ = {start, "abc"};
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(3, size);
  stream_.BackUp(1);
  EXPECT_TRUE(stream_.Next(&data, &size));
  EXPECT_EQ(1, size);
  EXPECT_EQ("c", std::string(static_cast<char const*>(data), size));
  EXPECT_EQ(23, stream_.ByteCount());
}

TEST_F(PushDeserializerTest, DeserializationThreading) {
  auto const written_trajectory = BuildTrajectory();
  int const byte_size = written_trajectory->ByteSize();
  auto const serialized_trajectory =
      std::make_unique<std::uint8_t[]>(byte_size);

  for (int i = 0; i < runs_per_test; ++i) {
    auto read_trajectory = make_not_null_unique<DiscreteTrajectory>();
    push_deserializer_ = std::make_unique<PushDeserializer>(
        deserializer_chunk_size, number_of_chunks);

    written_trajectory->SerializePartialToArray(&serialized_trajectory[0],
                                                byte_size);
    push_deserializer_->Start(
      std::move(read_trajectory), &PushDeserializerTest::CheckSerialization);
    Bytes bytes(serialized_trajectory.get(), byte_size);
    push_deserializer_->Push(bytes,
                             std::bind(&PushDeserializerTest::Stomp, bytes));
    push_deserializer_->Push(Bytes(), nullptr);

    // Destroying the deserializer waits until deserialization is done.
    push_deserializer_.reset();
  }
}

// Exercise concurrent serialization and deserialization.
TEST_F(PushDeserializerTest, SerializationDeserialization) {
  auto const trajectory = BuildTrajectory();
  int const byte_size = trajectory->ByteSize();
  for (int i = 0; i < runs_per_test; ++i) {
    auto read_trajectory = make_not_null_unique<DiscreteTrajectory>();
    auto written_trajectory = BuildTrajectory();
    auto storage = std::make_unique<std::uint8_t[]>(byte_size);
    std::uint8_t* data = &storage[0];

    pull_serializer_ = std::make_unique<PullSerializer>(serializer_chunk_size,
                                                        number_of_chunks);
    push_deserializer_ = std::make_unique<PushDeserializer>(
        deserializer_chunk_size, number_of_chunks);

    pull_serializer_->Start(std::move(written_trajectory));
    push_deserializer_->Start(
        std::move(read_trajectory), PushDeserializerTest::CheckSerialization);
    for (;;) {
      Bytes const bytes = pull_serializer_->Pull();
      std::memcpy(data, bytes.data, static_cast<std::size_t>(bytes.size));
      push_deserializer_->Push(Bytes(data, bytes.size),
                               std::bind(&PushDeserializerTest::Stomp,
                                         Bytes(data, bytes.size)));
      data = &data[bytes.size];
      if (bytes.size == 0) {
        break;
      }
    }

    // Destroying the deserializer waits until deserialization is done.  It is
    // important that this happens before |storage| is destroyed.
    pull_serializer_.reset();
    push_deserializer_.reset();
  }
}

// Check that deserialization fails if we stomp on one extra bytes.
TEST_F(PushDeserializerDeathTest, Stomp) {
  EXPECT_DEATH({
    const int stomp_chunk = 77;
    auto read_trajectory = make_not_null_unique<DiscreteTrajectory>();
    auto const trajectory = BuildTrajectory();
    int const byte_size = trajectory->ByteSize();
    auto serialized_trajectory =
        std::make_unique<std::uint8_t[]>(byte_size);
    trajectory->SerializePartialToArray(&serialized_trajectory[0], byte_size);
    push_deserializer_->Start(
      std::move(read_trajectory), &PushDeserializerTest::CheckSerialization);
    int left = byte_size;
    for (int i = 0; i < byte_size; i += stomp_chunk) {
      Bytes bytes(&serialized_trajectory[i], std::min(left, stomp_chunk));
      push_deserializer_->Push(bytes,
                               std::bind(&PushDeserializerTest::Stomp,
                                         Bytes(bytes.data, bytes.size + 1)));
      left -= stomp_chunk;
    }
    push_deserializer_->Push(Bytes(), nullptr);
    push_deserializer_.reset();
    }, "failed.*Parse");
}

}  // namespace internal_push_deserializer
}  // namespace base
}  // namespace principia
