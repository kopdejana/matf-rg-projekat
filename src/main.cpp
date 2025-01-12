#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(char const * path);
unsigned int loadCubemap(vector<std::string> faces);
void renderQuad();

// settings
const unsigned int SCR_WIDTH = 1500;
const unsigned int SCR_HEIGHT = 800;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Spotlight
bool bTorch = false;
float spotlightRed = 1.0f;
float spotlightGreen = 1.0f;
float spotlightBlue = 1.0f;
float spotlightIntensity = 1.0f;
bool hdr = true;
bool hdrKeyPressed = true;
float exposure = 0.5f;
bool bloodMoon = false;

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Blood Moon", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(false);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // build and compile shaders
    Shader ourShader("resources/shaders/model.vs", "resources/shaders/model.fs");
    Shader moonShader("resources/shaders/moon.vs", "resources/shaders/moon.fs");
    Shader fireflyShader("resources/shaders/firefly.vs", "resources/shaders/firefly.fs");
    Shader hdrShader("resources/shaders/hdr.vs", "resources/shaders/hdr.fs");
    Shader blurShader("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader bloomShader("resources/shaders/bloom.vs", "resources/shaders/bloom.fs");

    // load models
    Model treeModel("resources/objects/Tree/Tree Japanese maple N030123.obj");
    Model toriiModel("resources/objects/Torii/OldTorii.obj");
    Model lampModel("resources/objects/Lamp/Luster Grannys lamp N251121.obj");
    Model flowersModel("resources/objects/Flowers/Flowers pot N300622.obj");
    Model moonModel("resources/objects/moon/moon.obj");
    Model fireflyModel("resources/objects/firefly/sphere.obj");
    Model stairsModel("resources/objects/StonePlatforms/StonePlatform_B.obj");
    Model basePlatformModel("resources/objects/StonePlatforms/StonePlatform_A.obj");
    Model catModel("resources/objects/Cat/cat.obj");

    /////////////////////////////////////////////   SKYBOX  ///////////////////////////////////////////////////////////

    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vector<std::string> faces {
        FileSystem::getPath("resources/textures/skybox/sky.jpg"),
        FileSystem::getPath("resources/textures/skybox/sky.jpg"),
        FileSystem::getPath("resources/textures/skybox/sky.jpg"),
        FileSystem::getPath("resources/textures/skybox/sky.jpg"),
        FileSystem::getPath("resources/textures/skybox/sky.jpg"),
        FileSystem::getPath("resources/textures/skybox/sky.jpg")
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    /////////////////////////////////       HDR & BLOOM     ///////////////////////////////////////////////////////////

    // configure floating point framebuffer
    // ------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // create color buffers
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float grassVertices[] = {
            // positions         // texture Coords (swapped y coordinates because texture is flipped upside down)
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int grassVAO, grassVBO;
    glGenVertexArrays(1, &grassVAO);
    glGenBuffers(1, &grassVBO);
    glBindVertexArray(grassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(grassVertices), grassVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int grassTexture = loadTexture(FileSystem::getPath("resources/textures/grass.png").c_str());

    Shader grassShader("resources/shaders/grass.vs", "resources/shaders/grass.fs");
    grassShader.use();
    grassShader.setInt("texture1", 0);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ourShader.use();
    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);

    blurShader.use();
    blurShader.setInt("image", 0);
    bloomShader.use();
    bloomShader.setInt("scene", 0);
    bloomShader.setInt("bloomBlur", 1);
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);


        // render
        ourShader.use();
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Moon position
        float moonT = glfwGetTime();
        float moonV = 0.4;
        float moonR = 30.0;
        float moonX = cos(moonT*moonV)*moonR;
        float moonY = 15.0f;
        float moonZ = sin(moonT*moonV)*moonR;

        // Lamp1 light position
        float lampAngle = glm::radians((float)cos(glfwGetTime())*100.0f)/4.0f;
        float Y1init = 2.2f;
        float Z1init = -11.0f;
        float lamp1Y = Y1init + Y1init * cos(lampAngle);
        float lamp1Z = Z1init - Y1init * tan(lampAngle);

        // Lamp2 light position
        float Y2init = -1.2f;
        float Z2init = 17.0f;
        float lamp2Y = lamp1Y - 5.0f;
        float lamp2Z = lamp1Z + 11.0f + 17.0f;

        ourShader.use();

        // DirLight - Moon
        glm::vec3 moonColor;
        glm::vec3 moonLightColor;
        if (bloodMoon) {
            moonColor = glm::vec3(1.5, 0.3, 0.0);
            moonLightColor = glm::vec3(0.1f);
        }
        else {
            moonColor = glm::vec3(1.5, 1.0, 0.7);
            moonLightColor = moonColor;
        }
        ourShader.setVec3("dirLight.ambient", glm::vec3(0.0, 0.0, 0.0));
        ourShader.setVec3("dirLight.diffuse", moonLightColor);
        ourShader.setVec3("dirLight.specular", glm::vec3(0.0f));
        ourShader.setVec3("dirLight.direction", glm::vec3(-moonX, -moonY, -moonZ));
        ourShader.setVec3("viewPos", programState->camera.Position);

        // PointLight - Lamp1
        ourShader.setVec3("lamp1.ambient", glm::vec3(0.0, 0.0, 0.0));
        ourShader.setVec3("lamp1.diffuse", glm::vec3(1.0, 0.0, 0.3));
        ourShader.setVec3("lamp1.specular", glm::vec3(1.0, 0.0, 0.3)*3.0f);
        ourShader.setFloat("lamp1.constant", 1.0f);
        ourShader.setFloat("lamp1.linear", 0.09f);
        ourShader.setFloat("lamp1.quadratic", 0.03f);
        ourShader.setVec3("lamp1.position", glm::vec3(0.0f, lamp1Y, lamp1Z));

        // PointLight - Lamp2
        ourShader.setVec3("lamp2.ambient", glm::vec3(0.0, 0.0, 0.0));
        ourShader.setVec3("lamp2.diffuse", glm::vec3(1.0, 0.3, 0.0));
        ourShader.setVec3("lamp2.specular", glm::vec3(1.0, 0.3, 0.0)*3.0f);
        ourShader.setFloat("lamp2.constant", 1.0f);
        ourShader.setFloat("lamp2.linear", 0.09f);
        ourShader.setFloat("lamp2.quadratic", 0.03f);
        ourShader.setVec3("lamp2.position", glm::vec3(0.4f, lamp2Y, lamp2Z));

        // PointLights - fireflies
        float green = cos(glfwGetTime()) + 1.5f;
        float red = 2.0f;
        glm::vec3 fireflyColor = glm::vec3(red, green, 0.0f);
        glm::vec3 fireflyAmbient = glm::vec3(0.0, 0.0, 0.0);
        glm::vec3 fireflyDiffuse = fireflyColor * 0.5f;
        glm::vec3 fireflySpecular = fireflyColor * 0.5f;

        float fireflyConstant = 1.0f;
        float fireflyLinear = 1.0f;
        float fireflyQuadratic = 1.0f;

        // Torii firefly
        glm::vec3 toriiFireflyPos = glm::vec3(cos(glfwGetTime())*0.6+1.7f, 0.7f, -cos(glfwGetTime())*0.6f);
        ourShader.setVec3("fireflies[0].ambient", fireflyAmbient);
        ourShader.setVec3("fireflies[0].diffuse", fireflyDiffuse);
        ourShader.setVec3("fireflies[0].specular", fireflySpecular);
        ourShader.setFloat("fireflies[0].constant", fireflyConstant);
        ourShader.setFloat("fireflies[0].linear", fireflyLinear);
        ourShader.setFloat("fireflies[0].quadratic", fireflyQuadratic);
        ourShader.setVec3("fireflies[0].position", toriiFireflyPos);

        // Tree firefly
        glm::vec3 treeFireflyPos = glm::vec3(1.0f + cos(glfwGetTime()*2.0f)*0.4f, 10.5f, 7.0f);
        ourShader.setVec3("fireflies[1].ambient", fireflyAmbient);
        ourShader.setVec3("fireflies[1].diffuse", fireflyDiffuse);
        ourShader.setVec3("fireflies[1].specular", fireflySpecular);
        ourShader.setFloat("fireflies[1].constant", fireflyConstant);
        ourShader.setFloat("fireflies[1].linear", fireflyLinear);
        ourShader.setFloat("fireflies[1].quadratic", fireflyQuadratic);
        ourShader.setVec3("fireflies[1].position", treeFireflyPos);

        // Flowers firefly
        glm::vec3 flowersFireflyPos = glm::vec3(cos(glfwGetTime()) + 6.0, 2.0f, -cos(glfwGetTime()*4.0f));
        ourShader.setVec3("fireflies[2].ambient", fireflyAmbient);
        ourShader.setVec3("fireflies[2].diffuse", fireflyDiffuse);
        ourShader.setVec3("fireflies[2].specular", fireflySpecular);
        ourShader.setFloat("fireflies[2].constant", fireflyConstant);
        ourShader.setFloat("fireflies[2].linear", fireflyLinear);
        ourShader.setFloat("fireflies[2].quadratic", fireflyQuadratic);
        ourShader.setVec3("fireflies[2].position", flowersFireflyPos);

        // Spotlight - Torch
        ourShader.setBool("bTorch", bTorch);
        ourShader.setVec3("torch.ambient", glm::vec3(0.0, 0.0, 0.0));
        ourShader.setVec3("torch.diffuse", glm::vec3(spotlightRed, spotlightGreen, spotlightBlue)*spotlightIntensity);
        ourShader.setVec3("torch.specular", glm::vec3(spotlightRed, spotlightGreen, spotlightBlue)*spotlightIntensity);
        ourShader.setFloat("torch.constant", 1.0f);
        ourShader.setFloat("torch.linear", 0.09f);
        ourShader.setFloat("torch.quadratic", 0.03f);
        ourShader.setVec3("torch.position", programState->camera.Position);
        ourShader.setVec3("torch.direction", programState->camera.Front);
        ourShader.setFloat("torch.cutOff", cos(glm::radians(12.0f)));
        ourShader.setFloat("torch.outerCutOff", cos(glm::radians(15.0f)));

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // Base Platform
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -10.0f, 4.0f));
        model = glm::scale(model, glm::vec3(2.0f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        basePlatformModel.Draw(ourShader);

        // Smaller Platform
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -2.8f, -4.0f));
        model = glm::scale(model, glm::vec3(1.0f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        basePlatformModel.Draw(ourShader);

        // Stairs
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -2.2f, 10.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        stairsModel.Draw(ourShader);

        // Torii
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -11.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        toriiModel.Draw(ourShader);

        // Lamp
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 5.2f, -11.0f));
        model = glm::scale(model, glm::vec3(0.003f));
        model = glm::rotate(model, lampAngle, glm::vec3(1.0, 0.0, 0.0));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        lampModel.Draw(ourShader);

        // Cat
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(7.0f, -4.0f, 15.0f));
        model = glm::scale(model, glm::vec3(0.04f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        catModel.Draw(ourShader);

        // Torii2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.4f, -5.0, 17.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        toriiModel.Draw(ourShader);

        // Lamp2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.4f, 0.2f, 17.0f));
        model = glm::scale(model, glm::vec3(0.003f));
        model = glm::rotate(model, lampAngle, glm::vec3(1.0, 0.0, 0.0));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        lampModel.Draw(ourShader);


        // Tree
        glDisable(GL_CULL_FACE); // all leaves are rendered
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.05f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        treeModel.Draw(ourShader);
        // Flowers
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(6.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.003f));
        ourShader.setMat4("model", model);
        ourShader.setFloat("material.shininess", 1.0);
        flowersModel.Draw(ourShader);
        glEnable(GL_CULL_FACE);

        // Moon
        moonShader.use();
        moonShader.setVec3("lightColor", moonColor);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(moonX, moonY, moonZ));
        model = glm::scale(model, glm::vec3(1.5f));
        moonShader.setMat4("model", model);
        moonShader.setMat4("view", view);
        moonShader.setMat4("projection", projection);
        moonModel.Draw(moonShader);

        // Fireflies
        float fireflyScale = 1.0f;
        fireflyShader.use();
        fireflyShader.setVec3("color", fireflyColor);
        fireflyShader.setMat4("projection", projection);
        fireflyShader.setMat4("view", view);
        // Firefly - Flowers
        model = glm::mat4(1.0f);
        model = glm::translate(model, flowersFireflyPos);
        model = glm::scale(model, glm::vec3(fireflyScale));
        fireflyShader.setMat4("model", model);
        fireflyModel.Draw(fireflyShader);

        // Firefly - Tree
        model = glm::mat4(1.0f);
        model = glm::translate(model, treeFireflyPos);
        model = glm::scale(model, glm::vec3(fireflyScale));
        fireflyShader.setMat4("model", model);
        fireflyModel.Draw(fireflyShader);

        // Firefly - Torii
        model = glm::mat4(1.0f);
        model = glm::translate(model, toriiFireflyPos);
        model = glm::scale(model, glm::vec3(fireflyScale));
        fireflyShader.setMat4("model", model);
        fireflyModel.Draw(fireflyShader);

        // Grass
        glDisable(GL_CULL_FACE);
        grassShader.use();
        grassShader.setMat4("projection", projection);
        grassShader.setMat4("view", view);
        glBindVertexArray(grassVAO);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(1.2f, -3.8f, 17.35f));
        model = glm::scale(model, glm::vec3(2.0f));
        grassShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_CULL_FACE);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-2.3f, -3.8f, 17.4f));
        model = glm::scale(model, glm::vec3(2.0f));
        grassShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_CULL_FACE);

        //////////////////////////////////////  SKYBOX  //////////////////////////////////////////////////////////////

        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        /////////////////////////////////////    HDR & BLOOM     /////////////////////////////////////////////////////

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. blur bright fragments with two-pass Gaussian Blur
        // --------------------------------------------------
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
        // --------------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bloomShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        bloomShader.setInt("bloom", true);
        bloomShader.setFloat("exposure", exposure);
        renderQuad();

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &grassVAO);
    glDeleteBuffers(1, &grassVBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        programState->camera.ProcessYawPitch(-15.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        programState->camera.ProcessYawPitch(+15.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        programState->camera.ProcessYawPitch(0.0f, -15.0f);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        programState->camera.ProcessYawPitch(0.0f, +15.0f);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    {
        ImGui::Begin("Spotlight color");
        ImGui::SliderFloat("Red", &spotlightRed, 0.0f, 1.0f);
        ImGui::SliderFloat("Green", &spotlightGreen, 0.0f, 1.0f);
        ImGui::SliderFloat("Blue", &spotlightBlue, 0.0f, 1.0f);
        ImGui::SliderFloat("Intensity", &spotlightIntensity, 1.0f, 5.0f);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        bloodMoon = !bloodMoon;
        bTorch = !bTorch;
    }
}

unsigned int loadCubemap(vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad() {
    if (quadVAO == 0) {
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
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

unsigned int loadTexture(char const * path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}