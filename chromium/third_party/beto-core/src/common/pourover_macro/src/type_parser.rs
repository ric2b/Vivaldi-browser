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

//! Parsers for JNI type descriptors.

// There is code that is used in tests but not in the crate.
#![allow(dead_code)]

use core::fmt;
use core::num::NonZeroU8;
use nom::{IResult, Parser};

/// Describes a type in Java
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum JavaType<'a> {
    /// An array type.
    Array {
        /// The number of dimensions. The JVM spec limits this to 255.
        depth: NonZeroU8,
        /// The type of objects in the array.
        ty: NonArray<'a>,
    },
    /// A non-array type. See [`NonArray`].
    NonArray(NonArray<'a>),
}

impl<'a> JavaType<'a> {
    /// Check if this type is a primitive type.
    pub fn is_primitive(&self) -> bool {
        matches!(*self, Self::NonArray(NonArray::Primitive(_)))
    }

    fn parse(s: &'a str) -> IResult<&'a str, JavaType<'a>> {
        use nom::bytes::complete::take_while1;
        use nom::combinator::{map, map_opt, opt};

        let parse_array = opt(map_opt(take_while1(|c| c == '['), |brackets: &str| {
            u8::try_from(brackets.len()).ok().and_then(NonZeroU8::new)
        }));

        map(
            parse_array.and(NonArray::parse),
            |(depth, ty)| match depth {
                Some(depth) => JavaType::Array { depth, ty },
                None => JavaType::NonArray(ty),
            },
        )(s)
    }

    /// Try to parse a type from the given string in JNI descriptor format.
    pub fn try_from_str(s: &'a str) -> Option<Self> {
        run_parser(Self::parse, s)
    }
}

impl<'a> fmt::Display for JavaType<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            JavaType::Array { depth, ty } => {
                for _ in 0..depth.get() {
                    write!(f, "[")?;
                }
                write!(f, "{ty}")
            }
            JavaType::NonArray(ty) => write!(f, "{ty}"),
        }
    }
}

impl<'a> From<NonArray<'a>> for JavaType<'a> {
    fn from(ty: NonArray<'a>) -> Self {
        JavaType::NonArray(ty)
    }
}

impl<'a> From<Primitive> for JavaType<'a> {
    fn from(prim: Primitive) -> Self {
        JavaType::NonArray(NonArray::Primitive(prim))
    }
}

#[cfg(feature = "jni")]
impl<'a> From<JavaType<'a>> for jni::signature::ReturnType {
    fn from(ty: JavaType<'a>) -> Self {
        match ty {
            JavaType::Array { .. } => Self::Array,
            JavaType::NonArray(NonArray::Object { .. }) => Self::Object,
            JavaType::NonArray(NonArray::Primitive(p)) => Self::Primitive(p.into()),
        }
    }
}

/// Describes a non-array type in Java
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum NonArray<'a> {
    /// A primitive type. See [`Primitive`].
    Primitive(Primitive),
    /// An object type.
    Object {
        /// The class name in JNI form.
        cls: &'a str,
    },
}

impl<'a> NonArray<'a> {
    fn parse(s: &'a str) -> IResult<&'a str, NonArray<'a>> {
        use nom::branch::alt;
        use nom::bytes::complete::take_while1;
        use nom::character::complete::char;
        use nom::combinator::map;
        use nom::sequence::delimited;

        let parse_prim = map(Primitive::parse, NonArray::Primitive);

        let parse_object = map(
            delimited(char('L'), take_while1(|c| c != ';'), char(';')),
            |cls| NonArray::Object { cls },
        );

        alt((parse_prim, parse_object))(s)
    }
}

impl<'a> fmt::Display for NonArray<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            NonArray::Primitive(p) => write!(f, "{p}"),
            NonArray::Object { cls } => write!(f, "L{cls};"),
        }
    }
}

impl<'a> From<Primitive> for NonArray<'a> {
    fn from(p: Primitive) -> Self {
        Self::Primitive(p)
    }
}

/// Describes a primitive type in Java
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Primitive {
    /// Java `boolean`
    Boolean,
    /// Java `byte`
    Byte,
    /// Java `char`
    Char,
    /// Java `double`
    Double,
    /// Java `float`
    Float,
    /// Java `int`
    Int,
    /// Java `long`
    Long,
    /// Java `short`
    Short,
}

impl Primitive {
    fn parse(s: &str) -> IResult<&str, Primitive> {
        use nom::branch::alt;
        use nom::character::complete::char as match_char;
        use nom::combinator::value;

        let parse_prim = |c: char, p: Primitive| value(p, match_char(c));

        alt((
            parse_prim('Z', Primitive::Boolean),
            parse_prim('B', Primitive::Byte),
            parse_prim('C', Primitive::Char),
            parse_prim('D', Primitive::Double),
            parse_prim('F', Primitive::Float),
            parse_prim('I', Primitive::Int),
            parse_prim('J', Primitive::Long),
            parse_prim('S', Primitive::Short),
        ))(s)
    }

    pub fn as_char(&self) -> char {
        match self {
            Primitive::Boolean => 'Z',
            Primitive::Byte => 'B',
            Primitive::Char => 'C',
            Primitive::Double => 'D',
            Primitive::Float => 'F',
            Primitive::Int => 'I',
            Primitive::Long => 'J',
            Primitive::Short => 'S',
        }
    }
}

impl fmt::Display for Primitive {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match *self {
                Primitive::Boolean => 'Z',
                Primitive::Byte => 'B',
                Primitive::Char => 'C',
                Primitive::Double => 'D',
                Primitive::Float => 'F',
                Primitive::Int => 'I',
                Primitive::Long => 'J',
                Primitive::Short => 'S',
            }
        )
    }
}

#[cfg(feature = "jni")]
impl From<Primitive> for jni::signature::Primitive {
    fn from(p: Primitive) -> Self {
        match p {
            Primitive::Boolean => Self::Boolean,
            Primitive::Byte => Self::Byte,
            Primitive::Char => Self::Char,
            Primitive::Double => Self::Double,
            Primitive::Float => Self::Float,
            Primitive::Int => Self::Int,
            Primitive::Long => Self::Long,
            Primitive::Short => Self::Short,
        }
    }
}

/// A Java return type. This may be `void`.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ReturnType<'a> {
    /// Java `void`. Only valid in return position.
    Void,
    /// A non-void return type.
    Returns(JavaType<'a>),
}

impl<'a> ReturnType<'a> {
    fn parse(s: &'a str) -> IResult<&'a str, ReturnType<'a>> {
        use nom::branch::alt;
        use nom::character::complete::char;
        use nom::combinator::{map, value};

        alt((
            value(ReturnType::Void, char('V')),
            map(JavaType::parse, ReturnType::Returns),
        ))(s)
    }

    /// Try to parse a return type from the given string in JNI descriptor format.
    pub fn try_from_str(s: &'a str) -> Option<Self> {
        run_parser(Self::parse, s)
    }

    /// Check if the return type is `ReturnType::Void`
    pub fn is_void(&self) -> bool {
        matches!(*self, Self::Void)
    }
}

impl<'a> From<JavaType<'a>> for ReturnType<'a> {
    fn from(ty: JavaType<'a>) -> Self {
        Self::Returns(ty)
    }
}

#[cfg(feature = "jni")]
impl<'a> From<ReturnType<'a>> for jni::signature::ReturnType {
    fn from(ty: ReturnType<'a>) -> Self {
        match ty {
            ReturnType::Void => Self::Primitive(jni::signature::Primitive::Void),
            ReturnType::Returns(ty) => ty.into(),
        }
    }
}

/// A type signature of a Java method.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MethodSig<'a> {
    /// The types of each argument.
    pub args: Vec<JavaType<'a>>,
    /// The return type.
    pub ret: ReturnType<'a>,
}

impl<'a> MethodSig<'a> {
    fn parse(s: &'a str) -> IResult<&'a str, MethodSig<'a>> {
        use nom::character::complete::char;
        use nom::combinator::map;
        use nom::multi::many0;
        use nom::sequence::delimited;

        let parse_args = delimited(char('('), many0(JavaType::parse), char(')'));
        let mut parser = map(parse_args.and(ReturnType::parse), |(args, ret)| MethodSig {
            args,
            ret,
        });

        parser(s)
    }

    /// Try to parse a method signature from the given string in JNI descriptor format.
    pub fn try_from_str(s: &'a str) -> Option<Self> {
        run_parser(Self::parse, s)
    }
}

/// Helper to run a parser and return its result if it parsed the entire string.
fn run_parser<'a, O, E>(mut parser: impl Parser<&'a str, O, E>, s: &'a str) -> Option<O> {
    parser.parse(s).ok().and_then(|(rest, out)| {
        if !rest.is_empty() {
            return None;
        }
        Some(out)
    })
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing)]
mod test {
    use super::*;

    #[test]
    fn parse_primitive() {
        let (rest, parsed) = JavaType::parse("II").unwrap();
        assert_eq!("I", rest);
        assert_eq!(
            JavaType::NonArray(NonArray::Primitive(Primitive::Int)),
            parsed
        );
    }

    #[test]
    fn parse_object() {
        let (rest, parsed) = JavaType::parse("Ljava/lang/String;I").unwrap();
        assert_eq!("I", rest);
        assert_eq!(
            JavaType::NonArray(NonArray::Object {
                cls: "java/lang/String"
            }),
            parsed
        );
    }

    #[test]
    fn parse_primitive_array() {
        let (rest, parsed) = JavaType::parse("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[II").unwrap();
        assert_eq!("I", rest);
        assert_eq!(
            JavaType::Array {
                depth: NonZeroU8::new(255).unwrap(),
                ty: NonArray::Primitive(Primitive::Int)
            },
            parsed
        );
    }

    #[test]
    fn parse_object_array() {
        let (rest, parsed) = JavaType::parse("[[[Ljava/lang/String;I").unwrap();
        assert_eq!("I", rest);
        assert_eq!(
            JavaType::Array {
                depth: NonZeroU8::new(3).unwrap(),
                ty: NonArray::Object {
                    cls: "java/lang/String"
                }
            },
            parsed
        );
    }

    #[test]
    fn parse_invalid_array() {
        let opt = JavaType::try_from_str("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[I");

        assert_eq!(None, opt);
    }

    #[test]
    fn parse_invalid_method() {
        let parsed = MethodSig::try_from_str("I(I)I");
        assert_eq!(None, parsed);
    }

    #[test]
    fn parse_argless_method() {
        let parsed = MethodSig::try_from_str("()V").unwrap();
        assert_eq!(ReturnType::Void, parsed.ret);
        assert_eq!(0, parsed.args.len());
    }

    #[test]
    fn parse_void_method() {
        let parsed = MethodSig::try_from_str("([ILjava/lang/String;Z)V").unwrap();
        assert_eq!(ReturnType::Void, parsed.ret);
        assert_eq!(3, parsed.args.len());
        assert_eq!(
            JavaType::Array {
                depth: NonZeroU8::new(1).unwrap(),
                ty: NonArray::Primitive(Primitive::Int)
            },
            parsed.args[0]
        );
        assert_eq!(
            JavaType::NonArray(NonArray::Object {
                cls: "java/lang/String"
            }),
            parsed.args[1]
        );
        assert_eq!(
            JavaType::NonArray(NonArray::Primitive(Primitive::Boolean)),
            parsed.args[2]
        );
    }

    #[test]
    fn parse_nonvoid_method() {
        let parsed = MethodSig::try_from_str("([ILjava/lang/String;Z)D").unwrap();
        assert_eq!(
            ReturnType::Returns(JavaType::NonArray(NonArray::Primitive(Primitive::Double))),
            parsed.ret
        );
        assert_eq!(3, parsed.args.len());
        assert_eq!(
            JavaType::Array {
                depth: NonZeroU8::new(1).unwrap(),
                ty: NonArray::Primitive(Primitive::Int)
            },
            parsed.args[0]
        );
        assert_eq!(
            JavaType::NonArray(NonArray::Object {
                cls: "java/lang/String"
            }),
            parsed.args[1]
        );
        assert_eq!(
            JavaType::NonArray(NonArray::Primitive(Primitive::Boolean)),
            parsed.args[2]
        );
    }

    #[test]
    fn parse_trailing_data_will_error() {
        assert!(JavaType::try_from_str("Itrailing").is_none());
        assert!(JavaType::try_from_str("Lcom/example/Foo;trailing").is_none());
        assert!(JavaType::try_from_str("[Ztrailing").is_none());
        assert!(MethodSig::try_from_str("()Itrailing").is_none());
        assert!(MethodSig::try_from_str("()Lcom/example/Foo;trailing").is_none());
        assert!(MethodSig::try_from_str("()[Ztrailing").is_none());
    }

    #[test]
    fn java_type_roundtrip_through_display() {
        let tests = [
            "Z",
            "C",
            "B",
            "S",
            "I",
            "J",
            "F",
            "D",
            "Ljava/lang/String;",
            "[Z",
            "[[B",
            "[[[Ljava/lang/String;",
        ];

        for test in tests {
            let parsed = JavaType::try_from_str(test).unwrap();

            let display = format!("{parsed}");

            assert_eq!(test, display);
        }
    }

    #[test]
    fn test_is_prim() {
        assert!(JavaType::try_from_str("I").unwrap().is_primitive());
        assert!(JavaType::try_from_str("Z").unwrap().is_primitive());
        assert!(!JavaType::try_from_str("[I").unwrap().is_primitive());
        assert!(!JavaType::try_from_str("[Ljava/lang/String;")
            .unwrap()
            .is_primitive());
        assert!(!JavaType::try_from_str("Ljava/lang/String;")
            .unwrap()
            .is_primitive());
    }
}
