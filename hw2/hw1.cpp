/*
  CSCI 420 Computer Graphics, USC
  Assignment 2: Roller Coaster Simulator
*/

#include "basicPipelineProgram.h"
#include "texturePipelineProgram.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "glutHeader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

// for mouse control
int mousePos[2]; // x,y coordinate of the mouse position
int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not
typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;
// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework II";
// OpenGLMatrix
OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;

// represents one control point along the spline 
struct Point
{
    double x;
    double y;
    double z;
};
// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline
{
    int numControlPoints;
    Point* points;
};
// the spline array 
Spline* splines;
// total number of splines 
int numSplines;

// additional variables
vector<glm::vec3> splinePoints, splinePointsTangent, splinePointsNormal, splinePointsBinormal;
vector<glm::vec3> splineTriangles, splineTrianglesCol;
GLuint splinePointsVao, splinePointsPosVbo, splinePointsColVbo;
// for ground
TexturePipelinePorgram* texturePipelineProgram;
GLuint texture;
vector<glm::vec3> groundTriPoints;
vector<glm::vec2> texturePoints;
GLuint groundPointsVao, groundPointsPosVbo, groundPointsTexVbo, texHandle;
const float bound = 300.0f;
// for camera
int currFrame = 0;
float offsetScalar = 0.72f;
// phong shading: light and mesh
GLfloat La[4] = { 0.45f, 0.5f, 0.7f };
GLfloat Ld[4] = { 0.2f, 0.25f, 1.0f };
GLfloat Ls[4] = { 0.5f, 0.7f, 0.2f };
GLfloat ka[4] = { 0.36f, 0.36f, 0.36f };
GLfloat kd[4] = { 0.4f, 0.4f, 0.4f };
GLfloat ks[4] = { 0.7f, 0.7f, 0.7f };
GLfloat viewLightDirection[3];
GLfloat alpha = 1.5f;
glm::vec3 light = glm::vec3(0.0f,1.0f,0.0f);
// for screenshot
int numScreenShots = 0;
bool firstFrame = true;
// write a screenshot to the specified filename
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

void SetCamera()
{
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.LoadIdentity();
    glm::vec3 eye = splinePoints[currFrame] + offsetScalar * splinePointsBinormal[currFrame];
    glm::vec3 focus = splinePoints[currFrame] + splinePointsTangent[currFrame];
    glm::vec3 up = splinePointsBinormal[currFrame];
    matrix.LookAt(
        eye.x, eye.y, eye.z, // eye x,y,z
        focus.x, focus.y, focus.z, // focus point x,y,z (center of the image)
        up.x, up.y, up.z // up vector x,y,z
    );
}

void SetBindMatrices()
{
    // set normal, modelview, and projection matrix
    float n[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetNormalMatrix(n);
    GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "normalMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, n);
    
    float m[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetMatrix(m);

    float p[16];
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.GetMatrix(p);

    pipelineProgram->Bind();
    pipelineProgram->SetModelViewMatrix(m);
    pipelineProgram->SetProjectionMatrix(p);

    texturePipelineProgram->Bind();
    texturePipelineProgram->SetModelViewMatrix(m);
    texturePipelineProgram->SetProjectionMatrix(p);
}

void lightAndMeshHelper()
{
    pipelineProgram->Bind();
    GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "La");
    glUniform4fv(loc, 1, La);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ld");
    glUniform4fv(loc, 1, Ld);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ls");
    glUniform4fv(loc, 1, Ls);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ka");
    glUniform4fv(loc, 1, ka);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "kd");
    glUniform4fv(loc, 1, kd);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ks");
    glUniform4fv(loc, 1, ks);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "alpha");
    glUniform1f(loc, alpha);
    float view[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.LoadIdentity();
    matrix.GetMatrix(view);
    viewLightDirection[0] = (view[0] * light.x) + (view[4] * light.y) + (view[8] * light.z);
    viewLightDirection[1] = (view[1] * light.x) + (view[5] * light.y) + (view[9] * light.z);
    viewLightDirection[2] = (view[2] * light.x) + (view[6] * light.y) + (view[10] * light.z);
    loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "viewLightDirection");
    glUniform3fv(loc, 1, viewLightDirection);
}

void Render()
{
    // render
    pipelineProgram->Bind();
    glBindVertexArray(splinePointsVao);
    glDrawArrays(GL_TRIANGLES, 0, splineTriangles.size());

    texturePipelineProgram->Bind();
    GLint loc = glGetUniformLocation(texturePipelineProgram->GetProgramHandle(), "textureImage");
    glUniform1i(loc, 0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(groundPointsVao);
    glDrawArrays(GL_TRIANGLES, 0, groundTriPoints.size());
    // unbind vao
    glBindVertexArray(0);
}
void displayFunc()
{
  // render some stuff...
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  SetCamera();
  SetBindMatrices();
  lightAndMeshHelper();
  Render();
  glutSwapBuffers();
}

void idleFunc()
{
    // camera transition and screenshots
    if (currFrame < splinePoints.size() - 1) {
        currFrame++;
        //
        //if (numScreenShots < 1000) {
        //    if (numScreenShots == 0 && firstFrame) {
        //        firstFrame = false;
        //    }
        //    else {
        //        std::stringstream ss;
        //        ss << std::setw(3) << std::setfill('0') << numScreenShots;
        //        string name = ss.str();
        //        name = "screenshots/" + name + ".jpg";
        //        saveScreenshot(name.c_str());
        //        numScreenShots++;
        //    }
        //}
    }
    glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  matrix.Perspective(54.0f, (float)w / (float)h, 0.01f, 1000.0f);
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
}

void mouseMotionDragFunc(int x, int y)
{
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.01f;
        landTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the landscape
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the landscape
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // if CTRL and SHIFT are not pressed, we are in rotate mode
    default:
      controlState = ROTATE;
    break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // take a screenshot
      saveScreenshot("screenshot.jpg");
    break;
  }
}

int initTexture(const char* imageFilename, GLuint textureHandle)
{
    // read the texture image
    ImageIO img;
    ImageIO::fileFormatType imgFormat;
    ImageIO::errorType err = img.load(imageFilename, &imgFormat);

    if (err != ImageIO::OK)
    {
        printf("Loading texture from %s failed.\n", imageFilename);
        return -1;
    }

    // check that the number of bytes is a multiple of 4
    if (img.getWidth() * img.getBytesPerPixel() % 4)
    {
        printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
        return -1;
    }

    // allocate space for an array of pixels
    int width = img.getWidth();
    int height = img.getHeight();
    unsigned char* pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

    // fill the pixelsRGBA array with the image pixels
    memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
    for (int h = 0; h < height; h++)
        for (int w = 0; w < width; w++)
        {
            // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
            pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
            pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
            pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
            pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

            // set the RGBA channels, based on the loaded image
            int numChannels = img.getBytesPerPixel();
            for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
                pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
        }

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, textureHandle);

    // initialize the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

    // generate the mipmaps for this texture
    glGenerateMipmap(GL_TEXTURE_2D);

    // set the texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // query support for anisotropic texture filtering
    GLfloat fLargest;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
    printf("Max available anisotropic samples: %f\n", fLargest);
    // set anisotropic texture filtering
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

    // query for any errors
    GLenum errCode = glGetError();
    if (errCode != 0)
    {
        printf("Texture initialization error. Error code: %d.\n", errCode);
        return -1;
    }

    // de-allocate the pixel array -- it is no longer needed
    delete[] pixelsRGBA;

    return 0;
}

glm::vec3 GetNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3)
{
    glm::vec3 U = p2 - p1;
    glm::vec3 V = p3 - p1;
    return glm::cross(U, V);
}

// compute splinePoint, tangent, normal, and binormal
void computeSplines()
{
    const float s = 0.5f;
    glm::mat4 basis = glm::mat4(
        -s, 2.0f - s, s - 2.0f, s,
        2.0f * s, s - 3.0f, 3.0f - 2.0f * s, -s,
        -s, 0, s, 0,
        0, 1, 0, 0
    );
    // compute splinePoints and their unit tangent
    for (int i = 0; i < numSplines; i++)
    {
        for (int j = 0; j < splines[i].numControlPoints - 3; j++)
        {
            // control points
            Point ONE = splines[i].points[j];
            Point TWO = splines[i].points[j + 1];
            Point THREE = splines[i].points[j + 2];
            Point FOUR = splines[i].points[j + 3];

            // Generate Catmull-Rom Spline control Matrix
            glm::mat4x3 control = glm::mat4x3(
                ONE.x, ONE.y, ONE.z,
                TWO.x, TWO.y, TWO.z,
                THREE.x, THREE.y, THREE.z,
                FOUR.x, FOUR.y, FOUR.z
            );

            for (float u = 0.0f; u <= 1.0f; u += 0.005f) {
                // get spline point
                // p(u) = [u^3 u^2 u 1]
                glm::vec4 p(u*u*u, u*u, u, 1);
                glm::vec3 point = control * basis * p;
                splinePoints.push_back(point);

                // get tangent for the spline point: t(u) = p'(u) = [3u^2 2u 1 0]
                glm::vec4 t(3*u*u, 2*u, 1, 0);
                glm::vec3 tangent = control * basis * t;
                // get unit tangent
                glm::vec3 unitTangent = glm::normalize(tangent);
                splinePointsTangent.push_back(unitTangent);
            }
        }
    }

    // compute normal and binormal: Sloan's method
    for (int i = 0; i < splinePoints.size(); i++) {
        if (i == 0) { // generate starting set of axes at point P0, given T0
            // Pick an arbitrary vector V
            glm::vec3 v(0.0f, 1.0f, 0.0f);
            // Make N0 = unit(T0 x V)
            glm::vec3 normal = glm::normalize(glm::cross(splinePointsTangent[0], v));
            splinePointsNormal.push_back(normal);
            // Make B0 = unit(T0 x N0)
            glm::vec3 binormal = glm::normalize(glm::cross(splinePointsTangent[0], splinePointsNormal[0]));
            splinePointsBinormal.push_back(binormal);
        }
        else { // calculate set of P1 and T1, given P0
            // Make N1 = unit(B0 x T1)
            glm::vec3 normal = glm::normalize(glm::cross(splinePointsBinormal[i - 1], splinePointsTangent[i]));
            splinePointsNormal.push_back(normal);
            // Make B1 = unit(T1 x N1)
            glm::vec3 binormal = glm::normalize(glm::cross(splinePointsTangent[i], splinePointsNormal[i]));
            splinePointsBinormal.push_back(binormal);
        }
    }
}

void computeSplineTriangles()
{
    // compute all vertices of triangles for rail cross-section
    for (int i = 0; i < splinePoints.size() - 1; i++) {
        // compute vertices v0, v1, ..., v7 for each splinePoint
        const float alphaValue = 0.2f;
        glm::vec3 v0(
            splinePoints[i] + alphaValue * (-(splinePointsNormal[i]) + splinePointsBinormal[i])
        );
        glm::vec3 v1(
            splinePoints[i] + alphaValue * (splinePointsNormal[i] + splinePointsBinormal[i])
        );
        glm::vec3 v2(
            splinePoints[i] + alphaValue * (splinePointsNormal[i] - splinePointsBinormal[i])
        );
        glm::vec3 v3(
            splinePoints[i] + alphaValue * (-(splinePointsNormal[i]) - splinePointsBinormal[i])
        );

        glm::vec3 v4(
            splinePoints[i + 1] + alphaValue * (-(splinePointsNormal[i + 1]) + splinePointsBinormal[i + 1])
        );
        glm::vec3 v5(
            splinePoints[i + 1] + alphaValue * (splinePointsNormal[i + 1] + splinePointsBinormal[i + 1])
        );
        glm::vec3 v6(
            splinePoints[i + 1] + alphaValue * (splinePointsNormal[i + 1] - splinePointsBinormal[i + 1])
        );
        glm::vec3 v7(
            splinePoints[i + 1] + alphaValue * (-(splinePointsNormal[i + 1]) - splinePointsBinormal[i + 1])
        );
        // form triangle vertices and calculate normal
        glm::vec3 BN0 = splinePointsBinormal[i];
        glm::vec3 BN1 = splinePointsBinormal[i + 1];
        glm::vec3 N0 = splinePointsNormal[i];
        glm::vec3 N1 = splinePointsNormal[i + 1];
        // top triangle v1,v2,v5
        splineTriangles.push_back(v1);
        splineTriangles.push_back(v2);
        splineTriangles.push_back(v5);
        splineTrianglesCol.push_back(N0);
        splineTrianglesCol.push_back(N0);
        splineTrianglesCol.push_back(N1);
        // top triangle v6,v2,v5
        splineTriangles.push_back(v6);
        splineTriangles.push_back(v2);
        splineTriangles.push_back(v5);
        splineTrianglesCol.push_back(N1);
        splineTrianglesCol.push_back(N0);
        splineTrianglesCol.push_back(N1);
        // bot triangle v0,v3,v4
        splineTriangles.push_back(v0);
        splineTriangles.push_back(v3);
        splineTriangles.push_back(v4);
        splineTrianglesCol.push_back(-N0);
        splineTrianglesCol.push_back(-N0);
        splineTrianglesCol.push_back(-N1);
        // bot triangle v7,v3,v4
        splineTriangles.push_back(v7);
        splineTriangles.push_back(v3);
        splineTriangles.push_back(v4);
        splineTrianglesCol.push_back(-N1);
        splineTrianglesCol.push_back(-N0);
        splineTrianglesCol.push_back(-N1);
        // left triangle v3,v2,v7
        splineTriangles.push_back(v3);
        splineTriangles.push_back(v2);
        splineTriangles.push_back(v7);
        splineTrianglesCol.push_back(-BN0);
        splineTrianglesCol.push_back(-BN0);
        splineTrianglesCol.push_back(-BN1);
        // left triangle v6,v2,v7
        splineTriangles.push_back(v6);
        splineTriangles.push_back(v2);
        splineTriangles.push_back(v7);
        splineTrianglesCol.push_back(-BN1);
        splineTrianglesCol.push_back(-BN0);
        splineTrianglesCol.push_back(-BN1);
        // right triangle v0,v1,v4
        splineTriangles.push_back(v0);
        splineTriangles.push_back(v1);
        splineTriangles.push_back(v4);
        splineTrianglesCol.push_back(BN0);
        splineTrianglesCol.push_back(BN0);
        splineTrianglesCol.push_back(BN1);
        // right triangle v5,v1,v4
        splineTriangles.push_back(v5);
        splineTriangles.push_back(v1);
        splineTriangles.push_back(v4);
        splineTrianglesCol.push_back(BN1);
        splineTrianglesCol.push_back(BN0);
        splineTrianglesCol.push_back(BN1);
    }
}

void SplinesPushData()
{
    pipelineProgram->Bind();
    // bind vao
    glGenVertexArrays(1, &splinePointsVao);
    glBindVertexArray(splinePointsVao);
    // for position
    // bind vbo
    glGenBuffers(1, &splinePointsPosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, splinePointsPosVbo);
    glBufferData(GL_ARRAY_BUFFER, splineTriangles.size() * sizeof(glm::vec3), splineTriangles.data(), GL_STATIC_DRAW);
    // get location index of the “position” shader variable
    GLuint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    // set the layout of the “position” attribute data
    const void* offset = (const void*)0;
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // for normal
    // bind vbo
    glGenBuffers(1, &splinePointsColVbo);
    glBindBuffer(GL_ARRAY_BUFFER, splinePointsColVbo);
    glBufferData(GL_ARRAY_BUFFER, splineTrianglesCol.size() * sizeof(glm::vec3), splineTrianglesCol.data(), GL_STATIC_DRAW);
    // get the location index of the “normal” shader variable
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "normal");
    glEnableVertexAttribArray(loc); // enable the “normal” attribute
    // set the layout of the “normal” attribute data
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    glBindVertexArray(0); // unbind the VAO
}

void initSplines()
{
    computeSplines();
    computeSplineTriangles();
    SplinesPushData();
}

void computeGroundTri()
{
    glGenTextures(1, &texHandle);
    if (initTexture("mars.jpg", texture) == 0) {
        // triangle bottom-left, top-right, bottom-right
        groundTriPoints.push_back(glm::vec3(-bound, 50.0f, -bound));
        groundTriPoints.push_back(glm::vec3(bound, 50.0f, bound));
        groundTriPoints.push_back(glm::vec3(bound, 50.0f, -bound));
        texturePoints.push_back(glm::vec2(0, 0));
        texturePoints.push_back(glm::vec2(1, 1));
        texturePoints.push_back(glm::vec2(1, 0));
        // triangle bottom-left, top-right, top-left
        groundTriPoints.push_back(glm::vec3(-bound, 50.0f, -bound));
        groundTriPoints.push_back(glm::vec3(bound, 50.0f, bound));
        groundTriPoints.push_back(glm::vec3(-bound, 50.0f, bound));
        texturePoints.push_back(glm::vec2(0, 0));
        texturePoints.push_back(glm::vec2(1, 1));
        texturePoints.push_back(glm::vec2(0, 1));
    }
}

void GroundPushData()
{
    // bind vao
    glBindVertexArray(groundPointsVao);
    // for position
    // bind vbo
    glGenBuffers(1, &groundPointsPosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, groundPointsPosVbo);
    glBufferData(GL_ARRAY_BUFFER, groundTriPoints.size() * sizeof(glm::vec3), groundTriPoints.data(), GL_STATIC_DRAW);
    // get location index of the “position” shader variable
    GLuint loc = glGetAttribLocation(texturePipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    // set the layout of the “position” attribute data
    const void* offset = (const void*)0;
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // for texture
    // bind vbo
    glGenBuffers(1, &groundPointsTexVbo);
    glBindBuffer(GL_ARRAY_BUFFER, groundPointsTexVbo);
    glBufferData(GL_ARRAY_BUFFER, texturePoints.size() * sizeof(glm::vec2), texturePoints.data(), GL_STATIC_DRAW);
    // get the location index of the “texturePos” shader variable
    loc = glGetAttribLocation(texturePipelineProgram->GetProgramHandle(), "texCoord");
    glEnableVertexAttribArray(loc); // enable the ““texCoord” attribute
    // set the layout of the “texCoord” attribute data
    glVertexAttribPointer(loc, 2, GL_FLOAT, normalized, stride, offset);
    glBindVertexArray(0); // unbind the VAO
}

void initGround()
{
    computeGroundTri();
    GroundPushData();
}

void initScene(int argc, char *argv[])
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    pipelineProgram = new BasicPipelineProgram;
    int ret = pipelineProgram->Init(shaderBasePath);
    if (ret != 0) abort();
    
    texturePipelineProgram = new TexturePipelinePorgram;
    ret = texturePipelineProgram->Init(shaderBasePath);
    if (ret != 0) abort();

    // other initializations
    initSplines();
    initGround();

    std::cout << "GL error: " << glGetError() << std::endl;
}

int loadSplines(char* argv)
{
    char* cName = (char*)malloc(128 * sizeof(char));
    FILE* fileList;
    FILE* fileSpline;
    int iType, i = 0, j, iLength;

    // load the track file 
    fileList = fopen(argv, "r");
    if (fileList == NULL)
    {
        printf("can't open file\n");
        exit(1);
    }

    // stores the number of splines in a global variable 
    fscanf(fileList, "%d", &numSplines);

    splines = (Spline*)malloc(numSplines * sizeof(Spline));

    // reads through the spline files 
    for (j = 0; j < numSplines; j++)
    {
        i = 0;
        fscanf(fileList, "%s", cName);
        fileSpline = fopen(cName, "r");

        if (fileSpline == NULL)
        {
            printf("can't open file\n");
            exit(1);
        }

        // gets length for spline file
        fscanf(fileSpline, "%d %d", &iLength, &iType);

        // allocate memory for all the points
        splines[j].points = (Point*)malloc(iLength * sizeof(Point));
        splines[j].numControlPoints = iLength;

        // saves the data to the struct
        while (fscanf(fileSpline, "%lf %lf %lf",
            &splines[j].points[i].x,
            &splines[j].points[i].y,
            &splines[j].points[i].z) != EOF)
        {
            i++;
        }
    }

    free(cName);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <trackfile>\n", argv[0]);
        exit(0);
    }

    cout << "Initializing GLUT..." << endl;
    glutInit(&argc,argv);

    cout << "Initializing OpenGL..." << endl;

    #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    #endif

    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(0, 0);  
    glutCreateWindow(windowTitle);

    cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
    cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

    #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
    #endif

    // tells glut to use a particular display function to redraw 
    glutDisplayFunc(displayFunc);
    // perform animation inside idleFunc
    glutIdleFunc(idleFunc);
    // callback for mouse drags
    glutMotionFunc(mouseMotionDragFunc);
    // callback for idle mouse movement
    glutPassiveMotionFunc(mouseMotionFunc);
    // callback for mouse button changes
    glutMouseFunc(mouseButtonFunc);
    // callback for resizing the window
    glutReshapeFunc(reshapeFunc);
    // callback for pressing the keys on the keyboard
    glutKeyboardFunc(keyboardFunc);

    // init glew
    #ifdef __APPLE__
    // nothing is needed on Apple
    #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
        cout << "error: " << glewGetErrorString(result) << endl;
        exit(EXIT_FAILURE);
    }
    #endif
  
    // load the splines from the provided filename
    loadSplines(argv[1]);

    printf("Loaded %d spline(s).\n", numSplines);
    for (int i = 0; i < numSplines; i++)
        printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);

    // do initialization
    initScene(argc, argv);
    // sink forever into the glut loop
    glutMainLoop();
}


