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

use clap::value_parser;
use clap::Parser;

use crabby_avif::decoder::track::RepetitionCount;
use crabby_avif::decoder::*;
#[cfg(feature = "encoder")]
use crabby_avif::encoder::*;
use crabby_avif::utils::clap::CleanAperture;
use crabby_avif::utils::clap::CropRect;
use crabby_avif::utils::IFraction;
use crabby_avif::utils::UFraction;
use crabby_avif::*;

#[cfg(all(feature = "encoder", feature = "jpeg"))]
use crabby_avif::utils::reader::jpeg::JpegReader;
#[cfg(all(feature = "encoder", feature = "png"))]
use crabby_avif::utils::reader::png::PngReader;
#[cfg(feature = "encoder")]
use crabby_avif::utils::reader::y4m::Y4MReader;
#[cfg(feature = "encoder")]
use crabby_avif::utils::reader::Config;
#[cfg(feature = "encoder")]
use crabby_avif::utils::reader::Reader;

#[cfg(feature = "jpeg")]
use crabby_avif::utils::writer::jpeg::JpegWriter;
#[cfg(feature = "png")]
use crabby_avif::utils::writer::png::PngWriter;
use crabby_avif::utils::writer::y4m::Y4MWriter;
use crabby_avif::utils::writer::Writer;

use std::fs::File;
#[cfg(feature = "encoder")]
use std::io;
#[cfg(feature = "encoder")]
use std::io::Read;
#[cfg(feature = "encoder")]
use std::io::Write;
use std::num::NonZero;

fn depth_parser(s: &str) -> Result<u8, String> {
    match s.parse::<u8>() {
        Ok(8) => Ok(8),
        Ok(10) => Ok(10),
        Ok(12) => Ok(12),
        Ok(16) => Ok(16),
        _ => Err("Value must be one of 8, 10, 12 or 16".into()),
    }
}

macro_rules! split_and_check_count {
    ($parameter: literal, $input:ident, $delimiter:literal, $count:literal, $type:ty) => {{
        let values: Result<Vec<_>, _> = $input
            .split($delimiter)
            .map(|x| x.parse::<$type>())
            .collect();
        if values.is_err() {
            return Err(format!("Invalid {} string", $parameter));
        }
        let values = values.unwrap();
        if values.len() != $count {
            return Err(format!(
                "Invalid {} string. Expecting exactly {} values separated with a \"{}\"",
                $parameter, $count, $delimiter
            ));
        }
        values
    }};
}

fn clap_parser(s: &str) -> Result<CleanAperture, String> {
    let values = split_and_check_count!("clap", s, ",", 8, i32);
    let values: Vec<_> = values.into_iter().map(|x| x as u32).collect();
    Ok(CleanAperture {
        width: UFraction(values[0], values[1]),
        height: UFraction(values[2], values[3]),
        horiz_off: UFraction(values[4], values[5]),
        vert_off: UFraction(values[6], values[7]),
    })
}

fn crop_parser(s: &str) -> Result<CropRect, String> {
    let values = split_and_check_count!("crop", s, ",", 4, u32);
    Ok(CropRect {
        x: values[0],
        y: values[1],
        width: values[2],
        height: values[3],
    })
}

fn clli_parser(s: &str) -> Result<ContentLightLevelInformation, String> {
    let values = split_and_check_count!("clli", s, ",", 2, u16);
    Ok(ContentLightLevelInformation {
        max_cll: values[0],
        max_pall: values[1],
    })
}

fn pasp_parser(s: &str) -> Result<PixelAspectRatio, String> {
    let values = split_and_check_count!("pasp", s, ",", 2, u32);
    Ok(PixelAspectRatio {
        h_spacing: values[0],
        v_spacing: values[1],
    })
}

fn cicp_parser(s: &str) -> Result<Nclx, String> {
    let values = split_and_check_count!("cicp", s, "/", 3, u16);
    Ok(Nclx {
        color_primaries: values[0].into(),
        transfer_characteristics: values[1].into(),
        matrix_coefficients: values[2].into(),
        ..Default::default()
    })
}

fn scaling_mode_parser(s: &str) -> Result<IFraction, String> {
    let values = split_and_check_count!("scaling_mode", s, "/", 2, i32);
    Ok(IFraction(values[0], values[1]))
}

fn yuv_format_parser(s: &str) -> Result<PixelFormat, String> {
    match s {
        "420" => Ok(PixelFormat::Yuv420),
        "422" => Ok(PixelFormat::Yuv422),
        "444" => Ok(PixelFormat::Yuv444),
        "400" => Ok(PixelFormat::Yuv400),
        _ => Err(format!("Invalid yuv format: {s}")),
    }
}

#[derive(Parser)]
struct CommandLineArgs {
    /// AVIF Decode only: Disable strict decoding, which disables strict validation checks and
    /// errors
    #[arg(long, default_value = "false")]
    no_strict: bool,

    /// AVIF Decode only: Decode all frames and display all image information instead of saving to
    /// disk
    #[arg(short = 'i', long, default_value = "false")]
    info: bool,

    /// Number of threads to use for AVIF encoding/decoding
    #[arg(long)]
    jobs: Option<u32>,

    /// AVIF Decode only:  When decoding an image sequence or progressive image, specify which
    /// frame index to decode (Default: 0)
    #[arg(long, short = 'I')]
    index: Option<u32>,

    /// Output depth, either 8 or 16. (AVIF/PNG only; For y4m/yuv, source depth is retained; JPEG
    /// is always 8bit)
    #[arg(long, short = 'd', value_parser = depth_parser)]
    depth: Option<u8>,

    /// Output quality in 0..100. (JPEG/AVIF only, default: 90).
    #[arg(long, short = 'q', value_parser = value_parser!(u8).range(0..=100))]
    quality: Option<u8>,

    /// AVIF Encode only: Speed used for encoding.
    #[arg(long, short = 's', value_parser = value_parser!(u32).range(0..=10))]
    speed: Option<u32>,

    /// When decoding AVIF: Enable progressive AVIF processing. If a progressive image is
    /// encountered and --progressive is passed, --index will be used to choose which layer to
    /// decode (in progressive order).
    /// When encoding AVIF: Auto set parameters to encode a simple layered image supporting
    /// progressive rendering from a single input frame.
    #[arg(long, default_value = "false")]
    progressive: bool,

    /// Maximum image size (in total pixels) that should be tolerated (0 means unlimited)
    #[arg(long)]
    size_limit: Option<u32>,

    /// Maximum image dimension (width or height) that should be tolerated (0 means unlimited)
    #[arg(long)]
    dimension_limit: Option<u32>,

    /// AVIF Decode only: If the input file contains embedded Exif metadata, ignore it (no-op if absent)
    #[arg(long, default_value = "false")]
    ignore_exif: bool,

    /// AVIF Decode only: If the input file contains embedded XMP metadata, ignore it (no-op if absent)
    #[arg(long, default_value = "false")]
    ignore_xmp: bool,

    /// AVIF Encode only: Add irot property (rotation), in 0..3. Makes (90 * ANGLE) degree rotation
    /// anti-clockwise
    #[arg(long = "irot", value_parser = value_parser!(u8).range(0..=3))]
    irot_angle: Option<u8>,

    /// AVIF Encode only: Add imir property (mirroring). 0=top-to-bottom, 1=left-to-right
    #[arg(long = "imir", value_parser = value_parser!(u8).range(0..=1))]
    imir_axis: Option<u8>,

    /// AVIF Encode only: Add clap property (clean aperture). Width, Height, HOffset, VOffset (in
    /// numerator/denominator pairs)
    #[arg(long, value_parser = clap_parser)]
    clap: Option<CleanAperture>,

    /// AVIF Encode only: Add clap property (clean aperture) calculated from a crop rectangle. X,
    /// Y, Width, Height
    #[arg(long, value_parser = crop_parser)]
    crop: Option<CropRect>,

    /// AVIF Encode only: Add pasp property (aspect ratio). Horizontal spacing, Vertical spacing
    #[arg(long, value_parser = pasp_parser)]
    pasp: Option<PixelAspectRatio>,

    /// AVIF Encode only: Add clli property (content light level information). MaxCLL, MaxPALL
    #[arg(long, value_parser = clli_parser)]
    clli: Option<ContentLightLevelInformation>,

    /// AVIF Encode only: Set CICP values (nclx colr box) (P/T/M 3 raw numbers, use -r to set range
    /// flag)
    #[arg(long, value_parser = cicp_parser)]
    cicp: Option<Nclx>,

    /// AVIF Encode only: Provide an ICC profile payload to be associated with the primary item
    #[arg(long)]
    icc: Option<String>,

    /// AVIF Encode only: Provide an XMP metadata payload to be associated with the primary item
    #[arg(long)]
    xmp: Option<String>,

    /// AVIF Encode only: Provide an Exif metadata payload to be associated with the primary item
    #[arg(long)]
    exif: Option<String>,

    /// AVIF Encode only: Set frame scaling mode as given fraction
    #[arg(long, value_parser = scaling_mode_parser)]
    scaling_mode: Option<IFraction>,

    /// AVIF Encode only: log2 of number of tile rows
    #[arg(long, value_parser = value_parser!(i32).range(0..=6))]
    tilerowslog2: Option<i32>,

    /// AVIF Encode only: log2 of number of tile columns
    #[arg(long, value_parser = value_parser!(i32).range(0..=6))]
    tilecolslog2: Option<i32>,

    /// AVIF Encode only: Set tile rows and columns automatically. If specified, tilesrowslog2 and
    /// tilecolslog2 will be ignored
    #[arg(long, default_value = "false")]
    autotiling: bool,

    /// AVIF Encode only: Output format, one of 444, 422, 420 or 400. Ignored for y4m. For all
    /// other cases, auto defaults to 444.
    #[arg(long = "yuv", value_parser = yuv_format_parser)]
    yuv_format: Option<PixelFormat>,

    /// Input AVIF file
    #[arg(allow_hyphen_values = false)]
    input_file: String,

    /// Output file
    #[arg(allow_hyphen_values = false)]
    output_file: Option<String>,
}

fn print_data_as_columns(rows: &[(usize, &str, String)]) {
    let rows: Vec<_> = rows
        .iter()
        .filter(|x| !x.1.is_empty())
        .map(|x| (format!("{} * {}", " ".repeat(x.0 * 4), x.1), x.2.as_str()))
        .collect();

    // Calculate the maximum width for the first column.
    let mut max_col1_width = 0;
    for (col1, _) in &rows {
        max_col1_width = max_col1_width.max(col1.len());
    }

    for (col1, col2) in &rows {
        println!("{col1:<max_col1_width$} : {col2}");
    }
}

fn print_vec(data: &[u8]) -> String {
    if data.is_empty() {
        "Absent".to_string()
    } else {
        format!("Present ({} bytes)", data.len())
    }
}

fn print_image_info(decoder: &Decoder) {
    let image = decoder.image().unwrap();
    let mut image_data = vec![
        (
            0,
            "File Format",
            format!("{:#?}", decoder.compression_format()),
        ),
        (0, "Resolution", format!("{}x{}", image.width, image.height)),
        (0, "Bit Depth", format!("{}", image.depth)),
        (0, "Format", format!("{:#?}", image.yuv_format)),
        if image.yuv_format == PixelFormat::Yuv420 {
            (
                0,
                "Chroma Sample Position",
                format!("{:#?}", image.chroma_sample_position),
            )
        } else {
            (0, "", "".into())
        },
        (
            0,
            "Alpha",
            match (image.alpha_present, image.alpha_premultiplied) {
                (true, true) => "Premultiplied".to_string(),
                (true, false) => "Not premultiplied".to_string(),
                (false, _) => "Absent".to_string(),
            },
        ),
        (0, "Range", format!("{:#?}", image.yuv_range)),
        (
            0,
            "Color Primaries",
            format!("{:#?}", image.color_primaries),
        ),
        (
            0,
            "Transfer Characteristics",
            format!("{:#?}", image.transfer_characteristics),
        ),
        (
            0,
            "Matrix Coefficients",
            format!("{:#?}", image.matrix_coefficients),
        ),
        (0, "ICC Profile", print_vec(&image.icc)),
        (0, "XMP Metadata", print_vec(&image.xmp)),
        (0, "Exif Metadata", print_vec(&image.exif)),
    ];
    if image.pasp.is_none()
        && image.clap.is_none()
        && image.irot_angle.is_none()
        && image.imir_axis.is_none()
    {
        image_data.push((0, "Transformations", "None".to_string()));
    } else {
        image_data.push((0, "Transformations", "".to_string()));
        if let Some(pasp) = image.pasp {
            image_data.push((
                1,
                "pasp (Aspect Ratio)",
                format!("{}/{}", pasp.h_spacing, pasp.v_spacing),
            ));
        }
        if let Some(clap) = image.clap {
            image_data.push((1, "clap (Clean Aperture)", "".to_string()));
            image_data.push((2, "W", format!("{}/{}", clap.width.0, clap.width.1)));
            image_data.push((2, "H", format!("{}/{}", clap.height.0, clap.height.1)));
            image_data.push((
                2,
                "hOff",
                format!("{}/{}", clap.horiz_off.0, clap.horiz_off.1),
            ));
            image_data.push((
                2,
                "vOff",
                format!("{}/{}", clap.vert_off.0, clap.vert_off.1),
            ));
            match CropRect::create_from(&clap, image.width, image.height, image.yuv_format) {
                Ok(rect) => image_data.extend_from_slice(&[
                    (2, "Valid, derived crop rect", "".to_string()),
                    (3, "X", format!("{}", rect.x)),
                    (3, "Y", format!("{}", rect.y)),
                    (3, "W", format!("{}", rect.width)),
                    (3, "H", format!("{}", rect.height)),
                ]),
                Err(_) => image_data.push((2, "Invalid", "".to_string())),
            }
        }
        if let Some(angle) = image.irot_angle {
            image_data.push((1, "irot (Rotation)", format!("{angle}")));
        }
        if let Some(axis) = image.imir_axis {
            image_data.push((1, "imir (Mirror)", format!("{axis}")));
        }
    }
    image_data.push((0, "Progressive", format!("{:#?}", image.progressive_state)));
    if let Some(clli) = image.clli {
        image_data.push((0, "CLLI", format!("{}, {}", clli.max_cll, clli.max_pall)));
    }
    if decoder.gainmap_present() {
        let gainmap = decoder.gainmap();
        let gainmap_image = &gainmap.image;
        image_data.extend_from_slice(&[
            (
                0,
                "Gainmap",
                format!(
                "{}x{} pixels, {} bit, {:#?}, {:#?} Range, Matrix Coeffs. {:#?}, Base Image is {}",
                gainmap_image.width,
                gainmap_image.height,
                gainmap_image.depth,
                gainmap_image.yuv_format,
                gainmap_image.yuv_range,
                gainmap_image.matrix_coefficients,
                if gainmap.metadata.base_hdr_headroom.0 == 0 { "SDR" } else { "HDR" },
            ),
            ),
            (0, "Alternate image", "".to_string()),
            (
                1,
                "Color Primaries",
                format!("{:#?}", gainmap.alt_color_primaries),
            ),
            (
                1,
                "Transfer Characteristics",
                format!("{:#?}", gainmap.alt_transfer_characteristics),
            ),
            (
                1,
                "Matrix Coefficients",
                format!("{:#?}", gainmap.alt_matrix_coefficients),
            ),
            (1, "ICC Profile", print_vec(&gainmap.alt_icc)),
            (1, "Bit Depth", format!("{}", gainmap.alt_plane_depth)),
            (1, "Planes", format!("{}", gainmap.alt_plane_count)),
            if let Some(clli) = gainmap_image.clli {
                (1, "CLLI", format!("{}, {}", clli.max_cll, clli.max_pall))
            } else {
                (1, "", "".into())
            },
        ])
    } else {
        // TODO: b/394162563 - check if we need to report the present but ignored case.
        image_data.push((0, "Gainmap", "Absent".to_string()));
    }
    if image.image_sequence_track_present {
        image_data.push((
            0,
            "Repeat Count",
            match decoder.repetition_count() {
                RepetitionCount::Finite(x) => format!("{x}"),
                RepetitionCount::Infinite => "Infinite".to_string(),
                RepetitionCount::Unknown => "Unknown".to_string(),
            },
        ));
    }
    print_data_as_columns(&image_data);
}

fn max_threads(jobs: &Option<u32>) -> u32 {
    match jobs {
        Some(x) => {
            if *x == 0 {
                match std::thread::available_parallelism() {
                    Ok(value) => value.get() as u32,
                    Err(_) => 1,
                }
            } else {
                *x
            }
        }
        None => 1,
    }
}

fn create_decoder_and_parse(args: &CommandLineArgs) -> AvifResult<Decoder> {
    let mut settings = decoder::Settings {
        strictness: if args.no_strict { Strictness::None } else { Strictness::All },
        image_content_to_decode: ImageContentType::All,
        max_threads: max_threads(&args.jobs),
        allow_progressive: args.progressive,
        ignore_exif: args.ignore_exif,
        ignore_xmp: args.ignore_xmp,
        ..Default::default()
    };
    // These values cannot be initialized in the list above since we need the default values to be
    // retain unless they are explicitly specified.
    if let Some(size_limit) = args.size_limit {
        settings.image_size_limit = NonZero::new(size_limit);
    }
    if let Some(dimension_limit) = args.dimension_limit {
        settings.image_dimension_limit = NonZero::new(dimension_limit);
    }
    let mut decoder = Decoder::default();
    decoder.settings = settings;
    decoder
        .set_io_file(&args.input_file)
        .or(Err(AvifError::UnknownError(
            "Cannot open input file".into(),
        )))?;
    decoder.parse()?;
    Ok(decoder)
}

fn info(args: &CommandLineArgs) -> AvifResult<()> {
    let mut decoder = create_decoder_and_parse(args)?;
    println!("Image decoded: {}", args.input_file);
    print_image_info(&decoder);
    println!(
        " * {} timescales per second, {} seconds ({} timescales), {} frame{}",
        decoder.timescale(),
        decoder.duration(),
        decoder.duration_in_timescales(),
        decoder.image_count(),
        if decoder.image_count() == 1 { "" } else { "s" },
    );
    if decoder.image_count() > 1 {
        let image = decoder.image().unwrap();
        println!(
            " * {} Frames: ({} expected frames)",
            if image.image_sequence_track_present {
                "Image Sequence"
            } else {
                "Progressive Image"
            },
            decoder.image_count()
        );
    } else {
        println!(" * Frame:");
    }

    let mut index = 0;
    loop {
        match decoder.next_image() {
            Ok(_) => {
                println!("     * Decoded frame [{}] [pts {} ({} timescales)] [duration {} ({} timescales)] [{}x{}]",
                    index,
                    decoder.image_timing().pts,
                    decoder.image_timing().pts_in_timescales,
                    decoder.image_timing().duration,
                    decoder.image_timing().duration_in_timescales,
                    decoder.image().unwrap().width,
                    decoder.image().unwrap().height);
                index += 1;
            }
            Err(AvifError::NoImagesRemaining) => {
                return Ok(());
            }
            Err(err) => {
                return Err(err);
            }
        }
    }
}

fn get_extension(filename: &str) -> String {
    std::path::Path::new(filename)
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("")
        .to_lowercase()
}

fn decode(args: &CommandLineArgs) -> AvifResult<()> {
    let max_threads = max_threads(&args.jobs);
    println!(
        "Decoding with {max_threads} worker thread{}, please wait...",
        if max_threads == 1 { "" } else { "s" }
    );
    let mut decoder = create_decoder_and_parse(args)?;
    decoder.nth_image(args.index.unwrap_or(0))?;
    println!("Image Decoded: {}", args.input_file);
    println!("Image details:");
    print_image_info(&decoder);

    let output_filename = &args.output_file.as_ref().unwrap().as_str();
    let image = decoder.image().unwrap();
    let extension = get_extension(output_filename);
    let mut writer: Box<dyn Writer> = match extension.as_str() {
        "y4m" | "yuv" => {
            if !image.icc.is_empty() || !image.exif.is_empty() || !image.xmp.is_empty() {
                println!("Warning: metadata dropped when saving to {extension}");
            }
            Box::new(Y4MWriter::create(extension == "yuv"))
        }
        #[cfg(feature = "png")]
        "png" => Box::new(PngWriter { depth: args.depth }),
        #[cfg(feature = "jpeg")]
        "jpg" | "jpeg" => Box::new(JpegWriter {
            quality: args.quality,
        }),
        _ => {
            return Err(AvifError::UnknownError(format!(
                "Unknown output file extension ({extension})"
            )));
        }
    };
    let mut output_file = File::create(output_filename).or(Err(AvifError::UnknownError(
        "Could not open output file".into(),
    )))?;
    writer.write_frame(&mut output_file, image)?;
    println!(
        "Wrote image at index {} to output {}",
        args.index.unwrap_or(0),
        output_filename,
    );
    Ok(())
}

#[cfg(feature = "encoder")]
fn read_file(filepath: &String) -> io::Result<Vec<u8>> {
    let mut file = File::open(filepath)?;
    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer)?;
    Ok(buffer)
}

#[cfg(feature = "encoder")]
fn encode(args: &CommandLineArgs) -> AvifResult<()> {
    const DEFAULT_ENCODE_QUALITY: u8 = 90;
    let extension = get_extension(&args.input_file);
    let mut reader: Box<dyn Reader> = match extension.as_str() {
        "y4m" => Box::new(Y4MReader::create(&args.input_file)?),
        #[cfg(feature = "jpeg")]
        "jpg" | "jpeg" => Box::new(JpegReader::create(&args.input_file)?),
        #[cfg(feature = "png")]
        "png" => Box::new(PngReader::create(&args.input_file)?),
        _ => {
            return Err(AvifError::UnknownError(format!(
                "Unknown input file extension ({extension})"
            )));
        }
    };
    let reader_config = Config {
        yuv_format: args.yuv_format,
        depth: args.depth,
        ..Default::default()
    };
    let mut image = reader.read_frame(&reader_config)?;
    image.irot_angle = args.irot_angle;
    image.imir_axis = args.imir_axis;
    if let Some(clap) = args.clap {
        image.clap = Some(clap);
    }
    if let Some(crop) = args.crop {
        image.clap = Some(CleanAperture::create_from(
            &crop,
            image.width,
            image.height,
            image.yuv_format,
        )?);
    }
    image.pasp = args.pasp;
    image.clli = args.clli;
    if let Some(nclx) = &args.cicp {
        image.color_primaries = nclx.color_primaries;
        image.transfer_characteristics = nclx.transfer_characteristics;
        image.matrix_coefficients = nclx.matrix_coefficients;
    }
    if let Some(icc) = &args.icc {
        image.icc = read_file(icc).expect("failed to read icc file");
    }
    if let Some(exif) = &args.exif {
        image.xmp = read_file(exif).expect("failed to read exif file");
    }
    if let Some(xmp) = &args.xmp {
        image.xmp = read_file(xmp).expect("failed to read xmp file");
    }
    let mut settings = encoder::Settings {
        extra_layer_count: if args.progressive { 1 } else { 0 },
        speed: args.speed,
        mutable: MutableSettings {
            quality: args.quality.unwrap_or(DEFAULT_ENCODE_QUALITY) as i32,
            tiling_mode: if args.autotiling {
                TilingMode::Auto
            } else {
                TilingMode::Manual(
                    args.tilerowslog2.unwrap_or(0),
                    args.tilecolslog2.unwrap_or(0),
                )
            },
            ..Default::default()
        },
        ..Default::default()
    };
    if let Some(scaling_mode) = args.scaling_mode {
        settings.mutable.scaling_mode = ScalingMode {
            horizontal: scaling_mode,
            vertical: scaling_mode,
        };
    }
    let mut encoder = Encoder::create_with_settings(&settings)?;
    if reader.has_more_frames() {
        if args.progressive {
            println!("Automatic progressive encoding can only have one input image.");
            return Err(AvifError::InvalidArgument);
        }
        loop {
            // TODO: b/403090413 - Use a proper timestamp here.
            encoder.add_image_for_sequence(&image, 1000)?;
            if !reader.has_more_frames() {
                break;
            }
            image = reader.read_frame(&reader_config)?;
        }
    } else if args.progressive {
        // Encode the base layer with very low quality.
        settings.mutable.quality = 2;
        encoder.update_settings(&settings.mutable)?;
        encoder.add_image(&image)?;
        // Encode the second layer with the requested quality.
        settings.mutable.quality = args.quality.unwrap_or(DEFAULT_ENCODE_QUALITY) as i32;
        encoder.update_settings(&settings.mutable)?;
        encoder.add_image(&image)?;
    } else {
        encoder.add_image(&image)?;
    }

    let encoded_data = encoder.finish()?;
    let output_file = args.output_file.as_ref().unwrap();
    let mut file = File::create(output_file).expect("file creation failed");
    file.write_all(&encoded_data).expect("file writing failed");
    println!("Write output AVIF: {output_file}");
    Ok(())
}

#[cfg(not(feature = "encoder"))]
fn encode(_args: &CommandLineArgs) -> AvifResult<()> {
    Err(AvifError::InvalidArgument)
}

fn can_decode(filename: &str) -> bool {
    match get_extension(filename).as_str() {
        "avif" => true,
        #[cfg(feature = "heic")]
        "heic" | "heif" => true,
        _ => false,
    }
}

fn can_encode(filename: &str) -> bool {
    get_extension(filename) == "avif"
}

fn validate_args(args: &CommandLineArgs) -> AvifResult<()> {
    if can_decode(&args.input_file) {
        if args.info {
            if args.output_file.is_some()
                || args.quality.is_some()
                || args.depth.is_some()
                || args.index.is_some()
            {
                return Err(AvifError::UnknownError(
                    "--info contains unsupported extra arguments".into(),
                ));
            }
        } else {
            if args.output_file.is_none() {
                return Err(AvifError::UnknownError("output_file is required".into()));
            }
            let output_filename = &args.output_file.as_ref().unwrap().as_str();
            let extension = get_extension(output_filename);
            if args.quality.is_some() && extension != "jpg" && extension != "jpeg" {
                return Err(AvifError::UnknownError(
                    "quality is only supported for jpeg output".into(),
                ));
            }
            if args.depth.is_some() && extension != "png" {
                return Err(AvifError::UnknownError(
                    "depth is only supported for png output".into(),
                ));
            }
        }
    } else {
        // TODO: b/403090413 - validate encoding args.
    }
    Ok(())
}

fn main() {
    let args = CommandLineArgs::parse();
    if let Err(err) = validate_args(&args) {
        eprintln!("ERROR: {:#?}", err);
        std::process::exit(1);
    }
    let res = if can_decode(&args.input_file) {
        if args.info {
            info(&args)
        } else {
            decode(&args)
        }
    } else if let Some(output_file) = &args.output_file {
        if can_encode(output_file) {
            encode(&args)
        } else {
            eprintln!("Input/output file extensions not supported");
            std::process::exit(1);
        }
    } else {
        eprintln!(
            "Input file extension not supported: {}",
            get_extension(&args.input_file)
        );
        std::process::exit(1);
    };
    match res {
        Ok(_) => std::process::exit(0),
        Err(err) => {
            eprintln!("ERROR: {:#?}", err);
            std::process::exit(1);
        }
    }
}
