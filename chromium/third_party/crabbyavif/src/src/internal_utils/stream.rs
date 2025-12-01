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
pub struct IStream<'a> {
    // The bytes to parse.
    pub data: &'a [u8],
    // The number of entire bytes read so far within self.data.
    pub offset: usize,
    // If not zero, number of most significant bits already read in the
    // byte of self.data located at index offset.
    num_bits: u8,
}

impl IStream<'_> {
    pub(crate) fn create(data: &[u8]) -> IStream<'_> {
        IStream {
            data,
            offset: 0,
            num_bits: 0,
        }
    }

    fn check(&self, num_bytes: usize) -> AvifResult<()> {
        if self.bytes_left()? < num_bytes {
            return AvifError::bmff_parse_failed("");
        }
        Ok(())
    }

    pub(crate) fn sub_stream<'a>(&'a mut self, num_bytes: &BoxSize) -> AvifResult<IStream<'a>> {
        assert_eq!(self.num_bits, 0);
        let offset = self.offset;
        checked_incr!(
            self.offset,
            match num_bytes {
                BoxSize::FixedSize(num_bytes) => {
                    self.check(*num_bytes)?;
                    *num_bytes
                }
                BoxSize::UntilEndOfStream => self.bytes_left()?,
            }
        );
        Ok(IStream {
            data: &self.data[offset..self.offset],
            offset: 0,
            num_bits: 0,
        })
    }

    pub(crate) fn bytes_left(&self) -> AvifResult<usize> {
        if self.data.len() < self.offset {
            return AvifError::unknown_error("");
        }
        Ok(self.data.len() - self.offset)
    }

    pub(crate) fn has_bytes_left(&self) -> AvifResult<bool> {
        Ok(self.bytes_left()? > 0)
    }

    pub(crate) fn get_slice(&mut self, num_bytes: usize) -> AvifResult<&[u8]> {
        assert_eq!(self.num_bits, 0);
        self.check(num_bytes)?;
        let offset_start = self.offset;
        checked_incr!(self.offset, num_bytes);
        Ok(&self.data[offset_start..offset_start + num_bytes])
    }

    pub(crate) fn get_immutable_vec(&self, num_bytes: usize) -> AvifResult<Vec<u8>> {
        assert_eq!(self.num_bits, 0);
        self.check(num_bytes)?;
        Ok(self.data[self.offset..self.offset + num_bytes].to_vec())
    }

    fn get_vec(&mut self, num_bytes: usize) -> AvifResult<Vec<u8>> {
        assert_eq!(self.num_bits, 0);
        Ok(self.get_slice(num_bytes)?.to_vec())
    }

    pub(crate) fn read_u8(&mut self) -> AvifResult<u8> {
        assert_eq!(self.num_bits, 0);
        self.check(1)?;
        let value = self.data[self.offset];
        checked_incr!(self.offset, 1);
        Ok(value)
    }

    pub(crate) fn read_u16(&mut self) -> AvifResult<u16> {
        assert_eq!(self.num_bits, 0);
        Ok(u16::from_be_bytes(self.get_slice(2)?.try_into().unwrap()))
    }

    pub(crate) fn read_u24(&mut self) -> AvifResult<u32> {
        assert_eq!(self.num_bits, 0);
        Ok(self.read_uxx(3)? as u32)
    }

    pub(crate) fn read_u32(&mut self) -> AvifResult<u32> {
        assert_eq!(self.num_bits, 0);
        Ok(u32::from_be_bytes(self.get_slice(4)?.try_into().unwrap()))
    }

    pub(crate) fn read_u64(&mut self) -> AvifResult<u64> {
        assert_eq!(self.num_bits, 0);
        Ok(u64::from_be_bytes(self.get_slice(8)?.try_into().unwrap()))
    }

    pub(crate) fn read_i8(&mut self) -> AvifResult<i8> {
        assert_eq!(self.num_bits, 0);
        Ok(self.read_u8()? as i8)
    }

    pub(crate) fn read_i16(&mut self) -> AvifResult<i16> {
        assert_eq!(self.num_bits, 0);
        Ok(self.read_u16()? as i16)
    }

    pub(crate) fn read_i32(&mut self) -> AvifResult<i32> {
        assert_eq!(self.num_bits, 0);
        Ok(self.read_u32()? as i32)
    }

    pub(crate) fn read_i64(&mut self) -> AvifResult<i64> {
        assert_eq!(self.num_bits, 0);
        Ok(self.read_u64()? as i64)
    }

    pub(crate) fn skip_u32(&mut self) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.skip(4)
    }

    pub(crate) fn skip_u64(&mut self) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.skip(8)
    }

    pub(crate) fn read_fraction(&mut self) -> AvifResult<Fraction> {
        Ok(Fraction(self.read_i32()?, self.read_u32()?))
    }

    pub(crate) fn read_ufraction(&mut self) -> AvifResult<UFraction> {
        Ok(UFraction(self.read_u32()?, self.read_u32()?))
    }

    // Reads size characters of a non-null-terminated string.
    pub(crate) fn read_string(&mut self, num_bytes: usize) -> AvifResult<String> {
        Ok(String::from_utf8(self.get_vec(num_bytes)?).unwrap_or("".into()))
    }

    // Reads an xx-byte unsigner integer.
    pub(crate) fn read_uxx(&mut self, num_bytes: u8) -> AvifResult<u64> {
        assert_eq!(self.num_bits, 0);
        let n: usize = num_bytes.into();
        if n == 0 {
            return Ok(0);
        }
        if n > 8 {
            return AvifError::not_implemented();
        }
        let mut out = [0; 8];
        let start = out.len() - n;
        out[start..].copy_from_slice(self.get_slice(n)?);
        Ok(u64::from_be_bytes(out))
    }

    // Reads a null-terminated string.
    pub(crate) fn read_c_string(&mut self) -> AvifResult<String> {
        assert_eq!(self.num_bits, 0);
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
            return AvifError::bmff_parse_failed("");
        }
        Ok((version, flags))
    }

    pub(crate) fn skip(&mut self, num_bytes: usize) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.check(num_bytes)?;
        checked_incr!(self.offset, num_bytes);
        Ok(())
    }

    pub(crate) fn rewind(&mut self, num_bytes: usize) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        checked_decr!(self.offset, num_bytes);
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
        AvifError::bmff_parse_failed("uleb value did not terminate after 8 bytes")
    }

    fn read_bit(&mut self) -> AvifResult<u8> {
        Ok(self.read_bits(1)? as u8)
    }

    pub(crate) fn read_bits(&mut self, num_bits: usize) -> AvifResult<u32> {
        if num_bits > 31 {
            return AvifError::bmff_parse_failed(format!(
                "BitReader does not support reading {} bits at once",
                num_bits
            ));
        }
        let mut remaining_num_bits = num_bits as u8;
        let mut value = 0;
        while remaining_num_bits != 0 {
            self.check(1)?;
            let byte = self.data[self.offset] as u32;
            // Number of bits among num_bits that can be read in the first byte of self.bytes.
            let num_read_bits = std::cmp::min(8 - self.num_bits, remaining_num_bits);
            // Write the most significant bits first.
            let read_bits =
                (byte >> (8 - self.num_bits - num_read_bits)) & ((1u32 << num_read_bits) - 1);
            value = (value << num_read_bits) | read_bits;
            self.num_bits += num_read_bits;
            if self.num_bits == 8 {
                self.offset += 1;
                self.num_bits = 0;
            }
            remaining_num_bits -= num_read_bits;
        }
        Ok(value)
    }

    pub(crate) fn read_bool(&mut self) -> AvifResult<bool> {
        Ok(self.read_bit()? == 1)
    }

    #[allow(dead_code)]
    pub(crate) fn pad(&mut self) -> AvifResult<()> {
        if self.num_bits != 0 && self.read_bits(8 - self.num_bits as usize)? != 0 {
            return AvifError::bmff_parse_failed("Padding not set to 0");
        }
        Ok(())
    }

    pub(crate) fn skip_bits(&mut self, num_bits: usize) -> AvifResult<()> {
        let new_num_bits = checked_add!(self.num_bits as usize, num_bits)?;
        self.check(new_num_bits.div_ceil(8))?;
        self.offset += new_num_bits / 8;
        self.num_bits = (new_num_bits % 8) as u8;
        Ok(())
    }

    pub(crate) fn skip_uvlc(&mut self) -> AvifResult<()> {
        // See the section 4.10.3. uvlc() of the AV1 specification.
        let mut leading_zeros = 0u128; // leadingZeros
        while !self.read_bool()? {
            leading_zeros += 1;
        }
        if leading_zeros < 32 {
            self.skip_bits(leading_zeros as usize)?; // f(leadingZeros) value;
        }
        Ok(())
    }
}

#[cfg(feature = "encoder")]
#[derive(Default)]
pub struct OStream {
    // The bytes written so far.
    pub data: Vec<u8>,
    // If not zero, number of most significant bits already written in the last
    // byte of self.data.
    num_bits: u8,
    // The positions in self.data where are written the 4-byte sizes of the
    // boxes that were started but not yet finished.
    box_marker_offsets: Vec<usize>,
}

#[cfg(feature = "encoder")]
#[allow(dead_code)]
impl OStream {
    pub(crate) fn offset(&self) -> usize {
        assert_eq!(self.num_bits, 0);
        self.data.len()
    }

    pub(crate) fn try_reserve(&mut self, size: usize) -> AvifResult<()> {
        self.data
            .try_reserve(size)
            .map_err(AvifError::map_out_of_memory)
    }

    pub(crate) fn write_bits(&mut self, value: u32, num_bits: u8) -> AvifResult<()> {
        if num_bits == 0 || num_bits > 31 {
            return AvifError::unknown_error("");
        }
        if value >= (1 << num_bits) {
            return AvifError::unknown_error("");
        }
        let mut num_remaining_bits = num_bits;
        while num_remaining_bits != 0 {
            if self.num_bits == 0 {
                self.write_u8(0)?;
            }
            let byte = self.data.last_mut().unwrap();
            // Number of bits among num_bits that can be written in the last byte of self.data.
            let num_written_bits = std::cmp::min(8 - self.num_bits, num_remaining_bits);
            // Write the most significant bits first (somewhat big endian).
            let written_bits = (value >> (num_remaining_bits - num_written_bits))
                & ((1u32 << num_written_bits) - 1);
            *byte |= (written_bits as u8) << (8 - self.num_bits - num_written_bits);
            num_remaining_bits -= num_written_bits;
            self.num_bits = (self.num_bits + num_written_bits) % 8;
        }
        Ok(())
    }

    pub(crate) fn pad(&mut self) -> AvifResult<()> {
        if self.num_bits != 0 {
            self.write_bits(0, 8 - self.num_bits)?;
            assert_eq!(self.num_bits, 0);
        }
        Ok(())
    }

    pub(crate) fn write_bool(&mut self, value: bool) -> AvifResult<()> {
        self.write_bits(if value { 1 } else { 0 }, 1)
    }

    pub(crate) fn write_u8(&mut self, value: u8) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.try_reserve(1)?;
        self.data.push(value);
        Ok(())
    }

    pub(crate) fn write_u16(&mut self, value: u16) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.try_reserve(2)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u24(&mut self, value: u32) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        if value > 0xFFFFFF {
            return AvifError::invalid_argument();
        }
        self.try_reserve(3)?;
        self.data.extend_from_slice(&value.to_be_bytes()[1..]);
        Ok(())
    }

    pub(crate) fn write_u32(&mut self, value: u32) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.try_reserve(4)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u32_at_offset(&mut self, value: u32, offset: usize) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        let range = offset..offset + 4;
        check_slice_range(self.data.len(), &range)?;
        self.data[range].copy_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_u64(&mut self, value: u64) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.try_reserve(8)?;
        self.data.extend_from_slice(&value.to_be_bytes());
        Ok(())
    }

    pub(crate) fn write_str(&mut self, value: &str) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        let bytes = value.as_bytes();
        self.try_reserve(bytes.len())?;
        self.data.extend_from_slice(bytes);
        Ok(())
    }

    pub(crate) fn write_str_with_nul(&mut self, value: &str) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.write_str(value)?;
        self.write_u8(0)?;
        Ok(())
    }

    pub(crate) fn write_string(&mut self, value: &String) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        let bytes = value.as_bytes();
        self.try_reserve(bytes.len())?;
        self.data.extend_from_slice(bytes);
        Ok(())
    }

    pub(crate) fn write_string_with_nul(&mut self, value: &String) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
        self.write_string(value)?;
        self.write_u8(0)?;
        Ok(())
    }

    // Searches through existing data for the given slice starting at offset |start_offset| and
    // writes it to the stream if it does not exist already. Returns the offset in which the slice
    // was found or written to.
    pub(crate) fn write_slice_dedupe(
        &mut self,
        start_offset: usize,
        data: &[u8],
    ) -> AvifResult<usize> {
        Ok(
            match self.data[start_offset..]
                .windows(data.len())
                .position(|window| window == data)
            {
                Some(position) => start_offset + position,
                None => {
                    let offset = self.offset();
                    self.write_slice(data)?;
                    offset
                }
            },
        )
    }

    pub(crate) fn write_slice(&mut self, data: &[u8]) -> AvifResult<()> {
        assert_eq!(self.num_bits, 0);
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
        assert_eq!(self.num_bits, 0);
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
        assert_eq!(self.num_bits, 0);
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
        assert_eq!(stream.read_uxx(9), AvifError::not_implemented());
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

    #[test]
    fn read_bits() {
        let bytes = "abcd\0e\0".as_bytes();
        let mut stream = IStream::create(bytes);
        assert_eq!(stream.read_bits(8), Ok('a'.into()));
        // Read most significant bits first.
        assert_eq!(stream.read_bits(1), Ok(0));
        assert_eq!(stream.read_bits(7), Ok('b'.into()));
        // Read across bytes and most significant bytes first.
        assert_eq!(stream.read_bits(1), Ok(0));
        assert_eq!(stream.read_bits(15), Ok(('c' as u32) << 8 | 'd' as u32));

        assert_eq!(stream.read_bits(8), Ok('\0'.into()));
        assert_eq!(stream.read_bits(8), Ok('e'.into()));
        assert_eq!(stream.read_bits(1), Ok(0));
        assert_eq!(stream.pad(), Ok(()));
        assert!(stream.read_bits(1).is_err());
    }

    #[cfg(feature = "encoder")]
    #[test]
    fn write_bits() {
        let mut stream = OStream::default();
        assert_eq!(stream.write_bits(1, 1), Ok(()));
        assert_eq!(stream.data.len(), 1);
        assert_eq!(stream.write_bits(2, 3), Ok(()));
        assert_eq!(stream.data.len(), 1);
        assert_eq!(stream.write_bits(1, 4), Ok(()));
        assert_eq!(stream.data.len(), 1);
        assert_eq!(stream.write_bits(1, 4), Ok(()));
        assert_eq!(stream.data.len(), 2);
        assert_eq!(stream.write_bits(4, 4), Ok(()));
        assert_eq!(stream.data.len(), 2);
        assert_eq!(stream.write_u8(0xCC), Ok(()));
        assert_eq!(stream.data.len(), 3);
        assert_eq!(stream.data, vec![0xA1, 0x14, 0xCC]);

        // Supports from 1 to 31 bits.
        assert!(stream.write_bits(0, 0).is_err());
        assert_eq!(stream.write_bits(0, 1), Ok(()));
        assert_eq!(stream.write_bits(0, 31), Ok(()));
        assert!(stream.write_bits(0, 32).is_err());

        // Supports bits overlapping multiple bytes.
        assert_eq!(stream.write_bits(5, 5), Ok(()));
        assert_eq!(stream.write_bits(5, 4), Ok(()));
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

    #[cfg(feature = "encoder")]
    #[test]
    fn write_slice_dedupe() {
        let mut stream = OStream::default();

        assert!(stream.write_slice(&[1, 2, 3, 4, 5, 6]).is_ok());
        assert_eq!(stream.offset(), 6);

        // Duplicate slice should return an existing offset.
        assert_eq!(stream.write_slice_dedupe(0, &[3, 4, 5]), Ok(2));
        assert_eq!(stream.offset(), 6);

        // Non-duplicate slice should extend the stream and return the new offset.
        assert_eq!(stream.write_slice_dedupe(0, &[10, 11, 12]), Ok(6));
        assert_eq!(stream.offset(), 9);

        // Duplicate slice should return an existing offset.
        assert_eq!(stream.write_slice_dedupe(0, &[10, 11, 12]), Ok(6));
        assert_eq!(stream.offset(), 9);

        // Duplicate slice but outside the start offset should extend the stream and return the new
        // offset.
        assert_eq!(stream.write_slice_dedupe(4, &[3, 4, 5]), Ok(9));
        assert_eq!(stream.offset(), 12);
    }
}
