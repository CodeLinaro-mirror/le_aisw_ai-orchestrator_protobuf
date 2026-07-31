// Protocol Buffers - Google's data interchange format
// Copyright 2024 Google LLC.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

use googletest::description::Description;
use googletest::matcher::{Matcher, MatcherBase, MatcherResult};
use protobuf::__internal::MatcherEq;

////////////////////////////////////////////////////////////////////////////////
// Matchers
////////////////////////////////////////////////////////////////////////////////

/// Matches a protobuf message for equality.
///
/// This method may have false-negatives or false-positives in the face of unknown fields; see the
/// comments on `message_eq` in message.rs for precise semantics.
pub fn proto_eq<T: MatcherEq>(expected: T) -> MessageMatcher<T> {
    MessageMatcher { expected, partial: false }
}

/// A matcher that that can be used to check for partial proto equality.
///
/// In a partial match, only fields present in the expected protobuf are considered.
/// Extra fields set only in the actual protobuf will be ignored during comparison.
///
/// # Examples
/// ```rust
/// use googletest::prelude::*;
/// use protobuf_gtest_matchers::{partially, proto_eq};
///
/// expect_that!(actual, partially(proto_eq(expected)));
/// ```
pub fn partially<T: IntoPartialMessageMatcher>(matcher: T) -> MessageMatcher<T::Msg> {
    matcher.into_partial_message_matcher()
}

////////////////////////////////////////////////////////////////////////////////
// Implementation details
////////////////////////////////////////////////////////////////////////////////

#[derive(MatcherBase)]
pub struct MessageMatcher<T: MatcherEq> {
    expected: T,
    partial: bool,
}

impl<T: MatcherEq> MessageMatcher<T> {
    /// Configures this matcher to perform a partial comparison.
    pub fn partially(mut self) -> Self {
        self.partial = true;
        self
    }
}

impl<T> Matcher<&T> for MessageMatcher<T>
where
    T: MatcherEq,
{
    fn matches(&self, actual: &T) -> MatcherResult {
        if self.partial {
            actual.matches_partially(&self.expected).into()
        } else {
            actual.matches(&self.expected).into()
        }
    }

    fn describe(&self, matcher_result: MatcherResult) -> Description {
        match (matcher_result, self.partial) {
            (MatcherResult::Match, false) => format!("is equal to {:?}", self.expected).into(),
            (MatcherResult::NoMatch, false) => {
                format!("is not equal to {:?}", self.expected).into()
            }
            (MatcherResult::Match, true) => {
                format!("is partially equal to {:?}", self.expected).into()
            }
            (MatcherResult::NoMatch, true) => {
                format!("is not partially equal to {:?}", self.expected).into()
            }
        }
    }
}

impl<T> Matcher<T> for MessageMatcher<T>
where
    T: MatcherEq + Copy,
{
    fn matches(&self, actual: T) -> MatcherResult {
        if self.partial {
            actual.matches_partially(&self.expected).into()
        } else {
            actual.matches(&self.expected).into()
        }
    }

    fn describe(&self, matcher_result: MatcherResult) -> Description {
        match (matcher_result, self.partial) {
            (MatcherResult::Match, false) => format!("is equal to {:?}", self.expected).into(),
            (MatcherResult::NoMatch, false) => {
                format!("is not equal to {:?}", self.expected).into()
            }
            (MatcherResult::Match, true) => {
                format!("is partially equal to {:?}", self.expected).into()
            }
            (MatcherResult::NoMatch, true) => {
                format!("is not partially equal to {:?}", self.expected).into()
            }
        }
    }
}

/// A trait for types that can be converted into a partial `MessageMatcher`.
pub trait IntoPartialMessageMatcher {
    type Msg: MatcherEq;
    fn into_partial_message_matcher(self) -> MessageMatcher<Self::Msg>;
}

impl<T: MatcherEq> IntoPartialMessageMatcher for MessageMatcher<T> {
    type Msg = T;
    fn into_partial_message_matcher(self) -> MessageMatcher<T> {
        self.partially()
    }
}
