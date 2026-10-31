/**
 * @file llmacosaccessibility.cpp
 * @brief Development-only projection of actionable LLUI controls into macOS AX.
 */

#include "llviewerprecompiledheaders.h"

#include "llmacosaccessibility.h"

#if LL_DARWIN

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llrootview.h"
#include "llsdl_macos.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltexteditor.h"
#include "llui.h"
#include "llversioninfo.h"
#include "llviewerwindow.h"

#include <memory>

namespace
{
class LLMacOSLLUIAccessibility
{
public:
    explicit LLMacOSLLUIAccessibility(LLViewerWindow* viewer_window)
        : mViewerWindow(viewer_window)
    {
    }

    void install()
    {
        LLSdlMacOSLLUIAXCallbacks callbacks;
        callbacks.context = this;
        callbacks.get_controls = getControls;
        callbacks.perform_press = performPress;
        callbacks.set_value = setValue;
        ll_sdl_macos_install_llui_accessibility(
            mViewerWindow->getPlatformWindow(), callbacks);
    }

    void uninstall()
    {
        ll_sdl_macos_uninstall_llui_accessibility(this);
    }

private:
    static std::string labelFor(LLView* view)
    {
        std::string label;
        if (LLCheckBoxCtrl* checkbox = view->as<LLCheckBoxCtrl>())
        {
            label = checkbox->getLabel();
        }
        else if (LLButton* button = view->as<LLButton>())
        {
            label = button->getText();
        }
        else if (LLLineEditor* line = view->as<LLLineEditor>())
        {
            label = line->getLabel();
        }
        else if (LLMenuItemGL* item = view->as<LLMenuItemGL>())
        {
            label = item->getLabel();
        }

        LLStringUtil::trim(label);
        if (label.empty())
        {
            label = view->getName();
        }
        if (label.empty())
        {
            label = "Unnamed LLUI control";
        }
        return label;
    }

    static bool describe(LLView* view, LLSdlMacOSLLUIAXControl* control)
    {
        if (!view || !control || !view->isInVisibleChain())
        {
            return false;
        }

        LLUICtrl* ui_control = view->as<LLUICtrl>();
        if (!ui_control)
        {
            return false;
        }

        // Separators are visible LLMenuItemGL instances, but they have no
        // action and must not be advertised as pressable buttons.
        if (view->as<LLMenuItemSeparatorGL>())
        {
            return false;
        }

        if (LLCheckBoxCtrl* checkbox = view->as<LLCheckBoxCtrl>())
        {
            control->role = LLSdlMacOSLLUIAXRole::CHECK_BOX;
            control->checked = checkbox->getValue().asBoolean();
            control->value = checkbox->getValue().asString();
        }
        else if (LLComboBox* combo = view->as<LLComboBox>())
        {
            control->role = LLSdlMacOSLLUIAXRole::COMBO_BOX;
            control->value = combo->getValue().asString();
        }
        else if (LLLineEditor* line = view->as<LLLineEditor>())
        {
            control->role = line->isPasswordField()
                ? LLSdlMacOSLLUIAXRole::SECURE_TEXT_FIELD
                : LLSdlMacOSLLUIAXRole::TEXT_FIELD;
            if (!line->isPasswordField())
            {
                control->value = line->getText();
            }
        }
        else if (LLTextEditor* editor = view->as<LLTextEditor>())
        {
            control->role = LLSdlMacOSLLUIAXRole::TEXT_FIELD;
            control->value = editor->getText();
        }
        else if (view->as<LLSliderCtrl>() || view->as<LLSpinCtrl>())
        {
            control->role = LLSdlMacOSLLUIAXRole::SLIDER;
            control->value = ui_control->getValue().asString();
        }
        else if (view->as<LLButton>() || view->as<LLMenuItemGL>())
        {
            control->role = LLSdlMacOSLLUIAXRole::BUTTON;
            control->value = ui_control->getValue().asString();
        }
        else
        {
            return false;
        }

        const std::string path = view->getPathname();
        if (path.empty())
        {
            return false;
        }
        control->identifier = path;
        control->label = labelFor(view);
        control->help = view->getToolTip();
        control->enabled = view->isInEnabledChain();
        return true;
    }

    void collect(LLView* view,
                 std::vector<LLSdlMacOSLLUIAXControl>* controls) const
    {
        if (!view || !controls || !view->isInVisibleChain())
        {
            return;
        }

        // The login panel keeps its stricter protected-field bridge. Avoid
        // duplicating those controls in the general projection.
        if (view->getName() == "login_panel_holder")
        {
            return;
        }

        LLSdlMacOSLLUIAXControl control;
        if (describe(view, &control))
        {
            const double window_width = mViewerWindow->getWindowWidthScaled();
            const double window_height = mViewerWindow->getWindowHeightScaled();
            const LLRect screen = view->calcScreenRect();
            const double left = llclamp<double>(screen.mLeft, 0.0, window_width);
            const double right = llclamp<double>(screen.mRight, 0.0, window_width);
            const double bottom = llclamp<double>(screen.mBottom, 0.0, window_height);
            const double top = llclamp<double>(screen.mTop, 0.0, window_height);
            if (window_width > 0.0 && window_height > 0.0 &&
                right > left && top > bottom)
            {
                control.rect.x = left / window_width;
                control.rect.y = bottom / window_height;
                control.rect.width = (right - left) / window_width;
                control.rect.height = (top - bottom) / window_height;
                controls->push_back(std::move(control));
            }

            // Composite widgets own actionable implementation children. One
            // native element is clearer and avoids duplicate button/field AX
            // entries for a checkbox, combo, spinner or slider.
            return;
        }

        for (LLView* child : *view->getChildList())
        {
            collect(child, controls);
        }
    }

    static void getControls(
        void* context, std::vector<LLSdlMacOSLLUIAXControl>* controls)
    {
        auto* self = static_cast<LLMacOSLLUIAccessibility*>(context);
        if (!self || !controls || !self->mViewerWindow ||
            !self->mViewerWindow->getRootView())
        {
            return;
        }
        controls->clear();
        self->collect(self->mViewerWindow->getRootView(), controls);
    }

    LLUICtrl* resolve(const std::string& identifier) const
    {
        LLView* root = mViewerWindow ? mViewerWindow->getRootView() : nullptr;
        LLView* view = root
            ? LLUI::getInstance()->resolvePath(root, identifier)
            : nullptr;
        return view && view->isInVisibleChain() ? view->as<LLUICtrl>() : nullptr;
    }

    static bool performPress(void* context, const std::string& identifier)
    {
        auto* self = static_cast<LLMacOSLLUIAccessibility*>(context);
        LLUICtrl* control = self ? self->resolve(identifier) : nullptr;
        if (!control || !control->isInEnabledChain())
        {
            return false;
        }

        if (LLCheckBoxCtrl* checkbox = control->as<LLCheckBoxCtrl>())
        {
            checkbox->toggle();
            checkbox->onCommit();
            return true;
        }
        control->onCommit();
        return true;
    }

    static bool setValue(void* context, const std::string& identifier,
                         const std::string& value)
    {
        auto* self = static_cast<LLMacOSLLUIAccessibility*>(context);
        LLUICtrl* control = self ? self->resolve(identifier) : nullptr;
        if (!control || !control->isInEnabledChain())
        {
            return false;
        }

        if (LLComboBox* combo = control->as<LLComboBox>())
        {
            combo->setValue(value);
            if (combo->getValue().asString() != value)
            {
                return false;
            }
            combo->onCommit();
            return true;
        }
        if (LLLineEditor* line = control->as<LLLineEditor>())
        {
            if (!line->prevalidateInput(value))
            {
                return false;
            }
            line->setText(value);
            line->onCommit();
            return true;
        }
        if (LLTextEditor* editor = control->as<LLTextEditor>())
        {
            if (editor->getReadOnly())
            {
                return false;
            }
            editor->setText(value);
            editor->onCommit();
            return true;
        }
        if (LLCheckBoxCtrl* checkbox = control->as<LLCheckBoxCtrl>())
        {
            const bool checked = value == "1" || value == "true" ||
                                 value == "YES" || value == "yes";
            checkbox->setValue(checked);
            checkbox->onCommit();
            return true;
        }
        if (control->as<LLSliderCtrl>() || control->as<LLSpinCtrl>())
        {
            control->setValue(LLSD(value).asReal());
            control->onCommit();
            return true;
        }
        return false;
    }

    LLViewerWindow* mViewerWindow = nullptr;
};

std::unique_ptr<LLMacOSLLUIAccessibility> sAccessibility;
}

#endif // LL_DARWIN

void ll_install_macos_llui_accessibility(LLViewerWindow* viewer_window)
{
#if LL_DARWIN
    if (!viewer_window || sAccessibility ||
        LLVersionInfo::instance().getChannel() != "Alchemy Metal Development")
    {
        return;
    }
    sAccessibility = std::make_unique<LLMacOSLLUIAccessibility>(viewer_window);
    sAccessibility->install();
#else
    (void)viewer_window;
#endif
}

void ll_uninstall_macos_llui_accessibility(LLViewerWindow* viewer_window)
{
#if LL_DARWIN
    if (!sAccessibility || !viewer_window)
    {
        return;
    }
    sAccessibility->uninstall();
    sAccessibility.reset();
#else
    (void)viewer_window;
#endif
}
