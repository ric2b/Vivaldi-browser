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

use std::env;
use std::process::Command;

use crabby_avif::decoder::*;

fn main() {
    // let data: [u8; 32] = [
    //     0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00,
    //     0x00, 0x61, 0x76, 0x69, 0x66, 0x6d, 0x69, 0x66, 0x31, 0x6d, 0x69, 0x61, 0x66, 0x4d, 0x41,
    //     0x31, 0x41,
    // ];
    // let data: [u8; 32] = [
    //     0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x67, 0x00, 0x00, 0x00,
    //     0x00, 0x61, 0x76, 0x69, 0x68, 0x6d, 0x69, 0x66, 0x31, 0x6d, 0x69, 0x61, 0x66, 0x4d, 0x41,
    //     0x31, 0x41,
    // ];
    // let val = Decoder::peek_compatible_file_type(&data);
    // println!("val: {val}");
    // return;

    let args: Vec<String> = env::args().collect();

    if args.len() < 3 {
        println!("Usage: {} <input_avif> <output> [--no-png]", args[0]);
        std::process::exit(1);
    }
    let image_count;
    {
        let settings = Settings {
            strictness: Strictness::None,
            enable_decoding_gainmap: true,
            enable_parsing_gainmap_metadata: true,
            allow_progressive: true,
            ..Settings::default()
        };
        let mut decoder: Decoder = Default::default();
        decoder.settings = settings;
        match decoder.set_io_file(&args[1]) {
            Ok(_) => {}
            Err(err) => {
                println!("failed to set file io: {:#?}", err);
                std::process::exit(1);
            }
        };
        let res = decoder.parse();
        if res.is_err() {
            println!("parse failed! {:#?}", res);
            std::process::exit(1);
        }
        let _image = decoder.image();

        println!("\n^^^ decoder public properties ^^^");
        println!("image_count: {}", decoder.image_count());
        println!("timescale: {}", decoder.timescale());
        println!(
            "duration_in_timescales: {}",
            decoder.duration_in_timescales()
        );
        println!("duration: {}", decoder.duration());
        println!("repetition_count: {:#?}", decoder.repetition_count());
        println!("$$$ end decoder public properties $$$\n");

        image_count = decoder.image_count();
        //image_count = 1;
        let mut writer: crabby_avif::utils::y4m::Y4MWriter = Default::default();
        //let mut writer: crabby_avif::utils::raw::RawWriter = Default::default();
        writer.filename = Some(args[2].clone());
        //writer.rgb = true;

        for _i in 0..image_count {
            let res = decoder.nth_image(0);
            if res.is_err() {
                println!("next_image failed! {:#?}", res);
                std::process::exit(1);
            }
            let image = decoder.image().expect("image was none");
            let ret = writer.write_frame(image);
            if !ret {
                println!("error writing y4m file");
                std::process::exit(1);
            }
            println!("timing: {:#?}", decoder.image_timing());
        }
        println!("wrote {} frames into {}", image_count, args[2]);
    }
    if args.len() == 3 {
        if image_count <= 1 {
            let ffmpeg_infile = args[2].to_string();
            let ffmpeg_outfile = format!("{}.png", args[2]);
            let ffmpeg = Command::new("ffmpeg")
                .arg("-i")
                .arg(ffmpeg_infile)
                .arg("-frames:v")
                .arg("1")
                .arg("-y")
                .arg(ffmpeg_outfile)
                .output()
                .unwrap();
            if !ffmpeg.status.success() {
                println!("ffmpeg to convert to png failed");
                std::process::exit(1);
            }
            println!("wrote {}.png", args[2]);
        } else {
            let ffmpeg_infile = args[2].to_string();
            let ffmpeg_outfile = format!("{}.gif", args[2]);
            let ffmpeg = Command::new("ffmpeg")
                .arg("-i")
                .arg(ffmpeg_infile)
                .arg("-y")
                .arg(ffmpeg_outfile)
                .output()
                .unwrap();
            if !ffmpeg.status.success() {
                println!("ffmpeg to convert to gif failed");
                std::process::exit(1);
            }
            println!("wrote {}.gif", args[2]);
        }
    }
    std::process::exit(0);
}
