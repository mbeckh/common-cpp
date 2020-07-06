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

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <objidl.h>
#include <propidl.h>
#include <propvarutil.h>
#include <unknwn.h>
#include <windows.h>
#include <wtypes.h>

#include <cstddef>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <unordered_set>
