#include <fs/namespace.h>
#ifdef WIN32
#include <windows.h>
#include <osdep.h>
#elif MACOS
#include <osdep.h>
#else
#include <ps/ps.h>
#include <ps/lock.h>
#include <config.h>
#include <lib/klib.h>
#include <fs/mount.h>
#include <int/timer.h>
#include <syscall/unistd.h>
#endif

static INODE fs_lookup_inode(char* path);
static unsigned fs_get_free_fd(INODE node);
static INODE fs_get_fd(unsigned id);
static void* fs_clear_fd(unsigned fd, int* isdir);

static unsigned fs_dir_get_free_fd(DIR node);
static DIR fs_dir_get_fd(unsigned id);

unsigned fs_open(char* path)
{
    INODE node = fs_lookup_inode( path);
	DIR dir = 0;
    unsigned fd;

    if (!node) {
        return MAX_FD;
    }

	if (S_ISDIR(vfs_get_mode(node))){
		dir = vfs_open_dir(node);
		fd = fs_dir_get_free_fd(dir);
		vfs_free_inode(node);
		if (fd == MAX_FD) {
			vfs_close_dir(dir);
		}
	}else{
		fd = fs_get_free_fd(node);
		if (fd == MAX_FD) {
			vfs_free_inode(node);
		}
	}


    return fd;
}

void fs_close(unsigned int fd)
{
    INODE node = 0;
	DIR   dir = 0;
	int isdir;
	void* ret = fs_clear_fd(fd,&isdir);
	if (isdir) {
		dir = ret;
		if (dir) {
			vfs_close_dir(dir);
		}
	}else{
		node = ret;
		if (node) {
			vfs_free_inode(node); 
		}
	}
}

unsigned fs_read(unsigned fd, unsigned offset, void* buf, unsigned len)
{
    INODE node = fs_get_fd(fd);
    unsigned ret;

    if (!node) {
        return 0xffffffff;
    }

    ret = vfs_read_file(node, offset,buf,len);
    return ret;
}

unsigned fs_write(unsigned fd, unsigned offset, void* buf, unsigned len)
{
    INODE node = fs_get_fd(fd);
    unsigned ret;

    if (!node) {
        return 0xffffffff;
    }

    ret = vfs_write_file(node, offset,buf,len);
    return ret;
}

int fs_create(char* path, unsigned mode)
{  
	DIR dir;
	char dir_name[260] = { 0 };
	char* t;
	struct stat s;
	int stat_status;

	strcpy(dir_name, path);
	t = strrchr(dir_name, '/');
	if (!t)
		return 0;

	*t = '\0';
	t++;

	if (*dir_name == '\0'){
		dir = fs_opendir("/");
		stat_status = fs_stat("/", &s);
	}
	else{
		dir = fs_opendir(dir_name);
		stat_status = fs_stat(dir_name, &s);
	}

	if (!dir)
		return 0;

	if (stat_status == -1 || !S_ISDIR(s.st_mode)){
		fs_closedir(dir);
		return 0;
	}

	if (fs_stat(path, &s) != -1){
		fs_closedir(dir);
		return 0;
	}

	vfs_add_dir_entry(dir, mode, t);


	fs_closedir(dir);
    return 1;
}

int fs_delete(char* path)
{  
	DIR dir;
	char dir_name[260] = { 0 };
	char* t;
	struct stat s;
	int stat_status;

	strcpy(dir_name, path);
	t = strrchr(dir_name, '/');
	if (!t)
		return 0;

	*t = '\0';
	t++;

	if (*dir_name == '\0'){
		dir = fs_opendir("/");
		stat_status = fs_stat("/", &s);
	}
	else{
		dir = fs_opendir(dir_name);
		stat_status = fs_stat(dir_name, &s);
	}

	if (!dir)
		return 0;

	if (stat_status == -1 || !S_ISDIR(s.st_mode)){
		fs_closedir(dir);
		return 0;
	}

	if (fs_stat(path, &s) == -1){
		fs_closedir(dir);
		return 0;
	}

	vfs_del_dir_entry(dir, t);


	fs_closedir(dir);
	return 1;
}

int fs_rename(char* path, char* new)
{  
    UNIMPL;
    return 0;
}

int fs_stat(char* path, struct stat* s)
{
    INODE node = 0;
    int ret;
	
	node = fs_lookup_inode(path);
    if (!node) {
        return -1;
    }

    ret = vfs_copy_stat(node,s);
    vfs_free_inode(node);
    return ret;
}

DIR fs_opendir(char* path)
{
    INODE node = fs_lookup_inode(path);
    DIR ret;
    if (!node) {
        return 0;
    }
	if (!S_ISDIR(vfs_get_mode(node))){
		return 0;
	}

    ret = vfs_open_dir(node);
	vfs_free_inode(node);
	return ret;
}

int fs_readdir(DIR dir, char* name, unsigned* mode)
{
    INODE node = 0;

    if (!name || !mode) {
        return 0;
    }

    if (!dir) {
        *name = '\0';
        *mode = 0;
        return 0;
    }

    node = vfs_read_dir(dir);
	if (!node){
		*name = '\0';
		*mode = 0;
		return 0;
	}
	strcpy(name, vfs_get_name(node));
    *mode = vfs_get_mode(node);
    vfs_free_inode(node);
	return 1;
}

int sys_readdir(unsigned fd, struct dirent* entry)
{
	DIR dir = fs_dir_get_fd(fd);
    int ret = fs_readdir(dir, entry->d_name, &entry->d_mode);
    if (ret) {
        entry->d_namlen = strlen(entry->d_name); 
        entry->d_ino = 0;
    }
    return ret;
}

void fs_closedir(DIR dir)
{
    if (!dir) {
        return;
    }

    vfs_close_dir(dir);
}


static INODE fs_get_dirent_node(INODE node, char* name)
{
	DIR dir = 0;
	INODE entry = 0;
	int found = 0;

	if (!S_ISDIR(vfs_get_mode(node)))
	  return 0;
	
	dir = vfs_open_dir(node);
	entry = vfs_read_dir(dir);
	while(entry){
		if ( !strcmp(name, vfs_get_name(entry)) ){
			found = 1;
		  break;
		}
		
		vfs_free_inode(entry);
		entry = vfs_read_dir(dir);
	}

	vfs_close_dir(dir);
	if (found) {
		return entry;
	}else{
		return 0;
	}
}

static INODE fs_lookup_inode(char* path)
{
    struct filesys_type* type = (void*)0;
	INODE root;
	char parent[256];
	char* tmp;
	if (!path || !*path)
	  return 0;
	
	// find root file system
	type = mount_lookup("/");
	if (!type)
	  return 0;
	
	root = vfs_get_root(type);
	if (!strcmp(path, "/")){
		return root;
	}
	
	strcpy(parent, path);
	tmp = parent;

	// FIXME
	// check mount point every time takes too much effert
	// linux will cache dirent with mount point info attached
	// we ignore different mount point now, fix it later
	//
	do{
		INODE p;
		INODE node; 
		char* slash = 0;

		tmp++;
		p = root;

		slash = strchr(tmp, '/');
		while (slash){
			*slash = '\0';
			node = fs_get_dirent_node(p, tmp);
			vfs_free_inode(p);
			if (!node)
			  return 0;
			p = node;
			tmp = slash+1;
			*slash = '/';
			slash = strchr(tmp, '/');
		}

		node = fs_get_dirent_node(p, tmp);
		vfs_free_inode(p);
		return node;
	}while(0);
}

static unsigned fs_get_free_fd(INODE node)
{
    task_struct* cur = CURRENT_TASK();
    int i = 0;

    sema_wait(&cur->fd_lock);

    for (i = 0; i < MAX_FD; i++) {
        if (cur->fds[i].flag == 0) {
            cur->fds[i].file = node;
			cur->fds[i].file_off = 0;
			cur->fds[i].flag |= fd_flag_used;
            break;
        }
    }

    sema_trigger(&cur->fd_lock);

    return i;

}

static INODE fs_get_fd(unsigned fd)
{
    task_struct* cur = CURRENT_TASK();
    INODE node;

    if (fd >= MAX_FD) {
        return 0;
    }


    sema_wait(&cur->fd_lock);
	if (cur->fds[fd].flag & fd_flag_isdir) {
		node = 0;
	}else{
		node = cur->fds[fd].file;
	}
    sema_trigger(&cur->fd_lock);

    return node;
}

static unsigned fs_dir_get_free_fd(DIR node)
{
    task_struct* cur = CURRENT_TASK();
    int i = 0;

    sema_wait(&cur->fd_lock);

    for (i = 0; i < MAX_FD; i++) {
        if (cur->fds[i].flag == 0) {
            cur->fds[i].dir = node;
			cur->fds[i].file_off = 0;
			cur->fds[i].flag |= fd_flag_used;
			cur->fds[i].flag |= fd_flag_isdir;
            break;
        }
    }

    sema_trigger(&cur->fd_lock);

    return i;
}

static DIR fs_dir_get_fd(unsigned fd)
{
    task_struct* cur = CURRENT_TASK();
    DIR node;

    if (fd >= MAX_FD) {
        return 0;
    }


    sema_wait(&cur->fd_lock);
	if (!(cur->fds[fd].flag & fd_flag_isdir)) {
		node = 0;
	}else{
		node = cur->fds[fd].dir;
	}
    sema_trigger(&cur->fd_lock);

    return node;
}

static void* fs_clear_fd(unsigned fd, int* isdir)
{
    task_struct* cur = CURRENT_TASK();
    INODE node;

    if (fd >= MAX_FD) {
        return 0;
    }

    sema_wait(&cur->fd_lock);
	*isdir = (cur->fds[fd].flag & fd_flag_isdir);
    node = cur->fds[fd].file;
    cur->fds[fd].file = 0;
	cur->fds[fd].file_off = 0;
	cur->fds[fd].flag = 0;
    sema_trigger(&cur->fd_lock);

    return node;

}

void fs_flush(char* filesys)
{
	struct filesys_type* type = mount_lookup("/");
}

#ifdef TEST_NS
static void list_dir(char* name, int depth)
{
	char file[32];
	unsigned mode;
	DIR dir = fs_opendir(name);
	while (fs_readdir(dir, file, &mode)){
		int i = 0;
		if (!strcmp(file, ".") || !strcmp(file, "..") )
		  continue;

		for (i = 0; i < depth; i++)
		  printf("\t");
		printf("%s\n", file);
		if (S_ISDIR(mode)){
			char path[32];
			strcpy(path, name);
			if ( strcmp(name, "/") )
				strcat(path, "/");
			strcat(path, file);
			list_dir(path, depth+1);
		}
	}
	fs_closedir(dir);
}

static void test_stat(char* path)
{
	struct stat s;
	fs_stat(path, &s);
	printk("%s: is dir %d, size %d\n", path, S_ISDIR(s.st_mode), s.st_size);
}

static void test_write()
{	
	unsigned int fd = fs_open("/readme.txt");
	char* buf = 0;
	int i = 0;
	time_t now;
	unsigned t;
	timer_current(&now);
	printk("%d: write begin\n", now.seconds*60+now.milliseconds);
	if (fd == MAX_FD)
		return;

	buf = kmalloc(1024);
	memset(buf, 'd', 1024);
	t = time(0);
	for (i = 0; i < (4*1024); i++){
		if (i % 100 == 0) {
			unsigned span = time(t);
			unsigned speed = 0;
			if (span) {
				speed=((100*1024) / span) * 1000;
			}
			t = t+span;
			printk("write index %d, speed %h/s\n", i, speed);
			#ifdef DEBUG_FFS
			extern void report_time();
			extern void report_hdd_time();
			extern void report_cache();
			report_time();
			report_hdd_time();
			report_cache();
			#endif
		}
		fs_write(fd, i*1024, buf, 1024);
		
	}
	kfree(buf);
	fs_close(fd);

	timer_current(&now);
	printk("%d: write end\n", now.seconds*60+now.milliseconds);
}

static void test_read()
{	
	unsigned int fd = fs_open("/readme.txt");
	char* buf = 0;
	int i = 0;
	time_t now;
	unsigned t;
	timer_current(&now);
	printk("%d: read  begin\n", now.seconds*60+now.milliseconds);
	if (fd == MAX_FD)
		return;

	buf = kmalloc(1024);
	memset(buf, 0, 1024);
	t = time(0);
	while (1) {
		printk("");
		klogquota();
		for (i = 0; i < (4*1024); i++){
			if (i % 100 == 0) {
				unsigned span = time(0) - t;
				unsigned speed = 0;
				if (span) {
					speed=((100*1024) / span) * 1000;
				}
				t = t+span;
				printk("read index %d, speed %h/s\n", i, speed);
				#ifdef DEBUG_FFS
				extern void report_time();
				extern void report_hdd_time();
				extern void report_cache();
				report_time();
				report_hdd_time();
				report_cache();
				#endif
			}
			memset(buf, 0, 1024);
			fs_read(fd, i*1024, buf, 1024);
			
		}
	}
	kfree(buf);
	fs_close(fd);

	timer_current(&now);
	printk("%d: write end\n", now.seconds*60+now.milliseconds);
}

void test_ns()
{
	unsigned fd;
	char text[32];
	klogquota();

	printk("test_ns\n");
	//list_dir("/", 0);

	test_write();
	klogquota();
}
#endif
