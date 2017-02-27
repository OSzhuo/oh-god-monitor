#ifndef _OG_DEFS_H_
#define _OG_DEFS_H_

#include <stdint.h>

#define	DEBUG			7

#ifdef PATH_MAX
#define IBIG_PATH_MAX		PATH_MAX
#else
#define IBIG_PATH_MAX		4096
#endif
/* max name length(include nul) */
#define IBIG_NM_MAX		256

/*path///name///type///err///mtime///size(in bytes)///_///_*/
#define OG_LINE_LEN		(IBIG_PATH_MAX+64)

#define RUNTIME_LIST		"/ibig/ibig_monitor/runtime.list"
#define PIC_RUN			"/ibig/ibig_monitor/pic.runtime"

#define OG_TRUNC_HEAD		"/tmp/mnt"

#define OG_SOCK_FILE		"/tmp/og_monitor.sock"

/*action will be wrt the value*/
#define ACT_INIT		1
#define ACT_NEW			2
#define ACT_DEL			3
#define ACT_MV_F		4
#define ACT_MV_T		5
#define ACT_MODIFY		6

#define ACT_INIT_OK		9

/*type will be one of the following value*/
#define TYPE_D			'D'

#define TYPE_F			'F'
#define TYPE_A			'A'
#define TYPE_P			'P'
#define TYPE_V			'V'
#define TYPE_T			'T'
#define TYPE_O			'O'

/* if use readdir() instead of nftw(), these field may be remove, all status of file/dir read by og_record(or all by og_init/og_watch) */
/* like size, mtime, err, type... */
typedef struct _og_unit_t{
	int8_t		action;		/* action */
	int8_t		err;		/* if file access err */
	char		type;		/* file type */
	off_t		size;		/* total size, in bytes */
	time_t		mtime;		/* time of last modification */
	int		wd;		/* if wd<0, this unit must not DIR		\
					in ACT_INIT, wd means self			\
					in other ACT, wd means parent's wd	*/
	uint32_t	base_or_cookie;	/* only ACT_INIT base is the offset of filename	\
					in ACT_MV_F/T, it is the cookie */
/*	int	base_or_selfwd;	only in ACT_INIT base is the offset of the filename	\
					in other action, it is itself's wd */
	int		len;		/* the path length() (include '\0') */
	char		path[0];	/* file path(include /tmp/mnt/USB-disk-*) */
} _og_unit;

typedef struct og_file_unit_st {
	int8_t	err;		/* if file access err */
	char	type;		/* file type */
	off_t	size;		/* total size, in bytes */
	time_t	mtime;		/* time of last modification */
	int	wd;		/* if wd<0, this unit must not be DIR */
	int	len;		/* the path length() (include '\0') */
	char	name[0];	/* file name (include nul('\0')) */
} og_file_unit;

#define OG_SOCK_GET		1
#define OG_SOCK_START		2
#define OG_SOCK_LINE		3
#define OG_SOCK_END		4
#define OG_SOCK_INIT		5

/* used for write list to file */
typedef struct og_sock_node_st {
	int	action;		/* sock action */
	int	type;
	int	len;		/* leght of line, include '\0' */
	char	line[0];	/* this line */
} og_sock_node;

#define NM_LEN			512

/*flag to write type*/
#define EXT_FLAG_ALL		"PAVTO"
#define EXT_FLAG_A		"A"
#define EXT_FLAG_P		"P"
#define EXT_FLAG_V		"V"
#define EXT_FLAG_T		"T"
#define EXT_FLAG_O		"O"

/*types must lower a b c*/
#define MAX_EXT_LEN		8

#define EXT_LIST_P		"jpg", "png", "jpe", "jpeg", "gif", "bmp", "svg", "tif", "tiff", "wbmp", "fax", "ico", "psd"
#define EXT_LIST_A		"aac", "ape", "au", "amr", "acp", "aifc", "au", "flac", "flc", "mid", "mp1", "mp2", "mp3", "mpga", "m4a", "ram", "ra", "rmi", "rmm", "snd", "wav", "wax", "wma", "ogg", "xpl"
#define EXT_LIST_V		"asf", "asx", "avi", "ivf", "m2v", "m4e", "mlv", "movie", "mpa", "mp2v", "mpe", "mpg", "mpeg", "mpv", "mps", "mpv2", "rv", "wm", "wmx", "wvx", "wmv", "mp4", "mov", "mkv", "rmvb", "rm"
#define EXT_LIST_T		"pdf", "doc", "docx", "txt", "ppt", "pptx", "mobi", "epub", "azw", "azw3", "html", "xls", "xlsx", "htm", "xml"

#endif
