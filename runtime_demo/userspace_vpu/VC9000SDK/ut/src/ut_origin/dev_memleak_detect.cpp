#include "catch2/catch.hpp"
#include "device_memleak_detection.hpp"

TEST_CASE( "unitest_memleak_detection_end", "[sample][device][normal][memleak][unitest]" ) {
    // class DetectionFixture detecter;
    runDetection(false);
}