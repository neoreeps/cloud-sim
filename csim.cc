/* ------------------------------------------------------------
 * File:	csim.cc
 * Description:	generic cloud simulator
 *		See README
 * Supported Connectors:
 *      amazon
 *      atmos
 *
 * The Rewrite rule tab-delimits the fields and is required for
 * the HTML GET method to call our script.
 * ------------------------------------------------------------ */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <syslog.h>


/* ------------------------------------------------------------
 * Function:	simLog
 * Description:	Write to syslog
 * ------------------------------------------------------------ */
void simLog(int priority, const char *format, ...)
{
    va_list vap;
    va_start(vap, format);
    vsyslog(priority, format, vap);
}


/* ------------------------------------------------------------
 * Function:	getTime
 * Description:	Get high res time from microboottime
 * ------------------------------------------------------------ */
double getTime (void)
{
    double seconds;

    timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);
    seconds = ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);

    return seconds;
}


/* ------------------------------------------------------------
 * Function:	getDigest
 * Desription:	Use the same call the connector makes to generate
 *		the digest.
 * ------------------------------------------------------------ */
void getDigest (char *pathname, char *digest, bool useSSL)
{
    FILE *pin;
    char *ptr;

    char cmd[1024] = {0};
    char buffer[1024] = {0};

    *digest = 0;
    if (useSSL)
        sprintf (cmd, "/usr/bin/openssl dgst -sha %s", pathname);
    else
        sprintf (cmd, "/usr/bin/md5sum %s", pathname);

    if (NULL != (pin = popen (cmd, "r"))) {
    	while (1) {
    		fgets (buffer, 1024, pin);
    		if (feof (pin)) 
                break;
    		if (NULL != (ptr = strchr (buffer, 0x0d))) 
                *ptr = 0;
    		if (NULL != (ptr = strchr (buffer, 0x0a))) 
                *ptr = 0;

            if (useSSL) {
                if (NULL != (ptr = strstr (buffer, ")= "))) {
                    strcpy (digest, ptr + 3);
                    break;
                }
            }
            else if (NULL != strstr (buffer, pathname)) {
    		    if (NULL != (ptr = strchr (buffer, ' '))) {
    		    	*ptr = 0;
    		    	strcpy (digest, buffer);
    		    	break;
			   	}
		    }
		} // while
	    pclose (pin);
	}
}


/* ------------------------------------------------------------
 * Function:	createPath
 * Description:	Create/check for each component of the path
 *              Path will always have a leading /var/www
 * ------------------------------------------------------------ */
void createPath (char *directory)
{
    char *ptr;
    char base[8192] = {0};
    char work[8192] = {0};

    strcpy (base, directory);

    // We're given the entire path, strip the last component off
    if (NULL != (ptr = strrchr (base, '/')))
	*ptr = 0;

    ptr = strchr (base + 1, '/');
    while (true) {
    	strcpy (work, base);
    	work[ptr-base] = 0;
    	if (access (work, 0)) {
    		if (0 != (mkdir (work, 0777)))
    			simLog (LOG_ERR, "Path: %s, Error: Cannot Create Directory (err: %d)\n",
                    work, errno);
		}
	   
        if (NULL == (ptr = strchr (ptr + 1, '/'))) {
		    // create the last component
		    if (access (base, 0)) {
		    	if (0 != (mkdir (base, 0777)))
				    simLog (LOG_ERR, "Path: %s, Error: Cannot Create Directory (err: %d)\n",
                        work, errno);
			}
		    break;
		}
	}
}


/* ------------------------------------------------------------
 * Function:	main
 * ------------------------------------------------------------ */
int main (int argc, char **argv, char **envp)
{
    char buffer[65535] = {0};
    char pathname[1024] = {0};
    char digest[1024] = {0};

    FILE *fout, *fin;
    int count;
    long totalSize;
    double etime, stime;

    stime = getTime ();

    // Obtain pointers to request data, no need to allocate and copy as
    // they don't change for a given reqquest
    char *host =            getenv ("REMOTE_ADDR");
    char *requestUri =      getenv ("REQUEST_URI");
    char *requestMethod =   getenv ("REQUEST_METHOD");
    char *uid =             getenv ("HTTP_X_EMC_UID");
    char *signature =       getenv ("HTTP_X_EMC_SIGNATURE");
    char *wschecksum =      getenv ("HTTP_X_EMC_WSCHECKSUM");

    if (requestUri)
        sprintf (pathname, "/var/www/html%s", requestUri); 

    if (!wschecksum)
        wschecksum = getenv ("HTTP_CONTENT_MD5");

#ifdef DEBUG
    simLog(LOG_INFO, "%s START: Method: %s, Uri: %s, Path: %s, Checksum: %s\n",
            host, requestMethod, requestUri, pathname, wschecksum);

    for (int i = 0; i < argc; i++)
        simLog(LOG_INFO, "argv[%d]: %s", i, argv[i]);

    // Find Query String
    for (int x = 0; ; x++) {
	    if (NULL == envp[x])
            break;
        simLog(LOG_INFO, "envp[%d]: %s", x, envp[x]);
    }
#endif //DEBUG
    // ------------------------------------------------------------
    // HTTP DELETE - delete the object
    // ------------------------------------------------------------
    if (0 == strcasecmp (requestMethod, "DELETE")) {
#ifdef DEBUG
    	simLog (LOG_INFO, "%s DELETE Entered: RequestUri: %s\n", host, requestUri);
#endif

	    if (!access (pathname, 0)) {
	    	unlink (pathname);
	    	fprintf (stdout, "Status: 204 No Content\n");		// Script needs this
	    }
	    else {
	    	fprintf (stdout, "Status: 404 File not found\n");
	    }

	    fprintf (stdout, "Content-Type: text/plain\n");
	    fprintf (stdout, "Content-Length: 0\n");
	    fprintf (stdout, "\n");
	} // DELETE

    // ------------------------------------------------------------
    // HTTP GET - retrieve the object
    // ------------------------------------------------------------
    else if (0 == strcasecmp (requestMethod, "GET")) {
#ifdef DEBUG
    	simLog (LOG_INFO, "%s GET Entered: RequestUri: %s\n", host, pathname);
#endif
        if (!*pathname)
            return 0;

        struct stat st;
        st.st_size = 0;
        if (NULL != (fin = fopen (pathname, "rb"))) {
            getDigest (pathname, digest, uid ? true : false);

            if (!*digest) {
                simLog (LOG_ERR, "'md5sum %s' failed w/ (NULL) digest!\n", pathname);
                fprintf (stdout, "Status: 404 Not found\n");
                fprintf (stdout, "Content-Type: text/plain\n");
                fprintf (stdout, "Content-Length: 0\n");
                fprintf (stdout, "Connection: close\n");
                fprintf (stdout, "\n");
                return 0;
            }

            fstat (fileno (fin), &st);
            // Open is good, dump out HTTP header for caller
            fprintf (stdout, "Status: 200 OK\n");
            fprintf (stdout, "Content-Type: application/octet-stream\n");
            // Atmos will set a uid
            if (uid) {
                fprintf (stdout, "x-emc-policy: default\n");
                fprintf (stdout, "x-emc-wschecksum: sha0/0/%s\n", digest);
                fprintf (stdout, "Content-Length: %ld\n", (long)st.st_size);
            }
            else {
                fprintf (stdout, "Content-Length: %ld\n", (long)st.st_size);
                fprintf (stdout, "ETag: \"%s\"\n", digest);
            }
            fprintf (stdout, "\n");

            // Dump the file - hopefully it swallows 8bit
            totalSize = 0;
            while (true) {
                count = fread (buffer, 1, 65535, fin);
                if (count > 0) {
                    fwrite (buffer, 1, count, stdout);
                    totalSize += count;
                }
                else {
                    break;			// Short read, just bail
                }

                if (feof (fin))
                    break;
            }
#ifdef DEBUG
            simLog (LOG_INFO, "%s Sent: %s (%s) len:%ld, sent:%ld\n",
                    host, pathname, digest, (long)st.st_size, totalSize);
#endif
            fclose (fin);
        } // fopen
        else {		// open failed
            fprintf (stdout, "Status: 404 File cannot be opened for reading\n");
            fprintf (stdout, "Content-Type: text/plain\n");
            fprintf (stdout, "Content-Length: 0\n");
            fprintf (stdout, "Connection: close\n");
            fprintf (stdout, "\n");
        }
	} // GET

    // ------------------------------------------------------------
    // HTTP PUT/POST - create the object
    // ------------------------------------------------------------
    else if (0 == strcasecmp (requestMethod, "PUT") ||
             0 == strcasecmp (requestMethod, "POST")) {
#ifdef DEBUG
    	simLog (LOG_INFO, "%s %s Entered: RequestUri: %s\n", 
                host, requestMethod, pathname);
#endif

	    totalSize = 0;
	    if (*pathname) {
	    	createPath (pathname);
	    	if (NULL != (fout = fopen (pathname, "wb"))) {
	    		while (true) {
	    			count = fread (buffer, 1, 65535, stdin);

                    /* remove for verbose buff logging
                    for (int i = 0; i < count; i++)
                        simLog(LOG_INFO, "buff[i]: %x", buffer[i]);
                    */
	    			if (count > 0)
	    				fwrite (buffer, 1, count, fout);
				    else 
                        break;		// short read, just bail

				    totalSize += count;
				    if (feof (stdin)) 
                        break;
				}
			    fflush (fout);
			    fclose (fout);
			}
		}
	    else {
            totalSize = -1;
        }

	    getDigest (pathname, digest, uid ? true : false);
#ifdef DEBUG
	    simLog (LOG_INFO, "%s %s: %s (%s) len:%ld\n", 
                host, requestMethod, pathname, digest, totalSize);
#endif
        
        // Atmos will set UID
        if (uid) {
	        fprintf (stdout, "Status: 201 Created\n");		// the connector script needs this
            fprintf (stdout, "location: %s\n", pathname + 13);
        }
        else {
	        fprintf (stdout, "Status: 200 OK\n");		// the connector script needs this
        }
	    etime = getTime ();
	    fprintf (stdout, "Content-Length: 0\n");
	    fprintf (stdout, "Connection: close\n");
	    fprintf (stdout, "Content-Type: text/plain; charset=UTF-8\n");
        if (uid) {
            fprintf (stdout, "x-emc-delta: %.0f\n", etime - stime);
            fprintf (stdout, "x-emc-policy: default\n");
            // if checksum was sent, we need to return ours
            if (*wschecksum) {
                getDigest (pathname, digest, uid ? true : false);
                fprintf (stdout, "x-emc-wschecksum: sha0/0/%s\n", digest);
            }
        }
        else {
	        fprintf (stdout, "ETag: \"%s\"\n", digest);
        }
	    fprintf (stdout, "\n");
	} // PUT/POST
} // main

