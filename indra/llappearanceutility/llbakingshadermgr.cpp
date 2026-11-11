
/**
 * @file LLBakingShaderMgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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


#include "linden_common.h"

#include "llbakingshadermgr.h"

#include "alrendercapabilities.h"
#include "lldir.h"
#include "llvertexbuffer.h"

using std::make_pair;

bool                LLBakingShaderMgr::sInitialized = false;

//utility shaders
LLGLSLShader    gAlphaMaskProgram;


// static
LLBakingShaderMgr * LLBakingShaderMgr::instance()
{
    if(nullptr == sInstance)
    {
        sInstance = new LLBakingShaderMgr();
    }

    return static_cast<LLBakingShaderMgr*>(sInstance);
}

void LLBakingShaderMgr::initAttribsAndUniforms(void)
{
    if (mReservedAttribs.empty())
    {
        LLShaderMgr::initAttribsAndUniforms();
    }
}


//============================================================================
// Shader Management

bool LLBakingShaderMgr::setShaders()
{
    if (!sInitialized || !gRenderCapabilities.isMetal() || gGLManager.mInited)
    {
        LL_WARNS("ShaderLoading")
            << "The appearance utility requires the initialized native Metal backend"
            << LL_ENDL;
        return false;
    }

    // The original utility has one alpha-mask program, sampling only diffuseMap.
    // Native capture owns its executable shader, matrices, sampling and blend state;
    // this object remains the pass identity used by LLTexLayerSetBuffer.
    LLGLSLShader::sIndexedTextureChannels = 1;
    initAttribsAndUniforms();
    LLVertexBuffer::unbind();
    LL_INFOS("ShaderLoading")
        << "Configuring the native appearance alpha-mask pass without GLSL"
        << LL_ENDL;
    return loadShadersInterface();
}

void LLBakingShaderMgr::unloadShaders()
{
    gAlphaMaskProgram.unload();
}

bool LLBakingShaderMgr::loadShadersInterface()
{
    gAlphaMaskProgram.mName = "Alpha Mask Shader";
    gAlphaMaskProgram.mShaderFiles.clear();
    gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskV.glsl", GL_VERTEX_SHADER));
    gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskF.glsl", GL_FRAGMENT_SHADER));
    gAlphaMaskProgram.mShaderLevel = 2;
    gAlphaMaskProgram.setNativeAttributeMask(
        LLVertexBuffer::MAP_VERTEX |
        LLVertexBuffer::MAP_TEXCOORD0 |
        LLVertexBuffer::MAP_COLOR);

    if( !gAlphaMaskProgram.createShader() )
    {
        return false;
    }

    return gAlphaMaskProgram.isNativeConfigured();
}

std::string LLBakingShaderMgr::getShaderDirPrefix(void)
{
    return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/class");
}

void LLBakingShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
}
