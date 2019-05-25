namespace cgroup
{
    struct cgroup_guard
    {
        struct cgroup *cg;

        cgroup_guard(const char *cgroup_name)
        {
            cg = cgroup_new_cgroup(cgroup_name);
            if (!cg) throw runtime_error("cgroup_new_cgroup");
        }

        ~cgroup_guard()
        {
            cgroup_free(&cg);
        }

        void create_group(int x)
        {
            if (cgroup_create_group(cg, x) != 0)
                throw runtime_error("create_group");
        }

        void add_controller(const char *name)
        {
            if (cgroup_add_controller(cg, name) == nullptr)
                throw runtime_error("cgroup_add_controller");
        }

        void get_cgroup()
        {
            if (cgroup_get_cgroup(cg) != 0)
                throw runtime_error("get cgroup information");
        }

        void attach_task()
        {
            if (cgroup_attach_task(cg) != 0)
                throw runtime_error("cgroup_attach_task");
        }

        void delete_cgroup()
        {
            if (cgroup_delete_cgroup_ext(cg, CGFLAG_DELETE_IGNORE_MIGRATION | CGFLAG_DELETE_RECURSIVE) != 0)
                throw runtime_error("cgroup_delete_cgroup");
        }
    }
}


void cgroup_create()
{
    int ret;
    struct cgroup_controller *cg_controller;

    cgroup_guard cg;

    // Set up the memory restrictions
    cg.add_controller("memory");

    // RAM limit and RAM+swap limit are the same,
    // so no swapping can occur.
    cgroup_add_value(int64, "memory.limit_in_bytes", memsize);
    cgroup_add_value(int64, "memory.memsw.limit_in_bytes", memsize);

    // Set up CPU restrictions; we pin the task to a specific set of
    // cpus. We also give it exclusive access to those cores.
    // And set no limits on memory nodes.
    if (cpuset && strlen(cpuset)) {
        cg.add_controller("cpuset");

        // To make a cpuset exclusive, some additional setup
        // outside of judger required

        cgroup_add_value(string, "cpuset.mems", "0");
        cgroup_add_value(stirng, "cpuset.cpus", cpuset);
    } else {
        log() << "cpuset undefined";
    }

    cg.add_controller("cpuacct");

    cg.create_cgroup(1);

    log(VERBOSE) << "created cgroup '" << cgroupname << "'";
}

void cgroup_attach()
{
    cgroup_guard cg;
    cg.get_cgroup();
    cg.attach_task();
}

void cgroup_kill()
{
    void *ptr = nullptr;
    pid_t pid;

    while (true)
    {
        int ret = cgroup_get_task_begin(cgroupname, "memory", &ptr, &pid);
        cgroup_get_task_end(&ptr);
        if (ret == ECGEOF) break;
        kill(pid, SIGKILL);
    }
}

void cgroup_delete()
{
    cgroup_guard cg;
    cg.add_controller("cpuacct");
    cg.add_controller("memory");

    if (cpuset && strlen(cpuset))
    {
        cg.add_controller("cpuset");
    }

    cgroup.delete_cgroup();
}

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


