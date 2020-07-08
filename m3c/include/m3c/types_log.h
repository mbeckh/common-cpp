/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file

#pragma once

#include <propidl.h>
#include <rpc.h>
#include <wtypes.h>

struct IUnknown;
struct IStream;

namespace llamalog {
class LogLine;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
}  // namespace llamalog


//
// UUID
//

/// @brief Log a `UUID` structure as `01234567-1234-1234-1234-1234567890ab`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const UUID& arg);


//
// PROPVARIANT
//

/// @brief Log a `PROPVARIANT` structure as `VT_xx|<value>`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PROPVARIANT& arg);


//
// IUnknown
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, IUnknown* arg);


//
// IStream
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, IStream* arg);

//
// PROPERTYKEY
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PROPERTYKEY& arg);
