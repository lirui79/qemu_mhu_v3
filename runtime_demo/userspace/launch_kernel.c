// SPDX-License-Identifier: MIT

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "r52_runtime.h"

int main(void)
{
	struct r52_runtime runtime = { .fd = -1 };
	struct r52_gpu_launch launch = {
		.grid_x = 1024,
		.grid_y = 512,
		.grid_z = 1,
		.workgroup_x = 64,
		.workgroup_y = 64,
		.workgroup_z = 1,
	};
	struct r52_gpu_completion completion;
	struct r52_gpu_info info;

	if (r52_runtime_open(&runtime) < 0) {
		fprintf(stderr, "runtime open: %s\n", strerror(errno));
		return 1;
	}
	if (r52_runtime_query(&runtime, &info) < 0) {
		fprintf(stderr, "query: %s\n", strerror(errno));
		r52_runtime_close(&runtime);
		return 1;
	}
	printf("R52 GPU %04x:%04x, %u SM, ABI %u\n",
	       info.vendor_id, info.device_id, info.sm_count,
	       info.abi_version);

	if (r52_runtime_launch(&runtime, &launch, &completion, 5000) < 0) {
		fprintf(stderr, "launch: %s\n", strerror(errno));
		r52_runtime_close(&runtime);
		return 1;
	}
	printf("sequence=%" PRIu64 " completed in %" PRIu64 " us\n",
	       (uint64_t)completion.sequence,
	       (uint64_t)((completion.end_ns - completion.start_ns) / 1000));
	r52_runtime_close(&runtime);
	return 0;
}
