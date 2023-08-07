#ifndef _AC_CAMERA_H_
#define _AC_CAMERA_H_

#include "public/ap_module_instance.h"

#include "vendor/cglm/cglm.h"

#define AC_CAMERA_MODULE_NAME "AgcmCamera"

BEGIN_DECLS

struct ac_camera {
	vec3 center;
	vec3 center_src;
	vec3 center_dst;
	vec3 eye;
	float distance;
	float distance_src;
	float distance_dst;
	float pitch;
	float pitch_src;
	float pitch_dst;
	float yaw;
	float yaw_src;
	float yaw_dst;
	float t;
	float fov;
	float near;
	float far;
	boolean is_moving;
	mat4 view;
	mat4 proj;
};

struct ac_camera_module * ac_camera_create_module();

struct ac_camera * ac_camera_get_main(struct ac_camera_module * mod);

void ac_camera_init(
	struct ac_camera * cam,
	vec3 center,
	float distance,
	float pitch,
	float yaw,
	float fov,
	float near,
	float far);

void ac_camera_update(
	struct ac_camera * cam,
	float dt);

void ac_camera_translate(
	struct ac_camera * cam,
	float forward_mul,
	float right_mul,
	float up_mul);

void ac_camera_rotate(struct ac_camera * cam, float pitch, float yaw);

void ac_camera_slide(struct ac_camera * cam, float x, float y);

void ac_camera_zoom(struct ac_camera * cam, float distance);

void ac_camera_ray(
	struct ac_camera * cam,
	float sx,
	float sy,
	vec3 dir);

void ac_camera_up(struct ac_camera * cam, vec3 up);

void ac_camera_front(struct ac_camera * cam, vec3 front);

void ac_camera_right(struct ac_camera * cam, vec3 right);

void ac_camera_focus_on(struct ac_camera * cam, vec3 center, float distance);

END_DECLS

#endif /* _AC_CAMERA_H_ */