int readableTimeout(int fd, int sec, int usec)
{
    struct timeval tv;
    fd_set rset;

    tv.tv_sec = sec;
    tv.tv_usec = usec;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    return (select(fd + 1, &rset, NULL, NULL, &tv));
}
