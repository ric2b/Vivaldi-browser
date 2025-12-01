// Copyright 2025 Google LLC
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
use crate::*;

#[derive(Clone, Copy, Debug)]
pub enum SampleTransformUnaryOp {
    // Unary operators. L is the operand.
    Negation, // S = -L
    Absolute, // S = |L|
    Not,      // S = ~L
    Bsr,      // S = L<=0 ? 0 : truncate(log2(L)) (Bit Scan Reverse)
}

#[derive(Clone, Copy, Debug)]
pub enum SampleTransformBinaryOp {
    Sum,        // S = L + R
    Difference, // S = L - R
    Product,    // S = L * R
    Quotient,   // S = R==0 ? L : truncate(L / R)
    And,        // S = L & R
    Or,         // S = L | R
    Xor,        // S = L ^ R
    Pow,        // S = L==0 ? 0 : truncate(pow(L, R))
    Min,        // S = L<=R ? L : R
    Max,        // S = L<=R ? R : L
}

#[derive(Debug)]
pub enum SampleTransformToken {
    Constant(i64),
    ImageItem(usize), // 0-based item_idx in source items
    UnaryOp(SampleTransformUnaryOp),
    BinaryOp(SampleTransformBinaryOp),
}

#[derive(Debug, Default)]
pub struct SampleTransform {
    pub bit_depth: u8,
    pub num_inputs: usize, // Number of input images.
    pub tokens: Vec<SampleTransformToken>,
}

impl SampleTransformUnaryOp {
    pub(crate) fn apply(self, value: i64, bounds: (i64, i64)) -> i64 {
        let v = match self {
            SampleTransformUnaryOp::Negation => value.saturating_neg(),
            SampleTransformUnaryOp::Absolute => value.saturating_abs(),
            SampleTransformUnaryOp::Not => !value,
            SampleTransformUnaryOp::Bsr => {
                if value <= 0 {
                    0
                } else {
                    value.ilog2() as i64
                }
            }
        };
        v.clamp(bounds.0, bounds.1)
    }
}

impl SampleTransformBinaryOp {
    pub(crate) fn apply(self, left: i64, right: i64, bounds: (i64, i64)) -> i64 {
        let v = match self {
            SampleTransformBinaryOp::Sum => left.saturating_add(right),
            SampleTransformBinaryOp::Difference => left.saturating_sub(right),
            SampleTransformBinaryOp::Product => left.saturating_mul(right),
            SampleTransformBinaryOp::Quotient => {
                if right == 0 {
                    left
                } else {
                    left.saturating_div(right)
                }
            }
            SampleTransformBinaryOp::And => left & right,
            SampleTransformBinaryOp::Or => left | right,
            SampleTransformBinaryOp::Xor => left ^ right,
            SampleTransformBinaryOp::Pow => {
                if left == 0 || left == 1 {
                    left
                } else if left == -1 {
                    if right % 2 == 0 {
                        1
                    } else {
                        -1
                    }
                } else if right == 0 {
                    1
                } else if right == 1 {
                    left
                } else if right < 0 {
                    // L^R is in ]-1:1[ here, so truncating it always gives 0.
                    0
                } else {
                    left.saturating_pow(right.try_into().unwrap_or(u32::MAX))
                }
            }
            SampleTransformBinaryOp::Min => std::cmp::min(left, right),
            SampleTransformBinaryOp::Max => std::cmp::max(left, right),
        };
        v.clamp(bounds.0, bounds.1)
    }
}

pub(crate) enum StackItem {
    Values(Vec<i64>),
    Constant(i64),
    ImageItem(usize),
}

impl SampleTransformToken {
    pub(crate) fn apply(
        &self,
        stack: &mut Vec<StackItem>,
        extra_inputs: &[Image],
        plane: Plane,
        y: u32,
        width: usize,
        bounds: (i64, i64),
    ) -> AvifResult<()> {
        let result = match self {
            SampleTransformToken::Constant(c) => StackItem::Constant(*c),
            SampleTransformToken::ImageItem(item_idx) => StackItem::ImageItem(*item_idx),
            SampleTransformToken::UnaryOp(op) => {
                let value = stack.pop().unwrap();
                match value {
                    StackItem::Values(values) => {
                        StackItem::Values(values.iter().map(|v| op.apply(*v, bounds)).collect())
                    }
                    StackItem::Constant(c) => StackItem::Constant(op.apply(c, bounds)),
                    StackItem::ImageItem(item_idx) => {
                        if extra_inputs[item_idx].depth == 8 {
                            let row8 = extra_inputs[item_idx].row_exact(plane, y)?;
                            StackItem::Values(
                                row8.iter().map(|v| op.apply(*v as i64, bounds)).collect(),
                            )
                        } else {
                            let row16 = extra_inputs[item_idx].row16_exact(plane, y)?;
                            StackItem::Values(
                                row16.iter().map(|v| op.apply(*v as i64, bounds)).collect(),
                            )
                        }
                    }
                }
            }
            SampleTransformToken::BinaryOp(op) => {
                let right = stack.pop().unwrap();
                let left = stack.pop().unwrap();
                match (left, right) {
                    (StackItem::Values(left), StackItem::Values(right)) => StackItem::Values(
                        left.iter()
                            .zip(right.iter())
                            .map(|(l, r)| op.apply(*l, *r, bounds))
                            .collect(),
                    ),
                    (StackItem::Values(left), StackItem::Constant(right)) => StackItem::Values(
                        left.iter().map(|l| op.apply(*l, right, bounds)).collect(),
                    ),
                    (StackItem::Values(left), StackItem::ImageItem(right_idx)) => {
                        if extra_inputs[right_idx].depth == 8 {
                            let row8 = extra_inputs[right_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left[i], row8[i] as i64, bounds))
                                    .collect(),
                            )
                        } else {
                            let row16 = extra_inputs[right_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left[i], row16[i] as i64, bounds))
                                    .collect(),
                            )
                        }
                    }
                    (StackItem::Constant(left), StackItem::Values(right)) => StackItem::Values(
                        right.iter().map(|r| op.apply(left, *r, bounds)).collect(),
                    ),
                    (StackItem::Constant(left), StackItem::Constant(right)) => {
                        StackItem::Constant(op.apply(left, right, bounds))
                    }
                    (StackItem::Constant(left), StackItem::ImageItem(right_idx)) => {
                        if extra_inputs[right_idx].depth == 8 {
                            let row8 = extra_inputs[right_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left, row8[i] as i64, bounds))
                                    .collect(),
                            )
                        } else {
                            let row16 = extra_inputs[right_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left, row16[i] as i64, bounds))
                                    .collect(),
                            )
                        }
                    }
                    (StackItem::ImageItem(left_idx), StackItem::Values(right)) => {
                        if extra_inputs[left_idx].depth == 8 {
                            let row8 = extra_inputs[left_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(row8[i] as i64, right[i], bounds))
                                    .collect(),
                            )
                        } else {
                            let row16 = extra_inputs[left_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(row16[i] as i64, right[i], bounds))
                                    .collect(),
                            )
                        }
                    }
                    (StackItem::ImageItem(left_idx), StackItem::Constant(right)) => {
                        if extra_inputs[left_idx].depth == 8 {
                            let row8 = extra_inputs[left_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(row8[i] as i64, right, bounds))
                                    .collect(),
                            )
                        } else {
                            let row16 = extra_inputs[left_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(row16[i] as i64, right, bounds))
                                    .collect(),
                            )
                        }
                    }
                    (StackItem::ImageItem(left_idx), StackItem::ImageItem(right_idx)) => {
                        if extra_inputs[left_idx].depth == 8 && extra_inputs[right_idx].depth == 8 {
                            let left8 = extra_inputs[left_idx].row(plane, y)?;
                            let right8 = extra_inputs[right_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left8[i] as i64, right8[i] as i64, bounds))
                                    .collect(),
                            )
                        } else if extra_inputs[left_idx].depth == 8
                            && extra_inputs[right_idx].depth > 8
                        {
                            let left8 = extra_inputs[left_idx].row(plane, y)?;
                            let right16 = extra_inputs[right_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left8[i] as i64, right16[i] as i64, bounds))
                                    .collect(),
                            )
                        } else if extra_inputs[left_idx].depth > 8
                            && extra_inputs[right_idx].depth == 8
                        {
                            let left16 = extra_inputs[left_idx].row16(plane, y)?;
                            let right8 = extra_inputs[right_idx].row(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left16[i] as i64, right8[i] as i64, bounds))
                                    .collect(),
                            )
                        } else {
                            let left16 = extra_inputs[left_idx].row16(plane, y)?;
                            let right16 = extra_inputs[right_idx].row16(plane, y)?;
                            StackItem::Values(
                                (0..width)
                                    .map(|i| op.apply(left16[i] as i64, right16[i] as i64, bounds))
                                    .collect(),
                            )
                        }
                    }
                }
            }
        };
        stack.push(result);
        Ok(())
    }
}

impl SampleTransform {
    pub(crate) fn apply(&self, extra_inputs: &[Image], output: &mut Image) -> AvifResult<()> {
        let max_stack_size = self.tokens.len().div_ceil(2);
        let mut stack: Vec<StackItem> = create_vec_exact(max_stack_size)?;
        for plane in if output.has_alpha() { ALL_PLANES.as_slice() } else { YUV_PLANES.as_slice() }
        {
            self.apply_internal(*plane, extra_inputs, output, &mut stack)?;
        }
        Ok(())
    }

    #[cfg(feature = "encoder")]
    pub(crate) fn apply_to_planes(
        &self,
        category: Category,
        extra_inputs: &[Image],
        output: &mut Image,
    ) -> AvifResult<()> {
        let max_stack_size = self.tokens.len().div_ceil(2);
        let mut stack: Vec<StackItem> = create_vec_exact(max_stack_size)?;
        for plane in match category {
            Category::Color | Category::Gainmap => YUV_PLANES.as_slice(),
            Category::Alpha => &[Plane::A],
        } {
            self.apply_internal(*plane, extra_inputs, output, &mut stack)?;
        }
        Ok(())
    }

    fn apply_internal(
        &self,
        plane: Plane,
        extra_inputs: &[Image],
        output: &mut Image,
        stack: &mut Vec<StackItem>,
    ) -> AvifResult<()> {
        // AVIF specification Draft, 8 January 2025, Section 4.2.3.3.:
        // The result of any computation underflowing or overflowing the intermediate
        // bit depth is replaced by -^(2num_bits-1) and 2^(num_bits-1)-1, respectively.
        // Encoder implementations should not create files leading to potential computation
        // underflow or overflow. Decoder implementations shall check for computation
        // underflow or overflow and clamp the results accordingly. Computations with
        // operands of negative values use the two’s-complement representation.
        let bounds = match self.bit_depth {
            8 => (i8::MIN as i64, i8::MAX as i64),
            16 => (i16::MIN as i64, i16::MAX as i64),
            32 => (i32::MIN as i64, i32::MAX as i64),
            64 => (i64::MIN, i64::MAX),
            _ => unreachable!(),
        };

        let width = output.width(plane);

        // Process the image row by row.
        for y in 0..u32_from_usize(output.height(plane))? {
            for token in &self.tokens {
                token.apply(stack, extra_inputs, plane, y, width, bounds)?;
            }

            assert_eq!(stack.len(), 1);
            let result: StackItem = stack.pop().unwrap();

            let mut output_min: u16 = 0;
            let mut output_max: u16 = output.max_channel();
            if output.yuv_range == YuvRange::Limited && output.depth >= 8 {
                output_min = 16u16 << (output.depth - 8);
                output_max = 235u16 << (output.depth - 8);
            }
            match result {
                StackItem::Values(values) => {
                    if output.depth == 8 {
                        let output_row8 = output.row_mut(plane, y)?;
                        for x in 0..width {
                            let v = values[x].clamp(output_min as i64, output_max as i64);
                            output_row8[x] = v as u8;
                        }
                    } else {
                        let output_row16 = output.row16_mut(plane, y)?;
                        for x in 0..width {
                            let v = values[x].clamp(output_min as i64, output_max as i64);
                            output_row16[x] = v as u16;
                        }
                    }
                }
                StackItem::Constant(c) => {
                    if output.depth == 8 {
                        let output_row8 = output.row_exact_mut(plane, y)?;
                        let c8 = c.clamp(output_min as i64, output_max as i64) as u8;
                        for v in output_row8.iter_mut() {
                            *v = c8;
                        }
                    } else {
                        let output_row16 = output.row16_exact_mut(plane, y)?;
                        let c16 = c.clamp(output_min as i64, output_max as i64) as u16;
                        for v in output_row16.iter_mut() {
                            *v = c16;
                        }
                    }
                }
                StackItem::ImageItem(item_idx) => {
                    if output.depth == extra_inputs[item_idx].depth {
                        if output.depth == 8 {
                            output
                                .row_exact_mut(plane, y)?
                                .copy_from_slice(extra_inputs[item_idx].row_exact(plane, y)?);
                        } else {
                            output
                                .row16_exact_mut(plane, y)?
                                .copy_from_slice(extra_inputs[item_idx].row16_exact(plane, y)?);
                        }
                    } else if output.depth == 8 && extra_inputs[item_idx].depth > 8 {
                        let input_row16 = extra_inputs[item_idx].row16(plane, y)?;
                        let output_row8 = output.row_mut(plane, y)?;
                        for x in 0..width {
                            output_row8[x] = input_row16[x].clamp(output_min, output_max) as u8;
                        }
                    } else if output.depth > 8 && extra_inputs[item_idx].depth == 8 {
                        let input_row8 = extra_inputs[item_idx].row(plane, y)?;
                        let output_row16 = output.row16_mut(plane, y)?;
                        for x in 0..width {
                            output_row16[x] = input_row8[x] as u16;
                        }
                    } else {
                        // Both are high bit depth.
                        let input_row16 = extra_inputs[item_idx].row16(plane, y)?;
                        let output_row16 = output.row16_mut(plane, y)?;
                        for x in 0..width {
                            output_row16[x] = input_row16[x].clamp(output_min, output_max);
                        }
                    }
                }
            }
        }

        Ok(())
    }

    fn is_valid(&self) -> AvifResult<()> {
        let mut stack_size: i32 = 0;
        for token in &self.tokens {
            match token {
                SampleTransformToken::Constant(_) => {
                    stack_size += 1;
                }
                SampleTransformToken::ImageItem(item_idx) => {
                    if *item_idx >= self.num_inputs {
                        return AvifError::invalid_image_grid("invalid input image item index");
                    }
                    stack_size += 1;
                }
                SampleTransformToken::UnaryOp(_) => {
                    if stack_size < 1 {
                        return AvifError::invalid_image_grid(
                            "invalid stack size for unary operator",
                        );
                    }
                    // Pop one and push one; the stack size doesn't change.
                }
                SampleTransformToken::BinaryOp(_) => {
                    if stack_size < 2 {
                        return AvifError::invalid_image_grid(
                            "invalid stack size for binary operator",
                        );
                    }
                    stack_size -= 1; // Pop two and push one.
                }
            }
        }
        if stack_size != 1 {
            return AvifError::invalid_image_grid(
                "invalid stack size at the end of sample transform",
            );
        }
        Ok(())
    }

    pub(crate) fn create_from(
        bit_depth: u8,
        num_inputs: usize,
        tokens: Vec<SampleTransformToken>,
    ) -> AvifResult<Self> {
        let sample_transform = SampleTransform {
            bit_depth,
            num_inputs,
            tokens,
        };
        sample_transform.is_valid()?;
        Ok(sample_transform)
    }

    pub(crate) fn allocate_planes_and_apply(
        &self,
        extra_inputs: &[Image],
        output: &mut Image,
    ) -> AvifResult<()> {
        output.allocate_planes(Category::Color)?;
        if self.num_inputs > 0 && extra_inputs[0].has_alpha() {
            output.allocate_planes(Category::Alpha)?;
        }
        self.apply(extra_inputs, output)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::image::Image;
    use crate::image::YuvRange;
    use crate::utils::pixels::*;
    use test_case::test_case;

    // Constant
    #[test_case(8, 8, 16, YuvRange::Full, vec![],
        vec![SampleTransformToken::Constant(42)], 42)]
    // Limited range
    #[test_case(8, 8, 8, YuvRange::Limited, vec![],
        vec![SampleTransformToken::Constant(5)], 16)]
    // Image
    #[test_case(8, 8, 8, YuvRange::Limited, vec![1, 42, 3],
            vec![SampleTransformToken::ImageItem(1)], 42)]
    // Shift 8 bit image to 16 bit
    #[test_case(8, 32, 16, YuvRange::Full, vec![42],
        vec![SampleTransformToken::ImageItem(0),
        SampleTransformToken::Constant(256),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product)], 10752)]
    // Shift 12 bit image to 8 bit
    #[test_case(12, 16, 8, YuvRange::Full, vec![3022],
            vec![SampleTransformToken::ImageItem(0),
            SampleTransformToken::Constant(16),
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 188)]
    // Complex expression
    #[test_case(8, 8, 8, YuvRange::Limited, vec![],
        vec![
                SampleTransformToken::Constant(10),
                SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Negation),
                SampleTransformToken::Constant(4),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product),
                SampleTransformToken::Constant(2),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Difference),
                SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Negation),
            ], 42)]
    // Overflow
    #[test_case(8, 8, 8, YuvRange::Full, vec![],
        vec![
                SampleTransformToken::Constant(100),
                SampleTransformToken::Constant(100),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product),
                SampleTransformToken::Constant(-10),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Sum),
            ], 117)]
    // BinaryOp(Values, Values)
    #[test_case(8, 8, 8, YuvRange::Full, vec![42, 10],
                vec![SampleTransformToken::ImageItem(0), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
                SampleTransformToken::ImageItem(1), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 4)]
    // BinaryOp(Values, Constant)
    #[test_case(8, 8, 8, YuvRange::Full, vec![42],
        vec![SampleTransformToken::ImageItem(0), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
        SampleTransformToken::Constant(5), SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Difference)], 37)]
    // BinaryOp(Values, Image)
    #[test_case(8, 8, 8, YuvRange::Full, vec![42, 10],
        vec![SampleTransformToken::ImageItem(0), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
        SampleTransformToken::ImageItem(1),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 4)]
    // BinaryOp(Constant, Values)
    #[test_case(8, 8, 8, YuvRange::Full, vec![3],
        vec![SampleTransformToken::Constant(100), SampleTransformToken::ImageItem(0), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 33)]
    // BinaryOp(Constant, Constant)
    #[test_case(8, 8, 8, YuvRange::Full, vec![],
        vec![SampleTransformToken::Constant(100), SampleTransformToken::Constant(200), SampleTransformToken::BinaryOp(SampleTransformBinaryOp::And)], 64)]
    // BinaryOp(Constant, Image)
    #[test_case(8, 8, 8, YuvRange::Full, vec![3],
        vec![SampleTransformToken::Constant(100), SampleTransformToken::ImageItem(0),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 33)]
    // BinaryOp(Image, Values)
    #[test_case(8, 8, 8, YuvRange::Full, vec![42, 10],
        vec![SampleTransformToken::ImageItem(0),
        SampleTransformToken::ImageItem(1), SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 4)]
    // BinaryOp(Image, Constant)
    #[test_case(8, 8, 8, YuvRange::Full, vec![100],
        vec![        SampleTransformToken::ImageItem(0),
        SampleTransformToken::Constant(3),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 33)]
    // BinaryOp(Image, Image)
    #[test_case(8, 8, 8, YuvRange::Full, vec![42, 10],
        vec![SampleTransformToken::ImageItem(0), SampleTransformToken::ImageItem(1),
        SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient)], 4)]
    fn test_apply_generic(
        input_depth: u8,
        intermediate_depth: u8,
        output_depth: u8,
        yuv_range: YuvRange,
        input_image_values: Vec<u16>,
        tokens: Vec<SampleTransformToken>,
        expected_value: i32,
    ) -> AvifResult<()> {
        let width = 10;
        let height = 3;
        let yuv_format = PixelFormat::Yuv420;
        let num_inputs = input_image_values.len();
        let sample_transform =
            SampleTransform::create_from(intermediate_depth, num_inputs, tokens)?;
        let mut extra_inputs = create_vec_exact(num_inputs)?;
        for i in 0..num_inputs {
            extra_inputs.push(Image {
                width,
                height,
                depth: input_depth,
                yuv_format,
                yuv_range,
                ..Default::default()
            });
            extra_inputs[i].allocate_planes(Category::Color)?;
            if input_depth == 8 {
                extra_inputs[i].row_mut(Plane::Y, 0)?.copy_from_slice(&vec![
                    input_image_values[i]
                        as u8;
                    width as usize
                ]);
            } else {
                extra_inputs[i]
                    .row16_mut(Plane::Y, 0)?
                    .copy_from_slice(&vec![input_image_values[i]; width as usize]);
            }
        }

        let mut output = Image {
            width,
            height,
            depth: output_depth,
            yuv_format,
            yuv_range,
            ..Default::default()
        };
        output.allocate_planes(Category::Color)?;

        sample_transform.apply(&extra_inputs, &mut output)?;

        if output_depth == 8 {
            assert_eq!(
                output.row(Plane::Y, 0)?.first(),
                Some(&(expected_value as u8))
            );
        } else {
            assert_eq!(
                output.row16(Plane::Y, 0)?.first(),
                Some(&(expected_value as u16))
            );
        }

        Ok(())
    }

    #[test]
    fn test_apply_image_item() -> AvifResult<()> {
        let sample_transform = SampleTransform::create_from(
            8,
            2,
            vec![
                SampleTransformToken::ImageItem(0),
                SampleTransformToken::Constant(2),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product),
                SampleTransformToken::ImageItem(1),
                SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Sum),
            ],
        )?;
        let width = 2;
        let height = 1;
        let mut output = Image {
            width,
            height,
            depth: 8,
            yuv_format: PixelFormat::Yuv444,
            yuv_range: YuvRange::Full,
            ..Default::default()
        };
        output.allocate_planes(Category::Color)?;
        output.allocate_planes(Category::Alpha)?;
        let mut extra_inputs = Vec::new();
        let mut input_image = Image {
            width,
            height,
            depth: 8,
            yuv_format: PixelFormat::Yuv444,
            yuv_range: YuvRange::Full,
            ..Default::default()
        };
        input_image.allocate_planes(Category::Color)?;
        input_image.allocate_planes(Category::Alpha)?;
        input_image.row_mut(Plane::Y, 0)?.copy_from_slice(&[10, 20]);
        input_image.row_mut(Plane::U, 0)?.copy_from_slice(&[30, 40]);
        input_image.row_mut(Plane::V, 0)?.copy_from_slice(&[50, 60]);
        input_image.row_mut(Plane::A, 0)?.copy_from_slice(&[1, 80]);
        extra_inputs.push(input_image);
        let mut input_image = Image {
            width,
            height,
            depth: 8,
            yuv_format: PixelFormat::Yuv444,
            yuv_range: YuvRange::Full,
            ..Default::default()
        };
        input_image.allocate_planes(Category::Color)?;
        input_image.allocate_planes(Category::Alpha)?;
        input_image.row_mut(Plane::Y, 0)?.copy_from_slice(&[1, 2]);
        input_image.row_mut(Plane::U, 0)?.copy_from_slice(&[3, 4]);
        input_image.row_mut(Plane::V, 0)?.copy_from_slice(&[5, 6]);
        input_image.row_mut(Plane::A, 0)?.copy_from_slice(&[7, 8]);
        extra_inputs.push(input_image);

        sample_transform.apply(&extra_inputs, &mut output)?;

        assert_eq!(output.row(Plane::Y, 0), Ok::<&[u8], _>(&[21, 42]));
        assert_eq!(output.row(Plane::U, 0), Ok::<&[u8], _>(&[63, 84]));
        assert_eq!(output.row(Plane::V, 0), Ok::<&[u8], _>(&[105, 126]));
        // Second value capped at 127 because of "bit_depth: 8" in SampleTransform.
        assert_eq!(output.row(Plane::A, 0), Ok::<&[u8], _>(&[9, 127]));
        Ok(())
    }

    #[test_case(8, 8)]
    #[test_case(8, 10)]
    #[test_case(8, 16)]
    #[test_case(10, 8)]
    #[test_case(10, 10)]
    #[test_case(10, 16)]
    #[test_case(16, 8)]
    #[test_case(16, 10)]
    #[test_case(16, 16)]
    fn test_copy_image(input_bit_depth: u8, output_bit_depth: u8) -> AvifResult<()> {
        let sample_transform =
            SampleTransform::create_from(32, 1, vec![SampleTransformToken::ImageItem(0)])?;
        let width = 2;
        let height = 1;
        let mut output = Image {
            width,
            height,
            depth: output_bit_depth,
            yuv_format: PixelFormat::Yuv444,
            yuv_range: YuvRange::Full,
            ..Default::default()
        };
        output.allocate_planes(Category::Color)?;
        output.allocate_planes(Category::Alpha)?;
        let mut extra_inputs = Vec::new();
        let mut input_image = Image {
            width,
            height,
            depth: input_bit_depth,
            yuv_format: PixelFormat::Yuv444,
            yuv_range: YuvRange::Full,
            ..Default::default()
        };
        if input_bit_depth == 8 {
            input_image.planes[0] = Some(Pixels::Buffer(vec![10, 20, 99]));
            input_image.planes[1] = Some(Pixels::Buffer(vec![30, 40, 99]));
            input_image.planes[2] = Some(Pixels::Buffer(vec![50, 60, 99]));
            input_image.planes[3] = Some(Pixels::Buffer(vec![1, 80, 99]));
            input_image.row_bytes = [3; 4];
        } else {
            input_image.planes[0] = Some(Pixels::Buffer16(vec![10, 20, 99]));
            input_image.planes[1] = Some(Pixels::Buffer16(vec![30, 40, 99]));
            input_image.planes[2] = Some(Pixels::Buffer16(vec![50, 60, 99]));
            input_image.planes[3] = Some(Pixels::Buffer16(vec![1, 80, 99]));
            input_image.row_bytes = [6; 4];
        }
        input_image.image_owns_planes = [false; 4];
        extra_inputs.push(input_image);

        sample_transform.apply(&extra_inputs, &mut output)?;

        if output_bit_depth == 8 {
            assert_eq!(output.row(Plane::Y, 0), Ok::<&[u8], _>(&[10, 20]));
            assert_eq!(output.row(Plane::U, 0), Ok::<&[u8], _>(&[30, 40]));
            assert_eq!(output.row(Plane::V, 0), Ok::<&[u8], _>(&[50, 60]));
            assert_eq!(output.row(Plane::A, 0), Ok::<&[u8], _>(&[1, 80]));
        } else {
            assert_eq!(output.row16(Plane::Y, 0), Ok::<&[u16], _>(&[10, 20]));
            assert_eq!(output.row16(Plane::U, 0), Ok::<&[u16], _>(&[30, 40]));
            assert_eq!(output.row16(Plane::V, 0), Ok::<&[u16], _>(&[50, 60]));
            assert_eq!(output.row16(Plane::A, 0), Ok::<&[u16], _>(&[1, 80]));
        }
        Ok(())
    }

    #[test]
    fn test_pow() {
        let pow = SampleTransformBinaryOp::Pow;
        let clamp = (0, 255);
        assert_eq!(pow.apply(-2, i32::MIN as i64, clamp), 0);
        assert_eq!(pow.apply(-2, -3, clamp), 0);
        assert_eq!(pow.apply(-2, -2, clamp), 0);
        assert_eq!(pow.apply(-2, -1, clamp), 0);
        assert_eq!(pow.apply(-2, 0, clamp), 1);
        assert_eq!(pow.apply(-2, 1, clamp), 0); // -2 clamped
        assert_eq!(pow.apply(-2, 2, clamp), 4);
        assert_eq!(pow.apply(-2, 3, clamp), 0); // -8 clamped
        assert_eq!(pow.apply(-2, i32::MAX as i64 - 1, clamp), 255); // i32::MAX as i64 clamped
        assert_eq!(pow.apply(-2, i32::MAX as i64, clamp), 0); // i32::MIN as i64 clamped

        assert_eq!(pow.apply(-1, i32::MIN as i64, clamp), 1);
        assert_eq!(pow.apply(-1, -3, clamp), 0); // -1 clamped
        assert_eq!(pow.apply(-1, -2, clamp), 1);
        assert_eq!(pow.apply(-1, -1, clamp), 0); // -1 clamped
        assert_eq!(pow.apply(-1, 0, clamp), 1);
        assert_eq!(pow.apply(-1, 1, clamp), 0); // -1 clamped
        assert_eq!(pow.apply(-1, 2, clamp), 1);
        assert_eq!(pow.apply(-1, 3, clamp), 0); // -1 clamped
        assert_eq!(pow.apply(-1, i32::MAX as i64 - 1, clamp), 1);
        assert_eq!(pow.apply(-1, i32::MAX as i64, clamp), 0); // -1 clamped

        for v in [0, 1] {
            assert_eq!(pow.apply(v, i32::MIN as i64, clamp), v);
            assert_eq!(pow.apply(v, -2, clamp), v);
            assert_eq!(pow.apply(v, -1, clamp), v);
            assert_eq!(pow.apply(v, 0, clamp), v);
            assert_eq!(pow.apply(v, 1, clamp), v);
            assert_eq!(pow.apply(v, 2, clamp), v);
            assert_eq!(pow.apply(v, i32::MAX as i64, clamp), v);
        }

        assert_eq!(pow.apply(-(1 << 16), 3, clamp), 0); // i32::MIN as i64 clamped
        assert_eq!(pow.apply(1 << 16, 3, clamp), 255); // i32::MAX as i64 clamped
    }
}
