/*
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

#pragma once

#include "BaselineAlignment.h"
#include "GridLayoutFunctions.h"
#include "RenderStyleConstants.h"
#include "WritingMode.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

// This is the class that implements the Baseline Alignment logic, using internally the BaselineAlignmentState and
// BaselineGroup classes.
//
// The first phase is to collect the items that will participate in baseline alignment together. During this
// phase the required baseline-sharing groups will be created for each Baseline alignment-context shared by
// the items participating in the baseline alignment.
//
// Additionally, the baseline-sharing groups' offsets, max-ascend and max-descent will be computed and stored.
// This class also computes the baseline offset for a particular item, based on the max-ascent for its associated
// baseline-sharing group.
class GridBaselineAlignment {
public:
    // Collects the items participating in baseline alignment and updates the corresponding baseline-sharing
    // group of the Baseline Context the items belongs to.
    // All the baseline offsets are updated accordingly based on the added item.
    void updateBaselineAlignmentContext(ItemPosition, unsigned sharedContext, const RenderBox&, Style::GridTrackSizingDirection alignmentContextType);

    // Returns the baseline offset of a particular item, based on the max-ascent for its associated
    // baseline-sharing group
    LayoutUnit baselineOffsetForGridItem(ItemPosition, unsigned sharedContext, const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;

    // Sets the Grid Container's writing mode so that we can avoid the dependecy of the LayoutGrid class for
    // determining whether a grid item is orthogonal or not.
    void setWritingMode(WritingMode writingMode) { m_writingMode = writingMode; };

    // Clearing the Baseline Alignment context and their internal classes and data structures.
    void clear(Style::GridTrackSizingDirection alignmentContextType);

private:
    const BaselineGroup& baselineGroupForGridItem(ItemPosition, unsigned sharedContext, const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;
    LayoutUnit marginOverForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;
    LayoutUnit marginUnderForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;
    LayoutUnit logicalAscentForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType, ItemPosition) const;
    LayoutUnit ascentForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType, ItemPosition) const;
    LayoutUnit descentForGridItem(const RenderBox&, LayoutUnit, Style::GridTrackSizingDirection alignmentContextType, ExtraMarginsFromSubgrids) const;
    bool isDescentBaselineForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;
    bool isVerticalAlignmentContext(Style::GridTrackSizingDirection alignmentContextType) const;
    bool isOrthogonalGridItemForBaseline(const RenderBox&) const;
    bool isParallelToAlignmentAxisForGridItem(const RenderBox&, Style::GridTrackSizingDirection alignmentContextType) const;

    typedef HashMap<unsigned, std::unique_ptr<BaselineAlignmentState>, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> BaselineAlignmentStateMap;

    // Grid Container's writing mode, used to determine grid item's orthogonality.
    WritingMode m_writingMode;
    BaselineAlignmentStateMap m_rowAlignmentContextStates;
    BaselineAlignmentStateMap m_columnAlignmentContextStates;
};

} // namespace WebCore
