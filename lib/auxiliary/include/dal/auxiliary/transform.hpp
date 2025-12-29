#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <sung/basic/angle.hpp>


namespace dal {

    template <typename T>
    glm::tquat<T> rotate_quat(
        const glm::tquat<T>& q, sung::TAngle<T> angle, const glm::tvec3<T>& axis
    ) {
        return glm::normalize(glm::angleAxis<T>(angle.rad(), axis) * q);
    }


    template <typename T>
    class TransformQuat {

    public:
        using Angle = sung::TAngle<T>;

        void set_pos(T x, T y, T z) {
            pos_.x = x;
            pos_.y = y;
            pos_.z = z;
        }

        void rotate(Angle angle, const glm::tvec3<T>& axis) {
            rot_ = dal::rotate_quat(rot_, angle, axis);
        }

        void set_rotation(T w, T x, T y, T z) {
            rot_ = glm::quat(w, x, y, z);
            rot_ = glm::normalize(rot_);
        }

        void reset_rotation() { rot_ = glm::quat(1, 0, 0, 0); }

        void set_scale(T x) {
            scale_.x = x;
            scale_.y = x;
            scale_.z = x;
        }

        glm::tvec3<T> make_forward_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(0, 0, -1)
            );
        }

        glm::tvec3<T> make_up_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(0, 1, 0)
            );
        }

        glm::tvec3<T> make_right_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(1, 0, 0)
            );
        }

        glm::tmat4x4<T> make_model_mat() const {
            const auto rot_mat = glm::mat4_cast(rot_);
            const auto scale_mat = glm::scale(glm::tmat4x4<T>(1), scale_);
            const auto translate_mat = glm::translate(glm::tmat4x4<T>(1), pos_);
            return translate_mat * rot_mat * scale_mat;
        }

        glm::tmat4x4<T> make_view_mat() const {
            const auto rot_mat = glm::mat4_cast(glm::conjugate(rot_));
            const auto tran_mat = glm::translate(glm::tmat4x4<T>(1), -pos_);
            return rot_mat * tran_mat;
        }

        template <typename U>
        TransformQuat<U> copy() const {
            TransformQuat<U> ret;
            ret.rot_ = glm::tquat<U>{ rot_ };
            ret.pos_ = glm::tvec3<U>{ pos_ };
            ret.scale_ = glm::tvec3<U>{ scale_ };
            return ret;
        }

        glm::tquat<T> rot_{ 1, 0, 0, 0 };
        glm::tvec3<T> pos_{ 0, 0, 0 };
        glm::tvec3<T> scale_{ 1, 1, 1 };
    };

}  // namespace dal
