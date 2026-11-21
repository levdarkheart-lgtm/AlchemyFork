/**
 * @file llfeaturemanager.cpp
 * @brief LLFeatureManager class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include <iostream>
#include <fstream>

#include "llfeaturemanager.h"
#include "pipeline.h"
#include "lldir.h"

#include "alrendercapabilities.h"
#include "llgl.h"

#include "llviewercontrol.h"
#include "llcontrol.h"
#include "llstring.h"
#include "llstl.h"
#include "stringize.h"


#if LL_DARWIN
const char FEATURE_TABLE_FILENAME[] = "featuretable_mac.txt";
#elif LL_LINUX
const char FEATURE_TABLE_FILENAME[] = "featuretable_linux.txt";
#else
const char FEATURE_TABLE_FILENAME[] = "featuretable.txt";
#endif

LLFeatureInfo::LLFeatureInfo(const std::string& name, const bool available, const F32 level)
    : mValid(true), mName(name), mAvailable(available), mRecommendedLevel(level)
{
}

LLFeatureList::LLFeatureList(const std::string& name)
    : mName(name)
{
}

LLFeatureList::~LLFeatureList()
{
}

void LLFeatureList::addFeature(const std::string& name, const bool available, const F32 level)
{
    if (mFeatures.count(name))
    {
        LL_WARNS("RenderInit") << "LLFeatureList::Attempting to add preexisting feature " << name << LL_ENDL;
    }

    LLFeatureInfo fi(name, available, level);
    LL_DEBUGS_ONCE("RenderInit") << "Feature '" << name << "' "
                                 << (available ? "" : "not " ) << "available"
                                 << " at " << level
                                 << LL_ENDL;
    mFeatures[name] = fi;
}

bool LLFeatureList::isFeatureAvailable(const std::string& name)
{
    if (mFeatures.count(name))
    {
        return mFeatures[name].mAvailable;
    }

    LL_WARNS_ONCE("RenderInit") << "Feature " << name << " not on feature list!" << LL_ENDL;

    // changing this to true so you have to explicitly disable
    // something for it to be disabled
    return true;
}

F32 LLFeatureList::getRecommendedValue(const std::string& name)
{
    if (mFeatures.count(name) && isFeatureAvailable(name))
    {
        LL_DEBUGS_ONCE("RenderInit") << "Setting '" << name << "' to recommended value " <<  mFeatures[name].mRecommendedLevel << LL_ENDL;
        return mFeatures[name].mRecommendedLevel;
    }

    LL_WARNS_ONCE("RenderInit") << "Feature " << name << " not on feature list or not available!" << LL_ENDL;
    return 0;
}

bool LLFeatureList::maskList(LLFeatureList &mask)
{
    LL_DEBUGS_ONCE() << "Masking with " << mask.mName << LL_ENDL;
    //
    // Lookup the specified feature mask, and overlay it on top of the
    // current feature mask.
    //

    LLFeatureInfo mask_fi;

    feature_map_t::iterator feature_it;
    for (feature_it = mask.mFeatures.begin(); feature_it != mask.mFeatures.end(); ++feature_it)
    {
        mask_fi = feature_it->second;
        //
        // Look for the corresponding feature
        //
        if (!mFeatures.count(mask_fi.mName))
        {
            LL_WARNS("RenderInit") << "Feature " << mask_fi.mName << " in mask not in top level!" << LL_ENDL;
            continue;
        }

        LLFeatureInfo &cur_fi = mFeatures[mask_fi.mName];
        if (mask_fi.mAvailable && !cur_fi.mAvailable)
        {
            LL_WARNS("RenderInit") << "Mask attempting to reenabling disabled feature, ignoring " << cur_fi.mName << LL_ENDL;
            continue;
        }
        cur_fi.mAvailable = mask_fi.mAvailable;
        cur_fi.mRecommendedLevel = llmin(cur_fi.mRecommendedLevel, mask_fi.mRecommendedLevel);
        LL_DEBUGS("RenderInit") << "Feature mask " << mask.mName
                << " Feature " << mask_fi.mName
                << " Mask: " << mask_fi.mRecommendedLevel
                << " Now: " << cur_fi.mRecommendedLevel << LL_ENDL;
    }

    LL_DEBUGS("RenderInit") << "After applying mask " << mask.mName << std::endl;
        // Will conditionally call dump only if the above message will be logged, thanks
        // to it being wrapped by the LL_DEBUGS and LL_ENDL macros.
        dump();
    LL_CONT << LL_ENDL;

    return true;
}

void LLFeatureList::dump()
{
    LL_DEBUGS("RenderInit") << "Feature list: " << mName << LL_ENDL;

    LLFeatureInfo fi;
    feature_map_t::iterator feature_it;
    for (feature_it = mFeatures.begin(); feature_it != mFeatures.end(); ++feature_it)
    {
        fi = feature_it->second;
        LL_DEBUGS("RenderInit") << "With " << mName << " feature " << fi.mName << " " << fi.mAvailable << ":" << fi.mRecommendedLevel << LL_ENDL;
    }
}

static const std::vector<std::string> sGraphicsLevelNames = {
    "Low",
    "LowMid",
    "Mid",
    "MidHigh",
    "High",
    "HighUltra",
    "Ultra"
};

U32 LLFeatureManager::getMaxGraphicsLevel() const
{
    return static_cast<U32>(sGraphicsLevelNames.size()) - 1;
}

bool LLFeatureManager::isValidGraphicsLevel(U32 level) const
{
    return (level <= getMaxGraphicsLevel());
}

std::string LLFeatureManager::getNameForGraphicsLevel(U32 level) const
{
    if (isValidGraphicsLevel(level))
    {
        return sGraphicsLevelNames[level];
    }
    return STRINGIZE("Invalid graphics level " << level << ", valid are 0 .. "
                     << getMaxGraphicsLevel());
}

S32 LLFeatureManager::getGraphicsLevelForName(const std::string& name) const
{
    const std::string FixedFunction("FixedFunction");
    std::string rname(name);
    if (LLStringUtil::endsWith(rname, FixedFunction))
    {
        // chop off any "FixedFunction" suffix
        rname = rname.substr(0, rname.length() - FixedFunction.length());
    }
    for (S32 i(0), iend(getMaxGraphicsLevel()); i <= iend; ++i)
    {
        if (sGraphicsLevelNames[i] == rname)
        {
            return i;
        }
    }
    return -1;
}

LLFeatureList *LLFeatureManager::findMask(const std::string& name)
{
    if (mMaskList.count(name))
    {
        return mMaskList[name];
    }

    return NULL;
}

bool LLFeatureManager::maskFeatures(const std::string& name)
{
    LLFeatureList *maskp = findMask(name);
    if (!maskp)
    {
        LL_DEBUGS("RenderInit") << "Unknown feature mask " << name << LL_ENDL;
        return false;
    }
    LL_INFOS("RenderInit") << "Applying GPU Feature list: " << name << LL_ENDL;
    return maskList(*maskp);
}

bool LLFeatureManager::loadFeatureTables()
{
    // *TODO - if I or anyone else adds something else to the skipped list
    // make this data driven.  Put it in the feature table and parse it
    // correctly
    mSkippedFeatures.insert("RenderGamma");
    mSkippedFeatures.insert("RenderFogRatio");

    // first table is install with app
    std::string app_path = gDirUtilp->getAppRODataDir();
    app_path += gDirUtilp->getDirDelimiter();

    std::string filename;
    filename = FEATURE_TABLE_FILENAME;

    app_path += filename;

    bool parse_ok = parseFeatureTable(app_path);

    return parse_ok;
}


bool LLFeatureManager::parseFeatureTable(std::string filename)
{
    LL_INFOS("RenderInit") << "Attempting to parse feature table from " << filename << LL_ENDL;

    llifstream file;
    std::string name;
    U32     version;

    cleanupFeatureTables(); // in case an earlier attempt left partial results
    file.open(filename.c_str());     /*Flawfinder: ignore*/

    if (!file)
    {
        LL_WARNS("RenderInit") << "Unable to open feature table " << filename << LL_ENDL;
        return false;
    }

    // Check file version
    file >> name;
    if (name != "version")
    {
        LL_WARNS("RenderInit") << filename << " does not appear to be a valid feature table!" << LL_ENDL;
        return false;
    }
    file >> version;

    mTableVersion = version;
    LL_INFOS("RenderInit") << "Found feature table version " << version << LL_ENDL;

    LLFeatureList *flp = NULL;
    bool parse_ok = true;
    while (parse_ok && file >> name )
    {
        char buffer[MAX_STRING];         /*Flawfinder: ignore*/

        if (name.substr(0,2) == "//")
        {
            // This is a comment.
            file.getline(buffer, MAX_STRING);
            continue;
        }

        if (name == "list")
        {
            LL_DEBUGS("RenderInit") << "Before new list" << std::endl;
            if (flp)
            {
                flp->dump();
            }
            else
            {
                LL_CONT << "No current list";
            }
            LL_CONT << LL_ENDL;

            // It's a new mask, create it.
            file >> name;
            if (!mMaskList.count(name))
            {
                flp = new LLFeatureList(name);
                mMaskList[name] = flp;
            }
            else
            {
                LL_WARNS("RenderInit") << "Overriding mask '" << name << "'; this is invalid!" << LL_ENDL;
                parse_ok = false;
            }
        }
        else
        {
            if (flp)
            {
                S32 available;
                F32 recommended;
                file >> available >> recommended;
                flp->addFeature(name, available, recommended);
            }
            else
            {
                LL_WARNS("RenderInit") << "Specified parameter before <list> keyword!" << LL_ENDL;
                parse_ok = false;
            }
        }
    }
    file.close();

    if (!parse_ok)
    {
        LL_WARNS("RenderInit") << "Discarding feature table data from " << filename << LL_ENDL;
        cleanupFeatureTables();
    }

    return parse_ok;
}

bool LLFeatureManager::loadGPUClass()
{
    mGPUClass = GPU_CLASS_UNKNOWN;
    mGPUMemoryBandwidth = -1.f;
    mGPUString.clear();
    if (!gRenderCapabilities.isMetal())
    {
        LL_WARNS("RenderInit") << "GPU classification requires initialized native Metal capabilities"
                              << LL_ENDL;
        return false;
    }

    // Preserve the established Apple-GPU quality class. The manual native
    // texture benchmark remains a developer tool, not a startup classifier
    // or a measurement of physical GPU memory bandwidth.
    mGPUClass = GPU_CLASS_3;
    mGPUString = gRenderCapabilities.renderer();
    LL_INFOS("RenderInit") << "Native Metal renderer " << mGPUString
                         << "; selecting established Apple GPU class 3"
                         << LL_ENDL;
    return true;
}

void LLFeatureManager::cleanupFeatureTables()
{
    std::for_each(mMaskList.begin(), mMaskList.end(), DeletePairedPointer());
    mMaskList.clear();
}

void LLFeatureManager::initSingleton()
{
    // load the tables
    loadFeatureTables();

    // get the gpu class
    loadGPUClass();

    // apply the base masks, so we know if anything is disabled
    applyBaseMasks();
}

void LLFeatureManager::applyRecommendedSettings()
{
    // apply saved settings
    // cap the level at 2 (high)
    U32 level = llmax(GPU_CLASS_0, llmin(mGPUClass, GPU_CLASS_5));

    LL_INFOS("RenderInit") << "Applying Recommended Features for level " << level << LL_ENDL;

    setGraphicsLevel(level, false);
    gSavedSettings.setU32("RenderQualityPerformance", level);

    // now apply the tweaks to draw distance
    // these are double negatives, because feature masks only work by
    // downgrading values, so i needed to make a true value go to false
    // for certain cards, thus the awkward name, "Disregard..."
    if(!gSavedSettings.getBOOL("Disregard96DefaultDrawDistance"))
    {
        gSavedSettings.setF32("RenderFarClip", 96.0f);
    }
    else if(!gSavedSettings.getBOOL("Disregard128DefaultDrawDistance"))
    {
        gSavedSettings.setF32("RenderFarClip", 128.0f);
    }
}

// Sets the control a feature names to a value, in the control's own type.
static void set_feature_control(const std::string& name, F32 value)
{
    LLControlVariable* ctrl = gSavedSettings.getControl(name);
    if(ctrl == NULL)
    {
        LL_WARNS("RenderInit") << "AHHH! Control setting " << name << " does not exist!" << LL_ENDL;
        return;
    }

    // handle all the different types
    if(ctrl->isType(TYPE_BOOLEAN))
    {
        gSavedSettings.setBOOL(name, (bool)value);
    }
    else if (ctrl->isType(TYPE_S32))
    {
        gSavedSettings.setS32(name, (S32)value);
    }
    else if (ctrl->isType(TYPE_U32))
    {
        gSavedSettings.setU32(name, (U32)value);
    }
    else if (ctrl->isType(TYPE_F32))
    {
        gSavedSettings.setF32(name, value);
    }
    else
    {
        LL_WARNS("RenderInit") << "AHHH! Control variable is not a numeric type!" << LL_ENDL;
    }
}

void LLFeatureManager::applyFeatures(bool skipFeatures)
{
    // see featuretable.txt / featuretable_linux.txt / featuretable_mac.txt

#ifndef LL_RELEASE_FOR_DOWNLOAD
    dump();
#endif

    // scroll through all of these and set their corresponding control value
    for(feature_map_t::iterator mIt = mFeatures.begin();
        mIt != mFeatures.end();
        ++mIt)
    {
        // skip features you want to skip
        // do this for when you don't want to change certain settings
        if(skipFeatures)
        {
            if(mSkippedFeatures.find(mIt->first) != mSkippedFeatures.end())
            {
                continue;
            }
        }

        set_feature_control(mIt->first, getRecommendedValue(mIt->first));
    }
}

void LLFeatureManager::holdUnavailable(const std::string& name)
{
    LLFeatureList* maskp = findMask(name);
    if (!maskp)
    {
        return;
    }
    for (const auto& [feature, info] : maskp->getFeatures())
    {
        if (!info.mAvailable)
        {
            set_feature_control(feature, info.mRecommendedLevel);
        }
    }
}

void LLFeatureManager::setGraphicsLevel(U32 level, bool skipFeatures)
{
    flush_glerror(); // Whatever may have already happened (e.g., to cause us to change), don't let confuse it with new initializations.
    applyBaseMasks();

    // if we're passed an invalid level, default to "Low"
    std::string features(isValidGraphicsLevel(level)? getNameForGraphicsLevel(level) : "Low");

    maskFeatures(features);

    // Each feature's setting marks what it changes as it is set; the frame applies them all with one reload, and the
    // reload asked for here as well covers what changed without a listener.
    applyFeatures(skipFeatures);

    gPipeline.markRenderInputsDirty();
    gPipeline.refreshCachedSettings();
}

void LLFeatureManager::applyBaseMasks()
{
    // reapply masks
    mFeatures.clear();

    LLFeatureList* maskp = findMask("all");
    if(maskp == NULL)
    {
        LL_WARNS("RenderInit") << "AHH! No \"all\" in feature table!" << LL_ENDL;
        return;
    }

    mFeatures = maskp->getFeatures();

    // The class has no list of its own: it picks the graphics level (applyRecommendedSettings).
    if (mGPUClass >= 0 && mGPUClass < 6)
    {
        LL_INFOS("RenderInit") << "Setting GPU Class to Class" << (S32)mGPUClass << LL_ENDL;
    }
    else
    {
        LL_INFOS("RenderInit") << "Setting GPU Class to Unknown" << LL_ENDL;
        maskFeatures("Unknown");
    }

    // now all those wacky ones
    const bool have_renderer_caps = gRenderCapabilities.isInitialized();
    const bool is_nvidia = have_renderer_caps ? gRenderCapabilities.isNVIDIA() : gGLManager.mIsNVIDIA;
    const bool is_amd = have_renderer_caps ? gRenderCapabilities.isAMD() : gGLManager.mIsAMD;
    const bool is_intel = have_renderer_caps ? gRenderCapabilities.isIntel() : gGLManager.mIsIntel;
    const bool is_apple = have_renderer_caps ? gRenderCapabilities.isApple() : gGLManager.mIsApple;

    if (is_nvidia)
    {
        maskFeatures("NVIDIA");
    }
    if (is_amd)
    {
        maskFeatures("AMD");
    }
    if (is_intel)
    {
        maskFeatures("Intel");
    }
    if (is_apple)
    {
        maskFeatures("AppleGPU");
    }
    else
    {
        maskFeatures("NonAppleGPU");
    }
    // These masks constrain the retained GLSL layout. Native Metal pipelines
    // have explicit resource and stage layouts, so GL unit/varying limits do
    // not describe their capability.
    const bool apply_legacy_shader_limits = !have_renderer_caps ||
        gRenderCapabilities.hasLegacyShaderLimits();
    const S32 texture_image_units = have_renderer_caps
        ? gRenderCapabilities.textureImageUnits() : gGLManager.mNumTextureImageUnits;
    const S32 max_varying_vectors = have_renderer_caps
        ? gRenderCapabilities.maxVaryingVectors() : gGLManager.mMaxVaryingVectors;
    const bool has_anisotropic = have_renderer_caps
        ? gRenderCapabilities.hasAnisotropicFiltering() : gGLManager.mHasAnisotropic;
    const F32 max_anisotropy = have_renderer_caps
        ? gRenderCapabilities.maxAnisotropy() : gGLManager.mMaxAnisotropy;

    // GL 4.1 has at least 16 texture units, so this is the fewest a context past initGL has.
    if (apply_legacy_shader_limits && texture_image_units <= 16)
    {
        maskFeatures("TexUnit16orLess");
    }
    if (!has_anisotropic || 2.f > max_anisotropy)
    {
        maskFeatures("AnisotropicMissing");
    }
    if (gGLManager.isIntelLegacy())
    {
        // A lot of these chips claim 4.3 or 4.4 support, but don't seem to work.
        // https://code.blender.org/2019/04/supported-gpus-in-blender-2-80/
        // https://docs.blender.org/manual/en/latest/troubleshooting/gpu/windows/intel.html#legacy-intel-hd-4000-5000
        // https://www.intel.com/content/www/us/en/support/articles/000005524/graphics.html
        LL_INFOS("RenderInit") << "Intel driver below OpenGL 4.6: holding to the IntelLegacy list" << LL_ENDL;
        maskFeatures("IntelLegacy");

        // At every start, before anything reads the settings, and not only when a graphics level is
        // applied: what the list holds is held for a user who already had it on, as the false GL
        // version once held it whatever the settings said. The upload threads, vintage mode, the
        // probes and the waves are all on the list.
        holdUnavailable("IntelLegacy");

        // HDR is on no list: vintage mode switches it, and only when vintage mode changes.
        gSavedSettings.setBOOL("RenderHDREnabled", false);
    }
    if (apply_legacy_shader_limits && max_varying_vectors <= 16)
    {
        maskFeatures("VaryingVectors16orLess");
    }

    // now mask by gpu string
    // Replaces ' ' with '_' in mGPUString to deal with inability for parser to handle spaces
    std::string gpustr = mGPUString;
    for (std::string::iterator iter = gpustr.begin(); iter != gpustr.end(); ++iter)
    {
        if (*iter == ' ')
        {
            *iter = '_';
        }
    }

    //LL_INFOS() << "Masking features from gpu table match: " << gpustr << LL_ENDL;
    maskFeatures(gpustr);

    if (isSafe())
    {
        maskFeatures("safe");
    }
}

LLSD LLFeatureManager::getRecommendedSettingsMap()
{
    // Create the map and fill it with the hardware recommended settings.
    // It's needed to create an initial Default graphics preset (MAINT-6435).
    // The process is similar to the one LLFeatureManager::applyRecommendedSettings() does.

    LLSD map(LLSD::emptyMap());

    U32 level = llmax(GPU_CLASS_0, llmin(mGPUClass, GPU_CLASS_5));
    LL_INFOS("RenderInit") << "Getting the map of recommended settings for level " << level << LL_ENDL;

    std::string features(isValidGraphicsLevel(level) ? getNameForGraphicsLevel(level) : "Low");

    maskFeatures(features);

    LLControlVariable* ctrl = gSavedSettings.getControl("RenderQualityPerformance"); // include the quality value for correct preset loading
    map["RenderQualityPerformance"]["Value"] = (LLSD::Integer)level;
    map["RenderQualityPerformance"]["Comment"] = ctrl->getComment();;
    map["RenderQualityPerformance"]["Persist"] = 1;
    map["RenderQualityPerformance"]["Type"] = LLControlGroup::typeEnumToString(ctrl->type());



    for (feature_map_t::iterator mIt = mFeatures.begin(); mIt != mFeatures.end(); ++mIt)
    {
        LLControlVariable* ctrl = gSavedSettings.getControl(mIt->first);
        if (ctrl == NULL)
        {
            LL_WARNS("RenderInit") << "AHHH! Control setting " << mIt->first << " does not exist!" << LL_ENDL;
            continue;
        }

        if (ctrl->isType(TYPE_BOOLEAN))
        {
            map[mIt->first]["Value"] = (LLSD::Boolean)getRecommendedValue(mIt->first);
        }
        else if (ctrl->isType(TYPE_S32) || ctrl->isType(TYPE_U32))
        {
            map[mIt->first]["Value"] = (LLSD::Integer)getRecommendedValue(mIt->first);
        }
        else if (ctrl->isType(TYPE_F32))
        {
            map[mIt->first]["Value"] = (LLSD::Real)getRecommendedValue(mIt->first);
        }
        else
        {
            LL_WARNS("RenderInit") << "AHHH! Control variable is not a numeric type!" << LL_ENDL;
            continue;
        }
        map[mIt->first]["Comment"] = ctrl->getComment();;
        map[mIt->first]["Persist"] = 1;
        map[mIt->first]["Type"] = LLControlGroup::typeEnumToString(ctrl->type());
    }

    return map;
}
