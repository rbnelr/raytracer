#pragma once
#include "kiss.hpp"

#include <vector>

#include "color.hpp"
#include "image.hpp"

using namespace kiss;

struct Camera {
	v3	pos_world = rotate3_Z(deg(30)) * v3(0,-2, +0.8f);
	//v3	ypr = v3(0, deg(90 -10), 0);
	v3	ypr = v3(deg(30), deg(90 -10), 0);

	flt sens = deg(180) / 1000;
	flt	speed = 4;

	flt vfov = deg(55);
	flt near = 0.1f;

	m3x4 world_to_cam;
	m3x4 cam_to_world;

	bool update (v2 mouse_delta, bool look, v3 move_dir_cam, flt dt) {
		if (look) {
			ypr.x += mouse_delta.x * sens;
			ypr.y += mouse_delta.y * sens;

			ypr.x = wrap(ypr.x, deg(360));
			ypr.y = clamp(ypr.y, deg(0), deg(180));
		}
		world_to_cam = rotate3_Z(-ypr.z) * rotate3_X(-ypr.y) * rotate3_Z(-ypr.x) * translate(-pos_world);

		v3 move_dir_world = m3(cam_to_world) * move_dir_cam;

		pos_world += move_dir_world * speed * dt;

		return look || !equal(move_dir_cam, 0);
	}

	void calc_matricies () {
		world_to_cam = rotate3_Z(-ypr.z) * rotate3_X(-ypr.y) * rotate3_Z(-ypr.x) * translate(-pos_world);
		cam_to_world = translate(pos_world) * rotate3_Z(ypr.x) * rotate3_X(ypr.y) * rotate3_Z(ypr.z);
	}

	v3 get_screen_ray (v2 uv, flt screen_w_over_h, v3* ray_pos_world) const {
		v2 near_plane_size;
		near_plane_size.y = tanf(vfov / 2) * near;
		near_plane_size.x = near_plane_size.y * screen_w_over_h;

		v3 ray_pos_cam = v3(near_plane_size * (uv * 2 -1), -near);
		v3 ray_dir_cam = normalize(ray_pos_cam - 0);

		*ray_pos_world = cam_to_world * ray_pos_cam;
		return (m3)cam_to_world * ray_dir_cam;
	}
};

struct Ray {
	v3	pos;
	v3	dir;
};

struct Hit {
	flt	dist;
	v3	pos;
	v3	normal;
	v2	uv;
};

struct MatTexture {
	lrgb tint = 1;

	shared_ptr<Image<srgb8>> tex = nullptr;
	v2 uv_scale = 1;

	MatTexture (lrgb col): tint{col} {}
	MatTexture (shared_ptr<Image<srgb8>> tex, v2 uv_scale=1): tex{tex}, uv_scale{uv_scale} {}
	MatTexture (shared_ptr<Image<srgb8>> tex, lrgb tint, v2 uv_scale=1): tint{tint}, tex{tex}, uv_scale{uv_scale} {}

	lrgb sample (v2 uv) const {
		if (tex == nullptr) {
			return tint;
		} else {
			return (lrgb)tex->sample_nearest(uv * uv_scale) * tint;
		}
	}
};
struct MatTexture1 {
	flt tint = 1;

	shared_ptr<Image<Float>> tex = nullptr;
	v2 uv_scale = 1;

	MatTexture1 (flt val): tint{val} {}
	MatTexture1 (shared_ptr<Image<Float>> tex, v2 uv_scale=1): tex{tex}, uv_scale{uv_scale} {}
	MatTexture1 (shared_ptr<Image<Float>> tex, flt tint, v2 uv_scale=1): tint{tint}, tex{tex}, uv_scale{uv_scale} {}

	flt sample (v2 uv) const {
		if (tex == nullptr) {
			return tint;
		} else {
			return tex->sample_nearest(uv * uv_scale) * tint;
		}
	}
};

struct Material {

	MatTexture diffuse;

	MatTexture1 roughness;
	MatTexture emmisive;

	Material (MatTexture diff, MatTexture1 roughness=MatTexture1(0.8f), MatTexture emmisive=MatTexture(lrgb(0.0f))): diffuse{diff}, roughness{roughness}, emmisive{emmisive} {}
};

struct Object {
	virtual bool intersect (Ray const& ray, Hit* hit) const = 0;

	Material mat;

	Object (Material mat): mat{mat} {}
	virtual ~Object () {}

	template <typename FUNC>
	lrgb material_response (Hit const& hit, v3 ray_dir, FUNC raytrace) const;
};

struct Update_Pattern {
	iv2 size = 0;

	#if 0
	iv2 pos;

	void restart (iv2 screen_size) {
		size = screen_size;
		pos = 0;
	}
	void update (iv2 screen_size) {
		if (!equal(size, screen_size))
			restart(screen_size);
	}
	bool done () {
		return pos.x == size.x;
	}
	bool get_next_pixel (iv2* pixel) { // returns false when all pixels are processed
		if (pos.x == size.x)
			return false;
		
		*pixel = pos;

		pos.y += 1;
		if (pos.y == size.y) {
			pos.y = 0;
			pos.x += 1;
		}

		return true;
	}
	#else
	iv2 pos; // from center
	iv2 dir;
	int pixels_emitted;

	void restart (iv2 screen_size) {
		size = screen_size;
		pos = 0;
		dir = iv2(-1,+1);
		pixels_emitted = 0;
	}
	void update (iv2 screen_size) {
		if (!equal(size, screen_size))
			restart(screen_size);
	}
	bool done () {
		return pixels_emitted == (size.x * size.y);
	}

	iv2 map_pos (iv2 pos) {
		return pos + size / 2;
	}
	bool pos_on_screen (iv2 pos) {
		return all(pos >= 0 && pos < size);
	}
	void inc_pos () {

		if (equal(pos, iv2(0))) {
			pos = iv2(1,0);
		} else {
			pos += dir;

			if (pos.y == 0 && pos.x >= 0)
				pos.x += 1;
			if (any(pos == 0))
				dir = rotate90(dir);
		}
	}

	bool get_next_pixel (iv2* pixel) { // returns false when all pixels are processed
		if (done())
			return false;

		iv2 out_pos;
		do {
			out_pos = map_pos(pos);

			inc_pos();
		} while (!pos_on_screen(out_pos));

		pixels_emitted++;
		*pixel = out_pos;
		return true;
	}
	#endif
};

struct Scene {
	std::vector<unique_ptr<Object>> objs;

	//lrgb sky_col = srgb(170, 180, 255);
	shared_ptr<Image<lrgb>> skybox;
};

struct Raytracer {
	Scene scene;

	Camera cam;

	Update_Pattern update_pattern;

	Raytracer ();

	lrgb Raytracer::raytrace_pixel (iv2 pix_pos, iv2 size);

	void frame (kiss::Image<lrgb>* img);
};
