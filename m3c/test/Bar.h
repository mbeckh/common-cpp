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

#include "m3c/ComObject.h"

#include <guiddef.h>
#include <unknwn.h>

namespace m3c::test {

extern const IID IID_IBar;

MIDL_INTERFACE("9B4D833C-BDA0-422D-A55E-A4E681D3D75C")
IBar : public IUnknown{
		   // no additional methods
	   };

class Bar : public ComObject<IBar> {
	// default
};

}  // namespace m3c::test
