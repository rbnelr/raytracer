#include "kiss.hpp"
struct Ngon_Spinner {
	flt ori = 0;

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

	void draw_gradient (v2 pos, v2 size, v3 cola, v3 colb) {
		glBegin(GL_QUADS);

		glColor3f(cola.x, cola.y, cola.z);
		glVertex2f(pos.x - size.x/2, pos.y - size.y/2);

		glColor3f(colb.x, colb.y, colb.z);
		glVertex2f(pos.x + size.x/2, pos.y - size.y/2);

		glColor3f(colb.x, colb.y, colb.z);
		glVertex2f(pos.x + size.x/2, pos.y + size.y/2);

		glColor3f(cola.x, cola.y, cola.z);
		glVertex2f(pos.x - size.x/2, pos.y + size.y/2);

		glEnd();
	}

	void draw_spinning_ngon (int n, v2 pos, flt size) {
		ori += deg(2);
		ori = wrap(ori, deg(360));

		glBegin(GL_TRIANGLE_FAN);

		flt ang = deg(360) / (flt)n;

		for (int i=0; i<n; ++i) {

			v2 vert = (rotate2(-ang/2 + ang * i + ori) * v2(0,-1)) * size + pos.x;

			flt t = (flt)i / (flt)n;

			int a;
			flt b = wrap(t*3, 0.0f,1.0f, &a);

			v3 col = 0;
			col[a] = b;
			col[(a+1) % 3] = 1 -b;

			glColor3f(col.x, col.y, col.z);
			glVertex2f(vert.x, vert.y);
		}

		glEnd();
	}

	void draw (iv2 window_size, int ngon_n=3) {
		load_ortho_projection(10, window_size);

		glViewport(0,0, window_size.x, window_size.y);

		glClearColor(0.3f, 0.3f, 0.3f, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		draw_gradient(0, 10, 0, 1);

		draw_spinning_ngon(ngon_n, 0, 4);
	}
};

#include "timer.hpp"

int main () {
	auto kiss_wnd = kiss::Window("Kiss Test Window", 500);
	auto ogl = kiss::Opengl_Context(kiss_wnd, {3,1}, true, true);

	Ngon_Spinner ngon;

	kiss::DT_Measure dtm;
	float dt = dtm.start();

	for (;;) {
		auto inp = kiss_wnd.get_input();

		if (inp.close)
			break;

		ngon.draw(inp.window_size);

		ogl.swap_buffers();

		float dt = dtm.frame_end();
	}
	return 0;
}
