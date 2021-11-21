/*
Copyright 2021 Michael Beckh

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

#include "m3c/LogArgs.h"

#include "m3c/format.h"

void operator>>(const VARIANT& arg, _Inout_ m3c::LogFormatArgs& formatArgs) {
	formatArgs << FMT_FORMAT("{:t}", arg) << FMT_FORMAT("{:v}", arg);
}

void operator>>(const VARIANT& arg, _Inout_ m3c::LogEventArgs& eventArgs) {
	eventArgs + FMT_FORMAT("{:t}", arg) + FMT_FORMAT(L"{:v}", arg);
}

void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogFormatArgs& formatArgs) {
	formatArgs << FMT_FORMAT("{:t}", arg) << FMT_FORMAT("{:v}", arg);
}

void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogEventArgs& eventArgs) {
	eventArgs + FMT_FORMAT("{:t}", arg) + FMT_FORMAT(L"{:v}", arg);
}

void operator>>(const PROPERTYKEY& arg, _Inout_ m3c::LogEventArgs& eventArgs) {
	eventArgs + fmt::to_string(arg);
}

void operator>>(const WICRect& arg, _Inout_ m3c::LogFormatArgs& formatArgs) {
	formatArgs << arg.X << arg.Y << arg.Width << arg.Height;
}

void operator>>(const WICRect& arg, _Inout_ m3c::LogEventArgs& eventArgs) {
	eventArgs << arg.X << arg.Y << arg.Width << arg.Height;
}