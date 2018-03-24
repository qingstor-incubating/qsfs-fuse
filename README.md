# qsfs

[![Build Status](https://travis-ci.org/yunify/qsfs-fuse.svg?branch=master)][build link]
[![License](http://img.shields.io/badge/license-apache%20v2-blue.svg)][license link]

**qsfs** is a FUSE based filesystem that allows you to mount a qingstor bucket in Linux.


## Features

- Large subset of POSIX including reading/writing files, directories, symlinks, .etc,
  (chmod/chown/utimens will be supported later).
- File renames via server-side move.
- File Transfer:
  - Large files uploads via multipart parallel uploads.
  - Large files downloads via parallel byte-range downloads.
  - Large files transfer in chunks (10MB chunks by default). If you are uploading large files (e.g. larger than 1GB), you can increase the transfer buffer size and the max
  parallel transfers.
- Data integrity via MD5 hashes.
- Cache:
  - In-memory metadata caching.
  - In-memory file data caching. For a big file, partial file data may been stored in a local disk file when the im-memory file cache is not available.
- Logging/Debugging:
  - Support to log messages to console or to a directory.
  - Support turn on debug message to log by specifying debug option *-d*, this option
  will also enable FUSE debug mode.
  - You can turn on debug message from curl by specifying option *-U*.


## Installation

See the [INSTALL][install link] for installation instructions.


## Usage

Enter your QingCloud API access keys in a file `/path/to/cred`, go to [QingCloud Console][qingcloud console link] to create them if you do not have one:
```sh
 $ echo YourAccessKeyId:YourSecretKey > /path/to/cred
```

Make sure the file `/path/to/cred` has proper permissions (if you get 'permissions' error when mounting):
```sh
 $ chmod 600 /path/to/cred
```

Run qsfs with an existing bucket `mybucket` and directory `/path/to/mountpoint`:
```sh
 $ [sudo] qsfs mybucket /path/to/mountpoint -c=/path/to/cred
```

If you encounter any errors, enable debug output:
```sh
 $ [sudo] qsfs mybucket /path/to/mountpoint -c=/path/to/cred -d
```

You can also mount on boot by entering the following line to /etc/fstab:

```sh
qsfs#mybucket /path/to/mountpoint fuse _netdev,-z=pek3a,-c=/path/to/cred,allow_other 0 0
```

> Notice: in /etc/fstab, you need to use short options (for example, use short option `-c` instead of long option `--credentials`) in order to pass them to qsfs.

You can log messages to console:
```sh
 $ [sudo] qsfs mybucket /path/to/mountpoint -c=/path/to/cred -f
```

Or you can log messages to log file by specifying a directory `/path/to/logdir/`:
```sh
 $ [sudo] qsfs mybucket /path/to/mountpoint -c=/path/to/cred -l=/path/to/logdir/
```

Specify log level (INFO,WARN,ERROR and FATAL):
```sh
 $ [sudo] qsfs mybucket /path/to/mountpoint -c=/path/to/cred -L=INFO -d
```

To umount:
```sh
 $ [sudo] fusermount -uqz /path/to/mountpoint
```
  or
```sh
 $ [sudo] umount -l /path/to/mountpoint
```

For help:
```sh
 $ qsfs -h
```

## Command line options
Supported general options are listed as following,

| short | full | type | required | usage |
| ----- |------|:------:|:----------:|------ |
| -c | --credentials | string  | N | Specify credentials file, default path is `/etc/qsfs.cred`
| -z | --zone        | string  | N | Specify zone or region, default value is `pek3a`
| -l | --logdir      | string  | N | Specify log directory, default path is `/tmp/qsfs_log/`
| -L | --loglevel    | string  | N | Specify min log level (INFO,WARN,ERROR or FATAL), message lower than this level don't logged; default value is `INFO`.
| -F | --filemode    | octal   | N | Specify the permission bits in st_mode for file objects without x-qs-meta-mode header. The value is given in octal representation; default value is `0644`
| -D | --dirmode     | octal   | N | Specify the permission bits in st_mode for directory objects without x-qs-meta-mode header. The value is given in octal representation; default value is `0755`
| -u | --umaskmp     | octal   | N | Specify the permission bits in st_mode for the mount point directory. This option only works when you set with the fuse allow_other option. The resulting permission bits are the ones missing from the given umask value. The value is given in octal representation; default value is `0000`
| -r | --retries     | integer | N | Specify number of times to retry a failed transaction, default value is `3 times`
| -R | --reqtimeout  | integer | N | Specify time(seconds) to wait before timing out a request, default value is `30 seconds`
| -Z | --maxcache    | integer | N | Specify max in-memory cache size(MB) for files, default value is `200 MB`
| -k | --diskdir     | string  | N | Specify the directory to store file data when in-memory cache is not availabe, default path is `/tmp/qsfs_cache/`
| -t | --maxstat     | integer | N | Specify max count(K) of cached stat entrys, default value is `20 K`
| -e | --statexpire  | integer | N | Specify expire time(minutes) for stat entries, negative value will disable stat expire, default is no expire
| -i | --maxlist     | integer | N | Specify max count of files of ls operation. A value of zero will list all files, default is to list all files
| -n | --numtransfer | integer | N | Specify max number file tranfers to run in parallel, you can increase the value when transfer large files, default value is `5`
| -b | --bufsize     | integer | N | Specify file transfer buffer size(MB), this should be larger than 8MB, default value is `10 MB`
| -H | --host        | string  | N | Specify host name, default value is `qingstor.com`
| -p | --protocol    | string  | N | Specify protocol (https or http) default value is `https`
| -P | --port        | integer | N | Specify port, default is 443 for https and 80 for http
| -a | --agent       | string  | N | Specify additional user agent, default is empty

Supported miscellaneous options are list as following,

| short | full | type | required | usage |
| ----- |------|:------:|:----------:|------ |
| -m | --contentMD5  | bool | N | Enable writes with MD5 hashs to ensure data integrity
| -C | --clearlogdir | bool | N | Clear log directory at beginning
| -f | --forground   | bool | N | Turn on log to STDERR and enable FUSE foreground mode
| -s | --single      | bool | N | Turn on FUSE single threaded option - disable multi-threaded
| -d | --debug       | bool | N | Turn on debug messages to log and enable FUSE debug option
| -U | --curldbg     | bool | N | Turn on debug message from curl
| -h | --help        | bool | N | Print qsfs help
| -V | --version     | bool | N | Print qsfs version

You can also specify FUSE specific mount options with `-o opt [,opt...]` e.g. nonempty, allow_other, etc. See the FUSE's README for the full set.


## Limitations

Generally qingstor cannot offer the same performance or semantics as a local file system.  More specifically:
- Random writes or appends to files require rewriting the entire file
- Metadata operations such as listing directories have poor performance due to network latency
- [Eventual consistency][eventual consistency wiki] can temporarily yield stale data
- No atomic renames of files or directories
- No coordination between multiple clients mounting the same bucket
- No hard links


## Frequently Asked Questions

- [FAQ wiki page][faq wiki link]


## Support

If you notice any issue, please open an [issue][issue link] on GitHub. Please search the existing issues and see if others are also experiencing the issue before opening a new issue. Please include the version of qsfs, Compiler version, CMake version, and OS you’re using.


## License

See the [LICENSE][license link] for details. In summary, qsfs is licensed under the Apache License (Version 2.0, January 2004).


[build link]: https://travis-ci.org/yunify/qsfs-fuse
[eventual consistency wiki]: https://en.wikipedia.org/wiki/Eventual_consistency
[faq wiki link]: https://github.com/yunify/qsfs-fuse/wiki/FAQ
[install link]: https://github.com/yunify/qsfs-fuse/blob/master/INSTALL.md
[issue link]: https://github.com/yunify/qsfs-fuse/issues
[license link]: https://github.com/yunify/qsfs-fuse/blob/master/COPYING
[qingcloud console link]: https://console.qingcloud.com/access_keys/