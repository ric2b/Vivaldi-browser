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

use crate::internal_utils::*;
use crate::parser::mp4box::BoxSize;

#[derive(Debug)]
pub struct IBitStream<'a> {
    pub data: &'a [u8],
    pub bit_offset: usize,
}

impl IBitStream<'_> {
    fn read_bit(&mut self) -> AvifResult<u8> {
        let byte_offset = self.bit_offset / 8;
        if byte_offset >= self.data.len() {
            return Err(AvifError::BmffParseFailed("Not enough bits".into()));
        }
        let byte = self.data[byte_offset];
        let shift = 7 - (self.bit_offset % 8);
        self.bit_offset += 1;
        Ok((byte >> shift) & 0x01)
    }

    pub(crate) fn read(&mut self, n: usize) -> AvifResult<u32> {
        assert!(n <= 32);
        let mut value: u32 = 0;
        for _i in 0..n {
            value <<= 1;
            value |= self.read_bit()? as u32;
        }
        Ok(value)
    }

    pub(crate) fn read_bool(&mut self) -> AvifResult<bool> {
        let bit = self.read_bit()?;
        Ok(bit == 1)
    }

    pub(crate) fn skip(&mut self, n: usize) -> AvifResult<()> {
        if checked_add!(self.bit_offset, n)? > checked_mul!(self.data.len(), 8)? {
            return Err(AvifError::BmffParseFailed("Not enough bytes".into()));
        }
        self.bit_offset += n;
        Ok(())
    }

    pub(crate) fn skip_uvlc(&mut self) -> AvifResult<()> {
        // See the section 4.10.3. uvlc() of the AV1 specification.
        let mut leading_zeros = 0u128; // leadingZeros
        while !self.read_bool()? {
            leading_zeros += 1;
        }
        if leading_zeros < 32 {
            self.skip(leading_zeros as usize)?; // f(leadingZeros) value;
        }
        Ok(())
    }

    pub(crate) fn remaining_bits(&self) -> AvifResult<usize> {
        checked_sub!(checked_mul!(self.data.len(), 8)?, self.bit_offset)
    }
}

#[derive(Debug)]
pub struct IStream<'a> {
    pub data: &'a [u8],
    pub offset: usize,
}

impl IStream<'_> {
    pub(crate) fn create(data: &[u8]) -> IStream {
        IStream { data, offset: 0 }
    }

    fn check(&self, size: usize) -> AvifResult<()> {
        if self.bytes_left()? < size {
            return Err(AvifError::BmffParseFailed("".into()));
        }
        Ok(())
    }

    pub(crate) fn sub_stream(&mut self, size: &BoxSize) -> AvifResult<IStream> {
        let offset = self.offset;
        checked_incr!(
            self.offset,
            match size {
                BoxSize::FixedSize(size) => {
                    self.check(*size)?;
                    *size
                }
                BoxSize::UntilEndOfStream => self.bytes_left()?,
            }
        );
        Ok(IStream {
            data: &self.data[offset..self.offset],
            offset: 0,
        })
    }

    pub(crate) fn sub_bit_stream(&mut self, size: usize) -> AvifResult<IBitStream> {
        self.check(size)?;
        let offset = self.offset;
        checked_incr!(self.offset, size);
        Ok(IBitStream {
            data: &self.data[offset..self.offset],
            bit_offset: 0,
        })
    }

    pub(crate) fn bytes_left(&self) -> AvifResult<usize> {
        if self.data.len() < self.offset {
            return Err(AvifError::UnknownError("".into()));
        }
        Ok(self.data.len() - self.offset)
    }

    pub(crate) fn has_bytes_left(&self) -> AvifResult<bool> {
        Ok(self.bytes_left()? > 0)
    }

    pub(crate) fn get_slice(&mut self, size: usize) -> AvifResult<&[u8]> {
        self.check(size)?;
        let offset_start = self.offset;
        checked_incr!(self.offset, size);
        Ok(&self.data[offset_start..offset_start + size])
    }

    pub(crate) fn get_immutable_vec(&self, size: usize) -> AvifResult<Vec<u8>> {
        self.check(size)?;
        Ok(self.data[self.offset..self.offset + size].to_vec())
    }

    fn get_vec(&mut self, size: usize) -> AvifResult<Vec<u8>> {
        Ok(self.get_slice(size)?.to_vec())
    }

    pub(crate) fn read_u8(&mut self) -> AvifResult<u8> {
        self.check(1)?;
        let value = self.data[self.offset];
        checked_incr!(self.offset, 1);
        Ok(value)
    }

    pub(crate) fn read_u16(&mut self) -> AvifResult<u16> {
        Ok(u16::from_be_bytes(self.get_slice(2)?.try_into().unwrap()))
    }

    pub(crate) fn read_u24(&mut self) -> AvifResult<u32> {
        Ok(self.read_uxx(3)? as u32)
    }

    pub(crate) fn read_u32(&mut self) -> AvifResult<u32> {
        Ok(u32::from_be_bytes(self.get_slice(4)?.try_into().unwrap()))
    }

    pub(crate) fn read_u64(&mut self) -> AvifResult<u64> {
        Ok(u64::from_be_bytes(self.get_slice(8)?.try_into().unwrap()))
    }

    #[cfg(feature = "sample_transform")]
    pub(crate) fn read_i8(&mut self) -> AvifResult<i8> {
        Ok(self.read_u8()? as i8)
    }

    pub(crate) fn read_i16(&mut self) -> AvifResult<i16> {
        Ok(self.read_u16()? as i16)
    }

    pub(crate) fn read_i32(&mut self) -> AvifResult<i32> {
        Ok(self.read_u32()? as i32)
    }

    #[cfg(feature = "sample_transform")]
    pub(crate) fn read_i64(&mut self) -> AvifResult<i64> {
        Ok(self.read_u64()? as i64)
    }

    pub(crate) fn skip_u32(&mut self) -> AvifResult<()> {
        self.skip(4)
    }

    pub(crate) fn skip_u64(&mut self) -> AvifResult<()> {
        self.skip(8)
    }

    pub(crate) fn read_fraction(&mut self) -> AvifResult<Fraction> {
        Ok(Fraction(self.read_i32()?, self.read_u32()?))
    }

    pub(crate) fn read_ufraction(&mut self) -> AvifResult<UFraction> {
        Ok(UFraction(self.read_u32()?, self.read_u32()?))
    }

    // Reads size characters of a non-null-terminated string.
    pub(crate) fn read_string(&mut self, size: usize) -> AvifResult<String> {
        Ok(String::from_utf8(self.get_vec(size)?).unwrap_or("".into()))
    }

    // Reads an xx-byte unsigner integer.
    pub(crate) fn read_uxx(&mut self, xx: u8) -> AvifResult<u64> {
        let n: usize = xx.into();
        if n == 0 {
            return Ok(0);
        }
        if n > 8 {
            return Err(AvifError::NotImplemented);
        }
        let mut out = [0; 8];
        let start = out.len() - n;
        out[start..].copy_from_slice(self.get_slice(n)?);
        Ok(u64::from_be_bytes(out))
    }

    // Reads a null-terminated string.
    pub(crate) fn read_c_string(&mut self) -> AvifResult<String> {
        self.check(1)?;
        let null_position = self.data[self.offset..]
            .iter()
            .position(|&x| x == b'\0')
            .ok_or(AvifError::BmffParseFailed("".into()))?;
        let range = self.offset..self.offset + null_position;
        self.offset += null_position + 1;
        Ok(String::from_utf8(self.data[range].to_vec()).unwrap_or("".into()))
    }

    pub(crate) fn read_version_and_flags(&mut self) -> AvifResult<(u8, u32)> {
        let version = self.read_u8()?;
        let flags = self.read_u24()?;
        Ok((version, flags))
    }

    pub(crate) fn read_and_enforce_version_and_flags(
        &mut self,
        enforced_version: u8,
    ) -> AvifResult<(u8, u32)> {
        let (version, flags) = self.read_version_and_flags()?;
        if version != enforced_version {
            return Err(AvifError::BmffParseFailed("".into()));
        }
        Ok((version, flags))
    }

    pub(crate) fn skip(&mut self, size: usize) -> AvifResult<()> {
        self.check(size)?;
        checked_incr!(self.offset, size);
        Ok(())
    }

    pub(crate) fn rewind(&mut self, size: usize) -> AvifResult<()> {
        checked_decr!(self.offset, size);
        Ok(())
    }

    pub(crate) fn read_uleb128(&mut self) -> AvifResult<u32> {
        // See the section 4.10.5. of the AV1 specification.
        let mut value: u64 = 0;
        for i in 0..8 {
            // leb128_byte contains 8 bits read from the bitstream.
            let leb128_byte = self.read_u8()?;
            // The bottom 7 bits are used to compute the variable value.
            value |= u64::from(leb128_byte & 0x7F) << (i * 7);
            // The most significant bit is used to indicate that there are more
            // bytes to be read.
            if (leb128_byte & 0x80) == 0 {
                // It is a requirement of bitstream conformance that the value
                // returned from the leb128 parsing process is less than or
                // equal to (1 << 32)-1.
                return u32_from_u64(value);
            }
        }
        // It is a requirement of bitstream conformance that the most
        // significant bit of leb128_byte is equal to 0 if i is equal to 7.
        Err(AvifError::BmffParseFailed(
            "uleb value did not terminate after 8 bytes".into(),
        ))
    }
}

#[cfg(feature = "encoder")]
#[derive(Default)]
pub struct OStream {
    pub data: Vec<u8>,
    partial: Option<(u8, u8)>,
    box_marker_offsets: Vec<usize>,
}

#[cfg(feature = "encoder")]
#[allow(dead_code)]
impl OStream {
    pub(crate) fn offset(&self) -> usize {
        assert!(self.partial.is_none());
        self.data.len()
    }

    pub(crate) fn try_reserve(&mut self, size: usize) -> AvifResult<()> {
        self.data.try_reserve(size).or(Err(AvifError::OutOfMemory))
    }

    pub(crate) fn write_bits(&mut self, value: u8, num_bits: u8) -> AvifResult<()> {
        if num_bits == 0 || num_bits >= 8 {
            return Err(AvifError::UnknownError("".into()));
        }
        let (bits, offset) = self.partial.unwrap_or((0, 0));
        if offset + num_bits > 8 {
            // write_bits cannot overlap multiple bytes.
            return Err(AvifError::UnknownError("".into()));
        }
        let value_at_offset = (value & ((1 << num_bits) - 1)) << (8 - offset - num_bits);
        let bits = bits | value_at_offset;
        if offset + num_bits == 8 {
            self.partial = None;
            self.write_u8(bits)?;
        } else {
            self.partial = Some((bits, offset + num_bits));
        }
        Ok(())
    }

    pub(crate) fn write_bool(&mut self, value: bool) -> AvifResult<()> {
        self.write_bits(if value { 1 } else { 0 }, 1)
    }

    pub(crate) fn write_u8(&mut self, value: u8) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.try_reserve(1)?;
        self.data.push(value);
        Ok(())
    }

    pub(crate) fn write_u16(&mut self, value: u16) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.try_reserve(2)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u24(&mut self, value: u32) -> AvifResult<()> {
        assert!(self.partial.is_none());
        if value > 0xFFFFFF {
            return Err(AvifError::InvalidArgument);
        }
        self.try_reserve(3)?;
        self.data.extend_from_slice(&value.to_be_bytes()[1..]);
        Ok(())
    }

    pub(crate) fn write_u32(&mut self, value: u32) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.try_reserve(4)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u32_at_offset(&mut self, value: u32, offset: usize) -> AvifResult<()> {
        assert!(self.partial.is_none());
        let range = offset..offset + 4;
        check_slice_range(self.data.len(), &range)?;
        self.data[range].copy_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u64(&mut self, value: u64) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.try_reserve(8)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_str(&mut self, value: &str) -> AvifResult<()> {
        assert!(self.partial.is_none());
        let bytes = value.as_bytes();
        self.try_reserve(bytes.len())?;
        self.data.extend_from_slice(bytes);
        Ok(())
    }

    pub(crate) fn write_string(&mut self, value: &String) -> AvifResult<()> {
        assert!(self.partial.is_none());
        let bytes = value.as_bytes();
        self.try_reserve(bytes.len())?;
        self.data.extend_from_slice(bytes);
        Ok(())
    }

    pub(crate) fn write_string_with_nul(&mut self, value: &String) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.write_string(value)?;
        self.write_u8(0)?;
        Ok(())
    }

    pub(crate) fn write_slice(&mut self, data: &[u8]) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.try_reserve(data.len())?;
        self.data.extend_from_slice(data);
        Ok(())
    }

    pub(crate) fn write_ufraction(&mut self, value: UFraction) -> AvifResult<()> {
        self.write_u32(value.0)?;
        self.write_u32(value.1)
    }

    fn write_i32(&mut self, value: i32) -> AvifResult<()> {
        self.write_u32(value as u32)
    }

    pub(crate) fn write_fraction(&mut self, value: Fraction) -> AvifResult<()> {
        self.write_i32(value.0)?;
        self.write_u32(value.1)
    }

    fn start_box_impl(
        &mut self,
        box_type: &str,
        version_and_flags: Option<(u8, u32)>,
    ) -> AvifResult<()> {
        assert!(self.partial.is_none());
        self.box_marker_offsets.push(self.offset());
        // 4 bytes for size to be filled out later.
        self.write_u32(0)?;
        self.write_str(box_type)?;
        if let Some((version, flags)) = version_and_flags {
            self.write_u8(version)?;
            self.write_u24(flags)?;
        }
        Ok(())
    }

    pub(crate) fn start_box(&mut self, box_type: &str) -> AvifResult<()> {
        self.start_box_impl(box_type, None)
    }

    pub(crate) fn start_full_box(
        &mut self,
        box_type: &str,
        version_and_flags: (u8, u32),
    ) -> AvifResult<()> {
        self.start_box_impl(box_type, Some(version_and_flags))
    }

    pub(crate) fn finish_box(&mut self) -> AvifResult<()> {
        assert!(self.partial.is_none());
        let offset = self
            .box_marker_offsets
            .pop()
            .ok_or(AvifError::UnknownError("".into()))?;
        let box_size = u32_from_usize(checked_sub!(self.offset(), offset)?)?;
        self.write_u32_at_offset(box_size, offset)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn read_uxx() {
        let mut stream = IStream::create(&[1, 2, 3, 4, 5, 6, 7, 8]);
        assert_eq!(stream.read_uxx(0), Ok(0));
        assert_eq!(stream.offset, 0);
        assert_eq!(stream.read_uxx(1), Ok(1));
        assert_eq!(stream.offset, 1);
        stream.offset = 0;
        assert_eq!(stream.read_uxx(2), Ok(258));
        stream.offset = 0;
        assert_eq!(stream.read_u16(), Ok(258));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(3), Ok(66051));
        stream.offset = 0;
        assert_eq!(stream.read_u24(), Ok(66051));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(4), Ok(16909060));
        stream.offset = 0;
        assert_eq!(stream.read_u32(), Ok(16909060));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(5), Ok(4328719365));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(6), Ok(1108152157446));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(7), Ok(283686952306183));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(8), Ok(72623859790382856));
        stream.offset = 0;
        assert_eq!(stream.read_u64(), Ok(72623859790382856));
        stream.offset = 0;
        assert_eq!(stream.read_uxx(9), Err(AvifError::NotImplemented));
    }

    #[test]
    fn read_string() {
        let bytes = "abcd\0e".as_bytes();
        assert_eq!(IStream::create(bytes).read_string(4), Ok("abcd".into()));
        assert_eq!(IStream::create(bytes).read_string(5), Ok("abcd\0".into()));
        assert_eq!(IStream::create(bytes).read_string(6), Ok("abcd\0e".into()));
        assert!(matches!(
            IStream::create(bytes).read_string(8),
            Err(AvifError::BmffParseFailed(_))
        ));
        assert_eq!(IStream::create(bytes).read_c_string(), Ok("abcd".into()));
    }

    #[cfg(feature = "encoder")]
    #[test]
    fn write_bits() {
        let mut stream = OStream::default();
        assert_eq!(stream.write_bits(1, 1), Ok(()));
        assert_eq!(stream.data.len(), 0);
        assert_eq!(stream.write_bits(2, 3), Ok(()));
        assert_eq!(stream.data.len(), 0);
        assert_eq!(stream.write_bits(1, 4), Ok(()));
        assert_eq!(stream.data.len(), 1);
        assert_eq!(stream.write_bits(1, 4), Ok(()));
        assert_eq!(stream.data.len(), 1);
        assert_eq!(stream.write_bits(4, 4), Ok(()));
        assert_eq!(stream.data.len(), 2);
        assert_eq!(stream.write_u8(0xCC), Ok(()));
        assert_eq!(stream.data.len(), 3);
        assert_eq!(stream.data, vec![0xA1, 0x14, 0xCC]);
        assert!(stream.write_bits(1, 10).is_err());

        // Write 5 bits.
        assert_eq!(stream.write_bits(5, 5), Ok(()));
        // Now, trying to write 4 bits should fail since it overlaps more than 1 byte.
        assert!(stream.write_bits(5, 4).is_err());
    }

    #[cfg(feature = "encoder")]
    #[test]
    fn write_box() {
        let mut stream = OStream::default();
        assert!(stream.start_box("ftyp").is_ok());
        assert!(stream.write_u8(20).is_ok());
        assert!(stream.start_full_box("abcd", (0, 1)).is_ok());
        assert!(stream.write_u32(25).is_ok());
        assert!(stream.finish_box().is_ok());
        assert!(stream.finish_box().is_ok());
        assert!(stream.finish_box().is_err());
    }

    #[cfg(feature = "encoder")]
    #[test]
    fn write() {
        let mut stream = OStream::default();

        let u8value = 10;
        assert!(stream.write_u8(u8value).is_ok());
        assert_eq!(stream.offset(), 1);
        assert_eq!(stream.data[stream.data.len() - 1..], u8value.to_be_bytes());

        let u16value = 1000;
        assert!(stream.write_u16(u16value).is_ok());
        assert_eq!(stream.offset(), 3);
        assert_eq!(stream.data[stream.data.len() - 2..], u16value.to_be_bytes());

        let invalid_u24value = 0xFFFFFF1;
        assert!(stream.write_u24(invalid_u24value).is_err());
        let u24value = 12345678;
        assert!(stream.write_u24(u24value).is_ok());
        assert_eq!(stream.offset(), 6);
        assert_eq!(
            stream.data[stream.data.len() - 3..],
            u24value.to_be_bytes()[1..]
        );

        let u32value = 4294901760;
        assert!(stream.write_u32(u32value).is_ok());
        assert_eq!(stream.offset(), 10);
        assert_eq!(stream.data[stream.data.len() - 4..], u32value.to_be_bytes());

        assert!(stream.write_u32_at_offset(u32value, 4).is_ok());
        assert_eq!(stream.offset(), 10);
        assert_eq!(stream.data[4..8], u32value.to_be_bytes());
        assert!(stream.write_u32_at_offset(u32value, 20).is_err()); // invalid offset.

        let u64value = 0xFFFFFFFFFF;
        assert!(stream.write_u64(u64value).is_ok());
        assert_eq!(stream.offset(), 18);
        assert_eq!(stream.data[stream.data.len() - 8..], u64value.to_be_bytes());

        let strvalue = "hello";
        assert!(stream.write_str(strvalue).is_ok());
        assert_eq!(stream.offset(), 23);
        assert_eq!(&stream.data[stream.data.len() - 5..], strvalue.as_bytes());

        let stringvalue = String::from("hello");
        assert!(stream.write_string(&stringvalue).is_ok());
        assert_eq!(stream.offset(), 28);
        assert_eq!(
            &stream.data[stream.data.len() - 5..],
            stringvalue.as_bytes()
        );

        assert!(stream.write_string_with_nul(&stringvalue).is_ok());
        assert_eq!(stream.offset(), 34);
        assert_eq!(
            &stream.data[stream.data.len() - 6..stream.data.len() - 1],
            stringvalue.as_bytes()
        );
        assert_eq!(*stream.data.last().unwrap(), 0);

        let data = [100, 200, 50, 25];
        assert!(stream.write_slice(&data[..]).is_ok());
        assert_eq!(stream.offset(), 38);
        assert_eq!(&stream.data[stream.data.len() - 4..], &data[..]);

        let ufraction = UFraction(10, 20);
        assert!(stream.write_ufraction(ufraction).is_ok());
        assert_eq!(stream.offset(), 46);
        assert_eq!(
            stream.data[stream.data.len() - 8..stream.data.len() - 4],
            ufraction.0.to_be_bytes()
        );
        assert_eq!(
            stream.data[stream.data.len() - 4..],
            ufraction.1.to_be_bytes()
        );
    }
}
