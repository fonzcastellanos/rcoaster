#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "main.hpp"
#include "models.hpp"
#include "opengl.hpp"
#include "shader.hpp"
#include "status.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include "types.hpp"

#define BUFFER_OFFSET(offset) ((GLvoid *)(offset))

#define WINDOW_TITLE_BUFFER_SIZE 512
#define SCREENSHOT_FILENAME_BUFFER_SIZE 8
#define FILEPATH_BUFFER_SIZE 4096

#define FPS_DISPLAY_TIME_MSEC 1000

// Scene is contained in an axis-aligned bounding box.
#define SCENE_AABB_SIDE_LEN 256

// View frustum
#define FOV_Y 45
#define NEAR_Z 0.01
#define FAR_Z 10000

#define GROUND_TEX_REPEAT_COUNT 36
#define SKY_TEX_REPEAT_COUNT 1

#define RAIL_COLOR_RED 0.5
#define RAIL_COLOR_BLUE 0.5
#define RAIL_COLOR_GREEN 0.5
#define RAIL_COLOR_ALPHA 1
#define RAIL_HEAD_W 0.2
#define RAIL_HEAD_H 0.1
#define RAIL_WEB_W 0.1
#define RAIL_WEB_H 0.1
#define RAIL_GAUGE 2
#define RAIL_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR -2

#define CROSSTIE_SEPARATION_DIST 1
#define CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR -2

#define SPLINE_MAX_LINE_LEN 0.5

#define CAMERA_PATH_INDEX_STEP_PER_FRAME 3

const char *kWindowTitle = "Roller Coaster";

const char *const kVertexFormatStrings[kVertexFormat__Count]{"untextured",
                                                             "textured"};

const char *const kShaderFilepaths[kVertexFormat__Count][kShaderType__Count] = {
    {"shaders/untextured.vert.glsl", "shaders/untextured.frag.glsl"},
    {"shaders/textured.vert.glsl", "shaders/textured.frag.glsl"}};

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
  assert(track_filepath);
  assert(splines);

  auto &splines_ = *splines;

  std::FILE *track_file = std::fopen(track_filepath, "r");
  if (!track_file) {
    std::fprintf(stderr, "Failed to open track file %s.\n", track_filepath);
    return kStatus_IoError;
  }

  uint spline_count;
  int rc = std::fscanf(track_file, "%u", &spline_count);
  if (rc < 1) {
    std::fprintf(stderr, "Failed to read spline count from track file %s.\n",
                 track_filepath);
    std::fclose(track_file);
    return kStatus_IoError;
  }

  splines->resize(spline_count);

  char filepath[FILEPATH_BUFFER_SIZE];
  for (uint i = 0; i < spline_count; ++i) {
    rc = std::fscanf(track_file, "%s", filepath);
    if (rc < 1) {
      std::fprintf(stderr,
                   "Failed to read path of spline file from track file %s.\n",
                   track_filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    FILE *file = std::fopen(filepath, "r");
    if (!file) {
      std::fprintf(stderr, "Failed to open spline file %s.\n", filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    uint ctrl_point_count;
    uint type;
    rc = std::fscanf(file, "%u %u", &ctrl_point_count, &type);
    if (rc < 1) {
      std::fprintf(stderr,
                   "Failed to read control point count and spline type from "
                   "spline file %s.\n",
                   filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    splines_[i].resize(ctrl_point_count);

    uint j = 0;
    while ((rc = std::fscanf(file, "%f %f %f", &splines_[i][j].x,
                             &splines_[i][j].y, &splines_[i][j].z)) > 0) {
      ++j;
    }
    if (rc == 0) {
      std::fprintf(stderr,
                   "Failed to read control point from spline file %s.\n",
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

static Status InitTexture(const char *img_filepath, GLuint texture_name,
                          GLfloat anisotropy_degree) {
  assert(img_filepath);

  int w;
  int h;
  int channel_count;
  uchar *data =
      stbi_load(img_filepath, &w, &h, &channel_count, kRgbaChannel__Count);
  if (!data) {
    std::fprintf(stderr, "Failed to load image file %s.\n", img_filepath);
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

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                  anisotropy_degree);

  stbi_image_free(data);

  return kStatus_Ok;
}

Status SaveScreenshot(const char *filepath, int window_w, int window_h) {
  assert(filepath);
  assert(window_w >= 0);
  assert(window_h >= 0);

  std::vector<uchar> screenshot(window_w * window_h * kRgbChannel__Count);

  glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE,
               screenshot.data());

  stbi_flip_vertically_on_write(1);
  int rc = stbi_write_jpg(filepath, window_w, window_h, kRgbChannel__Count,
                          screenshot.data(), 95);
  if (rc == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file %s\n", filepath);
    return kStatus_IoError;
  }

  return kStatus_Ok;
}

static uint screenshot_count;
static uint record_video;

static uint window_w = 1280;
static uint window_h = 720;

static uint frame_count;
static int previous_time;
static uint avg_fps;

static MouseState mouse_state;

static WorldStateOp world_state_op = kWorldStateOp_Rotate;

static WorldState world_state = {{}, {}, {1, 1, 1}};

static uint camera_path_index;
static CameraPathVertices camera_path_vertices;

static uint rails_indices_count;
static uint ground_vertex_count;
static uint sky_vertex_count;
static uint crossties_vertex_count;

static GLuint program_names[kVertexFormat__Count];
static GLuint textures[kTexture__Count];
static GLuint vao_names[kVertexFormat__Count];
static GLuint vbo_names[kVbo__Count];

static glm::mat4 projection;

const glm::vec3 ground_position = {0, -SCENE_AABB_SIDE_LEN * (1.0f / 4), 0};
const glm::vec3 sky_position = {};
const glm::vec3 rails_position = {};
const glm::vec3 crossties_position = {};

static void OnWindowReshape(int w, int h) {
  window_w = w;
  window_h = h;

  glViewport(0, 0, window_w, window_h);

  float aspect = (float)window_w / window_h;

  projection = glm::perspective(glm::radians((float)FOV_Y), aspect,
                                (float)NEAR_Z, (float)FAR_Z);
}

static void OnPassiveMouseMotion(int x, int y) {
  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnMouseDrag(int x, int y) {
  // The change in mouse position since the last invocation of this function.
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
    default: {
      assert(false);
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

  // Keep track of whether CTRL and SHIFT keys are pressed.
  switch (glutGetModifiers()) {
    case GLUT_ACTIVE_CTRL: {
      world_state_op = kWorldStateOp_Translate;
      break;
    }
    case GLUT_ACTIVE_SHIFT: {
      world_state_op = kWorldStateOp_Scale;
      break;
    }
    // If CTRL and SHIFT are not pressed, we are in rotate mode.
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
    camera_path_index += CAMERA_PATH_INDEX_STEP_PER_FRAME;
  }

  int current_time = glutGet(GLUT_ELAPSED_TIME);
  int elapsed_time = current_time - previous_time;
  if (elapsed_time > FPS_DISPLAY_TIME_MSEC) {
    avg_fps = frame_count * (1000.0f / elapsed_time);
    frame_count = 0;
    previous_time = current_time;
  }

  char *title = new char[WINDOW_TITLE_BUFFER_SIZE];
  std::sprintf(title, "%s: %d fps , %d x %d resolution", kWindowTitle, avg_fps,
               window_w, window_h);
  glutSetWindowTitle(title);
  delete[] title;

  if (record_video) {
    char *filename = new char[SCREENSHOT_FILENAME_BUFFER_SIZE];
    std::sprintf(filename, "%03d.jpg", screenshot_count);
    SaveScreenshot(filename, window_w, window_h);
    std::printf("Saved screenshot to file %s.\n", filename);
    delete[] filename;

    ++screenshot_count;
  }

  glutPostRedisplay();
}

static void Display() {
  ++frame_count;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  auto i = camera_path_index;
  glm::mat4 model_view = glm::lookAt(
      camera_path_vertices.positions[i],
      camera_path_vertices.positions[i] + camera_path_vertices.tangents[i],
      camera_path_vertices.normals[i]);

  GLboolean is_row_major = GL_FALSE;

  /*********************
   * Untextured models
   *********************/

  GLuint prog = program_names[kVertexFormat_Untextured];
  GLint model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  // Rails

  glm::mat4 rails_model_view = glm::translate(model_view, rails_position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(rails_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindVertexArray(vao_names[kVertexFormat_Untextured]);

  glDrawElements(GL_TRIANGLES, rails_indices_count / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(0));
  glDrawElements(GL_TRIANGLES, rails_indices_count / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(rails_indices_count / 2 * sizeof(GLuint)));

  glBindVertexArray(0);

  /*******************
   * Textured models
   *******************/

  prog = program_names[kVertexFormat_Textured];
  model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  glBindVertexArray(vao_names[kVertexFormat_Textured]);

  GLint first = 0;

  // Ground

  glm::mat4 ground_model_view = glm::translate(model_view, ground_position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(ground_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Ground]);
  glDrawArrays(GL_TRIANGLES, first, ground_vertex_count);
  first += ground_vertex_count;

  // Sky

  glm::mat4 sky_model_view = glm::translate(model_view, sky_position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(sky_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Sky]);
  glDrawArrays(GL_TRIANGLES, first, sky_vertex_count);
  first += sky_vertex_count;

  // Crossties

  glm::mat4 crossties_model_view =
      glm::translate(model_view, crossties_position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(crossties_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Crosstie]);
  for (uint offset = 0; offset < crossties_vertex_count; offset += 36) {
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

  /*************************************
   * Configure GLUT.
   *************************************/

  glutInit(&argc, argv);

#ifdef __APPLE__
  glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB |
                      GLUT_DEPTH | GLUT_STENCIL);
#else
  glutInitContextVersion(3, 2);
  glutInitContextProfile(GLUT_CORE_PROFILE);
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

#ifdef linux
  GLenum result = glewInit();
  if (result != GLEW_OK) {
    std::fprintf(stderr, "glewInit failed: %s", glewGetErrorString(result));
    return EXIT_FAILURE;
  }
#endif

  /************************************
   * Create camera path.
   ************************************/

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

  // TODO: Support for multiple splines.
  assert(splines.size() == 1);
  EvalCatmullRomSpline(&splines[0], SPLINE_MAX_LINE_LEN,
                       &camera_path_vertices.positions,
                       &camera_path_vertices.tangents);

  CameraOrientation(&camera_path_vertices.tangents,
                    &camera_path_vertices.normals,
                    &camera_path_vertices.binormals);

  /*************************************
   * Create models.
   *************************************/

  std::vector<glm::vec3> ground_positions;
  std::vector<glm::vec2> ground_tex_coords;
  MakeAxisAlignedXzSquarePlane(SCENE_AABB_SIDE_LEN, GROUND_TEX_REPEAT_COUNT,
                               &ground_positions, &ground_tex_coords);
  ground_vertex_count = ground_positions.size();

  std::vector<glm::vec3> sky_positions;
  std::vector<glm::vec2> sky_tex_coords;
  MakeAxisAlignedBox(SCENE_AABB_SIDE_LEN, SKY_TEX_REPEAT_COUNT, &sky_positions,
                     &sky_tex_coords);
  sky_vertex_count = sky_positions.size();

  glm::vec4 rail_color(RAIL_COLOR_RED, RAIL_COLOR_GREEN, RAIL_COLOR_BLUE,
                       RAIL_COLOR_ALPHA);
  std::vector<glm::vec3> rail_positions;
  std::vector<glm::vec4> rail_colors;
  std::vector<GLuint> rail_indices;
  MakeRails(&camera_path_vertices, &rail_color, RAIL_HEAD_W, RAIL_HEAD_H,
            RAIL_WEB_W, RAIL_WEB_H, RAIL_GAUGE,
            RAIL_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR, &rail_positions,
            &rail_colors, &rail_indices);
  rails_indices_count = rail_indices.size();

  std::vector<glm::vec3> crossties_positions;
  std::vector<glm::vec2> crossties_tex_coords;
  MakeCrossties(&camera_path_vertices, CROSSTIE_SEPARATION_DIST,
                CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR,
                &crossties_positions, &crossties_tex_coords);
  crossties_vertex_count = crossties_positions.size();

  /************************************
   * Setup OpenGL state.
   ************************************/

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  // Setup shader programs.
  for (int i = 0; i < kVertexFormat__Count; ++i) {
    std::vector<GLuint> shader_names(kShaderType__Count);
    for (int j = 0; j < kShaderType__Count; ++j) {
      std::string content;

      Status status = LoadFile(kShaderFilepaths[i][j], &content);
      if (status != kStatus_Ok) {
        std::fprintf(stderr, "Failed to load shader file.\n");
        return EXIT_FAILURE;
      }

      status = MakeShaderObj(&content, (ShaderType)j, &shader_names[j]);
      if (status != kStatus_Ok) {
        std::fprintf(stderr, "Failed to make shader object from file %s.\n",
                     kShaderFilepaths[i][j]);
        return EXIT_FAILURE;
      }
    }

    Status status = MakeShaderProg(&shader_names, &program_names[i]);
    if (status != kStatus_Ok) {
      std::fprintf(stderr,
                   "Failed to make shader program for vertex format \"%s\".\n",
                   String((VertexFormat)i));
      return EXIT_FAILURE;
    }
  }

  // Setup textures.
  glGenTextures(kTexture__Count, textures);
  {
    GLfloat max_anisotropy_degree;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy_degree);

    std::printf("Maximum degree of anisotropy: %f\n", max_anisotropy_degree);

    GLfloat anisotropy_degree = max_anisotropy_degree * 0.5f;

    status = InitTexture(argv[2], textures[kTexture_Ground], anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize ground texture.\n");
      return EXIT_FAILURE;
    }

    status = InitTexture(argv[3], textures[kTexture_Sky], anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize sky texture.\n");
      return EXIT_FAILURE;
    }

    status =
        InitTexture(argv[4], textures[kTexture_Crosstie], anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize crosstie texture.\n");
      return EXIT_FAILURE;
    }
  }

  glGenBuffers(kVbo__Count, vbo_names);

  uint textured_vertex_count =
      ground_vertex_count + sky_vertex_count + crossties_vertex_count;
  uint untextured_vertex_count = rail_positions.size();

  // Buffer textured vertices.
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_TexturedVertices]);

    uint buffer_size =
        textured_vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec2));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = ground_vertex_count * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, ground_positions.data());
    offset += size;
    size = sky_vertex_count * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, sky_positions.data());
    offset += size;
    size = crossties_vertex_count * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, crossties_positions.data());

    offset += size;
    size = ground_vertex_count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, ground_tex_coords.data());
    offset += size;
    size = sky_vertex_count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, sky_tex_coords.data());
    offset += size;
    size = crossties_vertex_count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, crossties_tex_coords.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer untextured vertices.
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_UntexturedVertices]);

    uint buffer_size =
        untextured_vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec4));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = untextured_vertex_count * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, rail_positions.data());
    offset += size;
    size = untextured_vertex_count * sizeof(glm::vec4),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, rail_colors.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer untextured indices.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVbo_RailIndices]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, rails_indices_count * sizeof(GLuint),
               rail_indices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  glGenVertexArrays(kVertexFormat__Count, vao_names);

  // Setup textured VAO.
  {
    GLuint prog = program_names[kVertexFormat_Textured];
    GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
    GLuint tex_coord_loc = glGetAttribLocation(prog, "vert_tex_coord");

    glBindVertexArray(vao_names[kVertexFormat_Textured]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_TexturedVertices]);

    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(
        tex_coord_loc, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2),
        BUFFER_OFFSET(textured_vertex_count * sizeof(glm::vec3)));

    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_coord_loc);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Setup untextured VAO.
  {
    GLuint prog = program_names[kVertexFormat_Untextured];
    GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
    GLuint color_loc = glGetAttribLocation(prog, "vert_color");

    glBindVertexArray(vao_names[kVertexFormat_Untextured]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_UntexturedVertices]);

    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(
        color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4),
        BUFFER_OFFSET(untextured_vertex_count * sizeof(glm::vec3)));

    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(color_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVbo_RailIndices]);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  glutMainLoop();
}
