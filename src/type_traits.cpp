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

#include "m3c/type_traits.h"

namespace m3c {

template _Ret_z_ const char* SelectString(_In_z_ const char* const sz, _In_z_ const wchar_t* const wsz) noexcept;
template _Ret_z_ const wchar_t* SelectString(_In_z_ const char* const sz, _In_z_ const wchar_t* const wsz) noexcept;

}  // namespace m3c
