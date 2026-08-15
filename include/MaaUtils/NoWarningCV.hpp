#pragma once

#include "MaaUtils/Conf.h"

MAA_SUPPRESS_CV_WARNINGS_BEGIN
#include <opencv2/opencv.hpp>
// IAA: OpenCV 5.0 官方 iOS framework 的 opencv.hpp 聚合头漏了 geometry 模块
// （opencv_modules.hpp 定义了 HAVE_OPENCV_GEOMETRY 但聚合头没有对应 include），
// minAreaRect/contourArea/getPerspectiveTransform 已从 imgproc 移到 geometry。
// 所有 CV 代码都经此头引入，统一补上（Android 的 OpenCV 4.x 无此模块，用宏守卫）。
#ifdef HAVE_OPENCV_GEOMETRY
#include <opencv2/geometry.hpp>
#endif
MAA_SUPPRESS_CV_WARNINGS_END
