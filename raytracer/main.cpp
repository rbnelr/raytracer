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

#define CL_HPP_ENABLE_EXCEPTIONS 1

#include "opencl/cl2.hpp"

#include <fstream>
#include <vector>
#include <unordered_map>

namespace kiss {

	std::unordered_map<cl_int, cstr> ocl_error_codes = {
		{ CL_INVALID_VALUE,                   "CL_INVALID_VALUE" },
		{ CL_INVALID_DEVICE_TYPE,             "CL_INVALID_DEVICE_TYPE" },
		{ CL_INVALID_PLATFORM,                "CL_INVALID_PLATFORM" },
		{ CL_INVALID_DEVICE,                  "CL_INVALID_DEVICE" },
		{ CL_INVALID_CONTEXT,                 "CL_INVALID_CONTEXT" },
		{ CL_INVALID_QUEUE_PROPERTIES,        "CL_INVALID_QUEUE_PROPERTIES" },
		{ CL_INVALID_COMMAND_QUEUE,           "CL_INVALID_COMMAND_QUEUE" },
		{ CL_INVALID_HOST_PTR,                "CL_INVALID_HOST_PTR" },
		{ CL_INVALID_MEM_OBJECT,              "CL_INVALID_MEM_OBJECT" },
		{ CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_INVALID_IMAGE_FORMAT_DESCRIPTO" },
		{ CL_INVALID_IMAGE_SIZE,              "CL_INVALID_IMAGE_SIZE" },
		{ CL_INVALID_SAMPLER,                 "CL_INVALID_SAMPLER" },
		{ CL_INVALID_BINARY,                  "CL_INVALID_BINARY" },
		{ CL_INVALID_BUILD_OPTIONS,           "CL_INVALID_BUILD_OPTIONS" },
		{ CL_INVALID_PROGRAM,                 "CL_INVALID_PROGRAM" },
		{ CL_INVALID_PROGRAM_EXECUTABLE,      "CL_INVALID_PROGRAM_EXECUTABLE" },
		{ CL_INVALID_KERNEL_NAME,             "CL_INVALID_KERNEL_NAME" },
		{ CL_INVALID_KERNEL_DEFINITION,       "CL_INVALID_KERNEL_DEFINITION" },
		{ CL_INVALID_KERNEL,                  "CL_INVALID_KERNEL" },
		{ CL_INVALID_ARG_INDEX,               "CL_INVALID_ARG_INDEX" },
		{ CL_INVALID_ARG_VALUE,               "CL_INVALID_ARG_VALUE" },
		{ CL_INVALID_ARG_SIZE,                "CL_INVALID_ARG_SIZE" },
		{ CL_INVALID_KERNEL_ARGS,             "CL_INVALID_KERNEL_ARGS" },
		{ CL_INVALID_WORK_DIMENSION,          "CL_INVALID_WORK_DIMENSION" },
		{ CL_INVALID_WORK_GROUP_SIZE,         "CL_INVALID_WORK_GROUP_SIZE" },
		{ CL_INVALID_WORK_ITEM_SIZE,          "CL_INVALID_WORK_ITEM_SIZE" },
		{ CL_INVALID_GLOBAL_OFFSET,           "CL_INVALID_GLOBAL_OFFSET" },
		{ CL_INVALID_EVENT_WAIT_LIST,         "CL_INVALID_EVENT_WAIT_LIST" },
		{ CL_INVALID_EVENT,                   "CL_INVALID_EVENT" },
		{ CL_INVALID_OPERATION,               "CL_INVALID_OPERATION" },
		{ CL_INVALID_GL_OBJECT,               "CL_INVALID_GL_OBJECT" },
		{ CL_INVALID_BUFFER_SIZE,             "CL_INVALID_BUFFER_SIZE" },
		{ CL_INVALID_MIP_LEVEL,               "CL_INVALID_MIP_LEVEL" },
		{ CL_INVALID_GLOBAL_WORK_SIZE,        "CL_INVALID_GLOBAL_WORK_SIZE" },
		{ CL_INVALID_PROPERTY,                "CL_INVALID_PROPERTY" },
		{ CL_INVALID_IMAGE_DESCRIPTOR,        "CL_INVALID_IMAGE_DESCRIPTOR" },
		{ CL_INVALID_COMPILER_OPTIONS,        "CL_INVALID_COMPILER_OPTIONS" },
		{ CL_INVALID_LINKER_OPTIONS,          "CL_INVALID_LINKER_OPTIONS" },
		{ CL_INVALID_DEVICE_PARTITION_COUNT,  "CL_INVALID_DEVICE_PARTITION_COUNT" },
		{ CL_INVALID_PIPE_SIZE,               "CL_INVALID_PIPE_SIZE" },
		{ CL_INVALID_DEVICE_QUEUE,            "CL_INVALID_DEVICE_QUEUE" },
		{ CL_INVALID_SPEC_ID,                 "CL_INVALID_SPEC_ID" },
		{ CL_MAX_SIZE_RESTRICTION_EXCEEDED,   "CL_MAX_SIZE_RESTRICTION_EXCEEDE" },
	};

	void CL_CALLBACK ocl_errorproc (const char * errinfo, const void * private_info, cl::size_type cb, void * user_data) {

		fprintf(stderr, "OpenCL: error callback\n");

	}

	void OCL_ERROR (cstr info, cl_int err=CL_SUCCESS) {
		cstr errstr = "";
		if (err != CL_SUCCESS) {
			auto it = ocl_error_codes.find(err);
			errstr = it == ocl_error_codes.end() ? "<unknown error code>" : it->second;
		}

		fprintf(stderr, "OpenCL: %s failed [%s]\n", info, errstr);
	}

	class Opencl_Context {

	public:
		Opencl_Context () {
			try {
				cl::vector<cl::Platform> platforms;
				cl::Platform::get(&platforms);
				if (platforms.size() <= 0)
					throw std::runtime_error("cl::Platform::get(), no platforms");
			
				printf("OpenCL: platforms:\n");
				int i = 0;
				for (auto& p : platforms) {
					std::string vendor, name, profile, version, extensions;
				
					p.getInfo((cl_platform_info)CL_PLATFORM_VENDOR, &vendor);
					p.getInfo((cl_platform_info)CL_PLATFORM_NAME, &name);
					p.getInfo((cl_platform_info)CL_PLATFORM_PROFILE, &profile);
					p.getInfo((cl_platform_info)CL_PLATFORM_VERSION, &version);
					p.getInfo((cl_platform_info)CL_PLATFORM_EXTENSIONS, &extensions);
			
					printf("[%d] \"%s\" \"%s\" \"%s\" \"%s\"\nExtensions: \"%s\"\n", i++, vendor.c_str(), name.c_str(), profile.c_str(), version.c_str(), extensions.c_str());
				}
			
				cl_context_properties cprops[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0 };
			
				cl::Context context(CL_DEVICE_TYPE_GPU, cprops, ocl_errorproc, NULL);
			
				cl::vector<cl::Device> devices;
				devices = context.getInfo<CL_CONTEXT_DEVICES>();
				if (devices.size() <= 0)
					throw std::runtime_error("cl::Context::getInfo(), no devices");

				//////
				std::string outH(12, '-');

				cl::Buffer outCL(context, CL_MEM_WRITE_ONLY, outH.size() +1, NULL);
			
				//////
				std::ifstream file("kernel.cl");
				if (!file.is_open()) {
					OCL_ERROR("ifstream(\nkernel.cl\")");
					return;
				}

				cl::string prog = cl::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

				cl::Program program(context, prog);
				program.build(devices,"");
			
				cl::Kernel kernel(program, "hello");
			
				kernel.setArg(0, outCL);
			
				//////
				cl::CommandQueue queue(context, devices[0], CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE); // NVIDEA device on my desktop gives CL_INVALID_VALUE without CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, on laptop it works 
			
				//////
				cl::Event event;
				queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(outH.size() +1), cl::NDRange(1), NULL, &event);
			
				event.wait();
			
				(&outH[0])[12] = '-';

				queue.enqueueReadBuffer(outCL, CL_TRUE, 0, outH.length() +1, &outH[0]);
			
				printf("OpenCL output: \"%s\"\n", outH.c_str());
			} catch (cl::Error ex) {
				OCL_ERROR(ex.what(), ex.err());
			} catch (std::exception ex) {
				OCL_ERROR(ex.what());
			}
		}

	};
}

int main () {
	auto kiss_wnd = kiss::Window("Kiss Test Window", 500);
	auto ogl = kiss::Opengl_Context(kiss_wnd, {3,1}, true, true);
	auto ocl = kiss::Opencl_Context();

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
