/**
 * @file llmacosaccessibility.h
 * @brief Development-only projection of actionable LLUI controls into macOS AX.
 */

#pragma once

class LLViewerWindow;

void ll_install_macos_llui_accessibility(LLViewerWindow* viewer_window);
void ll_uninstall_macos_llui_accessibility(LLViewerWindow* viewer_window);
