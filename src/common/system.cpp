#include "common/system.hpp"
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

int get_userid(const char *name)
{
    struct passwd *pwd;

    errno = 0;
    pwd = getpwnam(name);
    
    if (!pwd || errno) return -1;
    return (int) pwd->pw_uid;
}

int get_groupid(const char *name)
{
    struct group *g;

    errno = 0;
    g = getgrnam(name);

    if (!g || errno) return -1;
    return (int) g->gr_gid;
}
