#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
  #include <sys/mount.h>
#else
  #include <sys/statfs.h>
  #include <sys/ioctl.h>
#  if !defined(__CYGWIN__)
#   include <linux/fs.h>
#  endif
#endif
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef __FreeBSD__
# include <sys/xattr.h>
#endif
#include <set>
#include <vector>
#include <mutex>
#include <locale.h>
#include "sudo_private.h"

namespace Sudo 
{
	template <class OBJ> class Opened
	{
		std::set<OBJ> _set;
		std::mutex _mutex;
		
	public:
		void Put(OBJ obj)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_set.insert(obj);
		}

		bool Check(OBJ obj)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			return (_set.find(obj) != _set.end());
		}
		
		bool Remove(OBJ obj)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			return (_set.erase(obj) != 0);
		}
	};
	
	static Opened<int> g_fds;
	static Opened<DIR *> g_dirs;

	static void OnSudoDispatch_Execute(BaseTransaction &bt)
	{
		std::string cmd;
		bt.RecvStr(cmd);
		int no_wait = bt.RecvInt();
		int r;
		if (no_wait) {
			r = fork();
			if (r == 0) {
				r = system(cmd.c_str());
				_exit(r);
				exit(r);
			}
			if (r != -1) {
				PutZombieUnderControl(r);
				r = 0;
			}
		} else {
			r = system(cmd.c_str());
		}
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_Close(BaseTransaction &bt)
	{
		int fd;
		bt.RecvPOD(fd);
		int r = g_fds.Remove(fd) ? close(fd) : -1;
		bt.SendPOD(r);
	}
	
	static void OnSudoDispatch_Open(BaseTransaction &bt)
	{
		std::string path;
		int flags;
		mode_t mode;

		bt.RecvStr(path);
		bt.RecvPOD(flags);
		bt.RecvPOD(mode);
		int r = open(path.c_str(), flags, mode);
		bt.SendPOD(r);
		if (r!=-1) 
			g_fds.Put(r);
		else
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_LSeek(BaseTransaction &bt)
	{		
		int fd;
		off_t offset;
		int whence;

		bt.RecvPOD(fd);
		bt.RecvPOD(offset);
		bt.RecvPOD(whence);
		
		off_t r = g_fds.Check(fd) ? lseek(fd, offset, whence) : -1;
		bt.SendPOD(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_Write(BaseTransaction &bt)
	{		
		int fd;
		size_t count;

		bt.RecvPOD(fd);
		bt.RecvPOD(count);
		
		std::vector<char> buf(count + 1);
		if (count)
			bt.RecvBuf(&buf[0], count);
		
		ssize_t r = g_fds.Check(fd) ? write(fd, &buf[0], count) : -1;
		bt.SendPOD(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_Read(BaseTransaction &bt)
	{
		int fd;
		size_t count;

		bt.RecvPOD(fd);
		bt.RecvPOD(count);
		
		std::vector<char> buf(count + 1);
		
		ssize_t r = g_fds.Check(fd) ? read(fd, &buf[0], count) : -1;
		bt.SendPOD(r);
		if (r==-1) {
			bt.SendErrno();
		} else if (r > 0) {
			bt.SendBuf(&buf[0], r);
		}
	}
	
	static void OnSudoDispatch_PWrite(BaseTransaction &bt)
	{		
		int fd;
		off_t offset;
		size_t count;

		bt.RecvPOD(fd);
		bt.RecvPOD(offset);
		bt.RecvPOD(count);
		
		std::vector<char> buf(count + 1);
		if (count)
			bt.RecvBuf(&buf[0], count);
		
		ssize_t r = g_fds.Check(fd) ? pwrite(fd, &buf[0], count, offset) : -1;
		bt.SendPOD(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_PRead(BaseTransaction &bt)
	{
		int fd;
		off_t offset;
		size_t count;

		bt.RecvPOD(fd);
		bt.RecvPOD(offset);
		bt.RecvPOD(count);
		
		std::vector<char> buf(count + 1);
		
		ssize_t r = g_fds.Check(fd) ? pread(fd, &buf[0], count, offset) : -1;
		bt.SendPOD(r);
		if (r==-1) {
			bt.SendErrno();
		} else if (r > 0) {
			bt.SendBuf(&buf[0], r);
		}
	}
	
	
	template <class STAT_STRUCT>
		static void OnSudoDispatch_StatCommon(int (*pfn)(const char *path, STAT_STRUCT *buf), BaseTransaction &bt)
	{		
		std::string path;
		bt.RecvStr(path);
		STAT_STRUCT s = {};
		int r = pfn(path.c_str(), &s);
		bt.SendPOD(r);
		if (r==0)
			bt.SendPOD(s);
	}

	
	static void OnSudoDispatch_FStat(BaseTransaction &bt)
	{
		int fd;
		bt.RecvPOD(fd);
		
		struct stat s;
		int r = g_fds.Check(fd) ? fstat(fd, &s) : -1;
		bt.SendPOD(r);
		if (r == 0)
			bt.SendPOD(s);
		else
			bt.SendErrno();
	}
	
	static void OnSudoFTruncate(BaseTransaction &bt)
	{
		int fd;
		off_t length;
		bt.RecvPOD(fd);
		bt.RecvPOD(length);
		
		int r = g_fds.Check(fd) ? ftruncate(fd, length) : -1;
		bt.SendPOD(r);
		if (r != 0)
			bt.SendErrno();
	}
	
	static void OnSudoFChmod(BaseTransaction &bt)
	{
		int fd;
		mode_t mode;
		bt.RecvPOD(fd);
		bt.RecvPOD(mode);
		
		int r = g_fds.Check(fd) ? fchmod(fd, mode) : -1;
		bt.SendPOD(r);
		if (r != 0)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_CloseDir(BaseTransaction &bt)
	{		
		DIR *d;
		bt.RecvPOD(d);
		int r = g_dirs.Remove(d) ? closedir(d) : -1;
		bt.SendPOD(r);
	}
	
	static void OnSudoDispatch_OpenDir(BaseTransaction &bt)
	{
		std::string path;
		bt.RecvStr(path);
		DIR *d = opendir(path.c_str());
		g_dirs.Put(d);
		bt.SendPOD(d);
		if (!d)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_ReadDir(BaseTransaction &bt)
	{
		DIR *d;
		bt.RecvPOD(d);
		bt.RecvErrno();
		struct dirent *de = g_dirs.Check(d) ? readdir(d) : nullptr;
		if (de) {
			bt.SendInt(0);
			bt.SendPOD(*de);
		} else {
			int err = errno;
			bt.SendInt(err ? err : -1);
		}
	}
	
	static void OnSudoDispatch_MkDir(BaseTransaction &bt)
	{
		std::string path;
		mode_t mode;
		
		bt.RecvStr(path);
		bt.RecvPOD(mode);
		
		int r = mkdir(path.c_str(), mode);
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_OnePathCommon(int (*pfn)(const char *path), BaseTransaction &bt)
	{
		std::string path;
		bt.RecvStr(path);
		int r = pfn(path.c_str());
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_ChDir(BaseTransaction &bt)
	{
		std::string path;
		bt.RecvStr(path);
		int r = chdir(path.c_str());
		bt.SendInt(r);
		if (r==-1) {
			bt.SendErrno();
		} else {
			char cwd[PATH_MAX + 1];
			if (!getcwd(cwd, sizeof(cwd)-1))
				cwd[0] = 0;
			bt.SendStr(cwd);
		}
	}
	
	static void OnSudoDispatch_ChMod(BaseTransaction &bt)
	{
		std::string path;
		mode_t mode;
		
		bt.RecvStr(path);
		bt.RecvPOD(mode);
		
		int r = chmod(path.c_str(), mode);
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
				
	static void OnSudoDispatch_ChOwn(BaseTransaction &bt)
	{
		std::string path;
		uid_t owner;
		gid_t group;
		
		bt.RecvStr(path);
		bt.RecvPOD(owner);
		bt.RecvPOD(group);
		
		int r = chown(path.c_str(), owner, group);
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
				
	static void OnSudoDispatch_UTimes(BaseTransaction &bt)
	{
		std::string path;
		struct timeval times[2];
		
		bt.RecvStr(path);
		bt.RecvPOD(times[0]);
		bt.RecvPOD(times[1]);
		
		int r = utimes(path.c_str(), times);
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_FUTimes(BaseTransaction &bt)
	{
		int fd = -1;
		struct timeval times[2];
		bt.RecvPOD(fd);
		bt.RecvPOD(times[0]);
		bt.RecvPOD(times[1]);

		off_t r = g_fds.Check(fd) ? futimes(fd, times) : -1;
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}

	static void OnSudoDispatch_TwoPathes(int (*pfn)(const char *, const char *), BaseTransaction &bt)
	{
		std::string path1, path2;
		
		bt.RecvStr(path1);
		bt.RecvStr(path2);
		
		int r = pfn(path1.c_str(), path2.c_str());
		bt.SendInt(r);
		if (r==-1)
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_RealPath(BaseTransaction &bt)
	{
		std::string path;
		bt.RecvStr(path);
		char resolved_path[PATH_MAX + 1] = { 0 };
		if (realpath(path.c_str(), resolved_path)) {
			bt.SendInt(0);
			bt.SendStr(resolved_path);
		} else {
			int err = errno;
			bt.SendInt( err ? err : -1 );
		}
	}
	
	static void OnSudoDispatch_ReadLink(BaseTransaction &bt)
	{
		std::string path;
		size_t bufsiz;
		bt.RecvStr(path);
		bt.RecvPOD(bufsiz);
		std::vector<char> buf(bufsiz + 1);
		ssize_t r = readlink(path.c_str(), &buf[0], bufsiz);
		bt.SendPOD(r);
		if (r >= 0 && r<= (ssize_t)bufsiz) {
			bt.SendBuf(&buf[0], r);
		} else
			bt.SendErrno();
	}
	
	static void OnSudoDispatch_FListXAttr(BaseTransaction &bt)
	{
#ifdef __FreeBSD__
		return;
#else
		int fd;
		size_t size;
		bt.RecvPOD(fd);
		bt.RecvPOD(size);
		std::vector<char> buf(size + 1);
		
#ifdef __APPLE__
		ssize_t r = g_fds.Check(fd) ? flistxattr(fd, &buf[0], size, 0) : -1;
#else
		ssize_t r = g_fds.Check(fd) ? flistxattr(fd, &buf[0], size) : -1;
#endif
		bt.SendPOD(r);
		if (r > 0 ) {
			bt.SendBuf(&buf[0], r);
		} else if (r < 0)
			bt.SendErrno();
#endif
	}

	static void OnSudoDispatch_FGetXAttr(BaseTransaction &bt)
	{
#ifdef __FreeBSD__
		return;
#else
		int fd;
		std::string name;
		size_t size;
		bt.RecvPOD(fd);
		bt.RecvStr(name);
		bt.RecvPOD(size);
		std::vector<char> buf(size + 1);
#ifdef __APPLE__
		ssize_t r = g_fds.Check(fd) ? fgetxattr(fd, name.c_str(), &buf[0], size, 0, 0) : -1;
#else
		ssize_t r = g_fds.Check(fd) ? fgetxattr(fd, name.c_str(), &buf[0], size) : -1;
#endif
		bt.SendPOD(r);
		if (r > 0 ) {
			bt.SendBuf(&buf[0], r);
		} else if (r < 0)
			bt.SendErrno();
#endif
	}
			
	static void OnSudoDispatch_FSetXAttr(BaseTransaction &bt)
	{
#ifdef __FreeBSD__
		return;
#else
		int fd;
		std::string name;
		size_t size;
		int flags;
		bt.RecvPOD(fd);
		bt.RecvStr(name);
		bt.RecvPOD(size);
		std::vector<char> buf(size + 1);
		bt.RecvBuf(&buf[0], size);
		bt.RecvPOD(flags);
		
#ifdef __APPLE__
		int r = g_fds.Check(fd) ? fsetxattr(fd, name.c_str(), &buf[0], size, 0, flags) : -1;
#else
		int r = g_fds.Check(fd) ? fsetxattr(fd, name.c_str(), &buf[0], size, flags) : -1;
#endif

		bt.SendPOD(r);
		if (r == -1 )
			bt.SendErrno();
#endif
	}
	
	static void OnSudoDispatch_FSFlagsGet(BaseTransaction &bt)
	{
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__CYGWIN__)
		std::string path;
		bt.RecvStr(path);
		int r = -1;
		int fd = open(path.c_str(), O_RDONLY);
		if (fd != -1) {
			int flags = 0;
			r = bugaware_ioctl_pint(fd, FS_IOC_GETFLAGS, &flags);
			close(fd);
			if (r == 0) {
				bt.SendInt(0);
				bt.SendInt(flags);
				return;
			}
		}
		bt.SendInt(-1);
		bt.SendErrno();
#endif
	}
	
	static void OnSudoDispatch_FSFlagsSet(BaseTransaction &bt)
	{
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__CYGWIN__)
		std::string path;
		bt.RecvStr(path);
		int flags = bt.RecvInt();
		
		int r = -1;
		int fd = open(path.c_str(), O_RDONLY);
		if (fd != -1) {
			r = bugaware_ioctl_pint(fd, FS_IOC_SETFLAGS, &flags);
			close(fd);
			if (r == 0) {
				bt.SendInt(0);
				return;
			}
		}
		bt.SendInt(-1);
		bt.SendErrno();
#endif
	}
	
	void OnSudoDispatch(SudoCommand cmd, BaseTransaction &bt)
	{
		//fprintf(stderr, "OnSudoDispatch: %u\n", cmd);
		switch (cmd) {
			case SUDO_CMD_PING:
				break;
				
			case SUDO_CMD_EXECUTE:
				OnSudoDispatch_Execute(bt);
				break;
				
			case SUDO_CMD_CLOSE:
				OnSudoDispatch_Close(bt);
				break;
				
			case SUDO_CMD_OPEN:
				OnSudoDispatch_Open(bt);
				break;
				
			case SUDO_CMD_LSEEK:
				OnSudoDispatch_LSeek(bt);
				break;

			case SUDO_CMD_WRITE:
				OnSudoDispatch_Write(bt);
				break;
			
			case SUDO_CMD_READ:
				OnSudoDispatch_Read(bt);
				break;

			case SUDO_CMD_PWRITE:
				OnSudoDispatch_PWrite(bt);
				break;
			
			case SUDO_CMD_PREAD:
				OnSudoDispatch_PRead(bt);
				break;
				
			case SUDO_CMD_STATFS:
				OnSudoDispatch_StatCommon<struct statfs>(&statfs, bt);
				break;
				
			case SUDO_CMD_STATVFS:
				OnSudoDispatch_StatCommon<struct statvfs>(&statvfs, bt);
				break;
				
			case SUDO_CMD_STAT:
				OnSudoDispatch_StatCommon<struct stat>(&stat, bt);
				break;
				
			case SUDO_CMD_LSTAT:
				OnSudoDispatch_StatCommon<struct stat>(&lstat, bt);
				break;
				
			case SUDO_CMD_FSTAT:
				OnSudoDispatch_FStat(bt);
				break;
				
			case SUDO_CMD_FTRUNCATE:
				OnSudoFTruncate(bt);
				break;

			case SUDO_CMD_FCHMOD:
				OnSudoFChmod(bt);
				break;
				
			case SUDO_CMD_CLOSEDIR:
				OnSudoDispatch_CloseDir(bt);
				break;
				
			case SUDO_CMD_OPENDIR:
				OnSudoDispatch_OpenDir(bt);
				break;
				
			case SUDO_CMD_READDIR:
				OnSudoDispatch_ReadDir(bt);
				break;
				
			case SUDO_CMD_MKDIR:
				OnSudoDispatch_MkDir(bt);
				break;
				
			case SUDO_CMD_CHDIR:
				OnSudoDispatch_ChDir(bt);
				break;
				
			case SUDO_CMD_RMDIR:
				OnSudoDispatch_OnePathCommon(&rmdir, bt);
				break;
			
			case SUDO_CMD_REMOVE:
				OnSudoDispatch_OnePathCommon(&remove, bt);
				break;
			
			case SUDO_CMD_UNLINK:
				OnSudoDispatch_OnePathCommon(&unlink, bt);
				break;
			
			case SUDO_CMD_CHMOD:
				OnSudoDispatch_ChMod(bt);
				break;
				
			case SUDO_CMD_CHOWN:
				OnSudoDispatch_ChOwn(bt);
				break;
				
			case SUDO_CMD_UTIMES:
				OnSudoDispatch_UTimes(bt);
				break;
			
			case SUDO_CMD_FUTIMES:
				OnSudoDispatch_FUTimes(bt);
				break;

			case SUDO_CMD_RENAME:
				OnSudoDispatch_TwoPathes(&rename, bt);
				break;

			case SUDO_CMD_SYMLINK:
				OnSudoDispatch_TwoPathes(&symlink, bt);
				break;
				
			case SUDO_CMD_LINK:
				OnSudoDispatch_TwoPathes(&link, bt);
				break;

			case SUDO_CMD_REALPATH:
				OnSudoDispatch_RealPath(bt);
				break;

			case SUDO_CMD_READLINK:
				OnSudoDispatch_ReadLink(bt);
				break;
				
			case SUDO_CMD_FLISTXATTR:
				OnSudoDispatch_FListXAttr(bt);
				break;
			
			case SUDO_CMD_FGETXATTR:
				OnSudoDispatch_FGetXAttr(bt);
				break;
			
			case SUDO_CMD_FSETXATTR:
				OnSudoDispatch_FSetXAttr(bt);
				break;
			
			case SUDO_CMD_FSFLAGSGET:
				OnSudoDispatch_FSFlagsGet(bt);
				break;
				
			case SUDO_CMD_FSFLAGSSET:
				OnSudoDispatch_FSFlagsSet(bt);
				break;
				
			default:
				throw "OnSudoDispatch - bad command";
		}
	}
	
	static void sudo_dispatcher_with_pipes(int pipe_request, int pipe_reply)
	{
		fprintf(stderr, "sudo_dispatcher(%d, %d)\n", pipe_request, pipe_reply);
		
		SudoCommand cmd = SUDO_CMD_INVALID;
		try {
			for (;;) {
				BaseTransaction bt(pipe_reply, pipe_request);
				bt.RecvPOD(cmd);
				OnSudoDispatch(cmd, bt);
				bt.SendPOD(cmd);
			}
		} catch (const char *what) {
			fprintf(stderr, "sudo_dispatcher - %s (cmd=%u errno=%u)\n", what, cmd, errno);
		}
	}
	
	
	extern "C" __attribute__ ((visibility("default"))) int sudo_main_dispatcher()
	{
		int pipe_reply = dup(STDOUT_FILENO);
		int pipe_request = dup(STDIN_FILENO);
		int fd = open("/dev/null", O_RDWR);
		if (fd!=-1) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDIN_FILENO);
			close(fd);
		} else
			perror("open /dev/null");

		setlocale(LC_ALL, "");//otherwise non-latin keys missing with XIM input method
		
		sudo_dispatcher_with_pipes(pipe_request, pipe_reply);
		close(pipe_request);
		close(pipe_reply);
		return 0;	
	}

}


