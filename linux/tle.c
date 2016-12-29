#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "tle.h"

static int tle_cp(const char *to, const char *from);

int tle_update(void)
{
    CURL *curl;
    FILE *fp;
    CURLcode res;
    long http_code = 0;

    if(!tle.update_enabled)
    {
        return -1;
    }

    curl = curl_easy_init();
    if(curl)
    {   
        fp = fopen(tle.tmp_file,"wb+");
        curl_easy_setopt(curl, CURLOPT_URL, tle.url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    else
    {
    	return -1;
    }

   	if (http_code != 200 || res == CURLE_ABORTED_BY_CALLBACK)
   	{
   		return -1;
   	}

   	if(tle_cp(tle.file, tle.tmp_file) < 0)
   	{
   		return -1;
   	}

    return 0;
}

int tle_load(void)
{
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(tle.file, "r");
    if (fp == NULL)
    {
    	fprintf(stderr, "\nError opening TLE file!");
        return -1;
    }

    int n = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
    	if(read > 50)
    	{
            if(tle.elements[n] != NULL)
            {
                free(tle.elements[n]);
            }
    		tle.elements[n] = malloc(read);
    		strncpy(tle.elements[n], line, read);
    		n++;
    		if(n > 1)
    		{
    			break;
    		}
    	}
    }

    fclose(fp);
    if (line)
        free(line);
    
    if(n != 2)
    {
    	fprintf(stderr,"\nError reading TLE from file!");
    	return -1;
    }

    return 0;
}

/* Internal cp implementation from http://stackoverflow.com/a/2180788 */
static int tle_cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}