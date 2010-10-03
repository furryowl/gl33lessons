#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include "common.h"
#include "math3d/math3d.h"
#include "OpenGL.h"
#include "GLWindow.h"
#include "Texture.h"

// пременные для хранения идентификаторов шейдерной программы и шейдеров
static GLuint shaderProgram = 0, vertexShader = 0, fragmentShader = 0, colorTexture = 0;

static int    cursorPos[2] = {0,0};

static float  meshRotation[2] = {0.0f, 0.0f}, cursorDelta[2] = {0.0f, 0.0f};

static mat4   modelMatrix(0.0f), viewMatrix(0.0f), projectionMatrix(0.0f);

static GLint  positionLocation = -1, texcoordLocation = -1, modelMatrixLocation = -1, 
              viewMatrixLocation = -1, projectionMatrixLocation = -1, colorTextureLocation = -1;

// Cube mesh
namespace Cube
{
	static GLuint VBO[3], VAO;

	static const uint32_t verticesCount = 24;
	static const uint32_t indicesCount  = 36;
	static const float    s             = 1.0f; // size

	// vertices
	static const float positions[verticesCount][3] = {
		{-s, s, s}, { s, s, s}, { s,-s, s}, {-s,-s, s}, // front
		{ s, s,-s}, {-s, s,-s}, {-s,-s,-s}, { s,-s,-s}, // back
		{-s, s,-s}, { s, s,-s}, { s, s, s}, {-s, s, s}, // top
		{ s,-s,-s}, {-s,-s,-s}, {-s,-s, s}, { s,-s, s}, // bottom
		{-s, s,-s}, {-s, s, s}, {-s,-s, s}, {-s,-s,-s}, // left
		{ s, s, s}, { s, s,-s}, { s,-s,-s}, { s,-s, s}  // right
	};

	// texcoords
	static const float texcoords[verticesCount][2] = {
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}, // front
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}, // back
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}, // top
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}, // bottom
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}, // left
		{0.0f,1.0f}, {1.0f,1.0f}, {1.0f,0.0f}, {0.0f,0.0f}  // right
	};

	// indices in CCW order
	static const uint32_t indices[indicesCount] = {
		 0, 3, 1,  1, 3, 2, // front
		 4, 7, 5,  5, 7, 6, // back
		 8,11, 9,  9,11,10, // top
		12,15,13, 13,15,14, // bottom
		16,19,17, 17,19,18, // left
		20,23,21, 21,23,22  // right
	};
}

// инициализаця OpenGL
bool GLWindowInit(const GLWindow *window)
{
	ASSERT(window);

	uint8_t  *shaderSource;
	uint32_t sourceLength;

	// устанавливаем вьюпорт на все окно
	glViewport(0, 0, window->width, window->height);

	// параметры OpenGL
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glActiveTexture(GL_TEXTURE0);
	if ((colorTexture = TextureCreateFromTGA("data/texture.tga")) == 0)
		return false;

	// создадим шейдерную программу и шейдеры для нее
	shaderProgram  = glCreateProgram();
	vertexShader   = glCreateShader(GL_VERTEX_SHADER);
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	if (!shaderProgram || !vertexShader || !fragmentShader)
	{
		LOG_ERROR("Creating shader program fail (0x%X)\n", (unsigned)glGetError());
		return false;
	}

	// загрузим вершинный шейдер
	if (!LoadFile("data/lesson.vs", true, &shaderSource, &sourceLength))
		return false;

	// зададим шейдеру исходный код и скомпилируем его
	glShaderSource(vertexShader, 1, (const GLchar**)&shaderSource, (const GLint*)&sourceLength);
	glCompileShader(vertexShader);

	delete[] shaderSource;

	// проверим статус шейдера
	if (ShaderStatus(vertexShader, GL_COMPILE_STATUS) != GL_TRUE)
		return false;

	// загрузим фрагментный шейдер
	if (!LoadFile("data/lesson.fs", true, &shaderSource, &sourceLength))
		return false;

	// зададим шейдеру исходный код и скомпилируем его
	glShaderSource(fragmentShader, 1, (const GLchar**)&shaderSource, (const GLint*)&sourceLength);
	glCompileShader(fragmentShader);

	delete[] shaderSource;

	// проверим статус шейдера
	if (ShaderStatus(fragmentShader, GL_COMPILE_STATUS) != GL_TRUE)
		return false;

	// присоеденим загруженные шейдеры к шейдерной программе
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);

	// проверим не было ли ошибок
	OPENGL_CHECK_FOR_ERRORS();

	// линковка шейдерной программы и проверка статуса линковки
	glLinkProgram(shaderProgram);
	if (ShaderProgramStatus(shaderProgram, GL_LINK_STATUS) != GL_TRUE)
		return false;

	// сделаем шейдер активным
	glUseProgram(shaderProgram);

	// создадим перспективную матрицу
	float aspectRatio = (float)window->width / (float)window->height;
	projectionMatrix  = perspective(45.0f, aspectRatio, 1.0f, 10.0f);
	modelMatrix       = rotation(0.0f, 0.0f, 0.0f);
	viewMatrix        = translation(0.0f, 0.0f, -6.0f);

	if ((projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix")) != -1)
		glUniformMatrix4fv(projectionMatrixLocation, 1, GL_TRUE, projectionMatrix.m);

	if ((modelMatrixLocation = glGetUniformLocation(shaderProgram, "modelMatrix")) != -1)
		glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.m);

	if ((viewMatrixLocation = glGetUniformLocation(shaderProgram, "viewMatrix")) != -1)
		glUniformMatrix4fv(viewMatrixLocation, 1, GL_TRUE, viewMatrix.m);

	if ((colorTextureLocation = glGetUniformLocation(shaderProgram, "colorTexture")) != -1)
		glUniform1i(colorTextureLocation, 0);

	// проверка на корректность шейдерной программы
	glValidateProgram(shaderProgram);
	if (ShaderProgramStatus(shaderProgram, GL_VALIDATE_STATUS) != GL_TRUE)
		return false;

	// проверим не было ли ошибок
	OPENGL_CHECK_FOR_ERRORS();

	// создадим и используем Vertex Array Object (VAO)
	glGenVertexArrays(1, &Cube::VAO);
	glBindVertexArray(Cube::VAO);

	glGenBuffers(3, Cube::VBO);

	glBindBuffer(GL_ARRAY_BUFFER, Cube::VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, Cube::verticesCount * (3 * sizeof(float)),
		Cube::positions, GL_STATIC_DRAW);

	positionLocation = glGetAttribLocation(shaderProgram, "position");
	if (positionLocation != -1)
	{
		glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
		glEnableVertexAttribArray(positionLocation);
	}

	glBindBuffer(GL_ARRAY_BUFFER, Cube::VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, Cube::verticesCount * (2 * sizeof(float)),
		Cube::texcoords, GL_STATIC_DRAW);

	texcoordLocation = glGetAttribLocation(shaderProgram, "texcoord");
	if (texcoordLocation != -1)
	{
		glVertexAttribPointer(texcoordLocation, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
		glEnableVertexAttribArray(texcoordLocation);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Cube::VBO[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Cube::indicesCount * sizeof(uint32_t),
		Cube::indices, GL_STATIC_DRAW);

	OPENGL_CHECK_FOR_ERRORS();

	return true;
}

// очистка OpenGL
void GLWindowClear(const GLWindow *window)
{
	ASSERT(window);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(3, Cube::VBO);

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &Cube::VAO);

	glUseProgram(0);
	glDeleteProgram(shaderProgram);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glDeleteTextures(1, &colorTexture);
}

// функция рендера
void GLWindowRender(const GLWindow *window)
{
	ASSERT(window);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgram);

	if (modelMatrixLocation != -1)
		glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.m);

	glBindVertexArray(Cube::VAO);
	glDrawElements(GL_TRIANGLES, Cube::indicesCount, GL_UNSIGNED_INT, NULL);

	// проверка на ошибки
	OPENGL_CHECK_FOR_ERRORS();
}

// функция обновления
void GLWindowUpdate(const GLWindow *window, double deltaTime)
{
	ASSERT(window);
	ASSERT(deltaTime >= 0.0); // проверка на возможность бага

	meshRotation[0] += (float)deltaTime * 0.1f * cursorDelta[1];
	meshRotation[1] += (float)deltaTime * 0.1f * cursorDelta[0];

	cursorDelta[0] *= 0.995f;
	cursorDelta[1] *= 0.995f;

	modelMatrix = rotation(meshRotation[0], meshRotation[1], 0.0f) * translation(0.0f, 0.0f, 2.0f);
}

// функция обработки ввода с клавиатуры и мыши
void GLWindowInput(const GLWindow *window)
{
	ASSERT(window);

	int xCenter = window->width / 2, yCenter = window->height / 2;

	InputGetCursorPos(cursorPos, cursorPos + 1);

	if (InputIsButtonDown(0))
	{
		cursorDelta[0] += (float)(xCenter - cursorPos[0]);
		cursorDelta[1] += (float)(cursorPos[1] - yCenter);
	}

	InputSetCursorPos(xCenter, yCenter);

	// выход из приложения по кнопке Esc
	if (InputIsKeyPressed(VK_ESCAPE))
		GLWindowDestroy();

	// переключение между оконным и полноэкранным режимом
	// осуществляется по нажатию комбинации Alt+Enter
	if (InputIsKeyDown(VK_MENU) && InputIsKeyPressed(VK_RETURN))
		GLWindowSetSize(window->width, window->height, !window->fullScreen);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	int result;

	LoggerCreate("lesson03.log");

	if (!GLWindowCreate("lesson03", 800, 600, false))
		return 1;

	result = GLWindowMainLoop();

	GLWindowDestroy();
	LoggerDestroy();

	return result;
}
