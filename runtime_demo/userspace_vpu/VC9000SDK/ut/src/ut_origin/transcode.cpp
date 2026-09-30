#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"
#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"
#include "catch2/catch.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "log.h"
#include "stream.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */

// TODO
#include "device_memleak_detection.hpp"

TEST_CASE( "unitest_memleak_detection_start", "[sample][device][normal][memleak][unitest]" ) {
    // class DetectionFixture detecter;
    runDetection(true);
}