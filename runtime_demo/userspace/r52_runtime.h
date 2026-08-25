/* SPDX-License-Identifier: MIT */
#ifndef R52_RUNTIME_H
#define R52_RUNTIME_H

#include <stdint.h>

#include "../driver/r52_gpu_uapi.h"

struct r52_runtime {
	int fd;
	struct r52_gpu_resources resources;
	void *shared;
	volatile uint32_t *pbx;
};

int r52_runtime_open(struct r52_runtime *runtime);
void r52_runtime_close(struct r52_runtime *runtime);
int r52_runtime_query(struct r52_runtime *runtime, struct r52_gpu_info *info);
int r52_runtime_launch(struct r52_runtime *runtime,
		       struct r52_gpu_launch *launch,
		       struct r52_gpu_completion *completion,
		       int timeout_ms);

#endif
