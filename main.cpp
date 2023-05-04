#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "main.hpp"
#include "models.hpp"
#include "openGLMatrix.h"
#include "opengl.hpp"
#include "shader.hpp"
#include "status.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include "types.hpp"

#define BUFFER_OFFSET(offset) ((GLvoid *)(offset))

#define FOV_Y 45
#define NEAR_Z 0.01
#define FAR_Z 10000

#define SCENE_AABB_SIDE_LEN 256
#define GROUND_VERTEX_COUNT 6
#define SKY_VERTEX_COUNT 36

#define RAIL_COLOR_RED 0.5
#define RAIL_COLOR_BLUE 0.5
#define RAIL_COLOR_GREEN 0.5
#define RAIL_COLOR_ALPHA 1
#define RAIL_HEAD_W 0.2
#define RAIL_HEAD_H 0.1
#define RAIL_WEB_W 0.1
#define RAIL_WEB_H 0.1
#define RAIL_GAUGE 2

#define CROSSTIE_SEPARATION_DIST 1
#define CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR -2

const char *kWindowTitle = "Roller Coaster";

const char *const kVertexFormatStrings[kVertexFormat__Count]{"untextured",
                                                             "textured"};

const char *const kShaderFilepaths[kVertexFormat__Count][kShaderType__Count] = {
    {"untextured.vert.glsl", "untextured.frag.glsl"},
    {"textured.vert.glsl", "textured.frag.glsl"}};

static const char *String(VertexFormat f) {
  assert(f < kVertexFormat__Count);
  return kVertexFormatStrings[f];
}

static void PressButton(MouseState *s, Button b) {
  assert(s);
  s->pressed_buttons |= 1 << b;
}

static void ReleaseButton(MouseState *s, Button b) {
  assert(s);
  s->pressed_buttons &= ~(1 << b);
}

static int IsButtonPressed(MouseState *s, Button b) {
  assert(s);
  return s->pressed_buttons >> b & 1;
}

static Button FromGlButton(int button) {
  switch (button) {
    case GLUT_LEFT_BUTTON: {
      return kButton_Left;
    }
    case GLUT_MIDDLE_BUTTON: {
      return kButton_Middle;
    }
    case GLUT_RIGHT_BUTTON: {
      return kButton_Right;
    }
    default: {
      assert(false);
    }
  }
}

static Status LoadSplines(const char *track_filepath,
                          std::vector<std::vector<glm::vec3>> *splines) {
  auto &splines_ = *splines;

  std::FILE *track_file = std::fopen(track_filepath, "r");
  if (!track_file) {
    std::fprintf(stderr, "Could not open track file %s\n", track_filepath);
    return kStatus_IoError;
  }

  uint spline_count;
  int ret = std::fscanf(track_file, "%u", &spline_count);
  if (ret < 1) {
    std::fprintf(stderr, "Could not read spline count from track file %s\n",
                 track_filepath);
    std::fclose(track_file);
    return kStatus_IoError;
  }

  splines->resize(spline_count);

  char filepath[4096];
  for (uint j = 0; j < spline_count; ++j) {
    int ret = std::fscanf(track_file, "%s", filepath);
    if (ret < 1) {
      std::fprintf(stderr,
                   "Could not read spline filename from track file %s\n",
                   track_filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    FILE *file = std::fopen(filepath, "r");
    if (!file) {
      std::fprintf(stderr, "Could not open spline file %s\n", filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    uint ctrl_point_count;
    uint type;
    ret = std::fscanf(file, "%u %u", &ctrl_point_count, &type);
    if (ret < 1) {
      std::fprintf(
          stderr,
          "Could not read control point count and type from spline file %s\n",
          filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    splines_[j].resize(ctrl_point_count);

    uint i = 0;
    while ((ret = std::fscanf(file, "%f %f %f", &splines_[j][i].x,
                              &splines_[j][i].y, &splines_[j][i].z)) > 0) {
      ++i;
    }
    if (ret == 0) {
      std::fprintf(stderr, "Could not read control point from spline file %s\n",
                   filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    std::fclose(file);
  }

  std::fclose(track_file);

  return kStatus_Ok;
}

static Status InitTexture(const char *img_filepath, GLuint texture_name) {
  assert(img_filepath);

  int w;
  int h;
  int channel_count;
  uchar *data =
      stbi_load(img_filepath, &w, &h, &channel_count, kRgbaChannel__Count);
  if (!data) {
    std::fprintf(stderr, "Could not load texture from file %s.\n",
                 img_filepath);
    return kStatus_IoError;
  }

  glBindTexture(GL_TEXTURE_2D, texture_name);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // set anisotropic texture filtering
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                  0.5f * fLargest);

  // query for any errors
  GLenum errCode = glGetError();
  if (errCode != 0) {
    std::fprintf(stderr, "Texture initialization error. Error code: %d.\n",
                 errCode);
    stbi_image_free(data);
    return kStatus_IoError;
  }

  stbi_image_free(data);

  return kStatus_Ok;
}

Status SaveScreenshot(const char *filepath, int window_w, int window_h) {
  std::vector<uchar> screenshot(window_w * window_h * kRgbChannel__Count);

  glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE,
               screenshot.data());

  stbi_flip_vertically_on_write(1);
  int ret = stbi_write_jpg(filepath, window_w, window_h, kRgbChannel__Count,
                           screenshot.data(), 95);
  if (ret == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file.\n");
    return kStatus_IoError;
  }

  return kStatus_Ok;
}

static uint screenshot_count = 0;
static uint record_video = 0;

static uint window_w = 1280;
static uint window_h = 720;

static uint frame_count = 0;

static OpenGLMatrix *matrix;

static GLuint program_names[kVertexFormat__Count];

static MouseState mouse_state = {};

static WorldStateOp world_state_op = kWorldStateOp_Rotate;

static WorldState world_state = {{}, {}, {1, 1, 1}};

static uint camera_path_index = 0;

static CameraPathVertices camera_path_vertices;
static std::vector<GLuint> rail_indices;
static TexturedVertices crosstie_vertices;
static SplineVertices spline_vertices;

static GLuint textures[kTexture_Count];
static GLuint vao_names[kVertexFormat__Count];
static GLuint vbo_names[kVbo_Count];

const glm::vec3 scene_aabb_center = {};
const glm::vec3 scene_aabb_size(SCENE_AABB_SIDE_LEN);

static void Timer(int val) {
  if (val) {
    char *temp = new char[512 + strlen(kWindowTitle)];
    // Update title bar info
    sprintf(temp, "%s: %d fps , %d x %d resolution", kWindowTitle,
            frame_count * 30, window_w, window_h);
    glutSetWindowTitle(temp);
    delete[] temp;

    if (record_video) {  // take a screenshot
      temp = new char[8];
      sprintf(temp, "%03d.jpg", screenshot_count);
      SaveScreenshot(temp, window_w, window_h);
      std::printf("Saved screenshot to file %s.\n", temp);
      delete[] temp;
      ++screenshot_count;
    }
  }

  frame_count = 0;
  glutTimerFunc(33, Timer, 1);  //~30 fps
}

static void OnWindowReshape(int w, int h) {
  window_w = w;
  window_h = h;

  glViewport(0, 0, window_w, window_h);

  float aspect = (float)window_w / window_h;

  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->LoadIdentity();
  matrix->Perspective(FOV_Y, aspect, NEAR_Z, FAR_Z);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // By default in ModelView
}

static void OnPassiveMouseMotion(int x, int y) {
  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnMouseDrag(int x, int y) {
  // the change in mouse position since the last invocation of this function
  glm::vec2 pos_delta = {x - mouse_state.position.x,
                         y - mouse_state.position.y};

  switch (world_state_op) {
    case kWorldStateOp_Translate: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.translation.x += pos_delta.x * 0.1f;
        world_state.translation.y -= pos_delta.y * 0.1f;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.translation.z += pos_delta.y;  // * 0.1;
      }
      break;
    }
    case kWorldStateOp_Rotate: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.rotation.x += pos_delta.y;
        world_state.rotation.y += pos_delta.x;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.rotation.z += pos_delta.y;
      }
      break;
    }
    case kWorldStateOp_Scale: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.scale.x *= 1 + pos_delta.x * 0.01f;
        world_state.scale.y *= 1 - pos_delta.y * 0.01f;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.scale.z *= 1 - pos_delta.y * 0.01f;
      }
      break;
    }
  }

  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnMousePressOrRelease(int button, int state, int x, int y) {
  Button b = FromGlButton(button);
  switch (state) {
    case GLUT_DOWN: {
      PressButton(&mouse_state, b);
      break;
    }
    case GLUT_UP: {
      ReleaseButton(&mouse_state, b);
      break;
    }
    default: {
      assert(false);
    }
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers()) {
    case GLUT_ACTIVE_CTRL: {
      world_state_op = kWorldStateOp_Translate;
      break;
    }
    case GLUT_ACTIVE_SHIFT: {
      world_state_op = kWorldStateOp_Scale;
      break;
    }
      // if CTRL and SHIFT are not pressed, we are in rotate mode
    default: {
      world_state_op = kWorldStateOp_Rotate;
      break;
    }
  }

  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnKeyPress(uchar key, int x, int y) {
  switch (key) {
    case 27: {  // ESC key
      exit(0);
      break;
    }
    case 'i': {
      SaveScreenshot("screenshot.jpg", window_w, window_h);
      break;
    }
    case 'v': {
      if (record_video == 1) {
        screenshot_count = 0;
      }
      record_video = !record_video;
      break;
    }
  }
}

static void Idle() {
  if (camera_path_index < Count(&camera_path_vertices)) {
    camera_path_index += 3;
  }

  // for example, here, you can save the screenshots to disk (to make the
  // animation)

  // make the screen update
  glutPostRedisplay();
}

static void Display() {
  ++frame_count;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  matrix->LoadIdentity();

  {
    auto i = camera_path_index;
    auto &p = camera_path_vertices.positions;
    auto &t = spline_vertices.tangents;
    auto &n = camera_path_vertices.normals;
    matrix->LookAt(p[i].x, p[i].y, p[i].z, p[i].x + t[i].x, p[i].y + t[i].y,
                   p[i].z + t[i].z, n[i].x, n[i].y, n[i].z);
  }

  float model_view_mat[16];
  float proj_mat[16];
  GLboolean is_row_major = GL_FALSE;

  /**************************
   * Untextured models
   ***************************/

  GLuint prog = program_names[kVertexFormat_Untextured];
  GLint model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  /* Rails */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vao_names[kVertexFormat_Untextured]);

  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(0));
  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(rail_indices.size() / 2 * sizeof(GLuint)));

  glBindVertexArray(0);

  /**********************
   * Textured models
   ***********************/

  prog = program_names[kVertexFormat_Textured];
  model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  glBindVertexArray(vao_names[kVertexFormat_Textured]);

  GLint first = 0;

  /* Ground */

  matrix->PushMatrix();

  float scene_aabb_half_side_len = glm::length(scene_aabb_size) * 0.5f;
  matrix->Translate(0, -scene_aabb_half_side_len * 0.5f, 0);
  matrix->Translate(-scene_aabb_center.x, -scene_aabb_center.y,
                    -scene_aabb_center.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureGround]);
  glDrawArrays(GL_TRIANGLES, first, GROUND_VERTEX_COUNT);
  first += GROUND_VERTEX_COUNT;

  /* Sky */

  matrix->PushMatrix();

  matrix->Translate(-scene_aabb_center.x, -scene_aabb_center.y,
                    -scene_aabb_center.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureSky]);
  glDrawArrays(GL_TRIANGLES, first, SKY_VERTEX_COUNT);
  first += SKY_VERTEX_COUNT;

  /* Crossties */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureCrosstie]);
  for (uint offset = 0; offset < Count(&crosstie_vertices); offset += 36) {
    glDrawArrays(GL_TRIANGLES, first + offset, 36);
  }

  glBindVertexArray(0);

  glutSwapBuffers();
}

int main(int argc, char **argv) {
  if (argc < 5) {
    std::fprintf(stderr,
                 "usage: %s <track-file> <ground-texture> <sky-texture> "
                 "<crosstie-texture>\n",
                 argv[0]);
    return EXIT_FAILURE;
  }

  glutInit(&argc, argv);

#ifdef __APPLE__
  glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB |
                      GLUT_DEPTH | GLUT_STENCIL);
#else
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

  glutInitWindowSize(window_w, window_h);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(kWindowTitle);

  std::printf("OpenGL Info: \n");
  std::printf("  Version: %s\n", glGetString(GL_VERSION));
  std::printf("  Renderer: %s\n", glGetString(GL_RENDERER));
  std::printf("  Shading Language Version: %s\n",
              glGetString(GL_SHADING_LANGUAGE_VERSION));

  glutDisplayFunc(Display);
  glutIdleFunc(Idle);
  glutMotionFunc(OnMouseDrag);
  glutPassiveMotionFunc(OnPassiveMouseMotion);
  glutMouseFunc(OnMousePressOrRelease);
  glutReshapeFunc(OnWindowReshape);
  glutKeyboardFunc(OnKeyPress);
  glutTimerFunc(0, Timer, 0);

// init glew
#ifdef __APPLE__
  // nothing is needed on Apple
#else
  // Windows, Linux
  GLint result = glewInit();
  if (result != GLEW_OK) {
    std::cout << "error: " << glewGetErrorString(result) << std::endl;
    exit(EXIT_FAILURE);
  }
#endif

  for (int i = 0; i < kVertexFormat__Count; ++i) {
    std::vector<GLuint> shader_names(kShaderType__Count);
    for (int j = 0; j < kShaderType__Count; ++j) {
      std::string content;

      Status status = LoadFile(kShaderFilepaths[i][j], &content);
      if (status != kStatus_Ok) {
        std::fprintf(stderr, "Failed to load shader file.\n");
        return EXIT_FAILURE;
      }

      status = MakeShader(&content, (ShaderType)j, &shader_names[j]);
      if (status != kStatus_Ok) {
        std::fprintf(stderr, "Failed to make shader from file %s.\n",
                     kShaderFilepaths[i][j]);
        return EXIT_FAILURE;
      }
    }

    Status status = MakeProgram(&shader_names, &program_names[i]);
    if (status != kStatus_Ok) {
      std::fprintf(stderr,
                   "Failed to make shader program for vertex format \"%s\".\n",
                   String((VertexFormat)i));
      return EXIT_FAILURE;
    }
  }

  TexturedVertices ground_vertices;
  AxisAlignedXzSquarePlane(SCENE_AABB_SIDE_LEN,
                           SCENE_AABB_SIDE_LEN * 0.25f * 0.5f,
                           &ground_vertices);

  TexturedVertices sky_vertices;
  AxisAlignedBox(SCENE_AABB_SIDE_LEN, 1, &sky_vertices);

  std::vector<std::vector<glm::vec3>> splines;
  Status status = LoadSplines(argv[1], &splines);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Could not load splines.\n");
  }
  std::printf("Loaded spline count: %lu\n", splines.size());

  for (uint i = 0; i < splines.size(); ++i) {
    std::printf("Control point count in spline %u: %lu\n", i,
                splines[i].size());
  }

  EvalCatmullRomSpline(&splines[0], &spline_vertices);

  MakeCameraPath(&spline_vertices, &camera_path_vertices);

  glm::vec4 rail_color(RAIL_COLOR_RED, RAIL_COLOR_GREEN, RAIL_COLOR_BLUE,
                       RAIL_COLOR_ALPHA);
  std::vector<Vertex> rail_vertices;
  MakeRails(&camera_path_vertices, &rail_color, RAIL_HEAD_W, RAIL_HEAD_H,
            RAIL_WEB_W, RAIL_WEB_H, RAIL_GAUGE, -2, &rail_vertices,
            &rail_indices);

  MakeCrossties(&camera_path_vertices, CROSSTIE_SEPARATION_DIST,
                CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR,
                &crosstie_vertices);

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  glGenTextures(kTexture_Count, textures);

  InitTexture(argv[2], textures[kTextureGround]);
  InitTexture(argv[3], textures[kTextureSky]);
  InitTexture(argv[4], textures[kTextureCrosstie]);

  glGenBuffers(kVbo_Count, vbo_names);

  assert(SKY_VERTEX_COUNT == Count(&sky_vertices));

  uint vertex_count =
      GROUND_VERTEX_COUNT + SKY_VERTEX_COUNT + Count(&crosstie_vertices);

  // Buffer textured vertices
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboTexturedVertices]);

    uint buffer_size = vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec2));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = GROUND_VERTEX_COUNT * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    ground_vertices.positions.data());
    offset += size;
    size = SKY_VERTEX_COUNT * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    sky_vertices.positions.data());
    offset += size;
    size = Count(&crosstie_vertices) * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crosstie_vertices.positions.data());

    offset += size;
    size = GROUND_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    ground_vertices.tex_coords.data());
    offset += size;
    size = SKY_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    sky_vertices.tex_coords.data());
    offset += size;
    size = Count(&crosstie_vertices) * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crosstie_vertices.tex_coords.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer untextured vertices
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboUntexturedVertices]);
    glBufferData(GL_ARRAY_BUFFER, rail_vertices.size() * sizeof(Vertex),
                 rail_vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  glGenVertexArrays(kVertexFormat__Count, vao_names);

  // Setup textured VAO
  {
    GLuint prog = program_names[kVertexFormat_Textured];
    GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
    GLuint tex_coord_loc = glGetAttribLocation(prog, "vert_tex_coord");

    glBindVertexArray(vao_names[kVertexFormat_Textured]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboTexturedVertices]);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(tex_coord_loc, 2, GL_FLOAT, GL_FALSE,
                          sizeof(glm::vec2),
                          BUFFER_OFFSET(vertex_count * sizeof(glm::vec3)));
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_coord_loc);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Setup untextured VAO
  {
    GLuint prog = program_names[kVertexFormat_Untextured];
    GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
    GLuint color_loc = glGetAttribLocation(prog, "vert_color");

    glBindVertexArray(vao_names[kVertexFormat_Untextured]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboUntexturedVertices]);

    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          BUFFER_OFFSET(sizeof(glm::vec3)));

    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(color_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVboRailIndices]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rail_indices.size() * sizeof(GLuint),
                 rail_indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // sink forever into the glut loop
  glutMainLoop();
}
