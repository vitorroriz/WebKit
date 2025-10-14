/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <wtf/EnumSet.h>
#include <wtf/HashSet.h>

namespace TestWebKitAPI {

enum class ExampleFlags : uint8_t {
    A,
    B,
    C,
    D = 31,
    E = 63,
};

TEST(WTF_EnumSet, EmptySet)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_EnumSet, ContainsOneFlag)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set = ExampleFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_EnumSet, Equal)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B };
    EXPECT_TRUE((set == EnumSetType { ExampleFlags::A, ExampleFlags::B }));
    EXPECT_TRUE((set == EnumSetType { ExampleFlags::B, ExampleFlags::A }));
    EXPECT_FALSE(set == ExampleFlags::B);
}

TEST(WTF_EnumSet, NotEqual)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set = ExampleFlags::A;
    EXPECT_TRUE(set != ExampleFlags::B);
    EXPECT_FALSE(set != ExampleFlags::A);
}

TEST(WTF_EnumSet, Or)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };
    EnumSetType set2 { ExampleFlags::C, ExampleFlags::D };
    EXPECT_TRUE(((set | ExampleFlags::A) == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(((set | ExampleFlags::D) == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(((set | set2) == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
}

TEST(WTF_EnumSet, OrAssignment)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };

    set |= { };
    EXPECT_TRUE((set == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));

    set |= { ExampleFlags::A };
    EXPECT_TRUE((set == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));

    set |= { ExampleFlags::C, ExampleFlags::D };
    EXPECT_TRUE((set == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C, ExampleFlags::D }));
}

TEST(WTF_EnumSet, Minus)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C };

    EXPECT_TRUE(((set - ExampleFlags::A) == EnumSetType { ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(((set - ExampleFlags::D) == EnumSetType { ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE((set - set).isEmpty());
}

TEST(WTF_EnumSet, AddAndRemove)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set;

    set.add(ExampleFlags::A);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    set.add({ ExampleFlags::B, ExampleFlags::C });
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.remove(ExampleFlags::B);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.remove({ ExampleFlags::A, ExampleFlags::C });
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
}

TEST(WTF_EnumSet, Set)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set;

    set.set(ExampleFlags::A, true);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    set.set({ ExampleFlags::B, ExampleFlags::C }, true);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.set(ExampleFlags::B, false);
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_TRUE(set.contains(ExampleFlags::C));

    set.set({ ExampleFlags::A, ExampleFlags::C }, false);
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
}

TEST(WTF_EnumSet, ContainsTwoFlags)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_EnumSet, ContainsTwoFlags2)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::D };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::E));
}

TEST(WTF_EnumSet, ContainsTwoFlags3)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::D, ExampleFlags::E };

    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
}

TEST(WTF_EnumSet, EmptyEnumSetToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_TRUE(set2.isEmpty());
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
}

TEST(WTF_EnumSet, EnumSetThatContainsOneFlagToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set = ExampleFlags::A;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));
    EXPECT_FALSE(set.contains(ExampleFlags::E));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::D));
    EXPECT_FALSE(set2.contains(ExampleFlags::E));
}

TEST(WTF_EnumSet, EnumSetThatContainsOneFlagToRawValueToEnumSet2)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set = ExampleFlags::E;
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::D));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::E));
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::D));
}

TEST(WTF_EnumSet, EnumSetThatContainsTwoFlagsToRawValueToEnumSet)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::C };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::A));
    EXPECT_TRUE(set.contains(ExampleFlags::C));
    EXPECT_FALSE(set.contains(ExampleFlags::B));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::A));
    EXPECT_TRUE(set2.contains(ExampleFlags::C));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
}

TEST(WTF_EnumSet, EnumSetThatContainsTwoFlagsToRawValueToEnumSet2)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::D, ExampleFlags::E };
    EXPECT_FALSE(set.isEmpty());
    EXPECT_TRUE(set.contains(ExampleFlags::D));
    EXPECT_TRUE(set.contains(ExampleFlags::E));
    EXPECT_FALSE(set.contains(ExampleFlags::A));
    EXPECT_FALSE(set.contains(ExampleFlags::B));
    EXPECT_FALSE(set.contains(ExampleFlags::C));

    auto set2 = EnumSetType::fromRaw(set.toRaw());
    EXPECT_FALSE(set2.isEmpty());
    EXPECT_TRUE(set2.contains(ExampleFlags::D));
    EXPECT_TRUE(set2.contains(ExampleFlags::E));
    EXPECT_FALSE(set2.contains(ExampleFlags::A));
    EXPECT_FALSE(set2.contains(ExampleFlags::B));
    EXPECT_FALSE(set2.contains(ExampleFlags::C));
}

TEST(WTF_EnumSet, TwoIteratorsIntoSameEnumSet)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::C, ExampleFlags::B };
    typename EnumSetType::iterator it1 = set.begin();
    typename EnumSetType::iterator it2 = it1;
    ++it1;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it1);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::B, *it2);
}

TEST(WTF_EnumSet, IterateOverEnumSetThatContainsTwoFlags)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::C };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::A, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::C, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, IterateOverEnumSetThatContainsFlags2)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::D, ExampleFlags::E };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::D, *it);
    ++it;
    EXPECT_STRONG_ENUM_EQ(ExampleFlags::E, *it);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, NextItemAfterLargestIn32BitFlagSet)
{
    enum class ThirtyTwoBitFlags : uint32_t {
        A = 31,
    };
    using EnumSetType = EnumSet<ThirtyTwoBitFlags>;
    EnumSetType set { ThirtyTwoBitFlags::A };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, NextItemAfterLargestIn64BitFlagSet)
{
    enum class SixtyFourBitFlags : uint32_t {
        A = 63,
    };
    using EnumSetType = EnumSet<SixtyFourBitFlags>;
    EnumSetType set { SixtyFourBitFlags::A };
    typename EnumSetType::iterator it = set.begin();
    typename EnumSetType::iterator end = set.end();
    EXPECT_TRUE(it != end);
    ++it;
    EXPECT_TRUE(it == end);
}

TEST(WTF_EnumSet, IterationOrderTheSameRegardlessOfInsertionOrder)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set1 = ExampleFlags::C;
    set1.add(ExampleFlags::A);

    EnumSetType set2 = ExampleFlags::A;
    set2.add(ExampleFlags::C);

    typename EnumSetType::iterator it1 = set1.begin();
    typename EnumSetType::iterator it2 = set2.begin();

    EXPECT_TRUE(*it1 == *it2);
    ++it1;
    ++it2;
    EXPECT_TRUE(*it1 == *it2);
}

TEST(WTF_EnumSet, OperatorAnd)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType a { ExampleFlags::A };
    EnumSetType ac { ExampleFlags::A, ExampleFlags::C };
    EnumSetType bc { ExampleFlags::B, ExampleFlags::C };
    {
        auto set = a & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = a & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac & bc;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ExampleFlags::A & bc;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ExampleFlags::A & ac;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = bc & ExampleFlags::A;
        EXPECT_FALSE(!!set);
        EXPECT_TRUE(set.isEmpty());
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac & ExampleFlags::A;
        EXPECT_TRUE(!!set);
        EXPECT_FALSE(set.isEmpty());
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
}

TEST(WTF_EnumSet, OperatorXor)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType a { ExampleFlags::A };
    EnumSetType ac { ExampleFlags::A, ExampleFlags::C };
    EnumSetType bc { ExampleFlags::B, ExampleFlags::C };
    {
        auto set = a ^ ac;
        EXPECT_FALSE(set.contains(ExampleFlags::A));
        EXPECT_FALSE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = a ^ bc;
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_TRUE(set.contains(ExampleFlags::C));
    }
    {
        auto set = ac ^ bc;
        EXPECT_TRUE(set.contains(ExampleFlags::A));
        EXPECT_TRUE(set.contains(ExampleFlags::B));
        EXPECT_FALSE(set.contains(ExampleFlags::C));
    }
}

TEST(WTF_EnumSet, ContainsAny)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B };

    EXPECT_TRUE(set.containsAny({ ExampleFlags::A }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::B }));
    EXPECT_FALSE(set.containsAny({ ExampleFlags::C }));
    EXPECT_FALSE(set.containsAny({ ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::B, ExampleFlags::C }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::C }));
    EXPECT_TRUE(set.containsAny({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
}

TEST(WTF_EnumSet, ContainsAll)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::A, ExampleFlags::B };

    EXPECT_TRUE(set.containsAll({ ExampleFlags::A }));
    EXPECT_TRUE(set.containsAll({ ExampleFlags::B }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::C, ExampleFlags::D }));
    EXPECT_TRUE(set.containsAll({ ExampleFlags::A, ExampleFlags::B }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::B, ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::C }));
    EXPECT_FALSE(set.containsAll({ ExampleFlags::A, ExampleFlags::B, ExampleFlags::C }));
}

TEST(WTF_EnumSet, ToSingleValue)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set { ExampleFlags::D };

    EXPECT_TRUE(set.toSingleValue());
    EXPECT_TRUE(set.toSingleValue().value() == ExampleFlags::D);

    set.add(ExampleFlags::A);

    EXPECT_FALSE(set.toSingleValue());

    set.remove(ExampleFlags::D);

    EXPECT_TRUE(set.toSingleValue());
    EXPECT_TRUE(set.toSingleValue().value() == ExampleFlags::A);

    set = { };

    EXPECT_FALSE(set.toSingleValue());
}

TEST(WTF_EnumSet, Size)
{
    using EnumSetType = EnumSet<ExampleFlags>;
    EnumSetType set;

    EXPECT_EQ(set.size(), 0u);
    set.add({ ExampleFlags::A, ExampleFlags::D });
    EXPECT_EQ(set.size(), 2u);
    set.remove(ExampleFlags::A);
    EXPECT_EQ(set.size(), 1u);
}

TEST(WTF_EnumSet, StorageSize)
{
    {
        enum class Enum : uint8_t {
            A,
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
    {
        enum class Enum : uint8_t {
            A,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 1u);
    }
    {
        enum class Enum : uint8_t {
            A = 7,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 1u);
    }
    {
        enum class Enum : uint8_t {
            A = 8,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 2u);
    }
    {
        enum class Enum : uint8_t {
            A = 15,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 2u);
    }
    {
        enum class Enum : uint8_t {
            A = 16,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 4u);
    }
    {
        enum class Enum : uint8_t {
            A = 31,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 4u);
    }
    {
        enum class Enum : uint8_t {
            A = 32,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
    {
        enum class Enum : uint8_t {
            A = 63,
            HighestEnumValue = A
        };
        EXPECT_EQ(sizeof(EnumSet<Enum>::StorageType), 8u);
    }
}

} // namespace TestWebKitAPI
