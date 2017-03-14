#include <errno.h>

#include <stdio.h>

static int _ftw_errno;


int _travel_all_files(const char *path, int nopenfd)
{

}

/**
 * if fn() return not 0, the tree walk stop.
 * 
 */
int _walk_file_tree_recursively(const char *path, int (*fn)(const char *path))
{
	DIR *dir;
	char *my_path;
	_wft_errno = 0;
	dir = opendir(path);
	if(!dir){
		if(ENOTDIR == errno){
			/* call fun() */
			return (*fn)(path, );
		}else{
			_ftw_errno = errno;
			return 0;
		}
	}

/*the path can not end with '/' */
	if(path[strlen(path)-1] == '/'){
		asprintf( &my_path, "%s", path );
	}else{
		my_path = (char *)path;
	}

	return 0;
}

int inotifytools_watch_recursively_with_exclude( char const * path, int events, char const ** exclude_list ) {
	DIR * dir;
	char * my_path;
	error = 0;
	dir = opendir( path );
	if ( !dir ) {
		// If not a directory, don't need to do anything special
		if ( errno == ENOTDIR ) {
			return inotifytools_watch_file( path, events );
		} else {
			error = errno;
			return 0;
		}
	}

	if ( path[strlen(path)-1] != '/' ) {
		nasprintf( &my_path, "%s/", path );
	}
	else {
		my_path = (char *)path;
	}

	static struct dirent * ent;
	char * next_file;
	static struct stat64 my_stat;
	ent = readdir( dir );
	// Watch each directory within this directory
	while ( ent ) {
		if ( (0 != strcmp( ent->d_name, "." )) && (0 != strcmp( ent->d_name, ".." )) ) {
			nasprintf(&next_file,"%s%s", my_path, ent->d_name);
			if ( -1 == lstat64( next_file, &my_stat ) ) {
				error = errno;
				free( next_file );
				if ( errno != EACCES ) {
					error = errno;
					if ( my_path != path ) free( my_path );
					closedir( dir );
					return 0;
				}
			} else if ( S_ISDIR( my_stat.st_mode ) && !S_ISLNK( my_stat.st_mode )) {
				free( next_file );
				nasprintf(&next_file,"%s%s/", my_path, ent->d_name);
				static int status;
				status = inotifytools_watch_recursively_with_exclude( next_file, events, exclude_list );
				// For some errors, we will continue.
				if ( !status && (EACCES != error) && (ENOENT != error) && (ELOOP != error) ) {
					free( next_file );
					if ( my_path != path ) free( my_path );
					closedir( dir );
					return 0;
				}
				free( next_file );
			} // if isdir and not islnk
			else {
				free( next_file );
			}
		}
		ent = readdir( dir );
		error = 0;
	}

	closedir( dir );

	int ret = inotifytools_watch_file( my_path, events );
	if ( my_path != path ) free( my_path );
        return ret;
}
