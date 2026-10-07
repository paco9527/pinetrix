#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "urlparser.h"
#include "log.h"

typedef struct _URLSTR_INFO
{
    int url_len;
    int host_idx;
    int port_idx;
    int path_idx;
    int param_idx;
    int anchor_idx;
}URLSTR_INFO;

int char_scan(char* input, int input_len, char symbol)
{
    int idx = 0;
    while(idx < input_len)
    {
        if(*input == symbol)
        {
            return idx;
        }
        idx++;
        input++;
    }
    return -1;
}

int fill_clip_info(URLSTR_INFO* str_info, URL_INFO* url_info)
{
    if(str_info->host_idx != 0)
    {
        url_info->scheme.head = url_info->org_url;
        url_info->scheme.len = str_info->host_idx;
    }
    
    if(str_info->host_idx != 0)
    {
        url_info->host.head = url_info->org_url + str_info->host_idx;
        if(str_info->port_idx != 0)
        {
            url_info->port.head = url_info->org_url + str_info->path_idx - str_info->port_idx;
            url_info->port.len = str_info->path_idx - str_info->port_idx;
            url_info->host.len = str_info->port_idx - str_info->host_idx;
        }
        else
        {
            url_info->host.len = str_info->path_idx - str_info->host_idx;
        }
    }

    if(str_info->port_idx != 0)
    {
        url_info->port.head = url_info->org_url + str_info->port_idx;
        if(str_info->path_idx != 0)
        {
            url_info->port.len = str_info->path_idx - str_info->port_idx;
        }
        else
        {
            url_info->port.len = str_info->url_len - str_info->port_idx;
        }
    }

    if(str_info->path_idx != 0)
    {
        url_info->path.head = url_info->org_url + str_info->path_idx;
        if(str_info->param_idx != 0)
        {
            url_info->path.len = str_info->param_idx - str_info->path_idx;
        }
        else if(str_info->anchor_idx != 0)
        {
            url_info->path.len = str_info->anchor_idx - str_info->path_idx;
        }
        else
        {
            url_info->path.len = str_info->url_len - str_info->path_idx;
        }
    }

    if(str_info->param_idx != 0)
    {
        url_info->param.head = url_info->org_url + str_info->param_idx;
        //有param和anchor
        if(str_info->anchor_idx == 0)
        {
            url_info->param.len = str_info->url_len - str_info->param_idx;
            url_info->anchor.head = NULL;
        }
        else
        {
            url_info->param.len = str_info->anchor_idx - str_info->param_idx;

            url_info->anchor.len = str_info->anchor_idx - str_info->param_idx;
            url_info->anchor.head = url_info->org_url + str_info->anchor_idx;
        }
    }
}

int urlparse(char* org_url, URL_INFO* out_url_info)
{
    URLSTR_INFO tmp_out_info = {0};
    if(!org_url)
    {
        LOG_DEBUG("input url is null");
        return -1;
    }
    URLSTR_INFO* out_info = &tmp_out_info;
    out_info->url_len = strlen(org_url);
    out_url_info->org_url = org_url;

    int idx = 0;
    int offset_idx = 0;
    int options_idx = 0;
    char* p_url = org_url;

    do
    {
        //parse scheme
        offset_idx = char_scan(p_url, out_info->url_len, '/');
        if(idx != -1)
        {
            idx = idx + offset_idx + 2;
            p_url = p_url + offset_idx + 2;
            if(idx >= out_info->url_len)
            {
                LOG_DEBUG("out of input buffer");
                break;
            }
            out_info->host_idx = idx;
        }

        //parse host
        offset_idx = char_scan(p_url, out_info->url_len-idx-1, '/');
        if(idx != -1)
        {
            //if it has port
            options_idx = char_scan(p_url, out_info->url_len-idx-1, ':');
            options_idx = options_idx + idx + 1;
            if(options_idx < idx + offset_idx + 1)
            {
                out_info->port_idx = options_idx;
            }
            idx = idx + offset_idx + 1;
            p_url = p_url + offset_idx + 1;
            if(idx >= out_info->url_len)
            {
                LOG_DEBUG("out of input buffer");
                break;
            }
            out_info->path_idx = idx;
        }

        //parse path
        offset_idx = char_scan(p_url, out_info->url_len-idx-1, '?');
        if(idx != -1)
        {
            idx = idx + offset_idx + 1;
            p_url = p_url + offset_idx + 1;
            if(idx >= out_info->url_len)
            {
                LOG_DEBUG("out of input buffer");
                break;
            }
            out_info->param_idx = idx;
        }

        //parse anchor
        offset_idx = char_scan(p_url, out_info->url_len-idx-1, '#');
        if(offset_idx != -1)
        {
            idx = idx + offset_idx + 1;
            p_url = p_url + offset_idx + 1;
            if(idx >= out_info->url_len)
            {
                LOG_DEBUG("out of input buffer");
                break;
            }
            out_info->anchor_idx = idx;
        }
    }while(0);
    fill_clip_info(out_info, out_url_info);
    
    return 0;
}

#if 0
//just an demo
int main(int argc, char** argv)
{
    URL_INFO info = {0};
//    char* input_url = "https://www.example.com:80/path/to/myfile.html?key1=value1&key2=value2#anchor";
//    char* input_url = "https://www.example.com:80/path/to/";
    printf("ass we can\n");
    urlparse(argv[1], &info);
    return 0;
}
#endif
