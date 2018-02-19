#ifndef PROV_CUBETRACK_H
#define PROV_CUBETRACK_H

#include <vector>
#include "../objects.hpp"
#include "../network/network.hpp"
#include <opencv2/dnn.hpp>
#include <opencv2/dnn/shape_utils.hpp>
#include <opencv2/videoio.hpp>
#include "../network/target.hpp"
#include "yolo_v2_class.hpp"

namespace ObjectTracking {

    class CubeTracker {
    public:
        CubeTracker();
        mutable std::mutex mutexTargets;
        std::vector<Target> targetsLast;
        std::vector<Target> targets;
        Tracker_optflow tracker_flow;

        void update(std::vector<Target>);
        void run(std::function<cv::Mat ()>);
    };

}

#endif