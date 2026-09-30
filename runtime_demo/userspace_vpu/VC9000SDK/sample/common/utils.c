#include "utils.h"
#include "vmpp_common.h"
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

void read_files_from_dir(struct vmpp_queue *files, const char *directory)
{
    char *url = NULL;
    struct stat st;
    struct dirent *filename;
    DIR *dir;
    int len = 0;

    dir = opendir(directory);
    if (NULL == dir) {
        LOG_ERROR("opendir failed: %s", directory);
        return;
    }

    /* read all the files in the dir ~ */
    while ((filename = readdir(dir)) != NULL) {
        // get rid of "." and ".."
        if (strcmp(filename->d_name, ".") == 0 || strcmp(filename->d_name, "..") == 0 ||
            strncmp(filename->d_name, ".", 1) == 0)
            continue;

        len = strlen(directory) + strlen(filename->d_name) + 2;
        url = (char *)malloc(len);
        if (!url) {
            LOG_ERROR("fail to malloc buffer for url: %s/%s", directory, filename->d_name);
            return;
        }
        memset(url, 0, len);
        sprintf(url, "%s/%s", directory, filename->d_name);

        memset(&st, 0, sizeof(struct stat));
        lstat(url, &st);
        if (S_ISREG(st.st_mode)) {
            vmpp_queue_push_back(files, url);
            continue;
        } else if (S_ISDIR(st.st_mode))
            read_files_from_dir(files, url);
        free(url);
    }
    closedir(dir);
}

int get_available_devices(struct vmpp_queue *devices)
{
    char *url = NULL;
    struct dirent *filename;
    DIR *dir;
    int len = 0;
    const char *directory = "/dev";
    const char *dev_str = "vastai_video";

    dir = opendir(directory);
    if (NULL == dir) {
        LOG_ERROR("opendir failed: %s", directory);
        return 0;
    }

    /* read all the files in the dir ~ */
    while ((filename = readdir(dir)) != NULL) {
        // get rid of "." and ".."
        if (strcmp(filename->d_name, ".") == 0 || strcmp(filename->d_name, "..") == 0 ||
            strncmp(filename->d_name, ".", 1) == 0)
            continue;

        if (!strncmp(dev_str, filename->d_name, strlen(dev_str))) {
            len = strlen(directory) + strlen(filename->d_name) + 2;
            url = (char *)malloc(len);
            if (!url) {
                LOG_ERROR("fail to malloc buffer for url: %s/%s", directory, filename->d_name);
                closedir(dir);
                return vmpp_queue_size(devices);
            }
            memset(url, 0, len);
            sprintf(url, "%s/%s", directory, filename->d_name);
            vmpp_queue_push_back(devices, url);
            LOG_TRACE("found device: %s ", url);
        }
    }
    closedir(dir);
    return vmpp_queue_size(devices);
}

int release_available_devices(struct vmpp_queue *devices, int dev_count)
{
    for (int i = 0; i < dev_count; i++) {
         void * dev = vmpp_queue_peek(devices, i);
         free(dev);
    }
    return 0;
}

int get_default_video_device(char *device, int size)
{
    struct vmpp_queue *devices = NULL;
    int dev_count = 0;
    char *dev = NULL;

    if (!device || size <= 0) {
        LOG_ERROR("invalid parameter for getting the default device");
        return -1;
    }

    if (vmpp_queue_init(&devices) < 0 || !devices) {
        LOG_ERROR("fail to init the queue for the device list");
        return -1;
    }

    dev_count = get_available_devices(devices);
    if (dev_count > 0)
        dev = (char *)vmpp_queue_peek(devices, 0);
    if (dev)
        snprintf(device, size, "%s", dev);

    release_available_devices(devices, dev_count);
    return dev ? 0 : -1;
}