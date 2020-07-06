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

#include "m3c/types_log.h"

#include "m3c/PropVariant.h"
#include "m3c/com_ptr.h"
#include "m3c/types_fmt.h"  // IWYU pragma: keep

#include <llamalog/custom_types.h>
#include <llamalog/llamalog.h>

#include <objidl.h>
#include <rpc.h>
#include <unknwn.h>
#include <wtypes.h>


//
// UUID
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const UUID& arg) {
	return logLine.AddCustomArgument(arg);
}


//
// PROPVARIANT
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PROPVARIANT& arg) {
	return operator<<(logLine, m3c::PropVariant(arg));
}


//
// IUnknown
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, IUnknown* arg) {
	m3c::com_ptr<IUnknown> ptr(arg);
	return operator<<(logLine, ptr);
}


//
// IStream
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, IStream* arg) {
	m3c::com_ptr<IStream> ptr(arg);
	return operator<<(logLine, ptr);
}


//
// PROPERTYKEY
//

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PROPERTYKEY& arg) {
	return logLine.AddCustomArgument(arg);
}
