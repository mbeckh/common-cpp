/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <guiddef.h>
#include <objbase.h>
#include <objidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <rpc.h>
#include <unknwn.h>
#include <windows.h>
#include <wtypes.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <utility>