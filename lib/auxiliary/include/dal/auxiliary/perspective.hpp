#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <sung/basic/angle.hpp>


namespace dal {

    template <typename T>
    glm::tmat4x4<T> make_perspective(
        sung::TAngle<T> fovy, T aspect, T zNear, T zFar
    ) {
        auto m = glm::perspectiveRH_ZO(fovy.rad(), aspect, zFar, zNear);
        m[1][1] *= -1;
        return m;
    }

    template <typename T>
    glm::tmat4x4<T> make_perspective(
        sung::TAngle<T> fovy, T width, T height, T zNear, T zFar
    ) {
        auto m = glm::perspectiveFovRH_ZO(
            fovy.rad(), width, height, zFar, zNear
        );
        m[1][1] *= -1;
        return m;
    }


    template <typename T>
    class PerspectiveCamera {

    public:
        using Angle = sung::TAngle<T>;

        void multiply_fov(T factor) {
            fov_.set_rad(
                sung::clamp<T>(fov_.rad() * factor, 0.001, SUNG_PI - 0.001)
            );
        }

        glm::tmat4x4<T> make_proj_mat(T aspect_ratio) const {
            return make_perspective(fov_, aspect_ratio, near_, far_);
        }

        glm::tmat4x4<T> make_proj_mat(T view_width, T view_height) const {
            return make_perspective(
                fov_,
                std::max<T>(view_width, 1),
                std::max<T>(view_height, 1),
                near_,
                far_
            );
        }

        Angle fov_ = Angle::from_deg(60);
        T near_ = 0.01;
        T far_ = 1000;
    };

}  // namespace dal
