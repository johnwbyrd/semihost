/*
 * ZBC Semihosting Opcode Metadata Table
 *
 * Single source of truth for syscall signatures.
 * Used by both client (to build requests) and host (to parse/dispatch).
 */

#include "zbc_semihost.h"

/*
 * Opcode table entries.
 *
 * Each entry describes:
 * - What chunks to emit in the request (params[])
 * - What the response contains (resp_type)
 * - Which args[] slot receives response data (resp_dest)
 * - Which args[] slot has the length limit (resp_len_slot)
 *
 * Chunk types in params[]:
 *   ZBC_CHUNK_NONE     - unused slot
 *   ZBC_CHUNK_PARM_INT - emit PARM chunk with args[slot] as signed int
 *   ZBC_CHUNK_PARM_UINT- emit PARM chunk with args[slot] as unsigned int
 *   ZBC_CHUNK_DATA_PTR - emit DATA chunk, ptr from args[slot], len from
 * args[slot+1] ZBC_CHUNK_DATA_STR - emit DATA chunk, null-terminated string
 * from args[slot] ZBC_CHUNK_DATA_BYTE- emit DATA chunk, single byte from
 * *(uint8_t*)args[slot]
 */

static const zbc_opcode_entry_t zbc_opcode_table[] = {
    /*
     * SH_SYS_OPEN (0x01)
     * ARM args: {path_ptr, mode, path_len}
     * Request: DATA(path, len=args[2]), PARM(mode=args[1]), PARM(len=args[2])
     * Response: int (fd or -1)
     */
    {SH_SYS_OPEN,
     3,
     {{ZBC_CHUNK_DATA_PTR, 0, 2},  /* DATA: ptr=args[0], len=args[2] */
      {ZBC_CHUNK_PARM_INT, 1, 0},  /* PARM: args[1] (mode) */
      {ZBC_CHUNK_PARM_UINT, 2, 0}, /* PARM: args[2] (len) */
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_CLOSE (0x02)
     * ARM args: {fd}
     * Request: PARM(fd)
     * Response: int (0 or -1)
     */
    {SH_SYS_CLOSE,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_WRITEC (0x03)
     * ARM args: {char_ptr}
     * Request: DATA(1 byte from *args[0])
     * Response: none (void)
     */
    {SH_SYS_WRITEC,
     1,
     {{ZBC_CHUNK_DATA_BYTE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_WRITE0 (0x04)
     * ARM args: {str_ptr}
     * Request: DATA(null-terminated string)
     * Response: none (void)
     */
    {SH_SYS_WRITE0,
     1,
     {{ZBC_CHUNK_DATA_STR, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_WRITE (0x05)
     * ARM args: {fd, buf_ptr, count}
     * Request: PARM(fd), DATA(buf, count), PARM(count)
     * Response: int (bytes NOT written)
     */
    {SH_SYS_WRITE,
     3,
     {{ZBC_CHUNK_PARM_INT, 0, 0},  /* fd */
      {ZBC_CHUNK_DATA_PTR, 1, 2},  /* ptr=args[1], len=args[2] */
      {ZBC_CHUNK_PARM_UINT, 2, 0}, /* count */
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_READ (0x06)
     * ARM args: {fd, buf_ptr, count}
     * Request: PARM(fd), PARM(count)
     * Response: int (bytes NOT read), DATA copied to args[1], max args[2]
     */
    {
        SH_SYS_READ,
        3,
        {{ZBC_CHUNK_PARM_INT, 0, 0},  /* fd */
         {ZBC_CHUNK_PARM_UINT, 2, 0}, /* count */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        1,
        2 /* copy DATA to args[1], max len from args[2] */
    },

    /*
     * SH_SYS_READC (0x07)
     * ARM args: none
     * Request: (no params)
     * Response: int (char or -1)
     */
    {SH_SYS_READC,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_ISERROR (0x08)
     * ARM args: {status}
     * Request: PARM(status)
     * Response: int (0 or 1)
     */
    {SH_SYS_ISERROR,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_ISTTY (0x09)
     * ARM args: {fd}
     * Request: PARM(fd)
     * Response: int (1=tty, 0=not)
     */
    {SH_SYS_ISTTY,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_SEEK (0x0A)
     * ARM args: {fd, pos}
     * Request: PARM(fd), PARM(pos)
     * Response: int (0 or -1)
     */
    {SH_SYS_SEEK,
     2,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_FLEN (0x0C)
     * ARM args: {fd}
     * Request: PARM(fd)
     * Response: int (length or -1)
     */
    {SH_SYS_FLEN,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_TMPNAM (0x0D)
     * ARM args: {buf_ptr, id, maxlen}
     * Request: PARM(id), PARM(maxlen)
     * Response: int (0 or -1), DATA copied to args[0], max args[2]
     */
    {
        SH_SYS_TMPNAM,
        3,
        {{ZBC_CHUNK_PARM_INT, 1, 0}, /* id */
         {ZBC_CHUNK_PARM_INT, 2, 0}, /* maxlen */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        0,
        2 /* copy DATA to args[0], max len from args[2] */
    },

    /*
     * SH_SYS_REMOVE (0x0E)
     * ARM args: {path_ptr, path_len}
     * Request: DATA(path), PARM(len)
     * Response: int (0 or -1)
     */
    {SH_SYS_REMOVE,
     2,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},  /* ptr=args[0], len=args[1] */
      {ZBC_CHUNK_PARM_UINT, 1, 0}, /* len */
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_RENAME (0x0F)
     * ARM args: {old_ptr, old_len, new_ptr, new_len}
     * Request: DATA(old), PARM(old_len), DATA(new), PARM(new_len)
     * Response: int (0 or -1)
     */
    {SH_SYS_RENAME,
     4,
     {
         {ZBC_CHUNK_DATA_PTR, 0, 1},  /* old: ptr=args[0], len=args[1] */
         {ZBC_CHUNK_PARM_UINT, 1, 0}, /* old_len */
         {ZBC_CHUNK_DATA_PTR, 2, 3},  /* new: ptr=args[2], len=args[3] */
         {ZBC_CHUNK_PARM_UINT, 3, 0}  /* new_len */
     },
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_CLOCK (0x10)
     * ARM args: none
     * Request: (no params)
     * Response: int (centiseconds)
     */
    {SH_SYS_CLOCK,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_TIME (0x11)
     * ARM args: none
     * Request: (no params)
     * Response: int (seconds since epoch)
     */
    {SH_SYS_TIME,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_SYSTEM (0x12)
     * ARM args: {cmd_ptr, cmd_len}
     * Request: DATA(cmd), PARM(len)
     * Response: int (exit code)
     */
    {SH_SYS_SYSTEM,
     2,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_ERRNO (0x13)
     * ARM args: none
     * Request: (no params)
     * Response: int (errno value)
     */
    {SH_SYS_ERRNO,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_GET_CMDLINE (0x15)
     * ARM args: {buf_ptr, size}
     * Request: PARM(size)
     * Response: int (0 or -1), DATA copied to args[0], max args[1]
     */
    {
        SH_SYS_GET_CMDLINE,
        2,
        {{ZBC_CHUNK_PARM_INT, 1, 0}, /* size */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        0,
        1 /* copy DATA to args[0], max len from args[1] */
    },

    /*
     * SH_SYS_HEAPINFO (0x16)
     * ARM args: {block_ptr}
     * Request: (no params)
     * Response: 4 PARM chunks (heap_base, heap_limit, stack_base, stack_limit)
     */
    {
        SH_SYS_HEAPINFO,
        1,
        {{ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_HEAPINFO,
        0,
        0 /* special handling for 4 pointers */
    },

    /*
     * SH_SYS_EXIT (0x18)
     * ARM args: {reason, subcode} (but often just {ADP_Stopped_*})
     * Request: PARM(reason), PARM(subcode)
     * Response: none (does not return)
     */
    {SH_SYS_EXIT,
     2,
     {{ZBC_CHUNK_PARM_UINT, 0, 0},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_EXIT_EXTENDED (0x20)
     * ARM args: {reason, subcode}
     * Request: PARM(reason), PARM(subcode)
     * Response: none (does not return)
     */
    {SH_SYS_EXIT_EXTENDED,
     2,
     {{ZBC_CHUNK_PARM_UINT, 0, 0},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_ELAPSED (0x30)
     * ARM args: {tick_ptr}
     * Request: (no params)
     * Response: 64-bit tick count in DATA, copied to args[0]
     */
    {
        SH_SYS_ELAPSED,
        1,
        {{ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_ELAPSED,
        0,
        0 /* special: 8 bytes to args[0] */
    },

    /*
     * SH_SYS_TICKFREQ (0x31)
     * ARM args: none
     * Request: (no params)
     * Response: int (ticks per second)
     */
    {SH_SYS_TICKFREQ,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_TIMER_CONFIG (0x32)
     * Configure periodic timer interrupt.
     * args[0]: rate_hz (uint32) - timer frequency in Hz, 0 = disable
     * Request: PARM[0] = rate_hz
     * Response: int (0 = success, negative = error)
     */
    {SH_SYS_TIMER_CONFIG,
     1,
     {{ZBC_CHUNK_PARM_UINT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_OPENDIR (0x80) -- Linux extension
     * Guest args: {path_ptr, path_len}
     * Request: DATA(path, len=args[1]), PARM(path_len)
     * Response: int (dir handle >= 0, or -1)
     */
    {SH_SYS_OPENDIR,
     2,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},  /* DATA: ptr=args[0], len=args[1] */
      {ZBC_CHUNK_PARM_UINT, 1, 0}, /* PARM: path_len */
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_READDIR (0x81) -- Linux extension
     * Guest args: {dir_handle, buf_ptr, buf_size}
     * Request: PARM(dir_handle), PARM(buf_size)
     * Response: int (bytes written, 0 = end of directory, -1 = error),
     * DATA copied to args[1] with max len from args[2].
     */
    {
        SH_SYS_READDIR,
        3,
        {{ZBC_CHUNK_PARM_INT, 0, 0},  /* dir_handle */
         {ZBC_CHUNK_PARM_UINT, 2, 0}, /* buf_size hint */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        1, /* copy DATA to args[1] */
        2  /* max len from args[2] */
    },

    /*
     * SH_SYS_CLOSEDIR (0x82) -- Linux extension
     * Guest args: {dir_handle}
     * Request: PARM(dir_handle)
     * Response: int (0 or -1)
     */
    {SH_SYS_CLOSEDIR,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_STAT (0x83) -- Linux extension
     * Guest args: {path_ptr, path_len, stat_buf_ptr, 48}
     * Request: DATA(path, len=args[1]), PARM(path_len)
     * Response: int (0 or -1), 48-byte stat struct in DATA, copied to
     * args[2] with max len from args[3] (must be SH_STAT_BUF_SIZE=48).
     */
    {
        SH_SYS_STAT,
        4,
        {{ZBC_CHUNK_DATA_PTR, 0, 1},  /* DATA: ptr=args[0], len=args[1] */
         {ZBC_CHUNK_PARM_UINT, 1, 0}, /* PARM: path_len */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        2, /* copy DATA to args[2] (stat buffer) */
        3  /* max len from args[3], must equal SH_STAT_BUF_SIZE */
    },

    /*
     * SH_SYS_FSTAT (0x84) -- Linux extension
     * Guest args: {fd, stat_buf_ptr, 48}
     * Request: PARM(fd)
     * Response: int (0 or -1), 48-byte stat struct in DATA, copied to
     * args[1] with max len from args[2] (must be SH_STAT_BUF_SIZE=48).
     */
    {
        SH_SYS_FSTAT,
        3,
        {{ZBC_CHUNK_PARM_INT, 0, 0},  /* PARM: fd */
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        1, /* copy DATA to args[1] (stat buffer) */
        2  /* max len from args[2], must equal SH_STAT_BUF_SIZE */
    },

    /*
     * SH_SYS_MKDIR (0x85) -- Linux extension
     * Guest args: {path_ptr, path_len, mode}
     * Request: DATA(path, len=args[1]), PARM(path_len), PARM(mode)
     * Response: int (0 or -1)
     */
    {SH_SYS_MKDIR,
     3,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},  /* DATA: path */
      {ZBC_CHUNK_PARM_UINT, 1, 0}, /* PARM: path_len */
      {ZBC_CHUNK_PARM_UINT, 2, 0}, /* PARM: mode */
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_RMDIR (0x86) -- Linux extension
     * Guest args: {path_ptr, path_len}
     * Request: DATA(path, len=args[1]), PARM(path_len)
     * Response: int (0 or -1)
     */
    {SH_SYS_RMDIR,
     2,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},  /* DATA: path */
      {ZBC_CHUNK_PARM_UINT, 1, 0}, /* PARM: path_len */
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_FTRUNCATE (0x87) -- Linux extension
     * Guest args: {fd, length_ptr, 8}
     * Request: PARM(fd), DATA(length, 8 bytes little-endian, len=args[2])
     * Response: int (0 or -1)
     *
     * The 8-byte DATA chunk carries the 64-bit length so a 16- or 32-bit
     * guest can still truncate to file sizes beyond its own pointer
     * width.
     */
    {SH_SYS_FTRUNCATE,
     3,
     {{ZBC_CHUNK_PARM_INT, 0, 0},  /* PARM: fd */
      {ZBC_CHUNK_DATA_PTR, 1, 2},  /* DATA: 8-byte length */
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_FSYNC (0x88) -- Linux extension
     * Guest args: {fd}
     * Request: PARM(fd)
     * Response: int (0 or -1)
     */
    {SH_SYS_FSYNC,
     1,
     {{ZBC_CHUNK_PARM_INT, 0, 0},  /* PARM: fd */
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_READC_POLL (0x89) -- Linux extension
     * Guest args: none
     * Request: (no params)
     * Response: int (0-255 char on success, -1 if no character available;
     * the -1 is not an error, just an empty poll)
     */
    {SH_SYS_READC_POLL,
     0,
     {{ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_LINK (0x8A) -- Linux extension (optional)
     * Guest args: {old_ptr, old_len, new_ptr, new_len}
     * Request: DATA(old), PARM(old_len), DATA(new), PARM(new_len)
     * Response: int (0 or -1). Same wire shape as SH_SYS_RENAME.
     */
    {SH_SYS_LINK,
     4,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_DATA_PTR, 2, 3},
      {ZBC_CHUNK_PARM_UINT, 3, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_SYMLINK (0x8B) -- Linux extension (optional)
     * Guest args: {target_ptr, target_len, linkpath_ptr, linkpath_len}
     * Request: DATA(target), PARM(target_len), DATA(linkpath), PARM(linkpath_len)
     * Response: int (0 or -1).
     */
    {SH_SYS_SYMLINK,
     4,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_DATA_PTR, 2, 3},
      {ZBC_CHUNK_PARM_UINT, 3, 0}},
     ZBC_RESP_INT,
     0,
     0},

    /*
     * SH_SYS_READLINK (0x8C) -- Linux extension (optional)
     * Guest args: {path_ptr, path_len, buf_ptr, buf_size}
     * Request: DATA(path), PARM(path_len), PARM(buf_size)
     * Response: int (bytes written to target buf; -1 on error). The
     * symlink target bytes are returned as DATA and copied to args[2]
     * with max length args[3]. The bytes are NOT NUL-terminated; the
     * caller relies on the int result for the length, matching POSIX
     * readlink(2).
     */
    {SH_SYS_READLINK,
     4,
     {{ZBC_CHUNK_DATA_PTR, 0, 1},
      {ZBC_CHUNK_PARM_UINT, 1, 0},
      {ZBC_CHUNK_PARM_UINT, 3, 0},
      {ZBC_CHUNK_NONE, 0, 0}},
     ZBC_RESP_DATA,
     2, /* copy DATA to args[2] */
     3  /* max len from args[3] */
    },

    /*
     * SH_SYS_LSTAT (0x8D) -- Linux extension (optional)
     * Guest args: {path_ptr, path_len, stat_buf_ptr, 48}
     * Request: DATA(path), PARM(path_len)
     * Response: int (0 or -1), 48-byte stat struct in DATA copied to
     * args[2]. Wire-identical to SH_SYS_STAT; the difference is that
     * lstat() does not traverse a terminal symlink and reports the
     * link's own metadata.
     */
    {
        SH_SYS_LSTAT,
        4,
        {{ZBC_CHUNK_DATA_PTR, 0, 1},
         {ZBC_CHUNK_PARM_UINT, 1, 0},
         {ZBC_CHUNK_NONE, 0, 0},
         {ZBC_CHUNK_NONE, 0, 0}},
        ZBC_RESP_DATA,
        2,
        3
    },

    /* End marker */
    {0, 0, {{ZBC_CHUNK_NONE, 0, 0}}, ZBC_RESP_INT, 0, 0}};

/*
 * Look up opcode in table.
 * Returns pointer to entry, or NULL if not found.
 */
const zbc_opcode_entry_t *zbc_opcode_lookup(int opcode) {
  const zbc_opcode_entry_t *p;

  for (p = zbc_opcode_table; p->opcode != 0 || p->arg_count != 0; p++) {
    if (p->opcode == opcode) {
      return p;
    }
  }
  return (const zbc_opcode_entry_t *)0;
}

/*
 * Get number of entries in opcode table (excluding end marker).
 */
int zbc_opcode_count(void) {
  const zbc_opcode_entry_t *p;
  int count = 0;

  for (p = zbc_opcode_table; p->opcode != 0 || p->arg_count != 0; p++) {
    count++;
  }
  return count;
}
