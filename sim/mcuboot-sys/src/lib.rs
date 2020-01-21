// Copyright (c) 2017-2019 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

mod area;
pub mod c;

// The API needs to be public, even though it isn't intended to be called by Rust code, but the
// functions are exported to C code.
pub mod api;

pub use crate::area::{AreaDesc, FlashId};
