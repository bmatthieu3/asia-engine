#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <map>
#include <string>
#include <fstream>

#include "shader.hpp"
#include "mesh.hpp"
#include "settings.hpp"
#include "viewer.hpp"

using namespace std;
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

class App {
    public:
        App(const std::string& name) : m_closed(false) {
            // glfw: initialize and configure
            // ------------------------------
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        #ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
        #endif

            // glfw window creation
            // --------------------
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primary);
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            window = glfwCreateWindow(mode->width, mode->height, name.c_str(), primary, NULL);
            if (window == NULL)
            {
                std::cout << "Failed to create GLFW window" << std::endl;
                glfwTerminate();
                return;
            }
            glfwMakeContextCurrent(window);
            glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

            // glad: load all OpenGL function pointers
            // ---------------------------------------
            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            {
                std::cout << "Failed to initialize GLAD" << std::endl;
                return;
            }

            // Loading shaders
            shared_ptr<Shader> textured = make_shared<Shader>("shaders/vertex.glsl", "shaders/fragment.glsl");
            m_shaders.insert(pair<string, shared_ptr<Shader>>("textured", textured));

            // Loading meshes
            unique_ptr<Mesh> mesh = make_unique<Mesh>(m_shaders["textured"]);
            m_meshes.push_back(std::move(mesh));
        }

        ~App() {
            glfwDestroyWindow(window);
            glfwTerminate();
        }

        void run() {
            // render loop
            // -----------
            while (!glfwWindowShouldClose(window))
            {
                // input
                // -----
                processInput(window);

                // render
                // ------
                glClearColor(0.f, 1.f, 0.f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                float time = glfwGetTime();

                for(uint32_t i = 0; i < m_meshes.size(); i++) {
                    m_meshes[i]->draw(m_viewer, time);
                }
        
                // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
                // -------------------------------------------------------------------------------
                glfwSwapBuffers(window);
                glfwPollEvents();
            }
        }

    private:
        bool m_closed;
        GLFWwindow* window;
        Viewer m_viewer;

        map<string, shared_ptr<Shader>> m_shaders;

        vector<unique_ptr<Mesh>> m_meshes;
};

int main(void)
{	
    App app("Curse Engine");
    app.run();
	
    return 0;
}