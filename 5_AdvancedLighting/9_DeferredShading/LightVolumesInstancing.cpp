#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <others/stb_image.h>

#include <iostream>
#include <vector>
#include <random>

#include <Camera.h>
#include <Shader.h>
#include <Model.h>

#include <others/Icosphere.h>

#include <map>

using namespace std;

static int drawMode = 0;
static std::map<int, const char *> drawModeMap = {
    {0, "Fill"},
    {1, "Wireframe"},
    {2, "Point"},
};

static int HEIGHT = 1080;
static int WIDTH = 1920;

static bool firstMove = true;
static float lastX = 0.0f;
static float lastY = 0.0f;

static float lastFrame = 0.0f;
static float deltaTime = 0.0f;

static Camera camera = Camera(glm::vec3(3.5f, -1.5f, 5), glm::vec3(0, 1, 0), -124, -19);
static std::shared_ptr<Shader> deferredShader = nullptr, wireframeShader = nullptr, pointShader = nullptr, simpleShader = nullptr, normalShader = nullptr;
static std::shared_ptr<Shader> debugQuadShader = nullptr;
static std::shared_ptr<Shader> lightVolumesShader = nullptr;
static std::shared_ptr<Shader> lightShader = nullptr;
static std::shared_ptr<Model> backpack = nullptr;
static unsigned int gBufferFBO;
static std::vector<glm::vec3> objectPositions;
static unsigned int gPosition, gNormal, gAlbedoSpec;
static std::vector<glm::vec3> lightPositions;
static std::vector<glm::vec3> lightColors;
static std::vector<float> lightRadius;
static float constant  = 1.0, linear    = 0.7, quadratic = 1.8; 
static unsigned int NR_LIGHTS = 50;
static unsigned int ubo, sphereVAO, indexCount;
// static vector<std::shared_ptr<Icosphere>> spheres;
static unsigned int depthRB;
static unsigned int vbo, ebo, lightParamsBO, lightPositionsBO, lightColorsBO, modelBO;
static bool bDrawNormal = false;
static bool bShowLightVolumes = true;
static std::vector<glm::vec3> positions, normals;
static std::vector<glm::vec2> uv;
static std::vector<unsigned int> indices;

static void frame_buffer_callback(GLFWwindow *, int , int);
static void cursor_pos_callback(GLFWwindow *, double, double);
static void mouse_scroll_callback(GLFWwindow *, double, double);
static void processInput(GLFWwindow * window);
static void renderCube();
static void renderQuad();
static void renderScene(const Shader &shader, unsigned int planeVAO);
static void renderScene3D(const Shader &shader);
static void renderQuadSimple();
static void renderCubeSimple();
static float lerp(float, float, float);
static void calculateLightInfo(GLFWwindow * window, bool);
static void compile_shaders(GLFWwindow * window);
// static void drawImGuiContent(GLFWwindow * window);
static void window_size_callback(GLFWwindow * window, int width, int height);

static bool bCursorOff = false;
static bool bPressed;

static void switch_cursor(GLFWwindow * window)
{
    if (!bCursorOff)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(window, cursor_pos_callback);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetCursorPosCallback(window, nullptr);
        firstMove = true;
    }
    bCursorOff = !bCursorOff;
}

static void switch_drawMode(GLFWwindow * window)
{
    // deferredShader->setInt("drawMode", drawMode);
    // debugQuadShader->setInt("drawMode", drawMode);
    // lightVolumesShader->setInt("drawMode", drawMode);
    // lightShader->setInt("drawMode", drawMode);
    // ** use custom shader
    switch(drawMode)
    {
        case 0:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case 1:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case 2:
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            break;
        default:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
    }

}

// using instance render all spheres
static void renderSpheres()
{
    if (sphereVAO == 0)
    {
        glGenVertexArrays(1, &sphereVAO);
        glBindVertexArray(sphereVAO);

        glGenBuffers(1, &vbo);
        glGenBuffers(1, &lightParamsBO);
        glGenBuffers(1, &lightPositionsBO);
        glGenBuffers(1, &lightColorsBO);
        glGenBuffers(1, &modelBO);
        glGenBuffers(1, &ebo);


        indices.clear();
        positions.clear();
        uv.clear();
        normals.clear();

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359;
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                glm::vec3 point(xPos, yPos, zPos);
                positions.push_back(point);
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(point);
            }
        }

        bool oddRow = false;
        for (int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        indexCount = indices.size();

        std::vector<float> data;
        for (int i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            data.push_back(normals[i].x);
            data.push_back(normals[i].y);
            data.push_back(normals[i].z);
            data.push_back(uv[i].x);
            data.push_back(uv[i].y);
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        float stride = (3 + 2 + 3) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

        vector<glm::vec3> lightParams;
        for (unsigned int i = 0; i < NR_LIGHTS; ++i)
        {
            lightParams.push_back(glm::vec3(lightRadius[i]));
        }
        glBindBuffer(GL_ARRAY_BUFFER, lightParamsBO);
        glBufferData(GL_ARRAY_BUFFER, lightParams.size() * sizeof(glm::vec3), &lightParams[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
        glVertexAttribDivisor(3, 1);

        glBindBuffer(GL_ARRAY_BUFFER, lightPositionsBO);
        glBufferData(GL_ARRAY_BUFFER, lightPositions.size() * sizeof(glm::vec3), &lightPositions[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
        glVertexAttribDivisor(4, 1);


        glBindBuffer(GL_ARRAY_BUFFER, lightColorsBO);
        glBufferData(GL_ARRAY_BUFFER, lightColors.size() * sizeof(glm::vec3), &lightColors[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
        glVertexAttribDivisor(5, 1);

        vector<glm::mat4> modelMatrices;
        for (unsigned int i = 0 ; i< NR_LIGHTS; ++i)
        {
            glm::vec3 lightPosition = lightPositions[i];
            float radius = lightRadius[i];
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lightPosition);
            model = glm::scale(model, glm::vec3(radius));
            modelMatrices.push_back(model);
        }
            
        std::size_t vec4Size = sizeof(glm::vec4);
        glBindBuffer(GL_ARRAY_BUFFER, modelBO);
        glBufferData(GL_ARRAY_BUFFER, modelMatrices.size() * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void *)0);
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void *)(vec4Size));
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void *)(vec4Size * 2));
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void *)(vec4Size * 3));
        glVertexAttribDivisor(6, 1);
        glVertexAttribDivisor(7, 1);
        glVertexAttribDivisor(8, 1);
        glVertexAttribDivisor(9, 1);
    }

    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    glBindVertexArray(sphereVAO);
    // glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    glDrawElementsInstanced(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0, NR_LIGHTS);
    // glDisable(GL_CULL_FACE);
}

// static void InitSpheres(GLFWwindow * window)
// {
//     spheres.clear();
//     for (unsigned int i = 0; i < NR_LIGHTS; ++i)
//     {
//         std::shared_ptr<Icosphere> sphere = std::make_shared<Icosphere>(1.0f, 3, false);    // radius, subdivision, smooth
//         spheres.push_back(sphere);
//     }
// }

void lightVolumesInstancing_setup(GLFWwindow * window)
{
//     glfwInit();
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//     glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

// #ifdef __APPLE__
//     glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
// #endif

//     GLFWwindow * window = glfwCreateWindow(WIDTH, HEIGHT, "lightVolumes", NULL, NULL);
//     if (window == NULL)
//     {
//         cout << "ERROR::CREATE WINDOW:: FAILED!" << endl;
//         glfwTerminate();
//         return -1;
//     }

//     glfwMakeContextCurrent(window);

//     if( ! gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) )
//     {
//         cout << "ERROR::GLAD LOADER INIT FAILED!" <<endl;
//         glfwTerminate();
//         return -1;
//     }

    
    glEnable(GL_DEPTH_TEST);
    
    // glfwSetFramebufferSizeCallback(window, frame_buffer_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwGetWindowSize(window, &WIDTH, &HEIGHT);
    // glfwSetCursorPosCallback(window, cursor_pos_callback);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    
    if (!backpack)
    {
        backpack = std::make_shared<Model>("Assets/backpack/backpack.obj", true);
    }

    // InitSpheres(window);
    
    glGenFramebuffers(1, &gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

    // position color buffer
    glGenTextures(1, &gPosition);
    
    // normal color buffer
    glGenTextures(1, &gNormal);
    
    // color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    

    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 }; 
    glDrawBuffers(3, attachments);

    glGenRenderbuffers(1, &depthRB);
    window_size_callback(window, WIDTH, HEIGHT);

    
    objectPositions.clear();
    objectPositions.push_back(glm::vec3(-3.0,  -3.0, -3.0));
    objectPositions.push_back(glm::vec3( 0.0,  -3.0, -3.0));
    objectPositions.push_back(glm::vec3( 3.0,  -3.0, -3.0));
    objectPositions.push_back(glm::vec3(-3.0,  -3.0,  0.0));
    objectPositions.push_back(glm::vec3( 0.0,  -3.0,  0.0));
    objectPositions.push_back(glm::vec3( 3.0,  -3.0,  0.0));
    objectPositions.push_back(glm::vec3(-3.0,  -3.0,  3.0));
    objectPositions.push_back(glm::vec3( 0.0,  -3.0,  3.0));
    objectPositions.push_back(glm::vec3( 3.0,  -3.0,  3.0));

    

    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    
    
    
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, 0, 3 * sizeof(glm::mat4));

    compile_shaders(window);

    // lighting info
    // -------------
    calculateLightInfo(window, false);
    // // Setup Dear ImGui context
    // IMGUI_CHECKVERSION();
    // ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // 

    // // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    

    // // Setup Platform/Renderer bindings
    // ImGui_ImplGlfw_InitForOpenGL(window, true);
    // ImGui_ImplOpenGL3_Init("#version 330 core");


    // while(!glfwWindowShouldClose(window))
    // {
        

    //     ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    //     glfwSwapBuffers(window);
    //     glfwPollEvents();
    // }

    // glfwTerminate();
}

void lightVolumesInstancing_imgui(GLFWwindow * window)
{
    // ImGui::Begin("Editor");
    // ImGui::Text("Lights Color & Position");
    // for (int i = 0; i < 4; ++i)
    // {
    //     ImGui::ColorEdit3(("light" + std::to_string(i) + " Color").c_str(), glm::value_ptr(lightColors[i]), ImGuiColorEditFlags_HDR);
    //     ImGui::DragFloat3(("light" + std::to_string(i) + " Position").c_str(), glm::value_ptr(lightPositions[i]), 0.05f, -88.f, 88.0f);
    //     ImGui::Separator();
    // }
    ImGui::Spacing();
    ImGui::Text("Light Info");
    bool bLightInfoChanged = false;
    bLightInfoChanged |= ImGui::SliderFloat("constant", &constant, 0, 42.f);
    bLightInfoChanged |= ImGui::SliderFloat("linear", &linear, 0.01f, 5.f);
    bLightInfoChanged |= ImGui::SliderFloat("quadratic", &quadratic, 0.01f, 3.f);
    const ImU32   u32_one = 1;
    bool lightCountChanged = ImGui::InputScalar("Light Count",      ImGuiDataType_U32,     &NR_LIGHTS,  &u32_one, NULL, "%u");
    if (bLightInfoChanged || lightCountChanged)
    {
        // if (lightCountChanged)
        // {
        //     InitSpheres(window);
        // }
        calculateLightInfo(window, !lightCountChanged);
    }
    // ImGui::SliderFloat("shininess", &shininess, 0, 256.f);
    // ImGui::SliderFloat("ambientStrength", &ambientStrength, 0, 32.f);
    // ImGui::SliderFloat("exposure", &exposure, 0, 128.f);
    ImGui::Separator();
    if (bCursorOff)
    {
        ImGui::Text("Press P to release control of the camera, and show cursor.");
    }
    else
    {
        ImGui::Text("Press P or Button follows to take control of the camera");
        if(ImGui::Button("Posses camera") && !bCursorOff)
        {
            switch_cursor(window);
        }
    }
    if(ImGui::Button("Compile Shader"))
    {
        compile_shaders(window);
    }
    ImGui::Separator();
    ImGui::Text("Current Draw Mode : %s", drawModeMap[drawMode]);
    if (ImGui::CollapsingHeader("Draw Mode Selection"))
    {
        for (int i = 0; i < 3; ++i)
        {
            ImGui::RadioButton(drawModeMap[i], &drawMode, i);
            if (i < 2)
            {
                ImGui::SameLine();
            }
        }
    }
    ImGui::Checkbox("DrawNormal", &bDrawNormal);
    ImGui::Checkbox("ShowLightVolumes", &bShowLightVolumes);
    glm::vec3 pos = camera.Position;
    ImGui::Text("Camera Position (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

    // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    // ImGui::Text("Press space key to navigating in scene");
    // ImGui::End();
}

int lightVolumesInstancing(GLFWwindow * window)
{
    processInput(window);
    // switch_drawMode(window);
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)WIDTH/(float)HEIGHT, 0.1f, 100.f);
    glm::mat4 view = camera.GetViewMatrix();
    
    if (drawMode == 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        deferredShader->use();
        deferredShader->setMat4("projection", projection);
        deferredShader->setMat4("view", view);
        for (unsigned int i = 0 ; i < objectPositions.size(); ++i)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(0.5f));
            deferredShader->setMat4("model", model);
            backpack->Draw(*deferredShader);
        }

        // draw light volumes sphere
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glDisable(GL_DEPTH_TEST);
        lightVolumesShader->use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
        lightVolumesShader->setMat4("projection", projection);
        lightVolumesShader->setMat4("view", view);
        lightVolumesShader->setVec3("viewPos", camera.Position);
        lightVolumesShader->setVec2("windowSize", glm::vec2(WIDTH, HEIGHT));
        lightVolumesShader->setFloat("shininess", 64);
        lightVolumesShader->setFloat("Linear", linear);
        lightVolumesShader->setFloat("Quadratic", quadratic);
        // for (unsigned int i = 0; i < lightPositions.size(); i++)
        // {
        //     glm::mat4 model = glm::mat4(1.0f);
        //     model = glm::translate(model, glm::vec3(lightPositions[i]));
        //     // cout<< "lightRadius[i]" << lightRadius[i] << endl;
        //     lightVolumesShader->setVec3("light.Position", lightPositions[i]);
        //     lightVolumesShader->setVec3("light.Color", lightColors[i]);
            
        //     lightVolumesShader->setFloat("light.Radius", lightRadius[i]);
        //     lightVolumesShader->setMat4("model", model);
        //     spheres[i]->draw();
        //     // sphere->setRadius(lightRadius[i]);
        //     // sphere->draw();
        // }
        renderSpheres();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        
        // render light position
        lightShader->use();
        lightShader->setMat4("projection", projection);
        lightShader->setMat4("view", view);
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[i]));
            model = glm::scale(model, glm::vec3(0.1f));
            lightShader->setMat4("model", model);
            lightShader->setVec3("lightColor", lightColors[i]);
            renderCubeSimple();
        }
    }
    else 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_DEPTH_BUFFER_BIT| GL_COLOR_BUFFER_BIT);
        // ** use simple shader - white color
        // simpleShader->use();
        // simpleShader->setMat4("projection", projection);
        // simpleShader->setMat4("view", view);
        // glm::mat4 model = glm::mat4(1.0f);
        // model = glm::scale(model, glm::vec3(2.0f));
        // simpleShader->setMat4("model", model);


        std::shared_ptr<Shader> usedShader = drawMode == 1 ? wireframeShader : pointShader;
        usedShader->use();
        usedShader->setVec3("viewPos", camera.Position);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));

        if (bShowLightVolumes)
        {
            usedShader->setBool("backFaceCull", true);
            usedShader->setVec3("color", glm::vec3(0.86, 0.62, 0.01));
            // ** TODO:
            // for (unsigned int i = 0; i < lightPositions.size(); i++)
            // {
            //     glm::mat4 model = glm::mat4(1.0f);
            //     model = glm::translate(model, glm::vec3(lightPositions[i]));
            //     // cout<< "lightRadius[i]" << lightRadius[i] << endl;
            //     glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            //     spheres[i]->draw();
            //     // spheres[i]->setRadius(lightRadius[i]);
            // }
            usedShader->setBool("backFaceCull", false);
        }

        usedShader->setVec3("color", glm::vec3(0, 0.496, 0.496));
        for (unsigned int i = 0 ; i < objectPositions.size(); ++i)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(0.5f));
            glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            backpack->Draw(*usedShader);
        }
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[i]));
            model = glm::scale(model, glm::vec3(0.1f));
            glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            renderCubeSimple();
        }
    }
    if (bDrawNormal)
    {
        normalShader->use();
        normalShader->setVec3("color", glm::vec3(0.92, 0.496, 0));
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
        if (bShowLightVolumes && drawMode != 0)
        {
            // ** TODO:
            // for (unsigned int i = 0; i < lightPositions.size(); i++)
            // {
            //     glm::mat4 model = glm::mat4(1.0f);
            //     model = glm::translate(model, glm::vec3(lightPositions[i]));
            //     // cout<< "lightRadius[i]" << lightRadius[i] << endl;
            //     // sphere->setRadius(lightRadius[i]);
            //     glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            //     spheres[i]->draw();
            // }
        }
        for (unsigned int i = 0 ; i < objectPositions.size(); ++i)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(0.5f));
            glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            backpack->Draw(*normalShader);
        }
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[i]));
            model = glm::scale(model, glm::vec3(0.1f));
            glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
            renderCubeSimple();
        }
    }
    

    // // debug G-Buffer start
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // debugquadShader->use();
    // debugquadShader->setInt("renderTexture", 0);
    // glClearColor(1, 1, 1, 1.0f);
    // glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    // glm::mat4 model = glm::mat4(1.0);
    // model = glm::translate(model, glm::vec3(-0.5f, 0.5f, 0));
    // model = glm::scale(model, glm::vec3(0.495));
    // debugquadShader->setMat4("model", model);
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, gPosition);
    // renderQuadSimple();

    // model = glm::mat4(1.0);
    // model = glm::translate(model, glm::vec3(0.5f, 0.5f, 0));
    // model = glm::scale(model, glm::vec3(0.495));
    // debugquadShader->setMat4("model", model);
    // glBindTexture(GL_TEXTURE_2D, gNormal);
    // renderQuadSimple();

    // model = glm::mat4(1.0);
    // model = glm::translate(model, glm::vec3(-0.5f, -0.5f, 0));
    // model = glm::scale(model, glm::vec3(0.495));
    // debugquadShader->setMat4("model", model);
    // glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    // renderQuadSimple();

    // model = glm::mat4(1.0);
    // model = glm::translate(model, glm::vec3(0.5f, -0.5f, 0));
    // model = glm::scale(model, glm::vec3(0.495));
    // debugquadShader->setMat4("model", model);
    // debugquadShader->setBool("onlyAlpha", true);
    // glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    // renderQuadSimple();
    // debugquadShader->setBool("onlyAlpha", false);
    // // debug G-Buffer end

    
    return 0;

}

static void window_size_callback(GLFWwindow * window, int width, int height)
{
    if (width > 0 && height > 0)
    {
        WIDTH = width;
        HEIGHT = height;
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

        glBindTexture(GL_TEXTURE_2D, gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, depthRB);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRB);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            cout << "ERROR::FRAME BUFFER INIT FAILED!" <<endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(0, 0, WIDTH, HEIGHT);
    }
}

static void compile_shaders(GLFWwindow * window)
{
    if (!simpleShader)
    {
        simpleShader = std::make_shared<Shader>("Shaders/2_1/ColorsVertexShader.vert", "Shaders/2_1/LightFragmentShader.frag", nullptr);
    }
    else
    {
        simpleShader->recompileFromSource();
    }
    if (!normalShader)
    {
        normalShader = std::make_shared<Shader>("Shaders/5_9/DrawNormalVectorVS.vert", "Shaders/5_9/DrawNormalVectorFS.frag", "Shaders/5_9/DrawNormalVectorGS.gs");
    }
    else
    {
        normalShader->recompileFromSource();
    }
    
    if (!deferredShader)
    {
        deferredShader = std::make_shared<Shader>("Shaders/5_9/GBufferVS.vert", "Shaders/5_9/GBufferFS.frag", nullptr);
    }
    else
    {
        deferredShader->recompileFromSource();
    }
    if (!wireframeShader)
    {
        wireframeShader = std::make_shared<Shader>("Shaders/5_9/Wireframe.vert", "Shaders/5_9/Wireframe.frag", "Shaders/5_9/Wireframe.gs");

    }
    else
    {
        wireframeShader->recompileFromSource();
    }
    wireframeShader->use();
    unsigned int blockIndex = glGetUniformBlockIndex(wireframeShader->ID, "Matrices");
    glUniformBlockBinding(wireframeShader->ID, blockIndex, 0);
    if (!pointShader)
    {
        pointShader = std::make_shared<Shader>("Shaders/5_9/Point.vert", "Shaders/5_9/Wireframe.frag", "Shaders/5_9/Point.gs");

    }
    else
    {
        pointShader->recompileFromSource();
    }
    
    // use this shader to debug G-Buffer
    if (!debugQuadShader)
    {
        debugQuadShader = std::make_shared<Shader>("Shaders/5_9/DebugQuadVS.vert", "Shaders/5_9/DebugQuadFS.frag", nullptr);

    }
    else
    {
        debugQuadShader->recompileFromSource();
    }
    if (!lightVolumesShader)
    {
        lightVolumesShader = std::make_shared<Shader>("Shaders/5_9/LightVolumesInst.vert", "Shaders/5_9/LightVolumesInst.frag", nullptr);

    }
    else
    {
        lightVolumesShader->recompileFromSource();
    }
    lightVolumesShader->use();
    lightVolumesShader->setInt("gPosition", 0);
    lightVolumesShader->setInt("gNormal", 1);
    lightVolumesShader->setInt("gAlbedoSpec", 2);
    if (!lightShader)
    {
        lightShader = std::make_shared<Shader>("Shaders/2_3/MaterialsVS23.vert", "Shaders/2_3/ExerciseLight23.frag", nullptr);

    }
    else
    {
        lightShader->recompileFromSource();
    }
    
}

static void calculateLightInfo(GLFWwindow * window, bool bFixedPosition)
{
    lightRadius.clear();
    // srand(13);
    if (!bFixedPosition)
    {
        lightPositions.clear();
        lightColors.clear();
        for (unsigned int i = 0; i < NR_LIGHTS; i++)
        {
            // calculate slightly random offsets
            float xPos = ((rand() % 100) / 100.0) * 6.0 - 3.0;
            float yPos = ((rand() % 100) / 100.0) * 6.0 - 4.0;
            float zPos = ((rand() % 100) / 100.0) * 6.0 - 3.0;
            lightPositions.push_back(glm::vec3(xPos, yPos, zPos));
            // also calculate random color
            float rColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
            float gColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
            float bColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0

            glm::vec3 lightColor = glm::vec3(rColor, gColor, bColor);
            lightColors.push_back(lightColor);
        }
        // sort(lightPositions.begin(), lightPositions.end(), [](glm::vec3 a, glm::vec3 b){

        // });
    }
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        
        glm::vec3 lightColor = lightColors[i];
        float lightMax  = std::fmaxf(std::fmaxf(lightColor.r, lightColor.g), lightColor.b);
        float radius    = 
        (-linear +  std::sqrtf(linear * linear - 4 * quadratic * (constant - (256.0 / 5.0) * lightMax))) 
        / (2 * quadratic);  
        lightRadius.push_back(radius);
        // spheres[i]->setRadius(radius);
    }
}

static void processInput(GLFWwindow * window)
{
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(FORWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(LEFT, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(RIGHT, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        bPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE && bPressed)
    {
        bPressed = false;
        switch_cursor(window);
    }
}

static void frame_buffer_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}


static void cursor_pos_callback(GLFWwindow * window, double xPos, double yPos)
{
    if (firstMove)
    {
        lastX = (float)xPos;
        lastY = (float)yPos;
        firstMove = false;
    }

    float offsetX = (float)xPos - lastX;
    float offsetY = lastY - (float)yPos;
    lastX = (float)xPos;
    lastY = (float)yPos;

    camera.ProcessMouseMovement(offsetX, offsetY);
}

static void mouse_scroll_callback(GLFWwindow * window, double offsetX, double offsetY)
{
    camera.ProcessMouseScroll((float)offsetY);
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
static unsigned int cubeVAO = 0;
static unsigned int cubeVBO = 0;

static void renderCubeSimple()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
    }
    float vertices[] = {
        // back face
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
         1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
        // front face
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
         1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
        // left face
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
        -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
        // right face
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
        1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
        1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
        // bottom face
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
        -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
         1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
        // top face
        -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f, // bottom-left        
         1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
         1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
         1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-right
        -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
    };
    // link vertex attributes
    glBindVertexArray(cubeVAO);
    // fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    // render Cube
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

static void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
static unsigned int quadVAO = 0;
static unsigned int quadVBO;

static void renderQuadSimple()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

static void renderQuad()
{
    if (quadVAO == 0)
    {

         // positions
        glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
        glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
        glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
        glm::vec3 pos4( 1.0f,  1.0f, 0.0f);
        // texture coordinates
        glm::vec2 uv1(0.0f, 1.0f);
        glm::vec2 uv2(0.0f, 0.0f);
        glm::vec2 uv3(1.0f, 0.0f);  
        glm::vec2 uv4(1.0f, 1.0f);
        // normal vector
        glm::vec3 nm(0.0f, 0.0f, 1.0f);

        glm::vec3 edge1 = pos2 - pos1;
        glm::vec3 edge2 = pos3 - pos1;

        glm::vec2 deltaUV1 = uv2 - uv1;
        glm::vec2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        
        glm::vec3 tangent1;
        tangent1.x = f * (edge1.x * deltaUV2.y - edge2.x * deltaUV1.y);
        tangent1.y = f * (edge1.y * deltaUV2.y - edge2.y * deltaUV1.y);
        tangent1.z = f * (edge1.z * deltaUV2.y - edge2.z * deltaUV1.y);
        tangent1 = glm::normalize(tangent1);

        glm::vec3 bitangent1;
        bitangent1.x = f * (edge2.x * deltaUV1.x - edge1.x * deltaUV2.x);
        bitangent1.y = f * (edge2.y * deltaUV1.x - edge1.y * deltaUV2.x);
        bitangent1.z = f * (edge2.z * deltaUV1.x - edge1.z * deltaUV2.x);
        bitangent1 = glm::normalize(bitangent1);

        edge1 = pos3 - pos1;
        edge2 = pos4 - pos1;

        deltaUV1 = uv3 - uv1;
        deltaUV2 = uv4 - uv1;

        f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        glm::vec3 tangent2;
        tangent2.x = f * (edge1.x * deltaUV2.y - edge2.x * deltaUV1.y);
        tangent2.y = f * (edge1.y * deltaUV2.y - edge2.y * deltaUV1.y);
        tangent2.z = f * (edge1.z * deltaUV2.y - edge2.z * deltaUV1.y);
        tangent2 = glm::normalize(tangent2);

        glm::vec3 bitangent2;
        bitangent2.x = f * (edge2.x * deltaUV1.x - edge1.x * deltaUV2.x);
        bitangent2.y = f * (edge2.y * deltaUV1.x - edge1.y * deltaUV2.x);
        bitangent2.z = f * (edge2.z * deltaUV1.x - edge1.z * deltaUV2.x);
        bitangent2 = glm::normalize(bitangent2);

        float quadVertices[] = {
            // positions        // texture Coords  // normal      // tangent                       //bitangent
            pos1.x, pos1.y, pos1.z, uv1.x, uv1.y,   nm.x, nm.y, nm.z,  tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos2.x, pos2.y, pos2.z, uv2.x, uv2.y,   nm.x, nm.y, nm.z,  tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos3.x, pos3.y, pos3.z, uv3.x, uv3.y,   nm.x, nm.y, nm.z,  tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,


            pos1.x, pos1.y, pos1.z, uv1.x, uv1.y,   nm.x, nm.y, nm.z,  tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos3.x, pos3.y, pos3.z, uv3.x, uv3.y,   nm.x, nm.y, nm.z,  tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos4.x, pos4.y, pos4.z, uv4.x, uv4.y,   nm.x, nm.y, nm.z,  tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,

        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void *)(5 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void *)(8 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void *)(11 * sizeof(float)));

    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// renders the 3D scene
// --------------------
static void renderScene(const Shader &shader, unsigned int planeVAO)
{
    // floor
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    // cubes
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 1.0));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 2.0));
    model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    model = glm::scale(model, glm::vec3(0.25));
    shader.setMat4("model", model);
    renderCube();
}


// renders the 3D scene
// --------------------
static void renderScene3D(const Shader &shader)
{
    // room cube
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(5.0f));
    shader.setMat4("model", model);
    glDisable(GL_CULL_FACE); // note that we disable culling here since we render 'inside' the cube instead of the usual 'outside' which throws off the normal culling methods.
    shader.setInt("reverse_normals", 1); // A small little hack to invert normals when drawing cube from the inside so lighting still works.
    renderCube();
    shader.setInt("reverse_normals", 0); // and of course disable it
    glEnable(GL_CULL_FACE);
    // cubes
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0));
    model = glm::scale(model, glm::vec3(0.75f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0));
    model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    model = glm::scale(model, glm::vec3(0.75f));
    shader.setMat4("model", model);
    renderCube();
}

static float lerp(float a,  float b,  float f) 
{
    return a + f * (b - a);
}
