#include "raytracer.hpp"
#include "timer.hpp"
using namespace kiss;

v3 reflect (v3 normal, v3 v) {
	return v - normal * (2 * dot(normal, v));
}

struct Sphere : Object {
	v3	pos;
	flt	radius;

	Sphere (v3 pos, flt radius, Material mat): Object{mat}, pos{pos}, radius{radius} {}
	virtual ~Sphere () {}

	virtual bool intersect (Ray const& ray, Hit* hit) const {
		v3 sphere_pos_ray = pos - ray.pos;
		flt sphere_dist = dot(ray.dir, sphere_pos_ray);

		v3 nearest_p_ray = ray.dir * sphere_dist;
		flt least_r_sqr = length_sqr(nearest_p_ray - sphere_pos_ray);

		if (least_r_sqr > radius*radius)
			return false; // ray misses sphere

		flt half_secant_len = sqrt(radius*radius - least_r_sqr); // c^2 - a^2 = b^2

		flt enter_dist = sphere_dist - half_secant_len;
		flt exit_dist = sphere_dist + half_secant_len;

		if (exit_dist < 0)
			return false; // sphere is behind ray

		if (enter_dist < 0)
			return false; // ray starts inside sphere

		hit->dist = enter_dist;
		hit->pos = ray.dir * hit->dist + ray.pos;
		hit->normal = normalize(hit->pos - pos);
		return true;
	}
};
struct Plane : Object {
	v3	pos;
	v2	size;

	Plane (v3 pos, v2 size, Material mat): Object{mat}, pos{pos}, size{size} {}
	virtual ~Plane () {}

	virtual bool intersect (Ray const& ray, Hit* hit) const {

		if (ray.dir.z == 0)
			return false; // ray parallel to plane, plane invisible

		v3 ray_pos_plane = ray.pos - pos;

		if (ray.dir.z * ray_pos_plane.z > 0)
			return false; // ray below plane pointing down or above pointing up

		v2 dxy = (v2)ray.dir / ray.dir.z * -ray_pos_plane.z;

		v2 hit_xy = (v2)ray_pos_plane + dxy;

		v2 hit_uv = hit_xy / size + 0.5f;

		if (any(hit_uv < 0 || hit_uv > 1))
			return false;

		hit->dist = length(v3(dxy, -ray_pos_plane.z));
		hit->pos = v3(hit_xy, 0) + pos;
		hit->normal = v3(0,0,+1);
		hit->uv = hit_uv;
		return true;
	}
};

Raytracer::Raytracer () {
	auto wood_tex1 = make_shared<Image<srgb8>>("textures/Wood_Particle_Board_002_basecolor.jpg");
	scene.skybox = make_shared<Image<lrgb>>("textures/Alexs_Apartment/Alexs_Apt_2k.hdr");

	scene.objs.push_back( make_unique<Sphere>(v3(0.0f,0.0f,0.5f), 0.5f, Material{srgb(255, 170, 160), 0.0f}) );

	scene.objs.push_back( make_unique<Sphere>(v3(1,0,0.3f), 0.3f, Material{srgb(55, 230, 160), 0.9f}) );
	scene.objs.push_back( make_unique<Sphere>(v3(-1,-0.4f,0.25f), 0.2f, Material{{srgb(140, 255, 160)}, {0.9f}, {lrgb(0.4f, 0.2f, 0.2f) * 10}}) );
	scene.objs.push_back( make_unique<Sphere>(v3(0.4f,-0.6f,0.25f), 0.25f, Material{srgb(100, 70, 255), 0.9f}) );

	scene.objs.push_back( make_unique<Plane>(0.0f, 5.0f, Material{MatTexture{wood_tex1, v2(5.0f)}}) );
}

bool show_debug = false;
lrgb debug_color = 0;

void DEBUG (lrgb col) {
	if (!show_debug) {
		debug_color = col;
		show_debug = true;
	}
}
void DEBUG (float f) {
	if (!show_debug) {
		debug_color = lrgb(f);
		show_debug = true;
	}
}

std::vector<v3> gen_sample_dirs_tangent (int samples) {
	std::vector<v3> arr;

	for (int elev_i=0; elev_i<samples/2; ++elev_i) {
		flt elev_t = (flt)elev_i / (flt)(samples/2);
		flt elev = lerp(deg(90), 0, elev_t);

		v3 dir = v3(0,1,0);
		dir = rotate3_X(elev) * dir;

		for (int azim_i=0; azim_i<samples; ++azim_i) {
			flt azim_t = (flt)azim_i / (flt)samples;
			flt azim = lerp(0, deg(360), azim_t);

			v3 dir_tangent = rotate3_Z(azim) * dir;

			arr.push_back(dir_tangent);
		}
	}

	return arr;
}

template <typename FUNC>
int sample_uniform (int samples, v3 normal, FUNC sample) {

	v3 tangent = equal(normal, v3(1,0,0)) ? v3(0,0,1) : v3(1,0,0);
	v3 cotangent = normalize(cross(normal, tangent));
	tangent = normalize(cross(cotangent, normal));

	m3 tangent_to_world = m3::rows(tangent, cotangent, normal);

	assert(samples >= 2);
	int total_samples = (samples/2) * samples;

	static std::vector<v3> sample_dirs;
	if (sample_dirs.size() != (size_t)total_samples)
		sample_dirs = gen_sample_dirs_tangent(samples);

	for (v3 sample_dir : sample_dirs) {
		v3 dir = tangent_to_world * sample_dir;
		sample(dir);
	}

	return total_samples;
};

int max_bounces = 2;

Object const* cast_ray (Scene const& scene, Ray const& ray, Object const* source_obj, Hit* hit) {
	Object const* hit_obj = nullptr;
	hit->dist = INF;
	
	for (auto& obj : scene.objs) {
		if (obj.get() != source_obj) { // prevent rays from hitting the object they bounced from
			Hit h;
			if (obj->intersect(ray, &h) && h.dist > 0 && h.dist < hit->dist) {
				hit_obj = obj.get();
				*hit = h;
			}
		}
	}

	return hit_obj;
}

lrgb raytrace (Scene const& scene, Ray const& ray, Object const* bounce_obj=nullptr, int bounces=0) {
	//DBGLOG("raytrace(ray.pos: %g %g %g ray.dir: %g %g %g, bounces: %d)\n", ray.pos.x, ray.pos.y, ray.pos.z, ray.dir.x, ray.dir.y, ray.dir.z, bounces);
	
	Hit hit;
	Object const* hit_obj = cast_ray(scene, ray, bounce_obj, &hit);

	if (hit_obj && bounce_obj == hit_obj) {
		DEBUG(lrgb(1,0,0));
		//assert(false);
	}

	if (hit.dist < 0.001f)
		return lrgb(1,0,1);

	if (!hit_obj) {
		return scene.skybox->sample_equirectangular(ray.dir);
	} else {
		if (bounces == max_bounces) {
			// what do?
			//return 0;
			return lrgb(0,1,0); // for debugging
		}

		return hit_obj->material_response(hit, ray.dir, [&] (Ray ray) {
			return raytrace(scene, ray, hit_obj, bounces + 1);
		});
	}
}

bool is_danger_float (flt f) { return !std::isnormal(f) && f != 0; } 

lrgb Raytracer::raytrace_pixel (iv2 pix_pos, iv2 size) {
	show_debug = false;

	Ray ray;
	ray.dir = cam.get_screen_ray((v2)pix_pos / (v2)size, (flt)size.x / (flt)size.y, &ray.pos);

	auto col = raytrace(scene, ray) * 3;

	bool danger = is_danger_float(col.x) || is_danger_float(col.y) || is_danger_float(col.z);
	if (danger) {
		printf("Beep boop dangerous float detected! %g %g %g at pixel %d %d\n", col.x, col.y, col.z, pix_pos.x, pix_pos.y);
	}
	
	if (show_debug) {
		col = debug_color;
	}

	return col;
}

void Raytracer::frame (kiss::Image<lrgb>* img, iv2 mouse_pos, Input& inp) {
	cam.update(0, false, 0, 0);
	cam.calc_matricies();

	if (all(mouse_pos >= 0 && mouse_pos < img->size)) {
		static iv2 dbg_pos = 0;

		if (inp.went_down('M')) {
			dbg_pos = mouse_pos;

			for (int x=0; x<img->size.x; ++x)
				img->get_pixel(iv2(x,dbg_pos.y)) += srgb(255,0,0);
			for (int y=0; y<img->size.y; ++y)
				img->get_pixel(iv2(dbg_pos.x,y)) += srgb(255,0,0);

			printf("debug raytrace pixel set to %d %d\n", dbg_pos.x,dbg_pos.y);
		}
		if (inp.went_down('N')) {

			Timer t;
			t.start();

			auto col = raytrace_pixel(dbg_pos, img->size);
			img->get_pixel(dbg_pos) = col;

			flt total_time = t.end();

			printf("debug raytrace pixel %d %d raytraced, time: %7.4f ms -> color %g %g %g\n", dbg_pos.x,dbg_pos.y, total_time * 1000, col.x, col.y, col.z);
		}
	}

	update_pattern.update(img->size);

	Timer timer;
	timer.start();

	flt max_time = 0.016f;

	static flt total_time = 0;
	static int total_pixels = 0;

	iv2 pixel_pos;
	while (timer.end() <= max_time && update_pattern.get_next_pixel(&pixel_pos)) {
		//DBGLOG("pixel: %d %d\n", pixel_pos.x, pixel_pos.y);

		Timer t;
		t.start();

		img->get_pixel(pixel_pos) = raytrace_pixel(pixel_pos, img->size);

		total_time += t.end();
		total_pixels++;
	}

	if (total_pixels > 0 && update_pattern.done()) {
		printf("%dx%d raytraced, total time: %7.4f ms, %7.4f ms per px\n", img->size.x,img->size.y, total_time * 1000, total_time / (flt)total_pixels * 1000);

		total_time = 0;
		total_pixels = 0;
	}
}

template <typename FUNC>
lrgb Object::material_response (Hit const& hit, v3 ray_dir, FUNC raytrace) const {
	lrgb diffuse = mat.diffuse.sample(hit.uv);
	flt roughness = mat.roughness.sample(hit.uv);
	lrgb emmisive = mat.emmisive.sample(hit.uv);

	lrgb col = emmisive;
	
	Ray bounced;
	bounced.pos = hit.pos;

	if (roughness == 0.0f) {

		bounced.dir = reflect(hit.normal, ray_dir);
		col += raytrace(bounced) * 0.95f;
	} else {
		
		lrgb diffuse_light = 0;

		int sample_count = sample_uniform(7, hit.normal, [&] (v3 dir) {
			bounced.dir = dir;
			
			flt d = dot(hit.normal, bounced.dir);
			
			diffuse_light += d * raytrace(bounced);
		});
		
		diffuse_light /= (flt)sample_count;

		col += (lrgb)(diffuse_light * diffuse);
	}

	return col;
}
