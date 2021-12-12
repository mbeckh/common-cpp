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

#include "m3c/com_ptr.h"

#include "m3c_events.h"

#include "m3c/LogArgs.h"
#include "m3c/exception.h"

#include <fmt/xchar.h>

#include <sal.h>

#include <string>

namespace m3c {

namespace internal {

void com_ptr_base::QueryInterface(_In_ IUnknown* const pUnknown, const IID& iid, _Outptr_ void** p) {
	M3C_COM_HR(pUnknown->QueryInterface(iid, p), evt::IUnknown_QueryInterface_H, iid);
}

}  // namespace internal


template <>
void operator>>(const com_ptr<IStream>& ptr, _Inout_ LogFormatArgs& formatArgs) {
	formatArgs + fmt_ptr(reinterpret_cast<IUnknown* const&>(ptr.m_ptr)) + FMT_FORMAT("{:n}", ptr);
}

template <>
void operator>>(const com_ptr<IStream>& ptr, _Inout_ LogEventArgs& eventArgs) {
	(eventArgs << reinterpret_cast<const void* const&>(ptr.m_ptr)) + FMT_FORMAT(L"{:n}", ptr);
}

}  // namespace m3c
