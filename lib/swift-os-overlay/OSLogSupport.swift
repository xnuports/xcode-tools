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
  // `let', not a computed property: the log calls carry
  // @_semantics("oslog.requires_constant_arguments"), and that requires
  // the level to be a constant the optimiser can fold.
  public static let `default` = OSLogType(rawValue: 0x00)
  public static let info      = OSLogType(rawValue: 0x01)
  public static let debug     = OSLogType(rawValue: 0x02)
  public static let error     = OSLogType(rawValue: 0x10)
  public static let fault     = OSLogType(rawValue: 0x11)
}

/// A log handle: a subsystem and category to write under.
///
/// Apple's is a Swift class over the Objective-C os_log_t.  This one
/// holds the same pointer opaquely, for the reason at the top of the
/// file.  `default` is the handle a program gets when it names none.
public final class OSLog: @unchecked Sendable {
  @usableFromInline
  internal let _handle: OpaquePointer?

  @usableFromInline
  internal init(handle: OpaquePointer?) {
    self._handle = handle
  }

  public init(subsystem: String, category: String) {
    self._handle = subsystem.withCString { s in
      category.withCString { c in
        _xt_os_log_create(s, c)
      }
    }
  }

  /// The process's default handle.  Passing a nil handle to
  /// _os_log_impl is what the C macros do when given OS_LOG_DEFAULT.
  public static let `default` = OSLog(handle: nil)

  /// A handle that discards everything written to it.
  public static let disabled = OSLog(subsystem: "", category: "")
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

  public init() {
    self._log = OSLog.default
  }

  public init(_ log: OSLog) {
    self._log = log
  }

  public init(subsystem: String, category: String) {
    self._log = OSLog(subsystem: subsystem, category: category)
  }

  /// The handle this logger writes to.
  public var logObject: OSLog { _log }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func log(level: OSLogType, _ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: level)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func log(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .default)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func trace(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .debug)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func debug(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .debug)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func info(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .info)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func notice(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .default)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func warning(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .error)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func error(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .error)
  }

  @_semantics("oslog.requires_constant_arguments")
  @_transparent
  @_optimize(none)
  public func critical(_ message: OSLogMessage) {
    _osLogEmit(message, log: _log, type: .fault)
  }

  @_semantics("oslog.requires_constant_arguments")
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
