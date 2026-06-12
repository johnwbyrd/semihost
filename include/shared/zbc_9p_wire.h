/**
 * @file zbc_9p_wire.h
 * @brief 9P2000.L wire constants for the ZBC 9p guest transport
 *
 * Single shared definition of the protocol subset the file transport
 * speaks (same anti-drift principle as zbc_protocol.h). The normative
 * opcode-to-9P mapping lives in docs/source/qemu-transports-proposal.rst.
 *
 * Message framing (all fields little-endian):
 *   size[4] type[1] tag[2] payload...
 * where size counts the entire message including itself. Strings are
 * len[2] followed by bytes (no NUL). Errors are Rlerror carrying a
 * Linux errno, which maps directly onto the RETN errno convention.
 */

#ifndef ZBC_9P_WIRE_H
#define ZBC_9P_WIRE_H

/*========================================================================
 * Message types (subset)
 *========================================================================*/

#define ZBC_9P_RLERROR    7   /* reply only */
#define ZBC_9P_TLOPEN     12
#define ZBC_9P_RLOPEN     13
#define ZBC_9P_TLCREATE   14
#define ZBC_9P_RLCREATE   15
#define ZBC_9P_TGETATTR   24
#define ZBC_9P_RGETATTR   25
#define ZBC_9P_TSETATTR   26
#define ZBC_9P_RSETATTR   27
#define ZBC_9P_TREADDIR   40
#define ZBC_9P_RREADDIR   41
#define ZBC_9P_TFSYNC     50
#define ZBC_9P_RFSYNC     51
#define ZBC_9P_TMKDIR     72
#define ZBC_9P_RMKDIR     73
#define ZBC_9P_TRENAMEAT  74
#define ZBC_9P_RRENAMEAT  75
#define ZBC_9P_TVERSION   100
#define ZBC_9P_RVERSION   101
#define ZBC_9P_TATTACH    104
#define ZBC_9P_RATTACH    105
#define ZBC_9P_TWALK      110
#define ZBC_9P_RWALK      111
#define ZBC_9P_TREAD      116
#define ZBC_9P_RREAD      117
#define ZBC_9P_TWRITE     118
#define ZBC_9P_RWRITE     119
#define ZBC_9P_TCLUNK     120
#define ZBC_9P_RCLUNK     121
#define ZBC_9P_TREMOVE    122
#define ZBC_9P_RREMOVE    123

/*========================================================================
 * Protocol constants
 *========================================================================*/

#define ZBC_9P_VERSION_STR   "9P2000.L"
#define ZBC_9P_NOTAG         0xFFFFU      /* tag for Tversion */
#define ZBC_9P_NOFID         0xFFFFFFFFUL /* afid for Tattach */
#define ZBC_9P_MAXWELEM      16           /* max names per Twalk */
#define ZBC_9P_HDR_SIZE      7            /* size[4] type[1] tag[2] */

/* qid: type[1] version[4] path[8] */
#define ZBC_9P_QID_SIZE      13
#define ZBC_9P_QTDIR         0x80

/* Tgetattr request mask bits (subset). BASIC selects mode, nlink,
 * uid, gid, rdev, atime, mtime, ctime, ino, size, blocks -- everything
 * the SYS_STAT 48-byte response layout needs. */
#define ZBC_9P_GETATTR_SIZE   0x00000200UL
#define ZBC_9P_GETATTR_BASIC  0x000007FFUL

/* Tsetattr request mask bits (subset). Only SIZE is used today, for
 * the ftruncate path. */
#define ZBC_9P_SETATTR_SIZE   0x00000008UL

/*
 * Rgetattr payload layout (after the 7-byte header):
 *   valid[8] qid[13] mode[4] uid[4] gid[4] nlink[8] rdev[8] size[8]
 *   blksize[8] blocks[8] atime[16] mtime[16] ctime[16] btime[16]
 *   gen[8] data_version[8]                            -- 153 bytes total
 */
#define ZBC_9P_RGETATTR_SIZE_OFFSET  49  /* of the size field, in payload */
#define ZBC_9P_RGETATTR_PAYLOAD_LEN  153

/*========================================================================
 * Tlopen / Tlcreate flags (Linux open(2) values, per 9P2000.L)
 *========================================================================*/

#define ZBC_9P_O_RDONLY   0x0000UL
#define ZBC_9P_O_WRONLY   0x0001UL
#define ZBC_9P_O_RDWR     0x0002UL
#define ZBC_9P_O_CREAT    0x0040UL
#define ZBC_9P_O_TRUNC    0x0200UL
#define ZBC_9P_O_APPEND   0x0400UL

#endif /* ZBC_9P_WIRE_H */
