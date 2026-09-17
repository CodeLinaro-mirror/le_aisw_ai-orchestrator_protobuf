// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

//! Reflection support for the C++ kernel.
//!
//! The descriptor itself is fetched through a per-message thunk emitted into
//! that message's generated C++ code, so that only protos which actually need
//! reflection pull in the full C++ runtime.
