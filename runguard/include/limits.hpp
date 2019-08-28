#pragma once

#include "runguard_options.hpp"

void cgroup_create(const struct runguard_options &);

/**
 * Move current process to the control group.
 * 
 * Attach to the control group to change settings
 * and monitor status.
 */
void cgroup_attach(const struct runguard_options &);

/**
 * Kill all processes in the control group.
 * 
 * Here, runguard will kill all child processes of
 * the monitored process after exiting.
 */
void cgroup_kill(const struct runguard_options &);

/**
 * 
 */
void cgroup_delete(const struct runguard_options &);

/**
 * Limit current process resources usage.
 */
void set_restrictions(const struct runguard_options &opt);

/**
 * Limit syscalls
 */
void set_seccomp(const struct runguard_options &opt);
