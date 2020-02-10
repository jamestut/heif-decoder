# HEIF Decoder

This program will read grid images contained inside the supplied HEIC file using the `heif` library by nokiatech. The images will be then decoded by `ffmpeg` and then stitched together before being encoded again by `ffmpeg` to the final output image format (PNG by default, with 8 bits per pixel).

This program will only compile and run on Linux systems (including WSL), because of the "convenience" syscalls used that are only available on Linux kernel.

## Usage

To use this program, ensure that the dependencies are met, and run:

`heifread "HEIF image file.heic"`

This program will produce several PNG files in the cwd with the following name format: 
`grid-(id).png`
where `(id)` is the internal ID of each individual image stored in the HEIC file.

## Dependencies

1. nokiatech's `heif` module
   (only tested against commit tag `v3.5.0`) for reading the HEIF file and encoded samples.
2. `ffmpeg` executable in `$PATH`
   Required for decoding the HEIF samples (most likely in `hvc1` format) and re-encoding stitched image to a proper image format, such as `PNG`.

## Compilation Guide

The linked `heif` submodule has to be built first. To build this module:

1. `cd` to the base directory of the submodule.
2. `cd` to the `build` folder.
3. run `cmake ../srcs`
4. run `cmake --build . -j (numthrd) `, where `(numthrd)` is the number of threads to spawn by cmake to speed up build process.

After the `heif` module has been built successfully, `cd` to the base directory of this repository, and run `make` to build this program.