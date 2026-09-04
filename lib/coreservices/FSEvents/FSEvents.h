/*
 * FSEvents.h -- the File System Events API.
 *
 * CoreServices is not open source and Apple publishes no source for
 * FSEvents, so this is reconstructed from the documented API rather
 * than copied.  The constant values are not from memory or from
 * reading Apple's header: each one was printed by a program compiled
 * against the real framework and the results pasted in, so they are
 * the values the running system actually uses.  The 22 functions are
 * exactly the 22 that FSEvents.framework exports.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FSEVENTS_H__
#define __FSEVENTS_H__

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <sys/types.h>
#include <stdint.h>

/*
 * The open-source CoreFoundation drop this SDK builds its headers from
 * predates CF_ASSUME_NONNULL_BEGIN, so use the underlying clang
 * pragmas directly rather than depending on a macro that may not be
 * there.
 */
#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#define __FSE_NULLABLE _Nullable
#else
#define __FSE_NULLABLE
#endif

__BEGIN_DECLS

typedef struct __FSEventStream *FSEventStreamRef;
typedef const struct __FSEventStream *ConstFSEventStreamRef;

typedef UInt32 FSEventStreamCreateFlags;
typedef UInt32 FSEventStreamEventFlags;
typedef UInt64 FSEventStreamEventId;

/* Flags for FSEventStreamCreate(). */
enum {
	kFSEventStreamCreateFlagNone		= 0x00000000,
	kFSEventStreamCreateFlagUseCFTypes	= 0x00000001,
	kFSEventStreamCreateFlagNoDefer		= 0x00000002,
	kFSEventStreamCreateFlagWatchRoot	= 0x00000004,
	kFSEventStreamCreateFlagIgnoreSelf	= 0x00000008,
	kFSEventStreamCreateFlagFileEvents	= 0x00000010,
	kFSEventStreamCreateFlagMarkSelf	= 0x00000020,
	kFSEventStreamCreateFlagUseExtendedData	= 0x00000040,
	kFSEventStreamCreateFlagFullHistory	= 0x00000080,
	kFSEventStreamCreateWithDocID		= 0x00000100
};

/* Flags handed to the callback for each event. */
enum {
	kFSEventStreamEventFlagNone		 = 0x00000000,
	kFSEventStreamEventFlagMustScanSubDirs	 = 0x00000001,
	kFSEventStreamEventFlagUserDropped	 = 0x00000002,
	kFSEventStreamEventFlagKernelDropped	 = 0x00000004,
	kFSEventStreamEventFlagEventIdsWrapped	 = 0x00000008,
	kFSEventStreamEventFlagHistoryDone	 = 0x00000010,
	kFSEventStreamEventFlagRootChanged	 = 0x00000020,
	kFSEventStreamEventFlagMount		 = 0x00000040,
	kFSEventStreamEventFlagUnmount		 = 0x00000080,
	kFSEventStreamEventFlagItemCreated	 = 0x00000100,
	kFSEventStreamEventFlagItemRemoved	 = 0x00000200,
	kFSEventStreamEventFlagItemInodeMetaMod	 = 0x00000400,
	kFSEventStreamEventFlagItemRenamed	 = 0x00000800,
	kFSEventStreamEventFlagItemModified	 = 0x00001000,
	kFSEventStreamEventFlagItemFinderInfoMod = 0x00002000,
	kFSEventStreamEventFlagItemChangeOwner	 = 0x00004000,
	kFSEventStreamEventFlagItemXattrMod	 = 0x00008000,
	kFSEventStreamEventFlagItemIsFile	 = 0x00010000,
	kFSEventStreamEventFlagItemIsDir	 = 0x00020000,
	kFSEventStreamEventFlagItemIsSymlink	 = 0x00040000,
	kFSEventStreamEventFlagOwnEvent		 = 0x00080000,
	kFSEventStreamEventFlagItemIsHardlink	 = 0x00100000,
	kFSEventStreamEventFlagItemIsLastHardlink = 0x00200000,
	kFSEventStreamEventFlagItemCloned	 = 0x00400000
};

#define kFSEventStreamEventIdSinceNow	0xFFFFFFFFFFFFFFFFULL

/*
 * Per-event extended-data keys, used when the stream was created with
 * kFSEventStreamCreateFlagUseExtendedData.
 */
extern const CFStringRef kFSEventStreamEventExtendedDataPathKey;
extern const CFStringRef kFSEventStreamEventExtendedFileIDKey;
extern const CFStringRef kFSEventStreamEventExtendedDocIDKey;

typedef struct {
	CFIndex		version;
	void *__FSE_NULLABLE info;
	CFAllocatorRetainCallBack __FSE_NULLABLE	retain;
	CFAllocatorReleaseCallBack __FSE_NULLABLE	release;
	CFAllocatorCopyDescriptionCallBack __FSE_NULLABLE copyDescription;
} FSEventStreamContext;

typedef void (*FSEventStreamCallback)(ConstFSEventStreamRef streamRef,
    void *__FSE_NULLABLE clientCallBackInfo, size_t numEvents,
    void *eventPaths, const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]);

/* Creating streams. */
extern FSEventStreamRef __FSE_NULLABLE
FSEventStreamCreate(CFAllocatorRef __FSE_NULLABLE allocator,
    FSEventStreamCallback callback,
    FSEventStreamContext *__FSE_NULLABLE context, CFArrayRef pathsToWatch,
    FSEventStreamEventId sinceWhen, CFTimeInterval latency,
    FSEventStreamCreateFlags flags);

extern FSEventStreamRef __FSE_NULLABLE
FSEventStreamCreateRelativeToDevice(CFAllocatorRef __FSE_NULLABLE allocator,
    FSEventStreamCallback callback,
    FSEventStreamContext *__FSE_NULLABLE context, dev_t deviceToWatch,
    CFArrayRef pathsToWatchRelativeToDevice,
    FSEventStreamEventId sinceWhen, CFTimeInterval latency,
    FSEventStreamCreateFlags flags);

/* Lifetime. */
extern void FSEventStreamRetain(FSEventStreamRef streamRef);
extern void FSEventStreamRelease(FSEventStreamRef streamRef);
extern void FSEventStreamInvalidate(FSEventStreamRef streamRef);

/* Scheduling. */
extern void FSEventStreamScheduleWithRunLoop(FSEventStreamRef streamRef,
    CFRunLoopRef runLoop, CFStringRef runLoopMode);
extern void FSEventStreamUnscheduleFromRunLoop(FSEventStreamRef streamRef,
    CFRunLoopRef runLoop, CFStringRef runLoopMode);
extern void FSEventStreamSetDispatchQueue(FSEventStreamRef streamRef,
    dispatch_queue_t __FSE_NULLABLE q);

/* Starting and stopping. */
extern Boolean FSEventStreamStart(FSEventStreamRef streamRef);
extern void FSEventStreamStop(FSEventStreamRef streamRef);
extern void FSEventStreamFlushAsync(FSEventStreamRef streamRef);
extern void FSEventStreamFlushSync(FSEventStreamRef streamRef);

/* Inspecting. */
extern FSEventStreamEventId
FSEventStreamGetLatestEventId(ConstFSEventStreamRef streamRef);
extern dev_t
FSEventStreamGetDeviceBeingWatched(ConstFSEventStreamRef streamRef);
extern CFArrayRef
FSEventStreamCopyPathsBeingWatched(ConstFSEventStreamRef streamRef);
extern CFStringRef
FSEventStreamCopyDescription(ConstFSEventStreamRef streamRef);
extern void FSEventStreamShow(ConstFSEventStreamRef streamRef);

extern Boolean
FSEventStreamSetExclusionPaths(FSEventStreamRef streamRef,
    CFArrayRef pathsToExclude);

/* Device and event-id queries. */
extern FSEventStreamEventId FSEventsGetCurrentEventId(void);
extern CFUUIDRef __FSE_NULLABLE FSEventsCopyUUIDForDevice(dev_t dev);
extern FSEventStreamEventId
FSEventsGetLastEventIdForDeviceBeforeTime(dev_t dev, CFAbsoluteTime time);
extern Boolean
FSEventsPurgeEventsForDeviceUpToEventId(dev_t dev,
    FSEventStreamEventId eventId);

__END_DECLS

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif
#undef __FSE_NULLABLE

#endif /* __FSEVENTS_H__ */
