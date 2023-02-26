// CMakeProject1.cpp : définit le point d'entrée de l'application.

#include "MainWindow.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "constants.h"
#include <stdio.h>
#include <format>

using namespace std;

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main()
{
	glfwSetErrorCallback(glfw_error_callback);
	// Setup window
	if (!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, std::format("{} {}", PROGRAM_TITLE, PROGRAM_VERSION).c_str(), NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		throw("Unable to context to OpenGL");

	int screen_width, screen_height;
	glfwGetFramebufferSize(window, &screen_width, &screen_height);
	glViewport(0, 0, screen_width, screen_height);

	MainWindow program(window, glsl_version);

	while (!glfwWindowShouldClose(window)) {
		// Poll and handle events such as MKB inputs, window resize
		glfwPollEvents();
		//glClearColor(0.45f, 0.55f, 0.60f, 1.00f);

		glClear(GL_COLOR_BUFFER_BIT);
		program.NewFrame();
		program.Update();
		program.Render();
		glfwSwapBuffers(window);
	}

	program.Shutdown();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
