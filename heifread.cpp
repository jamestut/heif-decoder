#include <algorithm>  // std::min_element
#include <fstream>
#include <iostream>
#include <map>
#include <stdio.h>
#include <memory>
#include <unistd.h>
#include <cstring>
#include <thread>
#include "heifreader.h"
#include "heifwriter.h"
#include "procspawn.h"

using namespace std;
using namespace HEIF;

#define BYTE_PER_PIXEL 3

struct DecodeBufferData {
    uint8_t* finalBuff;
    uint8_t* tileBuff;
    uint32_t tileWidth;
    uint32_t tileStride;
    uint32_t tileHeight;
    uint32_t tileBuffSize;
    uint32_t finalWidth;
    uint32_t finalStride;
    uint32_t finalHeight;
    uint32_t finalBuffSize;
    uint32_t rows;
    uint32_t columns;
};

void readHEIF(Reader* reader);

void readGrid(Reader* reader, Grid& grid, char* outFileName);

void readDecodedOutput(ProcessSpawn* ps, DecodeBufferData* data);

// global buffers so that we don't keep reallocating
constexpr uint64_t sampleBuffSizeMax = 8192*8192;
auto sampleBuff = make_unique<uint8_t[]>(sampleBuffSizeMax);

void readHEIF(Reader* reader) {
    FileInformation info;
    reader->getFileInformation(info);

    Array<ImageId> gridIds;
    if(reader->getItemListByType("grid", gridIds) != ErrorCode::OK) {
        puts("Error retreiving grids.");
        return;
    }
    if(!gridIds.size) {
        puts("HEIC data does not contain a grid.");
        return;
    }

    printf("Number of grids in file: %d\n", gridIds.size);

    Grid gridData;
    char outFileName[32];
    for(auto gridId : gridIds) {
        printf("Reading grid ID %d\n", gridId);
        if(reader->getItem(gridId, gridData) != ErrorCode::OK) {
            printf("Error reading grid %d\n", gridId);
            continue;
        }
        sprintf(outFileName, "grid-%d.png", gridId);
        readGrid(reader, gridData, outFileName);
    }
}

void readGrid(Reader* reader, Grid& grid, char* outFileName) {
    int totalImg = grid.rows*grid.columns;
    if(!totalImg) {
        puts("Grid is empty!");
        return;
    }

    printf("Grid Info:\n - width = %d\n - height = %d\n - rows = %d\n - columns = %d\n - total = %d\n", 
        grid.outputWidth, grid.outputHeight, grid.rows, grid.columns, grid.rows*grid.columns);


    DecodeBufferData data;
    // size of the 1st tile?
    data.rows = grid.rows;
    data.columns = grid.columns;
    reader->getWidth(grid.imageIds[0], data.tileWidth);
    reader->getHeight(grid.imageIds[0], data.tileHeight);
    data.finalWidth = data.tileWidth * grid.columns;
    data.finalHeight = data.tileHeight * grid.rows;
    data.tileStride = data.tileWidth * BYTE_PER_PIXEL;
    data.tileBuffSize = data.tileStride * data.tileHeight;
    data.finalStride = data.finalWidth * BYTE_PER_PIXEL;
    data.finalBuffSize = data.finalStride * data.finalHeight;

    // create buffer for the tile
    auto tileBuff = make_unique<uint8_t[]>(data.tileBuffSize);
    data.tileBuff = tileBuff.get();

    // allocate buffer for stitched final image (in ARGB format)
    // we assume users have a lot of memory!
    auto finalBuff = make_unique<uint8_t[]>(data.finalBuffSize);
    data.finalBuff = finalBuff.get();

    // hope that ffmpeg is able to deduce the format!
    char* argsHeicDecode[] = {"ffmpeg","-i", "-", "-f", "rawvideo", "-pix_fmt", "rgb24", "-", nullptr};
    ProcessSpawn ps("ffmpeg", argsHeicDecode);
    if(!ps.isReady()) {
        puts("Error starting ffmpeg.");
        return;
    }

    // do both reading and writing, to avoid overflowing ffmpeg's input buffer
    thread decodeReaderThrd = thread(readDecodedOutput, &ps, &data);

    // feed ffmpeg with all samples!
    uint64_t sampleBuffSize;
    bool hasError = false;
    for(auto imId : grid.imageIds) {
        // get encoded tile data
        sampleBuffSize = sampleBuffSizeMax;
        if(reader->getItemDataWithDecoderParameters(imId, sampleBuff.get(), sampleBuffSize) != ErrorCode::OK) {
            printf("Failed retreiving encoded image data on image ID %d\n", imId);
            hasError = true;
            break;
        }
        // feed encoded sample to ffmpeg
        ps.writeData(sampleBuff.get(), sampleBuffSize);
        
        if(!ps.isReady()) {
            printf("ffmpeg stopped during the decoding of image %d\n", imId);
            hasError = true;
            break;
        }
    }
    // Ctrl+D
    ps.stopInput();
    
    decodeReaderThrd.join();
    if(hasError)
        return;
    

    // we're done w/ the HEIC encoder
    ps.stop(false);

    // now, ARGB to PNG!
    char imgSizeStr[16];
    sprintf(imgSizeStr, "%dx%d", grid.outputWidth, grid.outputHeight);
    char* argsRawEncode[] = {"ffmpeg","-y","-f", "rawvideo", "-pix_fmt", "rgb24", "-s", imgSizeStr, "-i", "-", outFileName, nullptr};
    ProcessSpawn ps2("ffmpeg", argsRawEncode);
    // write cropped image data
    for(int i=0; i<grid.outputHeight; ++i)
        ps2.writeData(finalBuff.get() + (i * data.finalStride), grid.outputWidth * BYTE_PER_PIXEL);
    ps2.stop(false);
}

void readDecodedOutput(ProcessSpawn* ps, DecodeBufferData* data) {
    for(int row=0; row<data->rows; ++row) {
        for(int col=0; col<data->columns; ++col) {
            // read the sample that ffmpeg gave back
            // we will still copy the result anyway should an error occurs
            size_t rd = ps->readData(data->tileBuff, data->tileBuffSize);
            if(rd < data->tileBuffSize)
                printf("Incomplete decoded data while decoding row %d col %d\n", row, col);
            
            // check if decoder process is still alive
            if(!ps->isReady()) {
                printf("ffmpeg stopped during the decoding of row %d col %d\n", row, col);
                return;
            }

            // copy to main buffer at appropriate location
            uint8_t* baseFinal = data->finalBuff + (row * data->tileHeight * data->finalStride) + (col * data->tileStride);
            for(int i=0; i<data->tileHeight; ++i) {
                memcpy(baseFinal + (i * data->finalStride), data->tileBuff + (i * data->tileStride), data->tileStride);
            }
        }
    }
}

int main(int argc, char** argv) {
    // TODO: check if ffmpeg is available for use
    if(argc < 2) {
        puts("Usage: heifread (HEIF file)");
        return 0;
    }
    auto* reader = Reader::Create();
    if(reader->initialize(argv[1]) == ErrorCode::OK) {
        readHEIF(reader);
    }
    else {
        printf("Cannot open file '%s'\n", argv[1]);
    }
    Reader::Destroy(reader);
}
