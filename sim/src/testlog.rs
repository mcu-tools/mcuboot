//! Logging support for the test framework.
//!
//! https://stackoverflow.com/questions/30177845/how-to-initialize-the-logger-for-integration-tests
//!
//! The test framework runs the tests, possibly simultaneously, and in various orders.  This helper
//! function, which should be called at the beginning of each test, will setup logging for all of
//! the tests.

use env_logger;
use std::sync::{Once, ONCE_INIT};

static INIT: Once = ONCE_INIT;

/// Setup the logging system.  Intended to be called at the beginning of each test.
pub fn setup() {
    INIT.call_once(|| {
        env_logger::init();
    });
}
