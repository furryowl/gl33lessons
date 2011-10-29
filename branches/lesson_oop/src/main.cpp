/* TODO:
 * 1. Fix mouse capturing after focus lost
 */

#include "common.h"
#include "OpenGL.h"
#include "Window.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "RenderObject.h"
#include "Camera.h"
#include "Light.h"

class Window : public IWindow
{
public:
	bool initialize();
	void clear();
	void render();
	void update(double frameTime);

	void renderScene(const ShaderProgram &program, const Camera &camera);

	void input(const int32_t *cursorRel, int32_t wheelRel,
		uint8_t buttons, const uint8_t *keyStates, double frameTime);

protected:
	bool loadShaderProgram(ShaderProgram &program,
		const char *vname, const char *fname);

	Texture       m_objectTexture, m_objectTextureNormal, m_shadowTexture;
	ShaderProgram m_quadProgram, m_shadowProgram, m_renderProgram;
	Mesh          m_objectMesh, m_quadMesh;
	RenderObject  m_object, m_quadObject;
	Camera        m_camera, m_lightCamera;
	Material      m_objectMaterial, m_quadMaterial;
	Light         m_light;
	double        m_time;
	bool          m_renderQuad;

	Texture     m_colorTexture, m_normalTexture, m_depthTexture;
	Framebuffer m_defFBO, m_shadowFBO, m_renderFBO;
};

bool Window::loadShaderProgram(ShaderProgram &program,
	const char *vname, const char *fname)
{
	Shader m_vertex, m_fragment;

	if (!m_vertex.load(GL_VERTEX_SHADER, vname)
		|| !m_fragment.load(GL_FRAGMENT_SHADER, fname))
	{
		return false;
	}

	program.create();
	program.attach(m_vertex);
	program.attach(m_fragment);

	return program.link();
}

bool Window::initialize()
{
	m_time       = 0.0;
	m_renderQuad = false;

	glViewport(0, 0, m_width, m_height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	//glEnable(GL_MULTISAMPLE);

	if (!m_objectMesh.load("data/models/scene.bin", true, 0.05f))
		return false;

	m_objectTexture.create(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
	m_objectTextureNormal.create(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);

	if (!m_objectTexture.load2DPNG("data/textures/stoneDiffuse.png", true)
		|| !m_objectTextureNormal.load2DPNG("data/textures/stoneNormal.png", true))
	{
		return false;
	}

	if (!loadShaderProgram(m_shadowProgram, "data/shaders/depth.vs", "data/shaders/depth.fs")
		|| !loadShaderProgram(m_renderProgram, "data/shaders/shadowmap.vs", "data/shaders/shadowmap.fs")
		|| !loadShaderProgram(m_quadProgram, "data/shaders/quad.vs", "data/shaders/quad.fs"))
	{
		return false;
	}

	m_objectMaterial.setTexture(&m_objectTexture);
	m_objectMaterial.setTextureNormal(&m_objectTextureNormal);

	m_objectMaterial.setSpecular(0.5f, 0.5f, 0.5f, 1.0f);
	m_objectMaterial.setShininess(20.0f);

	m_object.setMesh(&m_objectMesh);
	m_object.setMaterial(&m_objectMaterial);

	const vec3 lightPosition(10.0f, 10.0f, 10.0f);
	m_light.setPosition(lightPosition.x, lightPosition.y, lightPosition.z, 0.0f);

	m_lightCamera.lookAt(lightPosition, -lightPosition, vec3_y);
	m_lightCamera.orthographic(-10.0f, 10.0f, -10.0f, 10.0f, -100.0f, 100.0f);
	//m_lightCamera.perspective(45.0f, (float)m_width / m_height, 1.0f, 50.0f);

	m_camera.lookAt(lightPosition, vec3_zero, vec3_y);
	m_camera.perspective(45.0f, (float)m_width / m_height, 0.1f, 50.0f);

	m_shadowTexture.create();
	m_shadowTexture.image2D(NULL, m_width * 2, m_height * 2,
		GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT);

	m_quadMesh.createFullscreenQuad();
	m_quadMaterial.setTexture(&m_shadowTexture);

	m_quadObject.setMesh(&m_quadMesh);
	m_quadObject.setMaterial(&m_quadMaterial);

	m_shadowFBO.create();
	m_shadowFBO.attach(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_shadowTexture);

	m_shadowFBO.setDrawBuffer(GL_NONE);
	m_shadowFBO.setReadBuffer(GL_NONE);

	if (!m_shadowFBO.complete())
		return false;

	m_colorTexture.create();
	m_colorTexture.image2D(NULL, m_width, m_height,
		GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

	m_normalTexture.create();
	m_normalTexture.image2D(NULL, m_width, m_height,
		GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);

	m_depthTexture.create();
	m_depthTexture.image2D(NULL, m_width, m_height,
		GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE);

	m_renderFBO.create();
	m_renderFBO.attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_colorTexture);
	m_renderFBO.attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, m_normalTexture);
	m_renderFBO.attach(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthTexture);

	GLenum modes[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	m_renderFBO.setDrawBuffers(2, modes);

	if (!m_renderFBO.complete())
		return false;

	m_defFBO.bind();

	GL_CHECK_FOR_ERRORS();

	return true;
}

void Window::clear()
{
	m_defFBO.bind();

	/*
	m_shadowFBO.destroy();
	m_shadowTexture.destroy();
	m_objectMesh.destroy();
	m_objectTexture.destroy();
	*/
}

void Window::renderScene(const ShaderProgram &program, const Camera &camera)
{
	program.bind();

	m_light.setup(program);
	m_lightCamera.setupLight(program);

	m_shadowTexture.setup(program, "depthTexture", 2, true);

	camera.setup(program, m_object);
	m_object.render(program);
}

void Window::render()
{
	m_shadowFBO.bind();

	glViewport(0, 0, m_width * 2, m_height * 2);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_FRONT);

	renderScene(m_shadowProgram, m_lightCamera);

	m_renderFBO.bind();

	glViewport(0, 0, m_width, m_height);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_BACK);

	renderScene(m_renderProgram, m_camera);

	m_defFBO.bind();

	m_quadProgram.bind();

	glViewport(0, 0, m_width, m_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask(GL_FALSE);

	m_quadMaterial.setTexture(&m_colorTexture);
	m_quadObject.render(m_quadProgram);

	if (!m_renderQuad)
		return;

	const int32_t vx = 3 * m_width / 4, vy = m_height,
		vw = m_width / 4, vh = m_height / 4;

	glViewport(vx, vy - vh, vw, vh);
	m_quadMaterial.setTexture(&m_normalTexture);
	m_quadObject.render(m_quadProgram);

	glViewport(vx, vy - vh * 2, vw, vh);
	m_quadMaterial.setTexture(&m_depthTexture);
	m_quadObject.render(m_quadProgram);

	glViewport(vx, vy - vh * 3, vw, vh);
	m_quadMaterial.setTexture(&m_shadowTexture);
	m_quadObject.render(m_quadProgram);

	GL_CHECK_FOR_ERRORS();
}

void Window::update(double frameTime)
{
	m_time += frameTime;
}

void Window::input(const int32_t *cursorRel, int32_t wheelRel,
	uint8_t buttons, const uint8_t *keyStates, double frameTime)
{
	(void)wheelRel;
	(void)buttons;

	if (keyStates[VK_ESCAPE] == INPUT_KEY_PRESSED)
		m_active = false;

	if (keyStates[VK_SPACE] == INPUT_KEY_PRESSED)
		m_renderQuad = !m_renderQuad;

	float moveX = (float)(keyStates['D'] > 0) - (float)(keyStates['A'] > 0);
	float moveZ = (float)(keyStates['S'] > 0) - (float)(keyStates['W'] > 0);

	m_camera.rotate(vec3(cursorRel[1], cursorRel[0], 0.0f) * 0.1f);
	m_camera.move(vec3(moveX, 0.0f, moveZ) * frameTime * 3);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	Window window;
	int    result;

	Log::create("lesson.log");

	if (!window.create("lesson", 800, 600, false))
	{
		Log::destroy();
		return 1;
	}

	result = window.mainLoop();

	window.destroy();
	Log::destroy();

	return result;
}