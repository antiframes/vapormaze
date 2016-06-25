// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include "windows.h"
#include "mmsystem.h"

#define ANIMATION_TIME 1.5
// Include GLEW
#include <GL/glew.h>


// Include GLFW
#include <glfw3.h>
GLFWwindow* g_pWindow;
unsigned int g_nWidth = 800, g_nHeight = 600;

// Include AntTweakBar
#include <AntTweakBar.h>
TwBar *g_pToolBar;

struct wall {
	bool toNorth;
	bool toEast;
};

struct wall walls[13][13];

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <shader.hpp>
#include <texture.hpp>
#include <controls.hpp>
#include <objloader.hpp>
#include <vboindexer.hpp>
#include <glerror.hpp>

void WindowSizeCallBack(GLFWwindow *pWindow, int nWidth, int nHeight) {

	g_nWidth = nWidth;
	g_nHeight = nHeight;
	glViewport(0, 0, g_nWidth, g_nHeight);
	TwWindowSize(g_nWidth, g_nHeight);
}

enum Behaviour{MOVING, ROTATING, IDLE};
enum Direction{NORTH,SOUTH,EAST,WEST};
Behaviour behaviour = IDLE;
Direction direction = NORTH;
vec3 translation = vec3(0);
bool rotatingLeft = false;
bool rotatingRight = false;

double currentTime = 0;
double finishAnimationTime;
double startRotationTime = 0;

boolean finishAnimation = false;

int goalX=1;
int goalY=1;
int lastX = 0;
int lastY = 0;

bool reachedGoal = false;
bool colliding = false;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void move();

void getWallInfo(std::vector<vec3> &vertices);


int main(void)
{
	srand(time(NULL));
        goalX = (rand()%8) + 2;
        goalY = (rand() % 6) + 4;

	printf("%d %d \n", goalX, goalY);

	int nUseMouse = 0;

	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}


	PlaySound(TEXT("sfx/macplus.wav"), NULL, SND_LOOP | SND_ASYNC);

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	g_pWindow = glfwCreateWindow(g_nWidth, g_nHeight, "Vapormaze", NULL, NULL);
	if (g_pWindow == NULL){
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(g_pWindow);


	glm::mat4 ViewMatrix = mat4(1.0);
	mat4 BaseTranslationMatrix = translate(ViewMatrix, vec3(-0.5, -0.5, -0.5));
	glm::mat4 ProjectionMatrix = mat4(0);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	check_gl_error();//OpenGL error from GLEW

	// Initialize the GUI
	TwInit(TW_OPENGL_CORE, NULL);
	TwWindowSize(g_nWidth, g_nHeight);

	// Set GLFW event callbacks. I removed glfwSetWindowSizeCallback for conciseness
	glfwSetMouseButtonCallback(g_pWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW); // - Directly redirect GLFW mouse button events to AntTweakBar
	glfwSetCursorPosCallback(g_pWindow, (GLFWcursorposfun)TwEventMousePosGLFW);          // - Directly redirect GLFW mouse position events to AntTweakBar
	glfwSetScrollCallback(g_pWindow, (GLFWscrollfun)TwEventMouseWheelGLFW);    // - Directly redirect GLFW mouse wheel events to AntTweakBar
	glfwSetKeyCallback(g_pWindow, (GLFWkeyfun)TwEventKeyGLFW);                         // - Directly redirect GLFW key events to AntTweakBar
	glfwSetCharCallback(g_pWindow, (GLFWcharfun)TwEventCharGLFW);                      // - Directly redirect GLFW char events to AntTweakBar
	glfwSetWindowSizeCallback(g_pWindow, WindowSizeCallBack);


	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(g_pWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(g_pWindow, g_nWidth / 2, g_nHeight / 2);

	// Pink background
	glClearColor(1.0f, 0.0f, 0.8f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("shaders/StandardShading.vertexshader", "shaders/MazeShader.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID      = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID  = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load the texture
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLuint TextureMaze = loadBMP_custom("mesh/textures.bmp");
        GLuint TextureSuzanne = loadBMP_custom("mesh/gold.bmp");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

	// Read our .obj file
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	bool res = loadOBJ("mesh/maze.obj", vertices, uvs, normals);

	getWallInfo(vertices);

	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs, indexed_normals);

	// Read our .obj file
	std::vector<glm::vec3> vertices_suzanne;
	std::vector<glm::vec2> uvs_suzanne;
	std::vector<glm::vec3> normals_suzanne;
	bool res_s = loadOBJ("mesh/suzanne.obj", vertices_suzanne, uvs_suzanne, normals_suzanne);

	std::vector<unsigned short> indices_suzanne;
	std::vector<glm::vec3> indexed_vertices_suzanne;
	std::vector<glm::vec2> indexed_uvs_suzanne;
	std::vector<glm::vec3> indexed_normals_suzanne;
	indexVBO(vertices_suzanne, uvs_suzanne, normals_suzanne, indices_suzanne, indexed_vertices_suzanne, indexed_uvs_suzanne, indexed_normals_suzanne);

	//load image texture
	// Load it into a VBO

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
	

	GLuint vertexbuffer_suzanne;
	glGenBuffers(1, &vertexbuffer_suzanne);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer_suzanne);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices_suzanne.size() * sizeof(glm::vec3), &indexed_vertices_suzanne[0], GL_STATIC_DRAW);

	GLuint uvbuffer_suzanne;
	glGenBuffers(1, &uvbuffer_suzanne);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer_suzanne);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs_suzanne.size() * sizeof(glm::vec2), &indexed_uvs_suzanne[0], GL_STATIC_DRAW);

	GLuint normalbuffer_suzanne;
	glGenBuffers(1, &normalbuffer_suzanne);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer_suzanne);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals_suzanne.size() * sizeof(glm::vec3), &indexed_normals_suzanne[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer_suzanne;
	glGenBuffers(1, &elementbuffer_suzanne);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer_suzanne);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_suzanne.size() * sizeof(unsigned short), &indices_suzanne[0], GL_STATIC_DRAW);
	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	// For speed computation
	double lastTime = glfwGetTime();
	double previousTime = lastTime;
	int nbFrames    = 0;


	double rotationDegrees = 0;
	boolean startingAnimation=true;





	do{
        check_gl_error();


		// Measure speed
		currentTime = glfwGetTime();
		nbFrames++;

		if (startingAnimation && currentTime >= ANIMATION_TIME) {
			startingAnimation = false;
			glfwSetKeyCallback(g_pWindow, key_callback);
		}

		double elapsedTime = currentTime - previousTime;
		double offset = 1.0 * (elapsedTime / 0.5);
		previousTime = currentTime;
		colliding = false;
		float limiar = 0.05;
		int currentX = (int) (translation.x + 0.1);
		int currentY = (int) (translation.z + 0.1);

		if (behaviour == MOVING) {

			//find next possible wall place
			int possibleWallX, possibleWallY;
			switch (direction)
			{
			case NORTH:
				if (currentY+1-translation.z>limiar)
				{
					if (walls[currentX][currentY + 1].toEast == true) {
						colliding = true;
					}
				}
				break;
			case SOUTH:
				if ( (translation.z - currentY) < limiar)
				{
					if (walls[currentX][currentY].toEast == true) {
						colliding = true;
					}
				}
				break;
			case EAST:
				if ((translation.x - currentX) < limiar)
				{
					if (walls[currentX][currentY].toNorth == true) {
						colliding = true;
					}
				}
				break;
			case WEST:
				if ((currentX - translation.x + 1) > limiar)
				{
					if (walls[currentX + 1][currentY].toNorth == true) {
						colliding = true;
					}
				}
				break;
			default:
				break;
			}

			if (!colliding)
			{

			switch (direction)
			{
			case NORTH:
				translation.z += offset;
				break;
			case SOUTH:
				translation.z -= offset;
				break;
			case EAST:
				translation.x -= offset;
				break;
			case WEST:
				translation.x += offset;
				break;
			default:
				break;
			}
			}
		}

		vec3 rotationAxis = vec3(0, 1, 0);
		if (behaviour == ROTATING)
		{
			colliding = false;
			double localDegrees = (currentTime - startRotationTime)/0.5 * 90;
			if (localDegrees > 90) {
				rotatingLeft = false;
				rotatingRight = false;
				behaviour = IDLE;
			}
			if (rotatingLeft)
				localDegrees = -localDegrees;
			int angleOffset = 0;
			switch (direction)
			{
			case NORTH:
				if (rotatingLeft)
					angleOffset = 90;
				else
					angleOffset = 270;
				break;
			case SOUTH:
				if (rotatingLeft)
					angleOffset = 270;
				else
					angleOffset = 90;
				break;
			case EAST:
				if (rotatingLeft)
					angleOffset = 180;
				else
					angleOffset = 0;
				break;
			case WEST:
				if (rotatingLeft)
					angleOffset = 0;
				else
					angleOffset = 180;
				break;
			default:
				break;
			}
			rotationDegrees = localDegrees + angleOffset ;
		}
		if (currentTime - lastTime >= 1.0) { 
			
			nbFrames = 0;
			lastTime += 1.0;
		}

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs(nUseMouse, g_nWidth, g_nHeight);
		ProjectionMatrix = getProjectionMatrix();
		mat4 RotationMatrix = rotate(ViewMatrix, (float)rotationDegrees + 180, rotationAxis);
		mat4 TranslationMatrix = translate(BaseTranslationMatrix, - translation);

		move();
		//ViewMatrix = lookAt(cameraPosition, direction, vec3(0, 1, 0));
		mat4 FinalMatrix = RotationMatrix * TranslationMatrix;

		
		glm::mat4 ModelMatrix      = glm::mat4(1.0);

		mat4 suzanneMatrix = mat4(1.0);

		suzanneMatrix = translate(suzanneMatrix, vec3(goalX + 0.5, 0.4, goalY + 0.5));
		suzanneMatrix = scale(suzanneMatrix, vec3(0.3, 0.3, 0.3));

		float rotationDegrees = (currentTime/ANIMATION_TIME) * 360;
		suzanneMatrix = rotate(suzanneMatrix, rotationDegrees, vec3(0, 1, 0));
		if (startingAnimation)
		{
			double modelHeight = currentTime/ANIMATION_TIME;
			if (modelHeight >= 1) modelHeight = 1;
			ModelMatrix = scale(ModelMatrix, vec3(1.0, modelHeight, 1.0));
		}
		if (finishAnimation)
		{
			double modelHeight = 1 - ((currentTime - finishAnimationTime) / ANIMATION_TIME);
			if (modelHeight <= 0) return 0;
			ModelMatrix = scale(ModelMatrix, vec3(1.0, modelHeight, 1.0));
		}

		glm::mat4 MVP = ProjectionMatrix * FinalMatrix * ModelMatrix;
		glm::mat4 MVP_suzanne = ProjectionMatrix * FinalMatrix * suzanneMatrix;

		glm::vec3 lightPos = glm::vec3(4, 4, 4);
		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureMaze);
		// Set our "myTextureSampler" sampler to user Texture Unit 0
		glUniform1i(TextureID, 0);

		// Send our transformation to the currently bound shader,
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &FinalMatrix[0][0]);

		
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,        // mode
			indices.size(),      // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0             // element array buffer offset
			);


		glBindTexture(GL_TEXTURE_2D, TextureSuzanne);

		// Send our transformation to the currently bound shader,
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP_suzanne[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &suzanneMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &FinalMatrix[0][0]);


		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer_suzanne);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer_suzanne);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer_suzanne);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer_suzanne);

		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,        // mode
			indices_suzanne.size(),      // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0             // element array buffer offset
			);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		
		// Swap buffers
		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(g_pWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
	glfwWindowShouldClose(g_pWindow) == 0);

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteBuffers(1, &elementbuffer);
	glDeleteBuffers(1, &vertexbuffer_suzanne);
	glDeleteBuffers(1, &uvbuffer_suzanne);
	glDeleteBuffers(1, &normalbuffer_suzanne);
	glDeleteBuffers(1, &elementbuffer_suzanne);
	glDeleteProgram(programID);
	glDeleteTextures(1, &TextureMaze);
	glDeleteTextures(1, &TextureSuzanne);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Terminate AntTweakBar and GLFW
	TwTerminate();
	glfwTerminate();

	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (finishAnimation) {
		behaviour = IDLE;
		return;
	}
	switch (key) {
	case GLFW_KEY_LEFT:
		if (behaviour == ROTATING) break;
		startRotationTime = currentTime;
		behaviour = ROTATING;
		rotatingLeft = true;
		switch (direction)
		{
		case NORTH:
			direction = WEST;
			break;
		case SOUTH:
			direction = EAST;
			break;
		case EAST:
			direction = NORTH;
			break;
		case WEST:
			direction = SOUTH;
			break;
		default:
			break;
		}
		break;
	case GLFW_KEY_RIGHT:
		if (behaviour == ROTATING) break;
		startRotationTime = currentTime;
		rotatingRight = true;
		behaviour = ROTATING;
		switch (direction)
		{
		case NORTH:
			direction = EAST;
			break;
		case SOUTH:
			direction = WEST;
			break;
		case EAST:
			direction = SOUTH;
			break;
		case WEST:
			direction = NORTH;
			break;
		default:
			break;
		}
		break;
	case GLFW_KEY_UP:
		if (behaviour != ROTATING)
		{
			if ((action == GLFW_PRESS || action == GLFW_REPEAT))
				behaviour = MOVING;
			else behaviour = IDLE;
		}
		break;
	default:
		behaviour = IDLE;
	}
}

float getDistanceToGoal(float x, float y) {
	float tmp1 = pow((x - goalX), 2);
	float tmp2 = pow((y - goalY), 2);
	return sqrt(tmp1 + tmp2);
}


class Sound
{
public:
	std::string szFileName;     // The filename and alias.
	std::string szCommand;      // The command to send.

	Sound() {
		szCommand = "close " + szFileName;
		mciSendString(szCommand.c_str(), NULL, 0, 0);
	}

	// Initializes the MP3 class.
	void Init(std::string szFileName) {
		this->szFileName = szFileName;

		szCommand = "open \"" + szFileName +
			"\" type mpegvideo alias " + szFileName;
		mciSendString(szCommand.c_str(), NULL, 0, 0);
	}

	// Play MP3
	void Play() {
		szCommand = "play " + szFileName + " from 0";
		mciSendString(szCommand.c_str(), NULL, 0, 0);
	}
};

void yellMonkey() {

	srand(time(NULL));
	int soundCode = (rand() % 6);
	
	Sound foo = Sound();

	switch (soundCode)
	{
	case 0:
		foo.Init("sfx/olha2.wav");
		break;
	case 1:
		foo.Init("sfx/olha.wav");
		break;
	case 2:
		foo.Init("sfx/macaco.wav");
		break;
	case 3:
		foo.Init("sfx/macaco2.wav");
		break;
	case 4:
		foo.Init("sfx/ha.wav");
		break;
	case 5:
		foo.Init("sfx/yeah.wav");
		break;
	default:
		break;
	}

	foo.Play();
}

void move() {
	bool movedBlock = false;
	float currentX = translation.x + 0.5;
	float currentY = translation.z + 0.5;

	float distanceToGoal = getDistanceToGoal((int) currentX, (int) currentY);
	float probabilityToYell = pow((1 / distanceToGoal), 2) * 0.25;

	float random = (float)rand() / (float)(RAND_MAX / 20);
	if (behaviour==MOVING && distanceToGoal<=3 && distanceToGoal>0.3)
	{
		if (random < probabilityToYell)
			yellMonkey();
		
	}

	if ((int) (currentX) != lastX) {
		movedBlock = true;
		lastX = (int) currentX;
	}
	if ((int) (currentY) != lastY) {
		movedBlock = true;
		lastY = (int) currentY;
	}
	if (movedBlock) {
		if (lastX==goalX && lastY==goalY)
		{

			PlaySound(TEXT("sfx/yeah.wav"), NULL, SND_LOOP | SND_ASYNC);
			reachedGoal = true;
			finishAnimationTime = currentTime;
			finishAnimation = true;
		}
	}
}



void getWallInfo(std::vector<vec3> &vertices) {
	//initialize walls matrix
	for (int i = 0; i < 13; i++)
		for (int j = 0; j < 13; j++) {
			walls[i][j].toNorth = false;
			walls[i][j].toEast = false;
		}
		
	vec3 v1,v2,v3;
	for (int i = 0; i < vertices.size(); i+=3) {
		v1 = vertices[i];
		v2 = vertices[i+1];
		v3 = vertices[i+2];

		int x, y;
		
		
		//if there are only two vertices on the ground, Y sum will be 1
		//therefore, a wall has been found
		if ((v1.y+v2.y+v3.y == 1))
		{
			
			if (v1.y==0)
			{
				if (v2.y == 0) {
					if (v1.x + v1.z < v2.x + v2.z) {
						if (v1.x < v2.x)
							walls[(int)v1.x][(int)v1.z].toEast = true;
						else
							walls[(int)v1.x][(int)v1.z].toNorth = true;
					}
				}
				else if (v1.x + v1.z < v3.x + v3.z) {
					if (v1.x + v1.z < v3.x + v3.z) {
						if (v1.x < v3.x)
							walls[(int)v1.x][(int)v1.z].toEast = true;
						else
							walls[(int)v1.x][(int)v1.z].toNorth = true;
					}
				}
			}
			else if (v2.x + v2.z < v3.x + v3.z) {
				if (v2.x + v2.z < v3.x + v3.z) {
					if (v2.x < v3.x)
						walls[(int)v2.x][(int)v2.z].toEast = true;
					else
						walls[(int)v2.x][(int)v2.z].toNorth = true;
				}
			}
		}
		

	}
}
