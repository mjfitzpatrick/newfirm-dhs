/* Bare nslookup utility (w/ minimal error checking) */
#include <stdio.h>		/* stderr, stdout */
#include <netdb.h>		/* hostent struct, gethostbyname() */
#include <arpa/inet.h>		/* inet_ntoa() to format IP address */
#include <netinet/in.h>		/* in_addr structure */

int server_ack(int p_sock, int wait);

int main(int argc, char **argv)
{
    struct hostent *host;	/* host information */
    struct in_addr h_addr;	/* internet address */
    int sock, port, size;
    long istat = 0;
    char resp[80];
    port = 2001;

    (void) sockUtilConnect(&istat, resp, &sock, port, "140.252.6.16");
    printf("dhsOpenConnect: sock: %d\n", sock);

    size = write(sock, resp, 0);
    printf("writting size: %d\n", size);

    exit(0);
    printf("localdo <%s>\n", getenv("LOCALDOMAIN"));
    if (argc != 2) {
	printf("USAGE: nslookup <inet_address>\n");
	exit(1);
    }
    if ((host = gethostbyname(argv[1])) == NULL) {
	printf("(mini) nslookup failed on '%s'\n", argv[1]);
	exit(1);
    }
    h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
    printf("%s\n", inet_ntoa(h_addr));

    if (server_ack(sock, 0) == 0) {
	printf("got server ack - recving data!\n");
    }
}

/*WAS static inline */
int server_ack(int p_sock, int wait)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    char buf;

    /* Watch proxy socket to see when it has input
     * Wait up to five seconds.
     */
    FD_ZERO(&rfds);
    FD_SET(p_sock, &rfds);
    tv.tv_sec = wait;
    tv.tv_usec = 0;

    printf("doing select");
    retval = select(p_sock + 1, &rfds, NULL, NULL, &tv);
    printf("returned from select");
    if (retval) {		/* possibly data */
	if (recv(p_sock, &buf, 1, MSG_PEEK) == -1) {
	    printf("ack failure:\n");
	    return -1;
	}
    }

    return 0;
}
