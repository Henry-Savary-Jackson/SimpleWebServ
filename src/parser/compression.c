#include "server.h"
#include <parser.h>
#include <utils.h>
#include <zconf.h>
#include <zlib.h>

int encode_zlib(char *data, int inSize, char **output, int *outSize)
{
    uLongf newSize = compressBound(inSize);
    ;
    *output = custom_alloc(newSize);
    int result = compress((Bytef *)*output, &newSize, (const Bytef *)data, (uLong)inSize);
    switch (result)
    {
    case Z_OK:
        *outSize = (int)newSize;
        return 0;
    default:
        return -1;
    }
}

// TODO FIX
int decode_zlib(char *data, int inSize, char **output, int *outSize)
{
    z_stream strm;
    strm.avail_in = inSize;
    strm.next_in = (Bytef *)data;
    decode_zlib_prepare(&strm);
    return decode_zstream(&strm, output, outSize);
}

int decode_zlib_prepare(z_streamp strm)
{

    strm->zfree = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->opaque = Z_NULL;
    return inflateInit(strm);
}

int encode_zlib_prepare(z_streamp strm)
{

    strm->zfree = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->opaque = Z_NULL;
    return deflateInit(strm, Z_DEFAULT_COMPRESSION);
}

int encode_gzip_prepare(z_streamp strm)
{
    strm->zfree = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->opaque = Z_NULL;
    return deflateInit2(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 | 15, 8, Z_DEFAULT_STRATEGY);
}

int decode_gzip_prepare(z_streamp strm)
{
    strm->zfree = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->opaque = Z_NULL;
    return inflateInit2(strm, 16 | 15);
}

int encode_gzip(char *data, int inSize, char **output, int *outSize)
{
    z_stream strm;
    encode_gzip_prepare(&strm);

    strm.next_in = (Bytef *)data;
    strm.avail_in = inSize;

    // allocate output buffer after init
    int bound = (int)deflateBound(&strm, inSize);
    *output = custom_alloc(bound);
    strm.next_out = (Bytef *)*output;
    strm.avail_out = bound;

    // run the deflation algo
    uLong resultDeflate = deflate(&strm, Z_FINISH);
    if (resultDeflate != Z_OK && resultDeflate != Z_STREAM_END)
    {
        return -1;
    }
    // get the sizeof data written
    *outSize = bound - (int)strm.avail_out;

    deflateEnd(&strm);
    return 0;
}

int compressChunk(z_streamp strm, char *chunk, int chunkSize, char *outChunk, int outChunkSize, bool isEOF)
{
    strm->next_in = (Bytef *)chunk;
    strm->avail_out = outChunkSize;
    strm->next_out = (Bytef *)outChunk;
    int ret = deflate(strm, isEOF?Z_FINISH : Z_SYNC_FLUSH );
    assert(ret != Z_STREAM_ERROR);
    return outChunkSize - (int)strm->avail_out;
}


int inflateChunk(z_streamp strm, int outSize, char *outChunk)
{

    int have = outSize;
    strm->avail_out = outSize;
    strm->next_out = (Bytef *)outChunk;
    int ret = inflate(strm, Z_SYNC_FLUSH);
    switch (ret)
    {
    case Z_NEED_DICT:
        ret = Z_DATA_ERROR; /* and fall through */
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
        (void)inflateEnd(strm);
        return ret;
    default:
        break;
    }
    // return the amount inflated
    return outSize - (int)strm->avail_out;
}

int decode_zstream(z_streamp strm, char **output, int *outSize)
{
    const int CHUNK_SIZE = 2048;
    GrowingBuffer outGrowingBuffer;
    initGrowingBuffer(&outGrowingBuffer, CHUNK_SIZE);
    int n_written = 0;
    while ((n_written = inflateChunk(strm, CHUNK_SIZE, outGrowingBuffer.ptr + outGrowingBuffer.size)) > 0)
    {
        outGrowingBuffer.size += n_written;
        increaseCapacityGrowingBuffer(&outGrowingBuffer, CHUNK_SIZE);
        if (n_written < CHUNK_SIZE)
        {
            // end
            break;
        }
    }
    if (n_written < 0)
    {
        // uh oh errror
        return n_written;
    }
    *outSize = outGrowingBuffer.size;
    *output = outGrowingBuffer.ptr;

    inflateEnd(strm);
    return 0;
}


int decode_gzip(char *data, int inSize, char **output, int *outSize)
{
    z_stream strm;
    strm.avail_in = inSize;
    strm.next_in = (Bytef *)data;
    decode_gzip_prepare(&strm);
    return decode_zstream(&strm, output, outSize);
}
