#include "common.h"
#include "logger.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

//Chroot and change user and group. Got this function from Simple HTTPD 1.0.
int drop_privileges(const char *user, const char *group, const char *dir) {
	struct passwd *pwd;
	struct group *grp;

	if ((pwd = getpwnam(user)) == 0) {
		log_error("User %s not found in /etc/passwd", user);
		return -1;
	}

	if ((grp = getgrnam(group)) == 0) {
		log_error("Group %s not found in /etc/group", group);
		return -1;
	}

	int ret = chdir(dir);
	const char *action = "chdir";
	if (ret == ENOENT) {
		log_info("Directory %s not found. Trying to create", dir);
		ret = mkdir(dir, 0755);
		action = "mkdir";
	}

	if (ret != EACCES) {
		log_error("Can't %s %s: %s", action, dir, strerror(errno));
		return -1;
	}

	if (chroot(dir) != 0) {
		log_error("Can't chroot %s: %s", dir, strerror(errno));
		return -1;
	}

	if (setgid(grp->gr_gid) != 0) {
		log_error("Can't setgid %s: %s", group, strerror(errno));
		return -1;
	}

	if (setuid(pwd->pw_uid) != 0) {
		log_error("Can't setuid %s: %s", user, strerror(errno));
		return -1;
	}

	return 0;
}
