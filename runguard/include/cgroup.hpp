#pragma once

#include <exception>
#include <string>

struct cgroup;
struct cgroup_controller;

struct cgroup_exception : public std::exception {
    cgroup_exception(std::string cgroup_op, int err);

    const char *what() const noexcept override;

    static void ensure(std::string cgroup_op, int err);
private:
    std::string errmsg;
};

struct cgroup_ctrl {
    struct cgroup_controller *ctrl;

    void add_value(const std::string &name, int64_t value);
    void add_value(const std::string &name, std::string value);

    int64_t get_value_int64(const std::string &name);
};

struct cgroup_guard {
    cgroup_guard(const std::string &cgroup_name);
    ~cgroup_guard();

    /**
     * Physically create a control group in kernel. The group is created in all
     * hierarchies, which cover controllers added by cgroup_add_controller().
     * All parameters set by cgroup_add_value_* functions are written.
     * The created groups has owner which was set by cgroup_set_uid_gid() and
     * permissions set by cgroup_set_permissions.
     * @param ignore_ownership When nozero, all errors are ignored when setting
     * 	owner of the group and/or its tasks file.
     * 	@todo what is ignore_ownership good for?
     * @retval #ECGROUPNOTEQUAL if not all specified controller parameters
     *      were successfully set.
     */
    void create_cgroup(int ignore_ownership);

    /**
     * Attach new controller to cgroup. This function just modifies internal
     * libcgroup structure, not the kernel control group.
     *
     * @param name Name of the controller, e.g. "freezer".
     * @return Created controller or NULL on error.
     */
    cgroup_ctrl add_controller(const std::string &name);

    /**
     * Return appropriate controller from given group.
     * The controller must be added before using cgroup_add_controller() or loaded
     * from kernel using cgroup_get_cgroup().
     * @param name Name of the controller, e.g. "freezer".
     */
    cgroup_ctrl get_controller(const std::string &name);

    /**
     * Read all information regarding the group from kernel.
     * Based on name of the group, list of controllers and all parameters and their
     * values are read from all hierarchies, where a group with given name exists.
     * All existing controllers are replaced. I.e. following code will fill @c root
     * with controllers from all hierarchies, because the root group is available in
     * all of them.
     * @code
     * struct cgroup *root = cgroup_new_cgroup("/");
     * cgroup_get_cgroup(root);
     * @endcode
     *
     * @todo what is this function good for? Why is not considered only the list of
     * controllers attached by cgroup_add_controller()? What owners will return
     * cgroup_get_uid_gid() if the group is in multiple hierarchies, each with
     * different owner of tasks file?
     *
     * @param cgroup The cgroup to load. Only it's name is used, everything else
     * 	is replaced.
     */
    void get_cgroup();

    /**
     * Move current task (=thread) to given control group.
     */
    void attach_task();

    /**
     * Physically remove a control group from kernel.
     * All tasks are automatically moved to parent group.
     * If #CGFLAG_DELETE_IGNORE_MIGRATION flag is used, the errors that occurred
     * during the task movement are ignored.
     * #CGFLAG_DELETE_RECURSIVE flag specifies that all subgroups should be removed
     * too. If root group is being removed with this flag specified, all subgroups
     * are removed but the root group itself is left undeleted.
     * @see cgroup_delete_flag.
     *
     * @param flags Combination of CGFLAG_DELETE_* flags, which indicate what and
     *	how to delete.
     */
    void delete_cgroup();

    static void init();

private:
    struct cgroup *cg;
};
