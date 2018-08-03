/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <guacamole/config.h>

#include <rdp/rdp_fs.h>
#include <rdp/rdp_status.h>
#include <rdp/rdp_stream.h>
#ifdef HAVE_BOOST
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#elif defined HAVE_LIBPTHREAD
#include <dirent.h>
#include <fnmatch.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <guacamole/client.h>
#include <guacamole/object.h>
#include <guacamole/pool.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

guac_rdp_fs* guac_rdp_fs_alloc(guac_client* client, const char* drive_path,
        int create_drive_path) {

    /* Create drive path if it does not exist */
    if (create_drive_path) {
        guac_client_log(client, GUAC_LOG_DEBUG,
               "%s: Creating directory \"%s\" if necessary.",
               __func__, drive_path);

        /* Log error if directory creation fails */
#ifdef HAVE_BOOST
		boost::filesystem::path p(drive_path);
		boost::system::error_code ec;
		if(!boost::filesystem::create_directories(p, ec) || ec)
		{
			guac_client_log(client, GUAC_LOG_ERROR,
				"Unable to create directory \"%s\": %s",
				drive_path, strerror(errno));
		}
#elif defined HAVE_LIBPTHREAD
        if (mkdir(drive_path, S_IRWXU) && errno != EEXIST) {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Unable to create directory \"%s\": %s",
                    drive_path, strerror(errno));
        }
#endif
    }

    guac_rdp_fs* fs = static_cast<guac_rdp_fs*>(
		malloc(sizeof(guac_rdp_fs)));

    fs->client = client;
    fs->drive_path = strdup(drive_path);
    fs->file_id_pool = guac_pool_alloc(0);
    fs->open_files = 0;

#ifdef HAVE_BOOST
	for (auto && file : fs->files)
	{
		file.absolute_path = nullptr;
		file.real_path = nullptr;
	}
#endif

    return fs;

}

void guac_rdp_fs_free(guac_rdp_fs* fs) {
    guac_pool_free(fs->file_id_pool);
    free(fs->drive_path);
    free(fs);
}

guac_object* guac_rdp_fs_alloc_object(guac_rdp_fs* fs, guac_user* user) {

    /* Init filesystem */
    guac_object* fs_object = guac_user_alloc_object(user);
    fs_object->get_handler = guac_rdp_download_get_handler;
    fs_object->put_handler = guac_rdp_upload_put_handler;
    fs_object->data = fs;

    /* Send filesystem to user */
    guac_protocol_send_filesystem(user->socket, fs_object, "Shared Drive");
    guac_socket_flush(user->socket);

    return fs_object;

}

void* guac_rdp_fs_expose(guac_user* user, void* data) {

    guac_rdp_fs* fs = (guac_rdp_fs*) data;

    /* No need to expose if there is no filesystem or the user has left */
    if (user == NULL || fs == NULL)
        return NULL;

    /* Allocate and expose filesystem object for user */
    return guac_rdp_fs_alloc_object(fs, user);

}

/**
 * Translates an absolute Windows path to an absolute path which is within the
 * "drive path" specified in the connection settings. No checking is performed
 * on the path provided, which is assumed to have already been normalized and
 * validated as absolute.
 *
 * @param fs
 *     The filesystem containing the file whose path is being translated.
 *
 * @param virtual_path
 *     The absolute path to the file on the simulated filesystem, relative to
 *     the simulated filesystem root.
 *
 * @param real_path
 *     The buffer in which to store the absolute path to the real file on the
 *     local filesystem.
 */
static void __guac_rdp_fs_translate_path(guac_rdp_fs* fs,
        const char* virtual_path, char* real_path) {

    /* Get drive path */
    char* drive_path = fs->drive_path;

    int i;

    /* Start with path from settings */
    for (i=0; i<GUAC_RDP_FS_MAX_PATH-1; i++) {

        /* Break on end-of-string */
        char c = *(drive_path++);
        if (c == 0)
            break;

        /* Copy character */
        *(real_path++) = c;

    }

    /* Translate path */
    for (; i<GUAC_RDP_FS_MAX_PATH-1; i++) {

        /* Stop at end of string */
        char c = *(virtual_path++);
        if (c == 0)
            break;

        /* Translate backslashes to forward slashes */
        if (c == '\\')
            c = '/';

        /* Store in real path buffer */
        *(real_path++)= c;

    }

    /* Null terminator */
    *real_path = 0;

}

int guac_rdp_fs_get_errorcode(int err) {

    /* Translate errno codes to GUAC_RDP_FS codes */
    if (err == ENFILE)  return GUAC_RDP_FS_ENFILE;
    if (err == ENOENT)  return GUAC_RDP_FS_ENOENT;
    if (err == ENOTDIR) return GUAC_RDP_FS_ENOTDIR;
    if (err == ENOSPC)  return GUAC_RDP_FS_ENOSPC;
    if (err == EISDIR)  return GUAC_RDP_FS_EISDIR;
    if (err == EACCES)  return GUAC_RDP_FS_EACCES;
    if (err == EEXIST)  return GUAC_RDP_FS_EEXIST;
    if (err == EINVAL)  return GUAC_RDP_FS_EINVAL;
    if (err == ENOSYS)  return GUAC_RDP_FS_ENOSYS;
    if (err == ENOTSUP) return GUAC_RDP_FS_ENOTSUP;

    /* Default to invalid parameter */
    return GUAC_RDP_FS_EINVAL;

}

int guac_rdp_fs_get_status(int err) {

    /* Translate GUAC_RDP_FS error code to RDPDR status code */
    if (err == GUAC_RDP_FS_ENFILE)  return STATUS_NO_MORE_FILES;
    if (err == GUAC_RDP_FS_ENOENT)  return STATUS_NO_SUCH_FILE;
    if (err == GUAC_RDP_FS_ENOTDIR) return STATUS_NOT_A_DIRECTORY;
    if (err == GUAC_RDP_FS_ENOSPC)  return STATUS_DISK_FULL;
    if (err == GUAC_RDP_FS_EISDIR)  return STATUS_FILE_IS_A_DIRECTORY;
    if (err == GUAC_RDP_FS_EACCES)  return STATUS_ACCESS_DENIED;
    if (err == GUAC_RDP_FS_EEXIST)  return STATUS_OBJECT_NAME_COLLISION;
    if (err == GUAC_RDP_FS_EINVAL)  return STATUS_INVALID_PARAMETER;
    if (err == GUAC_RDP_FS_ENOSYS)  return STATUS_NOT_IMPLEMENTED;
    if (err == GUAC_RDP_FS_ENOTSUP) return STATUS_NOT_SUPPORTED;

    /* Default to invalid parameter */
    return STATUS_INVALID_PARAMETER;

}

int guac_rdp_fs_open(guac_rdp_fs* fs, const char* path,
        int access, int file_attributes, int create_disposition,
        int create_options) {
    char real_path[GUAC_RDP_FS_MAX_PATH];
    char normalized_path[GUAC_RDP_FS_MAX_PATH];

    int file_id;
    guac_rdp_fs_file* file;

	int flags = 0;

#ifdef HAVE_BOOST
	bool create_file = false;
	bool fail_if_file_exists = false;
#elif defined HAVE_LIBPTHREAD
	struct stat file_stat;
	int fd;
#endif

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: path=\"%s\", access=0x%x, file_attributes=0x%x, "
            "create_disposition=0x%x, create_options=0x%x",
            __func__, path, access, file_attributes,
            create_disposition, create_options);

    /* If no files available, return too many open */
    if (fs->open_files >= GUAC_RDP_FS_MAX_FILES) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Too many open files.",
                __func__, path);
        return GUAC_RDP_FS_ENFILE;
    }

    /* If path empty, transform to root path */
    if (path[0] == '\0')
        path = "\\";

    /* If path is relative, the file does not exist */
    else if (path[0] != '\\' && path[0] != '/') {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Access denied - supplied path \"%s\" is relative.",
                __func__, path);
        return GUAC_RDP_FS_ENOENT;
    }

    /* Translate access into flags */
    if (access & ACCESS_GENERIC_ALL)
	{
#ifdef HAVE_BOOST
		flags = std::ios::in | std::ios::out;
#elif defined HAVE_LIBPTHREAD
		flags = O_RDWR;
#endif
    }
        
    else if ((access & ( ACCESS_GENERIC_WRITE
                       | ACCESS_FILE_WRITE_DATA
                       | ACCESS_FILE_APPEND_DATA))
          && (access & (ACCESS_GENERIC_READ  | ACCESS_FILE_READ_DATA)))
    {
#ifdef HAVE_BOOST
		flags = std::ios::in | std::ios::out;
#elif defined HAVE_LIBPTHREAD
		flags = O_RDWR;
#endif
    }
    else if (access & ( ACCESS_GENERIC_WRITE
                      | ACCESS_FILE_WRITE_DATA
                      | ACCESS_FILE_APPEND_DATA))
    {
#ifdef HAVE_BOOST
		flags = std::ios::out;
#elif defined HAVE_LIBPTHREAD
		flags = O_WRONLY;
#endif
    }
    else
	{
#ifdef HAVE_BOOST
		flags = std::ios::in;
#elif defined HAVE_LIBPTHREAD
		flags = O_RDONLY;
#endif
	}

    /* Normalize path, return no-such-file if invalid  */
    if (guac_rdp_fs_normalize_path(path, normalized_path)) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Normalization of path \"%s\" failed.", __func__, path);
        return GUAC_RDP_FS_ENOENT;
    }

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: Normalized path \"%s\" to \"%s\".",
            __func__, path, normalized_path);

    /* Translate normalized path to real path */
    __guac_rdp_fs_translate_path(fs, normalized_path, real_path);

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: Translated path \"%s\" to \"%s\".",
            __func__, normalized_path, real_path);

    switch (create_disposition) {

        /* Create if not exist, fail otherwise */
        case DISP_FILE_CREATE:
#ifdef HAVE_BOOST
			fail_if_file_exists = true;
			create_file = true;
#elif defined HAVE_LIBPTHREAD
			flags |= O_CREAT | O_EXCL;
#endif
            break;

        /* Open file if exists and do not overwrite, fail otherwise */
        case DISP_FILE_OPEN:
#ifdef HAVE_BOOST
			fail_if_file_exists = false;
			create_file = true;
#endif
            /* No flag necessary - default functionality of open */
            break;

        /* Open if exists, create otherwise */
        case DISP_FILE_OPEN_IF:
#ifdef HAVE_BOOST
			fail_if_file_exists = false;
			create_file = true;
#elif defined HAVE_LIBPTHREAD
			flags |= O_CREAT;
#endif
            break;

        /* Overwrite if exists, fail otherwise */
        case DISP_FILE_OVERWRITE:
#ifdef HAVE_BOOST
			fail_if_file_exists = false;
			create_file = false;
			flags |= std::ios::trunc;
#elif defined HAVE_LIBPTHREAD
			flags |= O_TRUNC;
#endif
            break;

        /* Overwrite if exists, create otherwise */
        case DISP_FILE_OVERWRITE_IF:
#ifdef HAVE_BOOST
			fail_if_file_exists = false;
			create_file = true;
			flags |= std::ios::trunc;
#elif defined HAVE_LIBPTHREAD
			flags |= O_CREAT | O_TRUNC;
#endif
            break;

        /* Supersede (replace) if exists, otherwise create */
        case DISP_FILE_SUPERSEDE:
#ifdef HAVE_BOOST
			fail_if_file_exists = false;
			create_file = true;
			flags |= std::ios::trunc;
			boost::filesystem::remove(boost::filesystem::path(real_path), boost::system::error_code());
#elif defined HAVE_LIBPTHREAD
			unlink(real_path);
			flags |= O_CREAT | O_TRUNC;
#endif
            break;

        /* Unrecognised disposition */
        default:
            return GUAC_RDP_FS_ENOSYS;

    }

    /* Create directory first, if necessary */
    if ((create_options & FILE_DIRECTORY_FILE) && (flags & O_CREAT)) {

        /* Create directory */
#ifdef HAVE_BOOST
		boost::filesystem::path p(real_path);
		boost::system::error_code ec;
		if(boost::filesystem::create_directories(p, ec))
		{
			if(ec)
			{
				guac_client_log(fs->client, GUAC_LOG_DEBUG,
					"%s: mkdir() failed: %s",
					__func__, strerror(errno));
				return guac_rdp_fs_get_errorcode(errno);
			}
			else
			{
				boost::filesystem::permissions(p, boost::filesystem::owner_all);
			}
		}
#elif defined HAVE_LIBPTHREAD
        if (mkdir(real_path, S_IRWXU)) {
            if (errno != EEXIST || (flags & O_EXCL)) {
                guac_client_log(fs->client, GUAC_LOG_DEBUG,
                        "%s: mkdir() failed: %s",
                        __func__, strerror(errno));
                return guac_rdp_fs_get_errorcode(errno);
            }
        }
		/* Unset O_CREAT and O_EXCL as directory must exist before open() */
		flags &= ~(O_CREAT | O_EXCL);
#endif
    }

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: native open: real_path=\"%s\", flags=0x%x",
            __func__, real_path, flags);

	/* Get file ID, init file */
	file_id = guac_pool_next_int(fs->file_id_pool);
	file = &(fs->files[file_id]);
	file->id = file_id;

#ifdef HAVE_BOOST
	boost::filesystem::path file_path(std::string(real_path).c_str());
	if (!(create_options & FILE_DIRECTORY_FILE))
	{
		if (fail_if_file_exists)
		{
			if (boost::filesystem::exists(file_path))
			{
				if (boost::filesystem::is_regular_file(file_path))
				{
					// File and exists
					guac_client_log(fs->client, GUAC_LOG_DEBUG,
						"%s: open() failed: %s", __func__, strerror(errno));
					return guac_rdp_fs_get_errorcode(errno);
				}
			}
		}
		else if (boost::filesystem::exists(file_path) && !boost::filesystem::is_directory(file_path))
		{
			// If the file exists, and is not a directory, we open it
			file->file_stream.reset(new boost::filesystem::fstream(std::string(real_path), flags));
		}
		else if (create_file)
		{
			// Create the file ignoring the flags since it doesnt exist
			std::ofstream(std::string(real_path));

			// Reaching here means the file does not exist, is not a directory, and needs to be created
			file->file_stream.reset(new boost::filesystem::fstream(std::string(real_path), flags));
		}
	}
#elif defined HAVE_LIBPTHREAD
    /* Open file */
    fd = open(real_path, flags, S_IRUSR | S_IWUSR);

    /* If file open failed as we're trying to write a dir, retry as read-only */
    if (fd == -1 && errno == EISDIR) {
        flags &= ~(O_WRONLY | O_RDWR);
        flags |= O_RDONLY;
        fd = open(real_path, flags, S_IRUSR | S_IWUSR);
    }

    if (fd == -1) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: open() failed: %s", __func__, strerror(errno));
        return guac_rdp_fs_get_errorcode(errno);
    }
#endif
    
#ifdef HAVE_LIBPTHREAD
	file->fd = fd;
    file->dir = NULL;
#endif
    file->dir_pattern[0] = '\0';
    file->absolute_path = strdup(normalized_path);
    file->real_path = strdup(real_path);
    file->bytes_written = 0;

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: Opened \"%s\" as file_id=%i",
            __func__, normalized_path, file_id);

#ifdef HAVE_BOOST
	WIN32_FIND_DATA data = {0};
	HANDLE h = FindFirstFile(real_path, &data);
	if (h)
	{
		FindClose(h);
		if (boost::filesystem::is_directory(real_path))
		{
			for (boost::filesystem::recursive_directory_iterator it(real_path); it != boost::filesystem::recursive_directory_iterator(); it++)
			{
				if (!boost::filesystem::is_directory(*it))
				{
					file->size += boost::filesystem::file_size(*it);
				}
			}
		}
		else
		{
			boost::system::error_code ec;
			file->size = boost::filesystem::file_size(std::string(real_path), ec);
			if (ec)
			{
				file->size = 0;
			}
		}
		auto time_caster = [](const FILETIME & time) -> uint64_t
		{
			ULARGE_INTEGER large;
			large.LowPart = time.dwLowDateTime;
			large.HighPart = time.dwHighDateTime;

			return large.QuadPart;
		};
		file->ctime = static_cast<uint64_t>(time_caster(data.ftCreationTime));
		file->mtime = static_cast<uint64_t>(time_caster(data.ftLastWriteTime));
		file->atime = static_cast<uint64_t>(time_caster(data.ftLastAccessTime));
		if (boost::filesystem::is_directory(real_path))
			file->attributes = FILE_ATTRIBUTE_DIRECTORY;
		else
			file->attributes = FILE_ATTRIBUTE_NORMAL;
	}
	else
	{
		file->size = 0;
		file->ctime = 0;
		file->mtime = 0;
		file->atime = 0;
		file->attributes = FILE_ATTRIBUTE_NORMAL;
	}

#elif defined HAVE_LIBPTHREAD
    /* Attempt to pull file information */
    if (fstat(fd, &file_stat) == 0) {

        /* Load size and times */
        file->size  = file_stat.st_size;
        file->ctime = WINDOWS_TIME(file_stat.st_ctime);
        file->mtime = WINDOWS_TIME(file_stat.st_mtime);
        file->atime = WINDOWS_TIME(file_stat.st_atime);

        /* Set type */
        if (S_ISDIR(file_stat.st_mode))
            file->attributes = FILE_ATTRIBUTE_DIRECTORY;
        else
            file->attributes = FILE_ATTRIBUTE_NORMAL;

    }

    /* If information cannot be retrieved, fake it */
    else {

        /* Init information to 0, lacking any alternative */
        file->size  = 0;
        file->ctime = 0;
        file->mtime = 0;
        file->atime = 0;
        file->attributes = FILE_ATTRIBUTE_NORMAL;

    }
#endif
    fs->open_files++;

    return file_id;

}

int guac_rdp_fs_read(guac_rdp_fs* fs, int file_id, int offset,
	void* buffer, int length) {

	int bytes_read = 0;

	guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
	if (file == NULL) {
		guac_client_log(fs->client, GUAC_LOG_DEBUG,
			"%s: Read from bad file_id: %i", __func__, file_id);
		return GUAC_RDP_FS_EINVAL;
	}

	/* Attempt read */
#ifdef HAVE_BOOST
	if (file->file_stream && file->file_stream->is_open())
	{		
		file->file_stream->seekg(offset);
		file->file_stream->read((char*)buffer, length);
		bytes_read = file->file_stream->gcount();
		file->file_stream->clear();
	}
#elif defined HAVE_LIBPTHREAD
    lseek(file->fd, offset, SEEK_SET);
    bytes_read = read(file->fd, buffer, length);
#endif
    /* Translate errno on error */
    if (bytes_read < 0)
        return guac_rdp_fs_get_errorcode(errno);
    return bytes_read;
}

int guac_rdp_fs_write(guac_rdp_fs* fs, int file_id, int offset,
        void* buffer, int length) {

    int bytes_written;

    guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
    if (file == NULL) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Write to bad file_id: %i", __func__, file_id);
        return GUAC_RDP_FS_EINVAL;
    }
    /* Attempt write */
#ifdef HAVE_BOOST
	if(file->file_stream && file->file_stream->is_open())
	{
		file->file_stream->seekp(offset);
		file->file_stream->write((char*)buffer, length);
		bytes_written = length;
	}
#elif defined HAVE_LIBPTHREAD
    lseek(file->fd, offset, SEEK_SET);
    bytes_written = write(file->fd, buffer, length);
#endif
    /* Translate errno on error */
    if (bytes_written < 0)
        return guac_rdp_fs_get_errorcode(errno);

    file->bytes_written += bytes_written;
    return bytes_written;

}

int guac_rdp_fs_rename(guac_rdp_fs* fs, int file_id,
        const char* new_path) {

    char real_path[GUAC_RDP_FS_MAX_PATH];
    char normalized_path[GUAC_RDP_FS_MAX_PATH];

    guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
    if (file == NULL) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Rename of bad file_id: %i", __func__, file_id);
        return GUAC_RDP_FS_EINVAL;
    }

    /* Normalize path, return no-such-file if invalid  */
    if (guac_rdp_fs_normalize_path(new_path, normalized_path)) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Normalization of path \"%s\" failed.",
                __func__, new_path);
        return GUAC_RDP_FS_ENOENT;
    }

    /* Translate normalized path to real path */
    __guac_rdp_fs_translate_path(fs, normalized_path, real_path);

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: Renaming \"%s\" -> \"%s\"",
            __func__, file->real_path, real_path);

#ifdef HAVE_BOOST
	boost::filesystem::path oldp(file->real_path);
	boost::filesystem::path newp(real_path);
	boost::system::error_code ec;
	boost::filesystem::rename(oldp, newp, ec);
	if(ec)
	{
		guac_client_log(fs->client, GUAC_LOG_DEBUG,
			"%s: rename() failed: \"%s\" -> \"%s\"",
			__func__, file->real_path, real_path);
		return guac_rdp_fs_get_errorcode(errno);
	}
#elif defined HAVE_LIBPTHREAD
    /* Perform rename */
    if (rename(file->real_path, real_path)) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: rename() failed: \"%s\" -> \"%s\"",
                __func__, file->real_path, real_path);
        return guac_rdp_fs_get_errorcode(errno);
    }
#endif
    return 0;

}

int guac_rdp_fs_delete(guac_rdp_fs* fs, int file_id) {

    /* Get file */
    guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
    if (file == NULL) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Delete of bad file_id: %i", __func__, file_id);
        return GUAC_RDP_FS_EINVAL;
    }
	guac_client_log(fs->client, GUAC_LOG_DEBUG, "DELETE TIME");
#ifdef HAVE_BOOST
	boost::system::error_code ec;
	if (file->file_stream)
		file->file_stream->close();
	
	boost::filesystem::remove(boost::filesystem::path(file->real_path), ec);
	for (size_t i = 0;i < GUAC_RDP_FS_MAX_FILES;i++)
	{
		if (i != file_id && fs->files[i].real_path && strcmp(fs->files[i].real_path, file->real_path) == 0)
		{
			guac_rdp_fs_close(fs, i);
		}
	}
#elif defined HAVE_LIBPTHREAD
    /* If directory, attempt removal */
    if (file->attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (rmdir(file->real_path)) {
            guac_client_log(fs->client, GUAC_LOG_DEBUG,
                    "%s: rmdir() failed: \"%s\"", __func__, file->real_path);
            return guac_rdp_fs_get_errorcode(errno);
        }
    }

    /* Otherwise, attempt deletion */
    else if (unlink(file->real_path)) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: unlink() failed: \"%s\"", __func__, file->real_path);
        return guac_rdp_fs_get_errorcode(errno);
    }
#endif
    return 0;

}

int guac_rdp_fs_truncate(guac_rdp_fs* fs, int file_id, int length) {

    /* Get file */
    guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
    if (file == NULL) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Delete of bad file_id: %i", __func__, file_id);
        return GUAC_RDP_FS_EINVAL;
    }

#ifdef HAVE_BOOST
	if (file->file_stream && file->file_stream->is_open())
	{
		boost::system::error_code ec;
		boost::filesystem::resize_file(boost::filesystem::path(file->real_path), 0, ec);
		if (ec)
		{
			guac_client_log(fs->client, GUAC_LOG_DEBUG,
				"%s: ftruncate() to %i bytes failed: \"%s\"",
				__func__, length, file->real_path);
			return guac_rdp_fs_get_errorcode(errno);
		}
		else
		{
			file->file_stream->seekp(0);
			file->file_stream->seekg(0);
		}
	}
#elif defined HAVE_LIBPTHREAD
    /* Attempt truncate */
    if (ftruncate(file->fd, length)) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: ftruncate() to %i bytes failed: \"%s\"",
                __func__, length, file->real_path);
        return guac_rdp_fs_get_errorcode(errno);
    }
#endif
    return 0;

}

void guac_rdp_fs_close(guac_rdp_fs* fs, int file_id) {

    guac_rdp_fs_file* file = guac_rdp_fs_get_file(fs, file_id);
    if (file == NULL) {
        guac_client_log(fs->client, GUAC_LOG_DEBUG,
                "%s: Ignoring close for bad file_id: %i",
                __func__, file_id);
        return;
    }

    file = &(fs->files[file_id]);

    guac_client_log(fs->client, GUAC_LOG_DEBUG,
            "%s: Closed \"%s\" (file_id=%i)",
            __func__, file->absolute_path, file_id);

    /* Close directory, if open */
#ifdef HAVE_BOOST
	if (file->file_stream && file->file_stream->is_open())
	{
		file->file_stream->close();
		file->file_stream.reset();
	}
	if (file->dir_iter)
	{
		file->dir_iter.reset();
	}
#elif defined HAVE_LIBPTHREAD
    if (file->dir != NULL)
        closedir(file->dir);

    /* Close file */
    close(file->fd);
#endif
    /* Free name */
	if(file->absolute_path)
		free(file->absolute_path);
	if(file->real_path)
		free(file->real_path);
#ifdef HAVE_BOOST
	file->absolute_path = nullptr;
	file->real_path = nullptr;
#endif

    /* Free ID back to pool */
    guac_pool_free_int(fs->file_id_pool, file_id);
    fs->open_files--;

}

const char* guac_rdp_fs_read_dir(guac_rdp_fs* fs, int file_id) {

    guac_rdp_fs_file* file;

    /* Only read if file ID is valid */
    if (file_id < 0 || file_id >= GUAC_RDP_FS_MAX_FILES)
        return NULL;
    file = &(fs->files[file_id]);
#ifdef HAVE_BOOST
	if (!file->dir_iter)
	{
		file->dir_iter.reset(new boost::filesystem::directory_iterator(file->real_path));
	}
	if ((*file->dir_iter) == boost::filesystem::directory_iterator())
	{
		return nullptr;
	}
	std::string name = (*file->dir_iter)->path().filename().string();
	(*file->dir_iter)++;
	return strdup(name.c_str());
#elif defined HAVE_LIBPTHREAD
	struct dirent* result;

    /* Open directory if not yet open, stop if error */
    if (file->dir == NULL) {
        file->dir = fdopendir(file->fd);
        if (file->dir == NULL)
            return NULL;
    }

    /* Read next entry, stop if error or no more entries */
    if ((result = readdir(file->dir)) == NULL)
        return NULL;

    /* Return filename */
    return result->d_name;
#endif
}

int guac_rdp_fs_normalize_path(const char* path, char* abs_path) {

    int i;
    int path_depth = 0;
    char path_component_data[GUAC_RDP_FS_MAX_PATH];
    const char* path_components[64];

    const char** current_path_component      = &(path_components[0]);
    const char*  current_path_component_data = &(path_component_data[0]);

    /* If original path is not absolute, normalization fails */
    if (path[0] != '\\' && path[0] != '/')
        return 1;

    /* Skip past leading slash */
    path++;

    /* Copy path into component data for parsing */
    strncpy(path_component_data, path, GUAC_RDP_FS_MAX_PATH-1);

    /* Find path components within path */
    for (i=0; i<GUAC_RDP_FS_MAX_PATH; i++) {

        /* If current character is a path separator, parse as component */
        char c = path_component_data[i];
        if (c == '/' || c == '\\' || c == 0) {

            /* Terminate current component */
            path_component_data[i] = 0;

            /* If component refers to parent, just move up in depth */
            if (strcmp(current_path_component_data, "..") == 0) {
                if (path_depth > 0)
                    path_depth--;
            }

            /* Otherwise, if component not current directory, add to list */
            else if (strcmp(current_path_component_data,   ".") != 0
                     && strcmp(current_path_component_data, "") != 0)
                path_components[path_depth++] = current_path_component_data;

            /* If end of string, stop */
            if (c == 0)
                break;

            /* Update start of next component */
            current_path_component_data = &(path_component_data[i+1]);

        } /* end if separator */

    } /* end for each character */

    /* If no components, the path is simply root */
    if (path_depth == 0) {
        strcpy(abs_path, "\\");
        return 0;
    }

    /* Ensure last component is null-terminated */
    path_component_data[i] = 0;

    /* Convert components back into path */
    for (; path_depth > 0; path_depth--) {

        const char* filename = *(current_path_component++);

        /* Add separator */
        *(abs_path++) = '\\';

        /* Copy string */
        while (*filename != 0)
            *(abs_path++) = *(filename++);

    }

    /* Terminate absolute path */
    *(abs_path++) = 0;
    return 0;

}

int guac_rdp_fs_convert_path(const char* parent, const char* rel_path, char* abs_path) {

    int i;
    char combined_path[GUAC_RDP_FS_MAX_PATH];
    char* current = combined_path;

    /* Copy parent path */
    for (i=0; i<GUAC_RDP_FS_MAX_PATH; i++) {

        char c = *(parent++);
        if (c == 0)
            break;

        *(current++) = c;

    }

    /* Add trailing slash */
    *(current++) = '\\';

    /* Copy remaining path */
    strncpy(current, rel_path, GUAC_RDP_FS_MAX_PATH-i-2);

    /* Normalize into provided buffer */
    return guac_rdp_fs_normalize_path(combined_path, abs_path);

}

guac_rdp_fs_file* guac_rdp_fs_get_file(guac_rdp_fs* fs, int file_id) {

    /* Validate ID */
    if (file_id < 0 || file_id >= GUAC_RDP_FS_MAX_FILES)
        return NULL;

    /* Return file at given ID */
    return &(fs->files[file_id]);

}

int guac_rdp_fs_matches(const char* filename, const char* pattern) {
#ifdef HAVE_BOOST
	return PathMatchSpecEx(filename, pattern, 0) != S_OK;
#elif defined HAVE_LIBPTHREAD
    return fnmatch(pattern, filename, FNM_NOESCAPE) != 0;
#endif
}

int guac_rdp_fs_get_info(guac_rdp_fs* fs, guac_rdp_fs_info* info) {

#ifdef HAVE_BOOST
	DWORD bytesPerSector, 
			sectorsPerCluster,
			numberOfFreeSectors,
			totalNumberOfClusters;
	if(!GetDiskFreeSpace(fs->drive_path, &sectorsPerCluster, &bytesPerSector, 
		&numberOfFreeSectors, &totalNumberOfClusters))
			return guac_rdp_fs_get_errorcode(errno);
	info->block_size = bytesPerSector*sectorsPerCluster;
	info->blocks_available = numberOfFreeSectors*info->block_size;
	info->blocks_total = totalNumberOfClusters*info->block_size;
	return 0;
#elif defined HAVE_LIBPTHREAD
    /* Read FS information */
    struct statvfs fs_stat;
    if (statvfs(fs->drive_path, &fs_stat))
        return guac_rdp_fs_get_errorcode(errno);

    /* Assign to structure */
    info->blocks_available = fs_stat.f_bfree;
    info->blocks_total = fs_stat.f_blocks;
    info->block_size = fs_stat.f_bsize;
    return 0;
#endif
}

int guac_rdp_fs_append_filename(char* fullpath, const char* path,
        const char* filename) {

    int i;

    /* Disallow "." as a filename */
    if (strcmp(filename, ".") == 0)
        return 0;

    /* Disallow ".." as a filename */
    if (strcmp(filename, "..") == 0)
        return 0;

    /* Copy path, append trailing slash */
    for (i=0; i<GUAC_RDP_FS_MAX_PATH; i++) {

        /*
         * Append trailing slash only if:
         *  1) Trailing slash is not already present
         *  2) Path is non-empty
         */

        char c = path[i];
        if (c == '\0') {
            if (i > 0 && path[i-1] != '/' && path[i-1] != '\\')
                fullpath[i++] = '/';
            break;
        }

        /* Copy character if not end of string */
        fullpath[i] = c;

    }

    /* Append filename */
    for (; i<GUAC_RDP_FS_MAX_PATH; i++) {

        char c = *(filename++);
        if (c == '\0')
            break;

        /* Filenames may not contain slashes */
        if (c == '\\' || c == '/')
            return 0;

        /* Append each character within filename */
        fullpath[i] = c;

    }

    /* Verify path length is within maximum */
    if (i == GUAC_RDP_FS_MAX_PATH)
        return 0;

    /* Terminate path string */
    fullpath[i] = '\0';

    /* Append was successful */
    return 1;

}

