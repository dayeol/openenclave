// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/file.h>
#include <openenclave/internal/mount.h>
#include <openenclave/internal/tests.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../libc/fs/blockdev.h"
#include "../../../libc/fs/buf.h"
#include "../../../libc/fs/oefs.h"

#define INIT

#define BLOCK_SIZE 512

static int _load(fs_t* fs, const char* path, void** data_out, size_t* size_out)
{
    int ret = -1;
    fs_file_t* file = NULL;
    buf_t buf = BUF_INITIALIZER;

    if (data_out)
        *data_out = NULL;

    if (size_out)
        *size_out = 0;

    if (!fs || !path || !data_out || !size_out)
        goto done;

    if (fs->fs_open(fs, path, 0, 0, &file))
        goto done;

    for (;;)
    {
        char data[4096];
        int32_t n;

        if (fs->fs_read(file, data, sizeof(data), &n) != OE_EOK)
            goto done;

        if (n == 0)
            break;

        if (buf_append(&buf, data, n) != 0)
            goto done;
    }

    *data_out = buf.data;
    *size_out = buf.size;
    memset(&buf, 0, sizeof(buf));

    ret = 0;

done:

    if (file)
        fs->fs_close(file);

    buf_release(&buf);

    return ret;
}

static void _dump_dir(fs_t* fs, const char* dirname)
{
    fs_errno_t r;
    fs_dir_t* dir;
    fs_dirent_t* ent;

    r = fs->fs_opendir(fs, dirname, &dir);
    OE_TEST(r == OE_EOK);
    OE_TEST(dir != NULL);

    printf("<<<<<<<< _dump_dir(%s)\n", dirname);

    while ((r = fs->fs_readdir(dir, &ent)) == OE_EOK && ent)
    {
        printf("name=%s\n", ent->d_name);
    }

    OE_TEST(r == OE_EOK);

    printf(">>>>>>>>\n\n");

    r = fs->fs_closedir(dir);
    OE_TEST(r == OE_EOK);
}

static void _create_files(fs_t* fs)
{
    const size_t NUM_FILES = 100;
    fs_errno_t r;
    fs_file_t* file;

    for (size_t i = 0; i < NUM_FILES; i++)
    {
        char path[FS_PATH_MAX];
        snprintf(path, sizeof(path), "/filename-%04zu", i);
        r = fs->fs_create(fs, path, 0, &file);
        OE_TEST(r == OE_EOK);
        OE_TEST(file != NULL);
        fs->fs_close(file);
    }
}

static void _remove_files_nnnn(fs_t* fs)
{
    const size_t NUM_FILES = 100;
    fs_errno_t r;

    for (size_t i = 0; i < NUM_FILES; i++)
    {
        char path[FS_PATH_MAX];
        snprintf(path, sizeof(path), "/filename-%04zu", i);
        r = fs->fs_unlink(fs, path);
        OE_TEST(r == OE_EOK);
    }
}

static void _create_dirs(fs_t* fs)
{
    const size_t NUM_DIRS = 10;
    fs_errno_t r;

    for (size_t i = 0; i < NUM_DIRS; i++)
    {
        char name[FS_PATH_MAX];
        snprintf(name, sizeof(name), "/dir-%04zu", i);

        r = fs->fs_mkdir(fs, name, 0);
        OE_TEST(r == OE_EOK);
    }

    r = fs->fs_mkdir(fs, "/aaa", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_mkdir(fs, "/aaa/bbb", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_mkdir(fs, "/aaa/bbb/ccc", 0);
    OE_TEST(r == OE_EOK);

    /* Stat root directory and check expectations. */
    {
        fs_stat_t stat;
        r = fs->fs_stat(fs, "/", &stat);
        OE_TEST(r == OE_EOK);

        OE_TEST(stat.st_dev == 0);
        OE_TEST(stat.st_ino != 0);
        OE_TEST(stat.st_mode == FS_S_DIR_DEFAULT);
        OE_TEST(stat.st_mode & FS_S_IFDIR);
        OE_TEST(stat.__st_padding == 0);
        OE_TEST(stat.st_nlink == 1);
        OE_TEST(stat.st_uid == 0);
        OE_TEST(stat.st_gid == 0);
        OE_TEST(stat.st_rdev == 0);
        OE_TEST(stat.st_size > sizeof(fs_dirent_t) * 2);
        OE_TEST(stat.st_blksize == FS_BLOCK_SIZE);
        OE_TEST(stat.st_blocks > 2);
        OE_TEST(stat.st_atime == 0);
        OE_TEST(stat.st_mtime == 0);
        OE_TEST(stat.st_ctime == 0);
    }

    /* Stat the directory and check expectations. */
    {
        fs_stat_t stat;
        r = fs->fs_stat(fs, "/aaa/bbb/ccc", &stat);
        OE_TEST(r == OE_EOK);

        OE_TEST(stat.st_dev == 0);
        OE_TEST(stat.st_ino != 0);
        OE_TEST(stat.st_mode == FS_S_DIR_DEFAULT);
        OE_TEST(stat.st_mode & FS_S_IFDIR);
        OE_TEST(stat.__st_padding == 0);
        OE_TEST(stat.st_nlink == 1);
        OE_TEST(stat.st_uid == 0);
        OE_TEST(stat.st_gid == 0);
        OE_TEST(stat.st_rdev == 0);
        OE_TEST(stat.st_size == sizeof(fs_dirent_t) * 2);
        OE_TEST(stat.st_blksize == FS_BLOCK_SIZE);
        OE_TEST(stat.st_blocks == 2);
        OE_TEST(stat.st_atime == 0);
        OE_TEST(stat.st_mtime == 0);
        OE_TEST(stat.st_ctime == 0);
    }
}

static void _remove_dir_nnnn(fs_t* fs)
{
    const size_t NUM_DIRS = 10;
    fs_errno_t r;

    for (size_t i = 0; i < NUM_DIRS; i++)
    {
        char name[FS_PATH_MAX];
        snprintf(name, sizeof(name), "/dir-%04zu", i);

        r = fs->fs_rmdir(fs, name);
        OE_TEST(r == OE_EOK);
    }
}

static void _dump_file(fs_t* fs, const char* path)
{
    void* data;
    size_t size;

    OE_TEST(_load(fs, path, &data, &size) == 0);

    printf("<<<<<<<< _dump_file(%s): %zu bytes\n", path, size);

    for (size_t i = 0; i < size; i++)
    {
        uint8_t b = *((uint8_t*)data + i);

        if (isprint(b))
            printf("%c", b);
        else
            printf("<%02x>", b);

        if (i + 1 == size)
            printf("\n");
    }

    printf(">>>>>>>>\n\n");

    free(data);
}

static void _update_file(fs_t* fs, const char* path)
{
    fs_errno_t r;
    fs_file_t* file = NULL;
    const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    const size_t FILE_SIZE = 1024;
    buf_t buf = BUF_INITIALIZER;
    int32_t n;

    r = fs->fs_open(fs, path, 0, 0, &file);
    OE_TEST(r == OE_EOK);

    for (size_t i = 0; i < FILE_SIZE; i++)
    {
        char c = alphabet[i % (sizeof(alphabet) - 1)];

        if (buf_append(&buf, &c, 1) != 0)
            OE_TEST(false);
    }

    r = fs->fs_write(file, buf.data, buf.size, &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == buf.size);
    fs->fs_close(file);

    /* Load the file to make sure the changes took. */
    {
        void* data;
        size_t size;

        OE_TEST(_load(fs, path, &data, &size) == 0);
        OE_TEST(size == buf.size);
        OE_TEST(memcmp(data, buf.data, size) == 0);

        free(data);
    }

    /* Stat the file and check expectations. */
    {
        fs_stat_t stat;
        r = fs->fs_stat(fs, path, &stat);
        OE_TEST(r == OE_EOK);

        OE_TEST(stat.st_dev == 0);
        OE_TEST(stat.st_ino != 0);
        OE_TEST(stat.st_mode == FS_S_REG_DEFAULT);
        OE_TEST(stat.st_mode & FS_S_IFREG);
        OE_TEST(stat.__st_padding == 0);
        OE_TEST(stat.st_nlink == 1);
        OE_TEST(stat.st_uid == 0);
        OE_TEST(stat.st_gid == 0);
        OE_TEST(stat.st_rdev == 0);
        OE_TEST(stat.st_size == buf.size);
        OE_TEST(stat.st_blksize == FS_BLOCK_SIZE);
        uint32_t n = (buf.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
        OE_TEST(stat.st_blocks == n);
        OE_TEST(stat.st_atime == 0);
        OE_TEST(stat.st_mtime == 0);
        OE_TEST(stat.st_ctime == 0);
    }

    buf_release(&buf);
}

static void _create_myfile(fs_t* fs)
{
    fs_errno_t r;
    fs_file_t* file;
    const char path[] = "/aaa/bbb/ccc/myfile";

    r = fs->fs_create(fs, path, 0, &file);
    OE_TEST(r == OE_EOK);

    const char message[] = "Hello World!";

    int32_t n;
    r = fs->fs_write(file, message, sizeof(message), &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == sizeof(message));

    fs->fs_close(file);
    _dump_file(fs, path);
}

static void _truncate_file(fs_t* fs, const char* path)
{
    fs_errno_t r = fs->fs_truncate(fs, path);
    OE_TEST(r == OE_EOK);
}

static void _remove_file(fs_t* fs, const char* path)
{
    fs_errno_t r = fs->fs_unlink(fs, path);
    OE_TEST(r == OE_EOK);
}

static void _remove_dir(fs_t* fs, const char* path)
{
    fs_errno_t r = fs->fs_rmdir(fs, path);
    OE_TEST(r == OE_EOK);
}

static void _test_lseek(fs_t* fs)
{
    fs_errno_t r;
    fs_file_t* file;
    const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    const size_t alphabet_length = OE_COUNTOF(alphabet) - 1;
    char buf[1093];
    ssize_t offset;
    int32_t nread;
    int32_t nwritten;

    r = fs->fs_create(fs, "/somefile", 0, &file);
    OE_TEST(r == OE_EOK);

    for (size_t i = 0; i < alphabet_length; i++)
    {
        memset(buf, alphabet[i], sizeof(buf));
        r = fs->fs_write(file, buf, sizeof(buf), &nwritten);
        OE_TEST(r == OE_EOK);
        OE_TEST(nwritten == sizeof(buf));
    }

    r = fs->fs_lseek(file, 0, FS_SEEK_CUR, &offset);
    OE_TEST(r == OE_EOK);
    OE_TEST(offset == sizeof(buf) * 26);

    r = fs->fs_lseek(file, -2 * sizeof(buf), FS_SEEK_CUR, &offset);
    OE_TEST(r == OE_EOK);

    memset(buf, 0, sizeof(buf));
    r = fs->fs_read(file, buf, sizeof(buf), &nread);
    OE_TEST(r == OE_EOK);
    OE_TEST(nread == sizeof(buf));

    {
        char tmp[sizeof(buf)];
        memset(tmp, 'y', sizeof(tmp));
        OE_TEST(memcmp(buf, tmp, sizeof(tmp)) == 0);
    }

    r = fs->fs_close(file);
    OE_TEST(r == OE_EOK);

    //_dump_file(fs, "/somefile");
}

static void _test_links(fs_t* fs)
{
    fs_errno_t r;
    fs_file_t* file;
    int32_t n;
    char buf[1024];
    fs_stat_t stat;

    r = fs->fs_mkdir(fs, "/dir1", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_mkdir(fs, "/dir2", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_create(fs, "/dir1/file1", 0, &file);
    OE_TEST(r == OE_EOK);
    r = fs->fs_write(file, "abcdefghijklmnopqrstuvwxyz", 26, &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == 26);
    fs->fs_close(file);

    r = fs->fs_create(fs, "/dir2/exists", 0, &file);
    OE_TEST(r == OE_EOK);
    fs->fs_close(file);

    r = fs->fs_link(fs, "/dir1/file1", "/dir2/file2");
    OE_TEST(r == OE_EOK);

    r = fs->fs_link(fs, "/dir1/file1", "/dir2/exists");
    OE_TEST(r == OE_EOK);

    r = fs->fs_open(fs, "/dir2/file2", 0, 0, &file);
    OE_TEST(r == OE_EOK);
    r = fs->fs_read(file, buf, sizeof(buf), &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == 26);
    OE_TEST(strncmp(buf, "abcdefghijklmnopqrstuvwxyz", 26) == 0);
    fs->fs_close(file);

    r = fs->fs_open(fs, "/dir2/exists", 0, 0, &file);
    OE_TEST(r == OE_EOK);
    r = fs->fs_read(file, buf, sizeof(buf), &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == 26);
    OE_TEST(strncmp(buf, "abcdefghijklmnopqrstuvwxyz", 26) == 0);
    fs->fs_close(file);

    r = fs->fs_stat(fs, "/dir1/file1", &stat);
    OE_TEST(r == OE_EOK);
    OE_TEST(stat.st_nlink == 3);

    r = fs->fs_unlink(fs, "/dir2/file2");
    OE_TEST(r == OE_EOK);
    r = fs->fs_stat(fs, "/dir1/file1", &stat);
    OE_TEST(r == OE_EOK);
    OE_TEST(stat.st_nlink == 2);

    r = fs->fs_unlink(fs, "/dir2/exists");
    OE_TEST(r == OE_EOK);
    r = fs->fs_stat(fs, "/dir1/file1", &stat);
    OE_TEST(r == OE_EOK);
    OE_TEST(stat.st_nlink == 1);

    r = fs->fs_unlink(fs, "/dir1/file1");
    OE_TEST(r == OE_EOK);

    fs->fs_rmdir(fs, "/dir1");
    OE_TEST(r == OE_EOK);

    fs->fs_rmdir(fs, "/dir2");
    OE_TEST(r == OE_EOK);
}

static void _test_rename(fs_t* fs)
{
    fs_errno_t r;
    fs_file_t* file;
    int32_t n;
    char buf[1024];

    r = fs->fs_mkdir(fs, "/dir1", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_mkdir(fs, "/dir2", 0);
    OE_TEST(r == OE_EOK);

    r = fs->fs_create(fs, "/dir1/file1", 0, &file);
    OE_TEST(r == OE_EOK);
    r = fs->fs_write(file, "abcdefghijklmnopqrstuvwxyz", 26, &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == 26);
    fs->fs_close(file);

    r = fs->fs_rename(fs, "/dir1/file1", "/dir2/file2");
    OE_TEST(r == OE_EOK);

    r = fs->fs_open(fs, "/dir2/file2", 0, 0, &file);
    OE_TEST(r == OE_EOK);
    r = fs->fs_read(file, buf, sizeof(buf), &n);
    OE_TEST(r == OE_EOK);
    OE_TEST(n == 26);
    OE_TEST(strncmp(buf, "abcdefghijklmnopqrstuvwxyz", 26) == 0);
    fs->fs_close(file);

    fs_stat_t stat;
    r = fs->fs_stat(fs, "/dir1/file1", &stat);
    OE_TEST(r != OE_EOK);
}

void run_tests(oe_block_dev_t* dev, size_t num_blocks)
{
    fs_t* fs;
    fs_errno_t r;

    /* Initialize the OEFS file. */
    r = oefs_mkfs(dev, num_blocks);
    OE_TEST(r == OE_EOK);

    /* Create an new OEFS object. */
    r = oefs_initialize(&fs, dev);
    OE_TEST(r == OE_EOK);

    /* Create some files. */
    _create_files(fs);

    /* Create some directories. */
    _create_dirs(fs);

    /* Dump some directories. */
    _dump_dir(fs, "/");
    _dump_dir(fs, "/dir-0001");
    _dump_dir(fs, "/aaa/bbb/ccc");

    /* Test updating of a file. */
    {
        const char path[] = "/filename-0001";
        _update_file(fs, path);
        _dump_file(fs, path);
        _truncate_file(fs, path);
        _dump_file(fs, path);
    }

    /* Create "/aaa/bbb/ccc/myfile" */
    _dump_dir(fs, "/aaa/bbb/ccc");
    _create_myfile(fs);
    _dump_dir(fs, "/aaa/bbb/ccc");
    _remove_file(fs, "/aaa/bbb/ccc/myfile");
    _dump_dir(fs, "/aaa/bbb/ccc");

    /* Remove some directories. */
    _dump_dir(fs, "/aaa/bbb");
    _remove_dir(fs, "/aaa/bbb/ccc");
    _dump_dir(fs, "/aaa/bbb");

    /* Remove some directories. */
    _dump_dir(fs, "/aaa");
    _remove_dir(fs, "/aaa/bbb");
    _dump_dir(fs, "/aaa");

    /* Remove some directories. */
    _dump_dir(fs, "/");
    _remove_dir(fs, "/aaa");
    _dump_dir(fs, "/");

    /* Remove directories named like dir-NNNN */
    _remove_dir_nnnn(fs);
    _dump_dir(fs, "/");

    /* Remove files named like filename-NNNN */
    _remove_files_nnnn(fs);
    _dump_dir(fs, "/");

    /* Test the lseek function. */
    _test_lseek(fs);

    /* Test the link function. */
    _test_links(fs);

    /* Test the rename function. */
    _test_rename(fs);

    fs->fs_release(fs);
}

int test_oefs(const char* oefs_filename)
{
    size_t num_blocks = 4 * 4096;

    /* Run tests on the host block device. */
    {
        oe_block_dev_t* dev;

        OE_TEST(oe_open_host_block_dev(oefs_filename, &dev) == 0);
        run_tests(dev, num_blocks);
        dev->close(dev);
    }

    /* Run tests on the RAM device. */
    {
        oe_block_dev_t* dev;
        size_t size;

        OE_TEST(oefs_size(num_blocks, &size) == OE_EOK);
        OE_TEST(oe_open_ram_block_dev(size, &dev) == 0);
        run_tests(dev, num_blocks);
        dev->close(dev);
    }

    return 0;
}

OE_SET_ENCLAVE_SGX(
    1,         /* ProductID */
    1,         /* SecurityVersion */
    true,      /* AllowDebug */
    10 * 1024, /* HeapPageCount */
    10 * 1024, /* StackPageCount */
    2);        /* TCSCount */