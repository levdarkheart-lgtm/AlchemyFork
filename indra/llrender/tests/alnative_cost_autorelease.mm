/**
 * @file alnative_cost_autorelease.mm
 * @brief Test-only frame-local pool around a C++ owner observation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#import <Foundation/Foundation.h>

void alTestWithAutoreleasePool(void (*body)(void*), void* context)
{
    @autoreleasepool { body(context); }
}
