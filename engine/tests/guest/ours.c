/*
 * ours: the host's program, run as the first task (pid 1) before the real
 * init (penhost.py scenario takeover: an injected execveat of a memfd at
 * init's first syscall).  ours <real init> <its argv...>: does its part,
 * then execs the real init with its own argv and environment.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv, char **envp)
{
    char buf[512];
    int n, fd, envc = 0;

    while (envp[envc]) {
        envc++;
    }
    n = snprintf(buf, sizeof(buf), "OURS pid=%d ppid=%d uid=%d argc=%d envc=%d init=%s\n",
                 getpid(), getppid(), getuid(), argc, envc, argc > 1 ? argv[1] : "-");
    write(1, buf, n);
    fd = open("/ours.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, buf, n);
        close(fd);
    }
    if (argc < 3) {
        return 1;
    }
    execve(argv[1], argv + 2, envp);
    n = snprintf(buf, sizeof(buf), "OURS execve %s failed\n", argv[1]);
    write(1, buf, n);
    return 127;
}
