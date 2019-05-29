#include "kiss.hpp"
using namespace kiss;

#include "color.hpp"
#include "image.hpp"
#include "texture.hpp"
#include "raytracer.hpp"

void load_ortho_projection (flt cam_h, iv2 viewport_size, flt near=-10, flt far=100) {
	flt cam_w = cam_h * ( (flt)viewport_size.x / (flt)viewport_size.y );

	flt x = 2.0f / cam_w;
	flt y = 2.0f / cam_h;

	flt a = 1.0f / (far - near);
	flt b = near * a;

	fm4 cam_to_clip = fm4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, 0, 1
	);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&cam_to_clip.arr[0][0]);
}

void draw_quad (v2 pos, v2 size, Texture const& tex) {
	glBindTexture(GL_TEXTURE_2D, tex.handle);

	glColor3f(1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);

	glTexCoord2f(0,0);
	glVertex2f(pos.x - size.x/2, pos.y - size.y/2);

	glTexCoord2f(1,0);
	glVertex2f(pos.x + size.x/2, pos.y - size.y/2);

	glTexCoord2f(1,1);
	glVertex2f(pos.x + size.x/2, pos.y + size.y/2);

	glTexCoord2f(0,1);
	glVertex2f(pos.x - size.x/2, pos.y + size.y/2);

	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

////
template <typename T>
struct Texture_Streamer {
	Image<T>	cpu_texture;
	Texture		gpu_texture;

	void resize (iv2 size) {
		if (!equal(cpu_texture.size, size)) {
			cpu_texture = Image<T>(size);
		}
	}

	void stream () {
		gpu_texture.reupload(GL_TEXTURE_2D, cpu_texture.get_pixels(), cpu_texture.size);
	}
};

void frame (kiss::Input& inp) {

	flt render_scale = 0.5f;
	flt cam_h = 1;

	load_ortho_projection(cam_h, inp.window_size); // this 'projection just displays the fullscreen rect that contains the cpu sided raytraced image'

	glViewport(0, 0, inp.window_size.x, inp.window_size.y);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	static Texture_Streamer<lrgb> texture;
	texture.resize(roundi((v2)inp.window_size * render_scale));

	iv2 mouse_pos_pix = floori(inp.get_mouse_pos_pixel_center() * render_scale);

	static Raytracer raytracer;
	raytracer.frame(&texture.cpu_texture, mouse_pos_pix, inp);

	texture.stream();

	flt cam_w = cam_h * ( (flt)inp.window_size.x / (flt)inp.window_size.y );

	draw_quad(0, v2(cam_w, cam_h), texture.gpu_texture);
}

#include "timer.hpp"

int main () {
	auto kiss_wnd = kiss::Window("Kiss Test Window", 500);
	auto ogl = kiss::Opengl_Context(kiss_wnd, {3,1}, true, true);

	kiss::DT_Measure dtm;
	float dt = dtm.start();

	for (;;) {
		auto inp = kiss_wnd.get_input();

		if (inp.close)
			break;

		frame(inp);

		ogl.swap_buffers();

		float dt = dtm.frame_end();
	}
	return 0;
}
