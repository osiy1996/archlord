#include "client/ac_camera.h"

static void calc_eye(struct ac_camera * cam)
{
	vec3 v  = { 0.f, 0, -cam->distance };
	mat4 m;
	glm_mat4_identity(m);
	glm_rotate_x(m, glm_rad(cam->pitch), m);
	glm_mat4_mulv3(m, v, 1.f, v);
	glm_mat4_identity(m);
	glm_rotate_y(m, glm_rad(cam->yaw), m);
	glm_mat4_mulv3(m, v, 1.f, v);
	glm_mat4_identity(m);
	glm_translate(m, cam->center);
	glm_mat4_mulv3(m, v, 1.f, cam->eye);
}

static void smooth_cam(struct ac_camera * cam)
{
	glm_vec3_copy(cam->center, cam->center_src);
	cam->distance_src = cam->distance;
	cam->pitch_src = cam->pitch;
	cam->yaw_src = cam->yaw;
	cam->t = 0.f;
	cam->is_moving = TRUE;
}

void ac_camera_init(
	struct ac_camera * cam,
	vec3 center,
	float distance,
	float pitch,
	float yaw,
	float fov,
	float near,
	float far)
{
	memset(cam, 0, sizeof(*cam));
	glm_vec3_copy(center, cam->center);
	glm_vec3_copy(cam->center, cam->center_src);
	glm_vec3_copy(cam->center, cam->center_dst);
	cam->distance = cam->distance_dst = distance;
	cam->pitch = cam->pitch_dst = pitch;
	cam->yaw = cam->yaw_dst = yaw;
	cam->fov = fov;
	cam->near = near;
	cam->far = far;
	calc_eye(cam);
}

void ac_camera_update(
	struct ac_camera * cam,
	float dt)
{
	if (cam->is_moving) {
		cam->t = glm_lerp(cam->t, 1.f, 10.f * dt);
		if (cam->t >= 1.f) {
			glm_vec3_copy(cam->center_dst, cam->center);
			cam->pitch = cam->pitch_dst;
			cam->yaw = cam->yaw_dst;
			cam->distance = cam->distance_dst;
			calc_eye(cam);
			cam->is_moving = FALSE;
		}
		else {
			glm_vec3_lerp(cam->center_src, cam->center_dst,
				cam->t, cam->center);
			cam->pitch = glm_lerp(cam->pitch_src,
				cam->pitch_dst, cam->t);
			cam->yaw = glm_lerp(cam->yaw_src,
				cam->yaw_dst, cam->t);
			cam->distance = glm_lerp(cam->distance_src,
				cam->distance_dst, cam->t);
			calc_eye(cam);
		}
	}
}

void ac_camera_translate(
	struct ac_camera * cam,
	float forward_mul,
	float right_mul,
	float up_mul)
{
	mat4 m;
	vec3 to_center;
	vec3 right;
	vec3 forward;
	vec3 up;
	glm_vec3_sub(cam->center, cam->eye, to_center);
	to_center[1] = 0.f;
	glm_vec3_normalize(to_center);
	glm_vec3_cross(to_center, GLM_YUP, right);
	glm_vec3_scale(to_center, forward_mul, forward);
	glm_vec3_normalize(right);
	glm_vec3_scale(right, right_mul, right);
	glm_vec3_scale(GLM_YUP, up_mul, up);
	glm_mat4_identity(m);
	glm_translate(m, forward);
	glm_translate(m, right);
	glm_translate(m, up);
	glm_mat4_mulv3(m, cam->center_dst, 1.f, cam->center_dst);
	smooth_cam(cam);
}

void ac_camera_rotate(struct ac_camera * cam, float pitch, float yaw)
{
	cam->pitch_dst += pitch;
	cam->pitch_dst = glm_clamp(cam->pitch_dst,
		-89.f, 89.f);
	cam->yaw_dst -= yaw;
	smooth_cam(cam);
}

void ac_camera_slide(struct ac_camera * cam, float x, float y)
{
	mat4 m;
	vec3 forward;
	vec3 right;
	vec3 up;
	glm_vec3_sub(cam->center, cam->eye, forward);
	glm_vec3_normalize(forward);
	glm_vec3_cross(forward, GLM_YUP, right);
	glm_vec3_normalize(right);
	glm_vec3_cross(right, forward, up);
	glm_vec3_normalize(up);
	glm_vec3_scale(right, x, right);
	glm_vec3_scale(up, y, up);
	glm_mat4_identity(m);
	glm_translate(m, right);
	glm_translate(m, up);
	glm_mat4_mulv3(m, cam->center_dst, 1.f, cam->center_dst);
	smooth_cam(cam);
}

void ac_camera_zoom(struct ac_camera * cam, float distance)
{
	cam->distance_dst -= distance;
	if (cam->distance_dst < 1.f)
		cam->distance_dst = 1.f;
	smooth_cam(cam);
}

void ac_camera_ray(
	struct ac_camera * cam,
	float sx,
	float sy,
	vec3 dir)
{
	mat4 m;
	vec4 sp = { sx, sy, 1.f, 1.f };
	vec4 vp;
	vec4 wp;
	glm_mat4_inv(cam->proj, m);
	glm_mat4_mulv(m, sp, vp);
	vp[2] = -1.f;
	vp[3] = 0.f;
	glm_mat4_inv(cam->view, m);
	glm_mat4_mulv(m, vp, wp);
	glm_vec4_copy3(wp, dir);
	glm_vec3_normalize(dir);
}

void ac_camera_up(struct ac_camera * cam, vec3 up)
{
	vec3 front;
	vec3 right;
	glm_vec3_sub(cam->center, cam->eye, front);
	glm_vec3_normalize(front);
	glm_vec3_cross(front, GLM_YUP, right);
	glm_vec3_normalize(right);
	glm_vec3_cross(right, front, up);
	glm_vec3_normalize(up);
}

void ac_camera_front(struct ac_camera * cam, vec3 front)
{
	glm_vec3_sub(cam->center, cam->eye, front);
	glm_vec3_normalize(front);
}

void ac_camera_right(struct ac_camera * cam, vec3 right)
{
	vec3 front;
	glm_vec3_sub(cam->center, cam->eye, front);
	glm_vec3_normalize(front);
	glm_vec3_cross(front, GLM_YUP, right);
	glm_vec3_normalize(right);
}

void ac_camera_focus_on(struct ac_camera * cam, vec3 center, float distance)
{
	glm_vec3_copy(center, cam->center_dst);
	cam->distance_dst = distance;
	smooth_cam(cam);
}
