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

    pub fn read(&mut self, n: usize) -> AvifResult<u32> {
        assert!(n <= 32);
        let mut value: u32 = 0;
        for _i in 0..n {
            value <<= 1;
            value |= self.read_bit()? as u32;
        }
        Ok(value)
    }

    pub fn read_bool(&mut self) -> AvifResult<bool> {
        let bit = self.read_bit()?;
        Ok(bit == 1)
    }

    pub fn skip(&mut self, n: usize) -> AvifResult<()> {
        if checked_add!(self.bit_offset, n)? > checked_mul!(self.data.len(), 8)? {
            return Err(AvifError::BmffParseFailed("Not enough bytes".into()));
        }
        self.bit_offset += n;
        Ok(())
    }

    pub fn skip_uvlc(&mut self) -> AvifResult<()> {
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

    pub fn remaining_bits(&self) -> AvifResult<usize> {
        checked_sub!(checked_mul!(self.data.len(), 8)?, self.bit_offset)
    }
}

#[derive(Debug)]
pub struct IStream<'a> {
    pub data: &'a [u8],
    pub offset: usize,
}

impl IStream<'_> {
    pub fn create(data: &[u8]) -> IStream {
        IStream { data, offset: 0 }
    }

    fn check(&self, size: usize) -> AvifResult<()> {
        if self.bytes_left()? < size {
            return Err(AvifError::BmffParseFailed("".into()));
        }
        Ok(())
    }

    pub fn sub_stream(&mut self, size: &BoxSize) -> AvifResult<IStream> {
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

    pub fn sub_bit_stream(&mut self, size: usize) -> AvifResult<IBitStream> {
        self.check(size)?;
        let offset = self.offset;
        checked_incr!(self.offset, size);
        Ok(IBitStream {
            data: &self.data[offset..self.offset],
            bit_offset: 0,
        })
    }

    pub fn bytes_left(&self) -> AvifResult<usize> {
        if self.data.len() < self.offset {
            return Err(AvifError::UnknownError("".into()));
        }
        Ok(self.data.len() - self.offset)
    }

    pub fn has_bytes_left(&self) -> AvifResult<bool> {
        Ok(self.bytes_left()? > 0)
    }

    pub fn get_slice(&mut self, size: usize) -> AvifResult<&[u8]> {
        self.check(size)?;
        let offset_start = self.offset;
        checked_incr!(self.offset, size);
        Ok(&self.data[offset_start..offset_start + size])
    }

    pub fn get_immutable_vec(&self, size: usize) -> AvifResult<Vec<u8>> {
        self.check(size)?;
        Ok(self.data[self.offset..self.offset + size].to_vec())
    }

    fn get_vec(&mut self, size: usize) -> AvifResult<Vec<u8>> {
        Ok(self.get_slice(size)?.to_vec())
    }

    pub fn read_u8(&mut self) -> AvifResult<u8> {
        self.check(1)?;
        let value = self.data[self.offset];
        checked_incr!(self.offset, 1);
        Ok(value)
    }

    pub fn read_u16(&mut self) -> AvifResult<u16> {
        Ok(u16::from_be_bytes(self.get_slice(2)?.try_into().unwrap()))
    }

    pub fn read_u24(&mut self) -> AvifResult<u32> {
        Ok(self.read_uxx(3)? as u32)
    }

    pub fn read_u32(&mut self) -> AvifResult<u32> {
        Ok(u32::from_be_bytes(self.get_slice(4)?.try_into().unwrap()))
    }

    pub fn read_u64(&mut self) -> AvifResult<u64> {
        Ok(u64::from_be_bytes(self.get_slice(8)?.try_into().unwrap()))
    }

    pub fn read_i32(&mut self) -> AvifResult<i32> {
        // For now this is used only for gainmap fractions where we need
        // wrapping conversion from u32 to i32.
        Ok(self.read_u32()? as i32)
    }

    pub fn skip_u16(&mut self) -> AvifResult<()> {
        self.skip(2)
    }

    pub fn skip_u32(&mut self) -> AvifResult<()> {
        self.skip(4)
    }

    pub fn skip_u64(&mut self) -> AvifResult<()> {
        self.skip(8)
    }

    pub fn read_fraction(&mut self) -> AvifResult<Fraction> {
        Ok(Fraction(self.read_i32()?, self.read_u32()?))
    }

    pub fn read_ufraction(&mut self) -> AvifResult<UFraction> {
        Ok(UFraction(self.read_u32()?, self.read_u32()?))
    }

    // Reads size characters of a non-null-terminated string.
    pub fn read_string(&mut self, size: usize) -> AvifResult<String> {
        Ok(String::from_utf8(self.get_vec(size)?).unwrap_or("".into()))
    }

    // Reads an xx-byte unsigner integer.
    pub fn read_uxx(&mut self, xx: u8) -> AvifResult<u64> {
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
    pub fn read_c_string(&mut self) -> AvifResult<String> {
        self.check(1)?;
        let null_position = self.data[self.offset..]
            .iter()
            .position(|&x| x == b'\0')
            .ok_or(AvifError::BmffParseFailed("".into()))?;
        let range = self.offset..self.offset + null_position;
        self.offset += null_position + 1;
        Ok(String::from_utf8(self.data[range].to_vec()).unwrap_or("".into()))
    }

    pub fn read_version_and_flags(&mut self) -> AvifResult<(u8, u32)> {
        let version = self.read_u8()?;
        let flags = self.read_u24()?;
        Ok((version, flags))
    }

    pub fn read_and_enforce_version_and_flags(
        &mut self,
        enforced_version: u8,
    ) -> AvifResult<(u8, u32)> {
        let (version, flags) = self.read_version_and_flags()?;
        if version != enforced_version {
            return Err(AvifError::BmffParseFailed("".into()));
        }
        Ok((version, flags))
    }

    pub fn skip(&mut self, size: usize) -> AvifResult<()> {
        self.check(size)?;
        checked_incr!(self.offset, size);
        Ok(())
    }

    pub fn rewind(&mut self, size: usize) -> AvifResult<()> {
        checked_decr!(self.offset, size);
        Ok(())
    }

    pub fn read_uleb128(&mut self) -> AvifResult<u32> {
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
}
