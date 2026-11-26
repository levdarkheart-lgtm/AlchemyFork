/** Native Aurora input convention bridge. SPDX-License-Identifier: LGPL-2.1-only */
#pragma once
#include "almetalaurora.h"

// Viewer reverse-Z clip already has Z in [0,1]. The native raster negates
// clip Y, while stored rows retain viewer UV=clip.xy*.5+.5. Aurora uses
// clip Y-up with UV=(x*.5+.5,.5-y*.5). Apply F=diag(1,-1,1,1) to the
// projection and F*currentToPrevious*F to the reprojection. No Z remap.
inline void alMetalAuroraSetViewerCamera(ALMetalAuroraFrame& frame,
    const float projection[16], const float current_to_previous[16]) noexcept
{
    for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 0; row < 4; ++row)
        {
            const float row_sign = row == 1 ? -1.f : 1.f;
            const float column_sign = column == 1 ? -1.f : 1.f;
            frame.projection[column * 4 + row] = projection[column * 4 + row] * row_sign;
            frame.motion_reprojection[column * 4 + row] =
                current_to_previous[column * 4 + row] * row_sign * column_sign;
        }
}
