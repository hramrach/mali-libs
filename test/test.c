/*
 * Copyright (c) 2011-2012 Luc Verhaegen <libv@skynet.be>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Hello triangle, adapted for native display on libMali.so.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

static inline void errorf(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}

struct mali_native_window native_window = {
	.width = 480,
	.height = 480,
};

static const char *vertex_shader_source =
	"attribute vec4 aPosition;    \n"
	"attribute vec4 aColor;       \n"
	"                             \n"
	"varying vec4 vColor;         \n"
	"                             \n"
	"void main()                  \n"
	"{                            \n"
	"    vColor = aColor;         \n"
	"    gl_Position = aPosition; \n"
	"}                            \n";
static const char *fragment_shader_source =
	"precision mediump float;     \n"
	"                             \n"
	"varying vec4 vColor;         \n"
	"                             \n"
	"void main()                  \n"
	"{                            \n"
	"    gl_FragColor = vColor;   \n"
	"}                            \n";

static GLfloat vVertices[] = {  0.0f,  0.5f, 0.0f,
			       -0.5f, -0.5f, 0.0f,
				0.5f, -0.5f, 0.0f };
static GLfloat vColors[] = {1.0f, 0.0f, 0.0f, 1.0f,
			    0.0f, 1.0f, 0.0f, 1.0f,
			    0.0f, 0.0f, 1.0f, 1.0f};

static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,

	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, 0,

	EGL_SAMPLES, 4,

	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,

	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PIXMAP_BIT,


	EGL_NONE
};

static EGLint window_attribute_list[] = {
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

int
main(int argc, char *argv[])
{
	EGLDisplay display;
	EGLint egl_major, egl_minor;
	EGLConfig config;
	EGLint num_config;
	EGLContext context;
	EGLSurface surface;
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint program;
	GLint ret;
	GLint width, height;
#ifdef USE_X11
	Display * X11 = 0;

        if (getenv("DISPLAY")) {
		XInitThreads();
		X11 = XOpenDisplay(getenv("DISPLAY"));
		if (!X11) {
			fprintf(stderr, "Cannot open X display!\n");
		} else {
			display = eglGetDisplay(X11);
			if(display == EGL_NO_DISPLAY) {
				fprintf(stderr, "No display found on X11! Framebuffer Mali library installed?\n");
				XCloseDisplay(X11);
				X11 = 0;
			}
		}
	}
	if (!X11)
#endif
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY)
		errorf("Error: No display found!\n");

	if (eglInitialize(display, &egl_major, &egl_minor) == EGL_FALSE) {
		GLuint r;
		r = eglGetError();
		if (r == EGL_NOT_INITIALIZED)
			fprintf(stderr, "Error: eglInitialise failed: %s\n", "EGL_NOT_INITIALIZED");
		else if (r == EGL_BAD_DISPLAY)
			fprintf(stderr, "Error: eglInitialise failed: %s\n", "EGL_BAD_DISPLAY");
		else
			fprintf(stderr, "Error: eglInitialise failed: 0x%x\n", r);
		return -1;
	}

	printf("EGL Version: \"%s\"\n",
	       eglQueryString(display, EGL_VERSION));
	printf("EGL Vendor: \"%s\"\n",
	       eglQueryString(display, EGL_VENDOR));
	printf("EGL Extensions: \"%s\"\n",
	       eglQueryString(display, EGL_EXTENSIONS));

	eglChooseConfig(display, config_attribute_list, &config, 1,
			&num_config);

	context = eglCreateContext(display, config, EGL_NO_CONTEXT,
				   context_attribute_list);
	if (context == EGL_NO_CONTEXT)
		errorf("Error: eglCreateContext failed: 0x%08X\n",
			eglGetError());

#ifdef USE_X11
	if (X11) {
		XVisualInfo *visInfo, visTemplate;
		int nvis;
		int x=0, y=0, w=480, h=480;
		Window root, xwin;
		XSetWindowAttributes attr;
		XSizeHints sizehints;
		EGLint vid;
		const char * title = "Mali EGL test";

		if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &vid))
			errorf("failed to get visual id\n");

		/* The X window visual must match the EGL config */
		visTemplate.visualid = vid;
		visInfo = XGetVisualInfo(X11, VisualIDMask, &visTemplate, &nvis);
		if (!visInfo)
			errorf("failed to get an visual of id 0x%x\n", vid);

		root = RootWindow(X11, DefaultScreen(X11));

		attr.background_pixel = 0;
		attr.border_pixel = 0;
		attr.colormap = XCreateColormap(X11, root, visInfo->visual, AllocNone);
		attr.event_mask = StructureNotifyMask | ExposureMask ;
		xwin = XCreateWindow(X11, root, x, y, w, h, 0, visInfo->depth, InputOutput, visInfo->visual,
				     CWBackPixel | CWBorderPixel | CWColormap | CWEventMask
				     , &attr);
		if (!xwin)
			errorf("failed to create a window\n");

		XFree(visInfo);

		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = w;
		sizehints.height = h;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(X11, xwin, &sizehints);
		XSetStandardProperties(X11, xwin,
				       title, title, None, (char **) NULL, 0, &sizehints);

		XMapWindow(X11, xwin);

		surface = eglCreateWindowSurface(display, config, xwin, NULL);

	} else
#endif
	surface = eglCreateWindowSurface(display, config, &native_window,
					 window_attribute_list);
	if (surface == EGL_NO_SURFACE)
		errorf("Error: eglCreateWindowSurface failed: "
			"0x%08X\n", eglGetError());

	if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
	    !eglQuerySurface(display, surface, EGL_HEIGHT, &height))
		errorf("Error: eglQuerySurface failed: 0x%08X\n",
			eglGetError());
	printf("Surface size: %dx%d\n", width, height);

	if (!eglMakeCurrent(display, surface, surface, context))
		errorf("Error: eglMakeCurrent() failed: 0x%08X\n",
			eglGetError());

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	if (!vertex_shader)
		errorf("Error: glCreateShader(GL_VERTEX_SHADER) "
			"failed: 0x%08X\n", glGetError());

	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		fprintf(stderr, "Error: vertex shader compilation failed!\n");
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(vertex_shader, ret, NULL, log);
			fprintf(stderr, "%s", log);
		}
		return -1;
	}

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	if (!fragment_shader)
		errorf("Error: glCreateShader(GL_FRAGMENT_SHADER) "
			"failed: 0x%08X\n", glGetError());

	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		fprintf(stderr, "Error: fragment shader compilation failed!\n");
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(fragment_shader, ret, NULL, log);
			fprintf(stderr, "%s", log);
		}
		return -1;
	}

	program = glCreateProgram();
	if (!program)
		errorf("Error: failed to create program!\n");

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glBindAttribLocation(program, 0, "aPosition");
	glBindAttribLocation(program, 1, "aColor");

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char *log;

		fprintf(stderr, "Error: program linking failed!\n");
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(program, ret, NULL, log);
			fprintf(stderr, "%s", log);
		}
		return -1;
	}
	glUseProgram(program);

#ifdef USE_X11
	while (1) {
		XEvent event;
		if(X11) {
			XNextEvent(X11, &event);
			switch (event.type) {
				case DestroyNotify:
					return 0;
				case ConfigureNotify:
					width = event.xconfigure.width;
					height = event.xconfigure.height;
				case Expose:
					break;
			}
		}
#endif

	glViewport(0, 0, width, height);

	glClearColor(0.2, 0.2, 0.2, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	eglSwapBuffers(display, surface);
#ifdef USE_X11
	if(!X11) return 0;
	}
#else
        return 0;
#endif
}
