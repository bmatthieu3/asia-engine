#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fstream>

#include "shader.hpp"
#include "model.hpp"
#include "screen.hpp"
#include "settings.hpp"
#include "viewer.hpp"
#include "stb_image.h"

using namespace std;

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
            unique_ptr<Viewer> main = make_unique<Viewer>(glm::vec3(5, 2, 5), glm::vec3(0));
            unique_ptr<Viewer> sun = make_unique<Viewer>(glm::vec3(8, 8, -8), glm::vec3(0));
            sun->setOrthoProjection();

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
            m_mode = glfwGetVideoMode(primary);
            glfwWindowHint(GLFW_RED_BITS, m_mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, m_mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, m_mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, m_mode->refreshRate);
            window = glfwCreateWindow(m_mode->width, m_mode->height, name.c_str(), primary, NULL);
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
            // configure global opengl state
            glEnable(GL_DEPTH_TEST);
            //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            // Tell stbi to flip the y-axis of the loaded image.
            stbi_set_flip_vertically_on_load(true);

            // Create a new framebuffer associated with a depth map.
            // This framebuffer will write into the map the depth of all the fragments
            // from a light point of view.
            m_depthMap = Texture::createDepthMap();

            glGenFramebuffers(1, &m_depthFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
            // Attach the depth map texture to the currently bound framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMap->id, 0);
            // As a framebuffer must be attached with a color buffer too, we tell him we will not use read and/or draw
            // from it so that we do not have to specify it.
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Set camera movement
            unique_ptr<CircleMovement> movement = make_unique<CircleMovement>(glm::vec3(0.f), 8, 5.f);
            sun->applyMovement(std::move(movement));
            unique_ptr<FirstPerson> FPSControlledMovement = make_unique<FirstPerson>(window, m_mode);
            main->applyMovement(std::move(FPSControlledMovement));

            m_viewers.insert(std::make_pair<string, unique_ptr<Viewer>>("sun", std::move(sun)));
            m_viewers.insert(std::make_pair<string, unique_ptr<Viewer>>("main", std::move(main)));

            // Loading shaders
            shared_ptr<Shader> animated = make_shared<Shader>("./shaders/vertex_anim.glsl", "./shaders/frag_textured.glsl");
            m_shaders.insert(pair<string, shared_ptr<Shader>>("animated", animated));
            shared_ptr<Shader> simple = make_shared<Shader>("./shaders/vertex.glsl", "./shaders/frag_textured.glsl");
            m_shaders.insert(pair<string, shared_ptr<Shader>>("static", simple));
            // Shader that draw a texture map onto a quad.
            // Useful for debugging purposes.
            shared_ptr<Shader> debugShader = make_shared<Shader>("./shaders/vertex_screen.glsl", "./shaders/frag_depth_map.glsl");
            m_shaders.insert(pair<string, shared_ptr<Shader>>("debug", debugShader));

            // Loading meshes
            /*unique_ptr<Model> bob = make_unique<Model>(m_shaders["textured"], "../resources/Content/boblampclean.md5mesh");
            bob->applyTransformation(glm::scale(glm::mat4(1.f), glm::vec3(0.05f)));
            m_models.push_back(std::move(bob));*/
            unique_ptr<Model> nanosuit = make_unique<Model>(m_shaders["static"], "./resources/nanosuit/nanosuit.obj");
            nanosuit->applyTransformation(glm::scale(glm::mat4(1.f), glm::vec3(0.2f)));
            m_models.push_back(std::move(nanosuit));
            
            Material material = Material {0.1};
            unique_ptr<Mesh> plane = Mesh::createPlane(material);
            plane->applyTransformation(glm::scale(glm::mat4(1.f), glm::vec3(10.f)));
            m_primitives.push_back(std::move(plane));

            // Create a screen quad object
            m_quad = make_unique<ScreenQuad>(m_depthMap);
            std::cout << "Init terminated successfully" << std::endl;
        }

        ~App() {
            glfwDestroyWindow(window);
            glfwTerminate();
        }

        void run() {
            // render loop
            // -----------
            float time;
            float prevTime = glfwGetTime();
            while (!glfwWindowShouldClose(window))
            {
                time = glfwGetTime();
                float dt = time - prevTime;
                // input
                // -----
                // Quit the program when pressing escape
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                    glfwSetWindowShouldClose(window, true);
                }

                // update
                // ------
                // Update the viewers
                for(auto& kv_viewer: m_viewers) {
                    kv_viewer.second->update(dt);
                }

                for(uint32_t i = 0; i < m_models.size(); i++) {
                    m_models[i]->update(time);
                }
                // render
                // ------
                // Write onto the depth FBO
                glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
                glClear(GL_DEPTH_BUFFER_BIT);
                writeToCurrentFBO(*m_viewers["sun"]);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // Write onto the screen FBO
                glViewport(0, 0, m_mode->width, m_mode->height);
                glClearColor(0.f, 1.f, 0.f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                writeToCurrentFBO(*m_viewers["main"]);
                //renderDebugTextureMap();

                // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
                // -------------------------------------------------------------------------------
                glfwSwapBuffers(window);
                glfwPollEvents();

                prevTime = time;
            }
        }

    private:
        void sendGlobalUniformsToShader(const shared_ptr<Shader> shader) {
            float time = glfwGetTime();
            shader->sendUniform1f("time", time);
            shader->sendUniform1i("screen_w", m_mode->width);
            shader->sendUniform1i("screen_h", m_mode->height);

            shader->sendUniformMatrix4fv("viewLightSpace", m_viewers["sun"]->getViewMatrix());
            shader->sendUniformMatrix4fv("clipLightSpace", m_viewers["sun"]->getProjectionMatrix());
            // Send sun directional light
            shader->sendUniform3f("sun.ambiant", glm::vec3(0.5, 0.5, 0.5));
            shader->sendUniform3f("sun.diffuse", glm::vec3(0.9, 1.0, 1.0));
            shader->sendUniform3f("sun.specular", glm::vec3(1));

            shader->sendUniform3f("sun.dir", m_viewers["sun"]->getSightDirection());
            shader->sendUniform3f("eyeWorldSpace", m_viewers["main"]->getPosition());

            glActiveTexture(GL_TEXTURE0);
            shader->sendUniform1i("depth_map", 0);
            glBindTexture(GL_TEXTURE_2D, m_depthMap->id);
        }

        void writeToCurrentFBO(const Viewer& viewer) {
            for(auto& model: m_models) {
                const shared_ptr<Shader> shader = model->getShader();
                shader->bind();
                sendGlobalUniformsToShader(shader);
                model->draw(viewer);
            }
            const shared_ptr<Shader> primitiveShader = m_shaders["static"];
            for(auto& primitive: m_primitives) {
                primitiveShader->bind();
                sendGlobalUniformsToShader(primitiveShader);
                primitive->draw(primitiveShader, viewer);
            }
        }

        void renderDebugTextureMap() {
            const shared_ptr<Shader> debugShader = m_shaders["debug"];
            m_quad->draw(debugShader);
        }

    private:
        bool m_closed;
        GLFWwindow* window;
        const GLFWvidmode* m_mode;
        map<string, unique_ptr<Viewer>> m_viewers;

        // Depth FrameBuffer object
        shared_ptr<Texture> m_depthMap;
        GLuint m_depthFBO;

        map<string, shared_ptr<Shader>> m_shaders;

        std::vector<unique_ptr<Model>> m_models;
        vector<unique_ptr<Mesh>> m_primitives;
        
        // Texture map screen vizualizer (for debugging purposes)
        unique_ptr<ScreenQuad> m_quad;
};

int main(void)
{	
    App app("Asia Engine");
    app.run();
	
    return 0;
}