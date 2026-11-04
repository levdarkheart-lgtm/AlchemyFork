/**
 * @file lldrawpoolmaterials.h
 * @brief LLDrawPoolMaterials class definition
 * @author Jonathan "Geenz" Goodman
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLDRAWPOOLMATERIALS_H
#define LL_LLDRAWPOOLMATERIALS_H

#include "v4coloru.h"
#include "v2math.h"
#include "v3math.h"
#include "llvertexbuffer.h"
#include "lldrawpool.h"

#include <array>

class LLViewerTexture;
class LLDrawInfo;
class LLGLSLShader;

class LLDrawPoolMaterials : public LLRenderPass
{
    LLGLSLShader *mShader;

    // True when this pass's render map is empty (no geometry). begin/render/end all
    // consult it so an empty pass skips the deferred shader bind/unbind entirely --
    // modern PBR/simple scenes leave most of the 12 legacy material passes empty.
    bool isPassEmpty(S32 pass);
public:
    LLDrawPoolMaterials();

    struct DeferredPassDescription
    {
        U32 render_type;
        U32 shader_mask;

        constexpr U32 alphaMode() const
        {
            return shader_mask & 0x3u;
        }

        constexpr bool hasNormalMap() const
        {
            return (shader_mask & 0x8u) != 0;
        }

        constexpr bool hasSpecularMap() const
        {
            return (shader_mask & 0x4u) != 0;
        }
    };

    // One authoritative description of the legacy material render maps and
    // shader feature masks. The GL draw pool and direct Metal scene traversal
    // both consume this table so their alpha/normal/specular semantics cannot
    // drift during the renderer cutover. Rigged maps use render_type + 1.
    inline static constexpr std::array<DeferredPassDescription, 12>
        DEFERRED_PASSES{{
            { PASS_MATERIAL, 0 },
            { PASS_MATERIAL_ALPHA_MASK, 2 },
            { PASS_MATERIAL_ALPHA_EMISSIVE, 3 },
            { PASS_SPECMAP, 4 },
            { PASS_SPECMAP_MASK, 6 },
            { PASS_SPECMAP_EMISSIVE, 7 },
            { PASS_NORMMAP, 8 },
            { PASS_NORMMAP_MASK, 10 },
            { PASS_NORMMAP_EMISSIVE, 11 },
            { PASS_NORMSPEC, 12 },
            { PASS_NORMSPEC_MASK, 14 },
            { PASS_NORMSPEC_EMISSIVE, 15 },
        }};

    enum
    {
        VERTEX_DATA_MASK =  LLVertexBuffer::MAP_VERTEX |
        LLVertexBuffer::MAP_NORMAL |
        LLVertexBuffer::MAP_TEXCOORD0 |
        LLVertexBuffer::MAP_TEXCOORD1 |
        LLVertexBuffer::MAP_TEXCOORD2 |
        LLVertexBuffer::MAP_COLOR |
        LLVertexBuffer::MAP_TANGENT
    };

    U32 getVertexDataMask() override { return VERTEX_DATA_MASK; }

    void render(S32 pass = 0) override { }
    S32  getNumPasses() override {return 0;}
    void prerender() override;

    S32 getNumDeferredPasses() override;
    void beginDeferredPass(S32 pass) override;
    void endDeferredPass(S32 pass) override;
    void renderDeferred(S32 pass) override;
};

#endif //LL_LLDRAWPOOLMATERIALS_H
