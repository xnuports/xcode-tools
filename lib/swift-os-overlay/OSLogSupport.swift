//===----------------------------------------------------------------------===//
//
// OSLogSupport.swift -- the parts of the os overlay that are not upstream.
//
// The interpolation machinery beside this file is Swift's own, from
// stdlib/private/OSLog.  What that module does not carry is the two
// types callers actually name -- OSLog and Logger -- and a real
// emitter: its OSLogTestHelper.swift serialises a message and then
// hands it to a stub instead of the system log.  These are those.
//
// The C entry points are bound by symbol rather than through the
// header.  os/log.h declares os_log_create and OS_LOG_DEFAULT in terms
// of os_log_t, and os_log_t is an Objective-C object type behind
// OS_OBJECT_DECL -- which needs the ObjectiveC module this SDK does
// not declare, so Swift imports none of them.  The symbols themselves
// are in libSystem and in the stub this SDK generates, so they are
// declared here directly and the pointer is kept opaque.
//
// Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

// No `@_exported import os' here: this module *is* os, and the Clang
// module of the same name is pulled in with -import-underlying-module,
// which is how an overlay does it.  Importing by name would be the
// module importing itself.

@_silgen_name("os_log_create")
@usableFromInline
internal func _xt_os_log_create(
  _ subsystem: UnsafePointer<CChar>,
  _ category: UnsafePointer<CChar>
) -> OpaquePointer

@_silgen_name("_os_log_impl")
@usableFromInline
internal func _xt_os_log_impl(
  _ dso: UnsafeRawPointer,
  _ log: OpaquePointer?,
  _ type: UInt8,
  _ format: UnsafePointer<CChar>,
  _ buffer: UnsafeMutablePointer<UInt8>,
  _ size: UInt32
)


/// The log levels, by their ABI values.
///
/// Apple's overlay writes __OS_LOG_TYPE_DEFAULT and friends, the
/// Swift-private spellings of the enumerators in os/log.h.  Those names
/// are not in scope when the module is built with
/// -import-underlying-module, so the values are named here instead;
/// they are fixed by the ABI and are what the header declares.
extension OSLogType {
  // @_transparent computed properties rather than `let'.
  //
  // A public static `let' is a global, and this SDK ships the overlay
  // as an interface with no library behind it, so that global would be
  // a symbol nothing defines.
  //
  // Apple's log calls carry
  // @_semantics("oslog.requires_constant_arguments"), which requires
  // the level to be a literal `let' and rejects a computed property
  // outright.  Those are gone from the methods below.  The attribute
  // exists so the constant-evaluation pass can fold an interpolation
  // into a preformatted buffer at compile time; this _osLogEmit builds
  // its buffer at runtime and folds nothing, so the constraint bought
  // nothing here and cost the levels.
  @_transparent
  public static var `default`: OSLogType { OSLogType(rawValue: 0x00) }
  @_transparent
  public static var info: OSLogType { OSLogType(rawValue: 0x01) }
  @_transparent
  public static var debug: OSLogType { OSLogType(rawValue: 0x02) }
  @_transparent
  public static var error: OSLogType { OSLogType(rawValue: 0x10) }
  @_transparent
  public static var fault: OSLogType { OSLogType(rawValue: 0x11) }
}

/// A log handle: a subsystem and category to write under.
///
/// Apple's is a Swift class over the Objective-C os_log_t.  This one is
/// a frozen struct holding the same pointer opaquely, and every member
/// is inlinable, which is not a stylistic choice.
///
/// This SDK ships an interface for the os overlay and no library to go
/// with it -- the runtime a program links is the system's libswiftos,
/// whose OSLog is Apple's and shares none of these internals.  Anything
/// here that needed an external symbol would compile, link against
/// whatever Apple's library happened to export under the same mangled
/// name, and be wrong.  A class cannot be frozen and a `static let`
/// emits a global, so both are avoided: with the storage inline and the
/// accessors transparent, the only symbols a caller ends up needing are
/// the C entry points in libSystem, which genuinely exist.
///
/// @_transparent rather than @inlinable: the latter is a permission the
/// optimiser may decline, and at -Onone it always does, so a debug
/// build referenced os.Logger.init(subsystem:category:) as an external
/// symbol and failed to link.  @_transparent is substituted before
/// optimisation runs at all.
@frozen
public struct OSLog: @unchecked Sendable {
  @usableFromInline
  internal let _handle: OpaquePointer?

  @usableFromInline
  @_transparent
  internal init(handle: OpaquePointer?) {
    self._handle = handle
  }

  @_transparent
  public init(subsystem: String, category: String) {
    self._handle = subsystem.withCString { s in
      category.withCString { c in
        _xt_os_log_create(s, c)
      }
    }
  }

  /// The process's default handle.  Passing a nil handle to
  /// _os_log_impl is what the C macros do when given OS_LOG_DEFAULT.
  @_transparent
  public static var `default`: OSLog { OSLog(handle: nil) }

  /// A handle that discards everything written to it.
  @_transparent
  public static var disabled: OSLog { OSLog(subsystem: "", category: "") }
}

/// The type used to log through a handle.
///
/// Apple's Logger is a struct over an os_log_t with one method per
/// level, each taking an OSLogMessage -- the interpolation the files
/// beside this one implement, so `\(x, privacy: .public)` works here as
/// it does there.
@frozen
public struct Logger: Sendable {
  @usableFromInline
  internal let _log: OSLog

  @_transparent
  public init() {
    self._log = OSLog.default
  }

  @_transparent
  public init(_ log: OSLog) {
    self._log = log
  }

  @_transparent
  public init(subsystem: String, category: String) {
    self._log = OSLog(subsystem: subsystem, category: category)
  }

  /// The handle this logger writes to.
  @_transparent
  public var logObject: OSLog { _log }

  @_transparent
  @_optimize(none)
  public func log(level: OSLogType, _ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: level)
  }

  @_transparent
  @_optimize(none)
  public func log(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .default)
  }

  @_transparent
  @_optimize(none)
  public func trace(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .debug)
  }

  @_transparent
  @_optimize(none)
  public func debug(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .debug)
  }

  @_transparent
  @_optimize(none)
  public func info(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .info)
  }

  @_transparent
  @_optimize(none)
  public func notice(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .default)
  }

  @_transparent
  @_optimize(none)
  public func warning(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .error)
  }

  @_transparent
  @_optimize(none)
  public func error(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .error)
  }

  @_transparent
  @_optimize(none)
  public func critical(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .fault)
  }

  @_transparent
  @_optimize(none)
  public func fault(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .fault)
  }
}

/// Serialise a message and hand it to the system log.
///
/// This is OSLogTestHelper's `_osLogTestHelper` with the stub at the
/// end replaced by the real call.  The serialisation above it is that
/// file's, unchanged in shape: preamble, argument count, then each
/// argument's closure writing into the buffer.
// No oslog.requires_constant_arguments here: that attribute requires
// every argument to be a compile-time constant, and this takes a log
// handle and a level.  Apple's helper carries it because it takes only
// the message.  Dropping it costs the format-string constant folding
// the optimiser would otherwise do, and nothing else.
@_transparent
@_optimize(none)
public func _osLogEmit(
  _ message: OSLogMessage,
  log: OSLog,
  type: OSLogType
) {
  let formatString = message.interpolation.formatString
  let preamble = message.interpolation.preamble
  let argumentCount = message.interpolation.argumentCount
  let bufferSize = message.bufferSize
  let objectCount = message.interpolation.objectArgumentCount
  let stringCount = message.interpolation.stringArgumentCount
  let uint32bufferSize = UInt32(bufferSize)
  let argumentClosures = message.interpolation.arguments.argumentClosures
  let formatStringPointer = _getGlobalStringTablePointer(formatString)

  let bufferMemory = UnsafeMutablePointer<UInt8>.allocate(capacity: bufferSize)
  let objectArguments = createStorage(capacity: objectCount, type: AnyObject.self)
  let stringArgumentOwners = createStorage(capacity: stringCount, type: Any.self)

  var currentBufferPosition = bufferMemory
  var objectArgumentsPosition = objectArguments
  var stringArgumentOwnersPosition = stringArgumentOwners

  serialize(preamble, at: &currentBufferPosition)
  serialize(argumentCount, at: &currentBufferPosition)
  argumentClosures.forEach {
    $0(&currentBufferPosition,
       &objectArgumentsPosition,
       &stringArgumentOwnersPosition)
  }

  _xt_os_log_impl(
    #dsohandle,
    log._handle,
    type.rawValue,
    formatStringPointer,
    bufferMemory,
    uint32bufferSize)

  destroyStorage(objectArguments, count: objectCount)
  destroyStorage(stringArgumentOwners, count: stringCount)
  bufferMemory.deallocate()
}

// The dso argument is #dsohandle, Swift's spelling of the __dso_handle
// the C macros pass: the anchor the dynamic loader uses to find the
// image a format string belongs to.
