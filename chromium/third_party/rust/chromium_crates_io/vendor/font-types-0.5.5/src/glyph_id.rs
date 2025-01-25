//! Glyph Identifiers
//!
//! Although these are treated as u16s in the spec, we choose to represent them
//! as a distinct type.

/// A 16-bit glyph identifier.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "bytemuck", derive(bytemuck::AnyBitPattern))]
#[repr(transparent)]
pub struct GlyphId(u16);

impl GlyphId {
    /// The identifier reserved for unknown glyphs
    pub const NOTDEF: GlyphId = GlyphId(0);

    /// Construct a new `GlyphId`.
    pub const fn new(raw: u16) -> Self {
        GlyphId(raw)
    }

    /// The identifier as a u16.
    pub const fn to_u16(self) -> u16 {
        self.0
    }

    /// The identifier as a u32.
    pub const fn to_u32(self) -> u32 {
        self.0 as u32
    }

    pub const fn to_be_bytes(self) -> [u8; 2] {
        self.0.to_be_bytes()
    }
}

impl Default for GlyphId {
    fn default() -> Self {
        GlyphId::NOTDEF
    }
}

impl From<u16> for GlyphId {
    fn from(value: u16) -> Self {
        Self(value)
    }
}

impl std::fmt::Display for GlyphId {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "GID_{}", self.0)
    }
}

impl From<GlyphId> for u32 {
    fn from(value: GlyphId) -> u32 {
        value.to_u32()
    }
}

crate::newtype_scalar!(GlyphId, [u8; 2]);
