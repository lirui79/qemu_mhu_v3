#ifndef __HSA_PROTOCOL_H__
#define __HSA_PROTOCOL_H__

#include <stdint.h>

/** \defgroup signals Signals
 *  @{
 */

/**
 * @brief Signal handle.
 */
typedef struct hsa_signal_s {
  /**
   * Opaque handle. Two handles reference the same object of the enclosing type
   * if and only if they are equal. The value 0 is reserved.
   */
  uint64_t handle;
} hsa_signal_t;

/**
 * @brief Packet type.
 */
typedef enum {
  /**
   * Vendor-specific packet.
   */
  HSA_PACKET_TYPE_VENDOR_SPECIFIC = 0,
  /**
   * The packet has been processed in the past, but has not been reassigned to
   * the packet processor. A packet processor must not process a packet of this
   * type. All queues support this packet type.
   */
  HSA_PACKET_TYPE_INVALID = 1,
  /**
   * Packet used by agents for dispatching jobs to kernel agents. Not all
   * queues support packets of this type (see ::hsa_queue_feature_t).
   */
  HSA_PACKET_TYPE_KERNEL_DISPATCH = 2,
  /**
   * Packet used by agents to delay processing of subsequent packets, and to
   * express complex dependencies between multiple packets. All queues support
   * this packet type.
   */
  HSA_PACKET_TYPE_BARRIER_AND = 3,
  /**
   * Packet used by agents for dispatching jobs to agents.  Not all
   * queues support packets of this type (see ::hsa_queue_feature_t).
   */
  HSA_PACKET_TYPE_AGENT_DISPATCH = 4,
  /**
   * Packet used by agents to delay processing of subsequent packets, and to
   * express complex dependencies between multiple packets. All queues support
   * this packet type.
   */
  HSA_PACKET_TYPE_BARRIER_OR = 5
} hsa_packet_type_t;

/**
 * @brief Scope of the memory fence operation associated with a packet.
 */
typedef enum {
  /**
   * No scope (no fence is applied). The packet relies on external fences to
   * ensure visibility of memory updates.
   */
  HSA_FENCE_SCOPE_NONE = 0,
  /**
   * The fence is applied with agent scope for the global segment.
   */
  HSA_FENCE_SCOPE_AGENT = 1,
  /**
   * The fence is applied across both agent and system scope for the global
   * segment.
   */
  HSA_FENCE_SCOPE_SYSTEM = 2
} hsa_fence_scope_t;

/**
 * @brief Sub-fields of the @a header field that is present in any AQL
 * packet. The offset (with respect to the address of @a header) of a sub-field
 * is identical to its enumeration constant. The width of each sub-field is
 * determined by the corresponding value in ::hsa_packet_header_width_t. The
 * offset and the width are expressed in bits.
 */
 typedef enum {
  /**
   * Packet type. The value of this sub-field must be one of
   * ::hsa_packet_type_t. If the type is ::HSA_PACKET_TYPE_VENDOR_SPECIFIC, the
   * packet layout is vendor-specific.
   */
   HSA_PACKET_HEADER_TYPE = 0,
  /**
   * Barrier bit. If the barrier bit is set, the processing of the current
   * packet only launches when all preceding packets (within the same queue) are
   * complete.
   */
   HSA_PACKET_HEADER_BARRIER = 8,
  /**
   * Acquire fence scope. The value of this sub-field determines the scope and
   * type of the memory fence operation applied before the packet enters the
   * active phase. An acquire fence ensures that any subsequent global segment
   * or image loads by any unit of execution that belongs to a dispatch that has
   * not yet entered the active phase on any queue of the same kernel agent,
   * sees any data previously released at the scopes specified by the acquire
   * fence. The value of this sub-field must be one of ::hsa_fence_scope_t.
   */
   HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE = 9,
   /**
    * @deprecated Renamed as ::HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE.
    */
   HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE = 9,
  /**
   * Release fence scope, The value of this sub-field determines the scope and
   * type of the memory fence operation applied after kernel completion but
   * before the packet is completed. A release fence makes any global segment or
   * image data that was stored by any unit of execution that belonged to a
   * dispatch that has completed the active phase on any queue of the same
   * kernel agent visible in all the scopes specified by the release fence. The
   * value of this sub-field must be one of ::hsa_fence_scope_t.
   */
   HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE = 11,
   /**
    * @deprecated Renamed as ::HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE.
    */
   HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE = 11
 } hsa_packet_header_t;

/**
 * @brief Width (in bits) of the sub-fields in ::hsa_packet_header_t.
 */
 typedef enum {
   HSA_PACKET_HEADER_WIDTH_TYPE = 8,
   HSA_PACKET_HEADER_WIDTH_BARRIER = 1,
   HSA_PACKET_HEADER_WIDTH_SCACQUIRE_FENCE_SCOPE = 2,
   /**
    * @deprecated Use HSA_PACKET_HEADER_WIDTH_SCACQUIRE_FENCE_SCOPE.
    */
   HSA_PACKET_HEADER_WIDTH_ACQUIRE_FENCE_SCOPE = 2,
   HSA_PACKET_HEADER_WIDTH_SCRELEASE_FENCE_SCOPE = 2,
   /**
    * @deprecated Use HSA_PACKET_HEADER_WIDTH_SCRELEASE_FENCE_SCOPE.
    */
   HSA_PACKET_HEADER_WIDTH_RELEASE_FENCE_SCOPE = 2
 } hsa_packet_header_width_t;

/**
 * @brief AQL kernel dispatch packet
 */
typedef struct hsa_kernel_dispatch_packet_s {
  /**
   * Packet header. Used to configure multiple packet parameters such as the
   * packet type. The parameters are described by ::hsa_packet_header_t.
   */
  uint16_t header;

  /**
   * Dispatch setup parameters. Used to configure kernel dispatch parameters
   * such as the number of dimensions in the grid. The parameters are described
   * by ::hsa_kernel_dispatch_packet_setup_t.
   */
  uint16_t setup;

  /**
   * X dimension of work-group, in work-items. Must be greater than 0.
   */
  uint16_t workgroup_size_x;

  /**
   * Y dimension of work-group, in work-items. Must be greater than
   * 0. If the grid has 1 dimension, the only valid value is 1.
   */
  uint16_t workgroup_size_y;

  /**
   * Z dimension of work-group, in work-items. Must be greater than
   * 0. If the grid has 1 or 2 dimensions, the only valid value is 1.
   */
  uint16_t workgroup_size_z;

  /**
   * Reserved. Must be 0.
   */
  uint16_t reserved0;

  /**
   * X dimension of grid, in work-items. Must be greater than 0. Must
   * not be smaller than @a workgroup_size_x.
   */
  uint32_t grid_size_x;

  /**
   * Y dimension of grid, in work-items. Must be greater than 0. If the grid has
   * 1 dimension, the only valid value is 1. Must not be smaller than @a
   * workgroup_size_y.
   */
  uint32_t grid_size_y;

  /**
   * Z dimension of grid, in work-items. Must be greater than 0. If the grid has
   * 1 or 2 dimensions, the only valid value is 1. Must not be smaller than @a
   * workgroup_size_z.
   */
  uint32_t grid_size_z;

  /**
   * Size in bytes of private memory allocation request (per work-item).
   */
  uint32_t private_segment_size;

  /**
   * Size in bytes of group memory allocation request (per work-group). Must not
   * be less than the sum of the group memory used by the kernel (and the
   * functions it calls directly or indirectly) and the dynamically allocated
   * group segment variables.
   */
  uint32_t group_segment_size;

  /**
   * Opaque handle to a code object that includes an implementation-defined
   * executable code for the kernel.
   */
  uint64_t kernel_object;

  /**
   * Pointer to a buffer containing the kernel arguments. May be NULL.
   *
   * The buffer must be allocated using ::hsa_memory_allocate, and must not be
   * modified once the kernel dispatch packet is enqueued until the dispatch has
   * completed execution.
   */
  uint64_t kernarg_address;

  /**
   * Reserved. Must be 0.
   */
  uint64_t reserved2;

  /**
   * Signal used to indicate completion of the job. The application can use the
   * special signal handle 0 to indicate that no signal is used.
   */
  hsa_signal_t completion_signal;

} hsa_kernel_dispatch_packet_t;



// AMD Signal Kind Enumeration Values.
typedef int64_t amd_signal_kind64_t;
enum amd_signal_kind_t {
  AMD_SIGNAL_KIND_INVALID = 0,
  AMD_SIGNAL_KIND_USER = 1,
  AMD_SIGNAL_KIND_DOORBELL = -1,
  AMD_SIGNAL_KIND_LEGACY_DOORBELL = -2
};

// AMD Signal.
typedef struct amd_signal_s {
  amd_signal_kind64_t kind;
  union {
    volatile int64_t value;
    volatile uint32_t* legacy_hardware_doorbell_ptr;
    volatile uint64_t* hardware_doorbell_ptr;
  };
  uint64_t event_mailbox_ptr;
  uint32_t event_id;
  uint32_t reserved1;
  uint64_t start_ts;
  uint64_t end_ts;
  union {
    void* queue_ptr;
    uint64_t reserved2;
  };
  uint32_t reserved3[2];
} amd_signal_t;



/**
 * @brief Queue type. Intended to be used for dynamic queue protocol
 * determination.
 */
typedef enum {
  /**
   * Queue supports multiple producers. Use of multiproducer queue mechanics is
   * required.
   */
  HSA_QUEUE_TYPE_MULTI = 0,
  /**
   * Queue only supports a single producer. In some scenarios, the application
   * may want to limit the submission of AQL packets to a single agent. Queues
   * that support a single producer may be more efficient than queues supporting
   * multiple producers. Use of multiproducer queue mechanics is not supported.
   */
  HSA_QUEUE_TYPE_SINGLE = 1,
  /**
   * Queue supports multiple producers and cooperative dispatches. Cooperative
   * dispatches are able to use GWS synchronization. Queues of this type may be
   * limited in number. The runtime may return the same queue to serve multiple
   * ::hsa_queue_create calls when this type is given. Callers must inspect the
   * returned queue to discover queue size. Queues of this type are reference
   * counted and require a matching number of ::hsa_queue_destroy calls to
   * release. Use of multiproducer queue mechanics is required. See
   * ::HSA_AMD_AGENT_INFO_COOPERATIVE_QUEUES to query agent support for this
   * type.
   */
  HSA_QUEUE_TYPE_COOPERATIVE = 2
} hsa_queue_type_t;

/**
 * @brief A fixed-size type used to represent ::hsa_queue_type_t constants.
 */
typedef uint32_t hsa_queue_type32_t;

/**
 * @brief Queue features.
 */
typedef enum {
  /**
   * Queue supports kernel dispatch packets.
   */
  HSA_QUEUE_FEATURE_KERNEL_DISPATCH = 1,

  /**
   * Queue supports agent dispatch packets.
   */
  HSA_QUEUE_FEATURE_AGENT_DISPATCH = 2
} hsa_queue_feature_t;

/**
 * @brief User mode queue.
 *
 * @details The queue structure is read-only and allocated by the HSA runtime,
 * but agents can directly modify the contents of the buffer pointed by @a
 * base_address, or use HSA runtime APIs to access the doorbell signal.
 *
 */
typedef struct hsa_queue_s {
  /**
   * Queue type.
   */
  hsa_queue_type32_t type;

  /**
   * Queue features mask. This is a bit-field of ::hsa_queue_feature_t
   * values. Applications should ignore any unknown set bits.
   */
  uint32_t features;

  /**
   * Starting address of the HSA runtime-allocated buffer used to store the AQL
   * packets. Must be aligned to the size of an AQL packet.
   */
  uint64_t base_address;

  /**
   * Signal object used by the application to indicate the ID of a packet that
   * is ready to be processed. The HSA runtime manages the doorbell signal. If
   * the application tries to replace or destroy this signal, the behavior is
   * undefined.
   *
   * If @a type is ::HSA_QUEUE_TYPE_SINGLE, the doorbell signal value must be
   * updated in a monotonically increasing fashion. If @a type is
   * ::HSA_QUEUE_TYPE_MULTI, the doorbell signal value can be updated with any
   * value.
   */
  hsa_signal_t doorbell_signal;

  /**
   * Maximum number of packets the queue can hold. Must be a power of 2.
   */
  uint32_t size;
  /**
   * Reserved. Must be 0.
   */
  uint32_t reserved1;
  /**
   * Queue identifier, which is unique over the lifetime of the application.
   */
  uint64_t id;

} hsa_queue_t;

// AMD Queue Properties.
typedef uint32_t amd_queue_properties32_t;

/* This is the original amd_queue_t structure. The definition is only kept
 * for reference purposes. This structure should not be used. */
typedef struct amd_queue_s {
  hsa_queue_t hsa_queue;
  uint32_t caps;
  uint32_t reserved1[3];
  volatile uint64_t write_dispatch_id;
  uint32_t group_segment_aperture_base_hi;
  uint32_t private_segment_aperture_base_hi;
  uint32_t max_cu_id;
  uint32_t max_wave_id;
  volatile uint64_t max_legacy_doorbell_dispatch_id_plus_1;
  volatile uint32_t legacy_doorbell_lock;
  uint32_t reserved2[9];
  volatile uint64_t read_dispatch_id;
  uint32_t read_dispatch_id_field_base_byte_offset;
  uint32_t compute_tmpring_size;
  uint32_t scratch_resource_descriptor[4];
  uint64_t scratch_backing_memory_location;
  uint64_t scratch_backing_memory_byte_size;
  uint32_t scratch_wave64_lane_byte_size;
  amd_queue_properties32_t queue_properties;
  uint32_t reserved3[2];
  hsa_signal_t queue_inactive_signal;
  uint32_t reserved4[14];
} amd_queue_t;



// Kernel descriptor. Must be kept backwards compatible.
typedef struct kernel_descriptor_s {
  uint32_t group_segment_fixed_size;
  uint32_t private_segment_fixed_size;
  uint32_t kernarg_size;
  uint8_t reserved0[4];
  int64_t kernel_code_entry_byte_offset;
  uint8_t reserved1[20];
  uint32_t compute_pgm_rsrc3; // GFX10+ and GFX90A+
  uint32_t compute_pgm_rsrc1;
  uint32_t compute_pgm_rsrc2;
  uint16_t kernel_code_properties;
  uint8_t reserved2[6];
} kernel_descriptor_t;

#endif
