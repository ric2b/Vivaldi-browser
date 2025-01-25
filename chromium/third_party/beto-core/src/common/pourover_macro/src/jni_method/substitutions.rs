// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Name substitutions for JNI name mangling. See
//! <https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/design.html#resolving_native_method_names>
//! for spec details.

pub fn substitute_method_chars(s: &str) -> String {
    s.chars()
        .flat_map(|c| -> CharIter {
            match c {
                '$' => "_00024".into(),
                '_' => "_1".into(),
                _ => c.into(),
            }
        })
        .collect()
}

pub fn substitute_package_chars(s: &str) -> String {
    s.chars()
        .flat_map(|c| -> CharIter {
            match c {
                '.' => "_".into(),
                '$' => "_00024".into(),
                '_' => "_1".into(),
                _ => c.into(),
            }
        })
        .collect()
}

pub fn substitute_class_chars(s: &str) -> String {
    s.chars()
        .flat_map(|c| -> CharIter {
            match c {
                // Use dot or dollar for inner classes: `'.' -> '$' -> "_00024"`
                '.' | '$' => "_00024".into(),
                '_' => "_1".into(),
                _ => c.into(),
            }
        })
        .collect()
}

/// A `char` iterator that can be created from either a `char` or a `&'static str`.
enum CharIter {
    One(core::option::IntoIter<char>),
    Many(core::str::Chars<'static>),
}

impl Iterator for CharIter {
    type Item = char;
    fn next(&mut self) -> Option<char> {
        match *self {
            Self::One(ref mut iter) => iter.next(),
            Self::Many(ref mut iter) => iter.next(),
        }
    }
}

impl From<char> for CharIter {
    fn from(c: char) -> Self {
        Self::One(Some(c).into_iter())
    }
}

impl From<&'static str> for CharIter {
    fn from(s: &'static str) -> Self {
        Self::Many(s.chars())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_char_iter_one() {
        let seq = CharIter::from('a').collect::<String>();
        assert_eq!("a", &seq);
    }

    #[test]
    fn test_char_iter_many() {
        let seq = CharIter::from("asdf").collect::<String>();
        assert_eq!("asdf", &seq);
    }

    #[test]
    fn test_substitute_method_chars() {
        let mangled = substitute_method_chars("method_with_under$cores");
        assert_eq!("method_1with_1under_00024cores", &mangled);
    }

    #[test]
    fn test_substitute_package_chars() {
        let mangled = substitute_package_chars("com.weird_name.java");
        assert_eq!("com_weird_1name_java", &mangled);
    }

    #[test]
    fn test_substitute_class_chars() {
        // Both dot and dollar should work here

        let mangled = substitute_class_chars("Foo.Inner");
        assert_eq!("Foo_00024Inner", &mangled);

        let mangled = substitute_class_chars("Foo$Inner");
        assert_eq!("Foo_00024Inner", &mangled);
    }
}
