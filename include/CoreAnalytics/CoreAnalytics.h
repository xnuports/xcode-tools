/*
 * CoreAnalytics/CoreAnalytics.h
 *
 * ld64's src/ld/Options.cpp includes this Apple private framework
 * header, but references nothing from it -- the include is the only
 * occurrence in the file.  CoreAnalytics has no open-source release, so
 * this empty shim exists purely to let Options.cpp compile.
 *
 * If a future ld64 drop actually calls into CoreAnalytics (telemetry
 * event submission), this file is not enough and the calls will need to
 * be stubbed out deliberately.
 *
 * Copyright (c) 2026 Sunneva N. Mariu
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _XCODE_TOOLS_COREANALYTICS_H_
#define _XCODE_TOOLS_COREANALYTICS_H_

#endif /* _XCODE_TOOLS_COREANALYTICS_H_ */
