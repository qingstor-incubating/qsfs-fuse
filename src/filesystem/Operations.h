// +-------------------------------------------------------------------------
// | Copyright (C) 2017 Yunify, Inc.
// +-------------------------------------------------------------------------
// | Licensed under the Apache License, Version 2.0 (the "License");
// | You may not use this work except in compliance with the License.
// | You may obtain a copy of the License in the LICENSE file, or at:
// |
// | http://www.apache.org/licenses/LICENSE-2.0
// |
// | Unless required by applicable law or agreed to in writing, software
// | distributed under the License is distributed on an "AS IS" BASIS,
// | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// | See the License for the specific language governing permissions and
// | limitations under the License.
// +-------------------------------------------------------------------------

#ifndef QSFS_FILESYSTEM_OPERATIONS_H_
#define QSFS_FILESYSTEM_OPERATIONS_H_

#include <sys/stat.h>

#include "configure/IncludeFuse.h"  // for fuse.h

namespace QS {

namespace FileSystem {

//
// FUSE Invariants (https://github.com/libfuse/libfuse/wiki/Invariants)
//
// There are a number of assumptions that one can safely make when implementing
// a filesystem using fuse. In general:
//
//     The VFS takes care of avoiding some race conditions:
//
//         while "Unlink()" is running on a specific file, it cannot be
//         interrupted by a "Chmod()", "Link()" or "Open()" call from a
//         different thread on the same file.
//
//         while "Rmdir()" is running, no files can be created in the directory
//         that "Rmdir()" is acting on.
//
//     If a request returns invalid values (e.g. in the structure returned by
//     "Getattr()" or in the link target returned by "Symlink()") or if a
//     request appears to have failed (e.g. if a "Create()" request succeds but
//     a subsequent "Getattr()" (that fuse calls automatically) indicates that
//     no regular file has been created), the syscall returns EIO to the caller.
//
// When using the high-level API (defined in fuse.h):
//
//     All requests are absolute, i.e. all paths begin with / and include the
//     complete path to a file or a directory. Symlinks, . and .. are already
//     resolved.
//
//     For every request you can get except for "Getattr()", "Read()" and
//     "Write()", usually for every path argument (both source and destination
//     for link and rename, but only the source for symlink), you will get a
//     "Getattr()" request just before the callback.
//
//     For example, suppose I store file names of files in a filesystem also
//     into a database. To keep data in sync, I would like, for each filesystem
//     operation that succeeds, to check if the file exists on the database. I
//     just do this in the "Getattr()" call, since all other calls will be
//     preceded by a getattr.
//
//     The value of the "st_dev" attribute in the "Getattr()" call are ignored
//     by fuse and an appropriate anomynous device number is inserted instead.
//
//     The arguments for every request are already verified as much as possible.
//     This means that, for example "readdir()" is only called with an existing
//     directory name, "Readlink()" is only called with an existing symlink,
//     "Symlink()" is only called if there isn't already another object with the
//     requested linkname, "read()" and "Write()" are only called if the file
//     has been opened with the correct flags.
//

void InitializeFUSECallbacks(struct fuse_operations* fuseOps);

int qsfs_getattr(const char* path, struct stat* statbuf);
int qsfs_readlink(const char* path, char* link, size_t size);
int qsfs_mknod(const char* path, mode_t mode, dev_t dev);
int qsfs_mkdir(const char* path, mode_t mode);
int qsfs_unlink(const char* path);
int qsfs_rmdir(const char* path);
int qsfs_symlink(const char* path, const char* link);
int qsfs_rename(const char* path, const char* newpath);
int qsfs_link(const char* path, const char* newpath);
int qsfs_chmod(const char* path, mode_t mode);
int qsfs_chown(const char* path, uid_t uid, gid_t gid);
int qsfs_truncate(const char* path, off_t newsize);
int qsfs_open(const char* path, struct fuse_file_info* fi);
int qsfs_read(const char* path, char* buf, size_t size, off_t offset,
              struct fuse_file_info* fi);
int qsfs_write(const char* path, const char* buf, size_t size, off_t offset,
               struct fuse_file_info* fi);
int qsfs_statfs(const char* path, struct statvfs* statv);
int qsfs_flush(const char* path, struct fuse_file_info* fi);
int qsfs_release(const char* path, struct fuse_file_info* fi);
int qsfs_fsync(const char* path, int datasync, struct fuse_file_info* fi);
int qsfs_setxattr(const char* path, const char* name, const char* value,
                  size_t size, int flags);
int qsfs_getxattr(const char* path, const char* name, char* value, size_t size);
int qsfs_listxattr(const char* path, char* list, size_t size);
int qsfs_removexattr(const char* path, const char* name);
int qsfs_opendir(const char* path, struct fuse_file_info* fi);
int qsfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info* fi);
int qsfs_releasedir(const char* path, struct fuse_file_info* fi);
int qsfs_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi);
void* qsfs_init(struct fuse_conn_info* conn);
void qsfs_destroy(void* userdata);
int qsfs_access(const char* path, int mask);
int qsfs_create(const char* path, mode_t mode, struct fuse_file_info* fi);
int qsfs_ftruncate(const char* path, off_t offset, struct fuse_file_info* fi);
int qsfs_fgetattr(const char* path, struct stat* statbuf,
                  struct fuse_file_info* fi);
int qsfs_lock(const char* path, struct fuse_file_info* fi, int cmd,
              struct flock* lock);
int qsfs_utimens(const char* path, const struct timespec tv[2]);
int qsfs_write_buf(const char* path, struct fuse_bufvec* buf, off_t off,
                   struct fuse_file_info*);
int qsfs_read_buf(const char* path, struct fuse_bufvec** bufp, size_t size,
                  off_t off, struct fuse_file_info* fi);
int qsfs_fallocate(const char* path, int, off_t offseta, off_t offsetb,
                   struct fuse_file_info* fi);

}  // namespace FileSystem
}  // namespace QS


#endif  // QSFS_FILESYSTEM_OPERATIONS_H_
