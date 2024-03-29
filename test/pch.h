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

#pragma once

#include <m4t/IStreamMock.h>
#include <m4t/LogListener.h>
#include <m4t/MallocSpy.h>
#include <m4t/m4t.h>

#include <fmt/args.h>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <detours_gmock.h>
#include <evntprov.h>
#include <guiddef.h>
#include <oaidl.h>
#include <objbase.h>
#include <objidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include <rpc.h>
#include <sal.h>
#include <sddl.h>
#include <unknwn.h>
#include <wincodec.h>
#include <winmeta.h>
#include <wtypes.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cfloat>
#include <chrono>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <ostream>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
