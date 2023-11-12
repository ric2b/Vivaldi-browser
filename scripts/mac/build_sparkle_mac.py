# Copyright 2020-2022 Vivaldi Technologies. All rights reserved.

import os
import subprocess
import shutil
import argparse

def clean(output_folder):
    if (os.path.exists(output_folder)):
        shutil.rmtree(output_folder)
    os.mkdir(output_folder)

def build(project_folder, output_folder, signed):
    project_name = "Sparkle.xcodeproj"
    build_command = "xcodebuild -project " + project_folder + "/" + project_name + \
        " -scheme Distribution -configuration Release -derivedDataPath " + output_folder + \
        " -destination platform=macOS" + \
        " build CODE_SIGN_IDENTITY="" CODE_SIGNING_ALLOWED=NO"
    if signed:
        build_command += " GCC_PREPROCESSOR_DEFINITIONS='${inherited} VIVALDI_SIGNED_BUILD=1'"
    build_command += " > /dev/null"

    subprocess.call(build_command,cwd=project_folder,shell=True)

def main():
    parser = argparse.ArgumentParser(
        description='Build the sparkle autoupdate framework.')
    parser.add_argument(
        '--project-folder',
        required=True,
        help='Path to the input Sparkle code, where Sparkle.xcodeproj is.')
    parser.add_argument(
        '--output-folder',
        required=True,
        help='Path to the output directory for the built sparkle library.')
    parser.add_argument(
        '--vivaldi-release-kind',
        dest='vivaldi_release_kind',
        required=True,
        help='The type of Vivaldi build')

    args = parser.parse_args()

    output_folder = args.output_folder
    project_folder = args.project_folder

    signed = False
    if args.vivaldi_release_kind != "vivaldi_sopranos":
        signed = True

    clean(output_folder)
    build(project_folder, output_folder, signed)

if __name__ == '__main__':
    main()