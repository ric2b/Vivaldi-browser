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

use crate::*;

fn expand_coeffs(y: f32, v: f32) -> [f32; 3] {
    [y, 1.0 - y - v, v]
}

impl ColorPrimaries {
    pub fn y_coeffs(&self) -> [f32; 3] {
        // These values come from computations in Section 8 of
        // https://www.itu.int/rec/T-REC-H.273-201612-S
        match self {
            ColorPrimaries::Unknown | ColorPrimaries::Srgb | ColorPrimaries::Unspecified => {
                expand_coeffs(0.2126, 0.0722)
            }
            ColorPrimaries::Bt470m => expand_coeffs(0.299, 0.1146),
            ColorPrimaries::Bt470bg => expand_coeffs(0.222, 0.0713),
            ColorPrimaries::Bt601 | ColorPrimaries::Smpte240 => expand_coeffs(0.212, 0.087),
            ColorPrimaries::GenericFilm => expand_coeffs(0.2536, 0.06808),
            ColorPrimaries::Bt2020 => expand_coeffs(0.2627, 0.0593),
            ColorPrimaries::Xyz => expand_coeffs(0.0, 0.0),
            ColorPrimaries::Smpte431 => expand_coeffs(0.2095, 0.0689),
            ColorPrimaries::Smpte432 => expand_coeffs(0.229, 0.0793),
            ColorPrimaries::Ebu3213 => expand_coeffs(0.2318, 0.096),
        }
    }
}

fn calculate_yuv_coefficients_from_cicp(
    color_primaries: ColorPrimaries,
    matrix_coefficients: MatrixCoefficients,
) -> Option<[f32; 3]> {
    match matrix_coefficients {
        MatrixCoefficients::ChromaDerivedNcl => Some(color_primaries.y_coeffs()),
        MatrixCoefficients::Bt709 => Some(expand_coeffs(0.2126, 0.0722)),
        MatrixCoefficients::Fcc => Some(expand_coeffs(0.30, 0.11)),
        MatrixCoefficients::Bt470bg | MatrixCoefficients::Bt601 => {
            Some(expand_coeffs(0.299, 0.114))
        }
        MatrixCoefficients::Smpte240 => Some(expand_coeffs(0.212, 0.087)),
        MatrixCoefficients::Bt2020Ncl => Some(expand_coeffs(0.2627, 0.0593)),
        _ => None,
    }
}

pub fn calculate_yuv_coefficients(
    color_primaries: ColorPrimaries,
    matrix_coefficients: MatrixCoefficients,
) -> [f32; 3] {
    // Return known coefficients or fall back to BT.601.
    calculate_yuv_coefficients_from_cicp(color_primaries, matrix_coefficients).unwrap_or(
        calculate_yuv_coefficients_from_cicp(color_primaries, MatrixCoefficients::Bt601).unwrap(),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::internal_utils::assert_eq_f32_array;

    #[test]
    fn yuv_coefficients() {
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Bt601),
            &[0.299f32, 0.587f32, 0.114f32], // Kr,Kg,Kb as https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
        );
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Unspecified),
            &[0.299f32, 0.587f32, 0.114f32], // Falls back to Bt601.
        );
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Smpte240),
            &[0.212f32, 1f32 - 0.212 - 0.087, 0.087f32], // Kr,Kg,Kb as https://en.wikipedia.org/wiki/YCbCr#SMPTE_240M_conversion
        );
    }
}
