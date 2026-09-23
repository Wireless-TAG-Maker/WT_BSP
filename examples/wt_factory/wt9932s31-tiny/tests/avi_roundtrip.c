/**
 * @file avi_roundtrip.c
 * @author Wireless-Tag
 * @brief Host-side recorder use cases: finalize, bounds, abort and no overwrite.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#include "avi_writer.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL);
    assert(fseek(input, 0, SEEK_END) == 0);
    long length = ftell(input);
    assert(length > 4);
    rewind(input);
    uint8_t *jpeg = malloc(length);
    assert(jpeg != NULL && fread(jpeg, 1, length, input) == (size_t)length);
    fclose(input);

    avi_writer_t writer = {0};
    assert(avi_writer_open(&writer, argv[2], 64, 48, 25, 5) == 0);
    avi_writer_t duplicate = {0};
    assert(avi_writer_open(&duplicate, argv[2], 64, 48, 25, 5) == -1 && errno == EEXIST);
    assert(avi_writer_append(&writer, jpeg, length - 1, 0) == -1 && errno == EINVAL);
    assert(avi_writer_append(&writer, jpeg, length, 0) == 0);
    assert(avi_writer_append(&writer, jpeg, length, 0) == -1 && errno == EINVAL);
    for (int i = 1; i < 5; i++) {
        assert(avi_writer_append(&writer, jpeg, length, i * 42000) == 0);
    }
    assert(avi_writer_append(&writer, jpeg, length, 210000) == -1 && errno == ENOSPC);
    assert(avi_writer_close(&writer) == 0);
    assert(writer.file == NULL && writer.index == NULL);
    assert(avi_writer_open(&writer, argv[2], 64, 48, 25, 5) == -1 && errno == EEXIST);
    avi_writer_abort(&writer);
    avi_writer_abort(&writer);
    free(jpeg);
    return 0;
}
