#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from urllib.request import urlretrieve
import multiprocessing

import logging
import sys

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger()

if not sys.stdout.isatty():
    logger.handlers = []

URL_TEMPLATE = (
    "https://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/%s"
)
SOURCE_FILES = (
    "noto-cjk-20210501.tar.bz2",
    "crosextrafonts-20130214.tar.gz",
    "notofonts-20250214.tar.xz",
    "robotofonts-2.132.tar.bz2",
    "lohitfonts-cros-2.5.5.tar.bz2",
    "ko-nanumfonts-3.20.tar.bz2",
)
SOURCES = [URL_TEMPLATE % source for source in SOURCE_FILES]
SOURCES += ["https://github.com/googlefonts/roboto-flex/releases/download/3.100/roboto-flex-fonts.zip"]

CONTAINER_DOWNLOAD_DIR = "/testfonts"

# If you want to update these, copy the contents of the .stamp file from the target directory here.
EXPECTED_HASHES = """
noto-cjk-20210501.tar.bz2      5c70f246c392bf15aeaee22b2fba26bb0ac54e47c00a4b9c3b556bd06fe89281
crosextrafonts-20130214.tar.gz c48d1c2fd613c9c06c959c34da7b8388059e2408d2bb19845dc3ed35f76e4d09
notofonts-20250214.tar.xz      cb0f9edc030e07eca58b799c607d000dfde588911d8e228fc82d5f61b9e5b6b3
robotofonts-2.132.tar.bz2      087c6f0708fe71f71a056f70fdbd5a85f4d2ce916670a98bd4be10b168abe16a
lohitfonts-cros-2.5.5.tar.bz2  ce0ce2a5098c8ffc52327cc030576df7f5328ad9fd8a3289e2476990ad133ff1
ko-nanumfonts-3.20.tar.bz2     59f9b6d7fcf63ca2bea7156ad66c784a1f0601d47be1b11237e17733d7112832
roboto-flex-fonts.zip          02e0f5db84e69f254958434269d83aa6057b06f3c2a21042bb81b1afe1a0c8c6
"""

STAMP_FILE = ".stamp"


def download_file(url, tmp_dir, target_dir):
    filename = os.path.basename(url)
    filepath = os.path.join(tmp_dir, filename)
    logger.info(f"Downloading {url} to {filepath}")
    urlretrieve(url, filepath)
    return filepath


def compute_sha256(filepath):
    """Computes the SHA-256 hash of a file."""
    import hashlib

    hasher = hashlib.sha256()
    with open(filepath, "rb") as file:
        while True:
            chunk = file.read(4096)
            if not chunk:
                break
            hasher.update(chunk)
    return hasher.hexdigest()


def extract_archive(filepath, target_dir):
    """Extracts an archive to the target directory."""
    filename = os.path.basename(filepath)
    logger.info(f"Extracting {filepath} to {target_dir}")
    if filename.endswith((".tar.bz2", ".tar.gz", ".tar.xz")):
        subprocess.run(["tar", "xf", filepath, "-C", target_dir], check=True)
    elif filename.endswith(".zip"):
        shutil.unpack_archive(filepath, target_dir)
    else:
        raise ValueError(f"Unsupported archive type: {filename}")


def download_and_extract(url, tmp_dir, target_dir):
    archivepath = download_file(url, tmp_dir, target_dir)
    archivehash = compute_sha256(archivepath)
    extract_archive(archivepath, target_dir)
    return os.path.basename(archivepath), archivehash


def stamp_target_dir(target_dir, downloaded_files_hashes):
    if not downloaded_files_hashes or len(downloaded_files_hashes) == 0:
        logger.error("No files to stamp")
        return

    stamp_path = os.path.join(target_dir, STAMP_FILE)
    if os.path.exists(stamp_path):
        os.remove(stamp_path)

    max_filename_length = (
        max(len(filename) for filename, _ in downloaded_files_hashes)
        if downloaded_files_hashes
        else 0
    )

    hash_results = "\n".join(
        f"{filename:<{max_filename_length}} {hash}"
        for filename, hash in downloaded_files_hashes
    )
    with open(stamp_path, "w") as f:
        f.write(hash_results)

    if not stamp_hashes_match(stamp_path):
        raise ValueError("Downloaded files do not match recorded hashes!")


def stamp_hashes_match(stamp_path):
    if not os.path.exists(stamp_path):
        return False

    lines = []
    with open(stamp_path, "r") as f:
        lines = f.readlines()

    expected_hashes = {}
    for line in EXPECTED_HASHES.strip().split("\n"):
        filename, hash = line.split()
        expected_hashes[filename] = hash

    for line in lines:
        filename, hash = line.split()
        if filename not in expected_hashes:
            logger.error(f"Unexpected file in stamp: {filename}")
            return False
        if expected_hashes[filename] != hash:
            logger.error(
                f"Hash mismatch for {filename}: expected {expected_hashes[filename]}, got {hash}"
            )
            return False
    return True


def main():
    """Main function to parse arguments and download/extract fonts."""

    parser = argparse.ArgumentParser(description="Download and extract ChromeOS fonts.")

    parser.add_argument("--target-dir", help="Target directory for extracted fonts.")
    parser.add_argument(
        "--try-symlink",
        action="store_true",
        help="Try to symlink test files previously downloaded to the container.",
    )
    args = parser.parse_args()

    target_dir = args.target_dir

    if args.try_symlink:
        if os.path.exists(CONTAINER_DOWNLOAD_DIR) and stamp_hashes_match(
            os.path.join(CONTAINER_DOWNLOAD_DIR, STAMP_FILE)
        ):
            try:
                os.symlink(CONTAINER_DOWNLOAD_DIR, target_dir, target_is_directory=True)
            except FileExistsError:
                logger.debug(
                    f"Target directory {target_dir} already exists. Skipping symlink."
                )
            except OSError as e:
                logger.warning(f"Failed to create symlink: {e}")
            logger.info(
                f"Symlinked test fonts directory from {CONTAINER_DOWNLOAD_DIR} to {target_dir}."
            )
        else:
            logger.debug("Pre-downloaded test fonts not found.")

    if stamp_hashes_match(os.path.join(target_dir, STAMP_FILE)):
        logger.info("Fonts already downloaded and extracted.")
        return 0

    os.makedirs(target_dir, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp_dir:
        with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
            hashes = pool.starmap(
                download_and_extract,
                [(source, tmp_dir, target_dir) for source in SOURCES],
            )
            stamp_target_dir(target_dir, hashes)


if __name__ == "__main__":
    main()
