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

#include "cli.hpp"
#include "main.hpp"
#include "models.hpp"
#include "opengl.hpp"
#include "scene.hpp"
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

// View frustum
#define FOV_Y 45
#define NEAR_Z 0.01
#define FAR_Z 10000

#define SPLINE_MAX_LINE_LEN 0.5
#define CAMERA_PATH_INDEX_STEP_PER_FRAME 3

const char *kUsageMessage =
    "usage: %s <track-filepath> <ground-texture-filepath> "
    "<sky-texture-filepath> <crosstie-texture-filepath>\n";

const char *kWindowTitle = "rcoaster";

const char *const kVertexFormatStrings[kVertexFormat__Count]{"textured",
                                                             "colored"};

const char *const kShaderFilepaths[kVertexFormat__Count][kShaderType__Count] = {
    {"shaders/textured.vert.glsl", "shaders/textured.frag.glsl"},
    {"shaders/colored.vert.glsl", "shaders/colored.frag.glsl"}};

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

Status SaveScreenshot(const char *filepath, uint window_w, uint window_h) {
  assert(filepath);
  assert(window_w >= 0);
  assert(window_h >= 0);

  uchar *buffer = new uchar[window_w * window_h * kRgbChannel__Count];

  glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE, buffer);
  stbi_flip_vertically_on_write(1);
  int rc = stbi_write_jpg(filepath, window_w, window_h, kRgbChannel__Count,
                          buffer, 95);

  delete[] buffer;

  if (rc == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file %s\n", filepath);
    return kStatus_IoError;
  }

  return kStatus_Ok;
}

static Scene scene;

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

static GLuint program_names[kVertexFormat__Count];
static GLuint textures[kTexture__Count];
static GLuint vao_names[kVertexFormat__Count];
static GLuint vbo_names[kVbo__Count];

static glm::mat4 projection;

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
#ifdef __APPLE__
      std::exit(EXIT_SUCCESS);
#elif defined(linux)
      glutLeaveMainLoop();
#else
#error Unsupported platform.
#endif
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
  if (camera_path_index < camera_path_vertices.count) {
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

  uint i = camera_path_index;
  glm::mat4 model_view = glm::lookAt(
      camera_path_vertices.positions[i],
      camera_path_vertices.positions[i] + camera_path_vertices.tangents[i],
      camera_path_vertices.normals[i]);

  GLboolean is_row_major = GL_FALSE;

  /*********************
   * Colored models
   *********************/

  GLuint prog = program_names[kVertexFormat_Colored];
  GLint model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  // Rails

  glm::mat4 rails_model_view = glm::translate(model_view, scene.rails.position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(rails_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindVertexArray(vao_names[kVertexFormat_Colored]);

  glDrawElements(GL_TRIANGLES, scene.rails.vertices.indices.size() / 2,
                 GL_UNSIGNED_INT, BUFFER_OFFSET(0));
  glDrawElements(
      GL_TRIANGLES, scene.rails.vertices.indices.size() / 2, GL_UNSIGNED_INT,
      BUFFER_OFFSET(scene.rails.vertices.indices.size() / 2 * sizeof(GLuint)));

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

  glm::mat4 ground_model_view =
      glm::translate(model_view, scene.ground.position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(ground_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Ground]);
  glDrawArrays(GL_TRIANGLES, first, scene.ground.vertices.count);
  first += scene.ground.vertices.count;

  // Sky

  glm::mat4 sky_model_view = glm::translate(model_view, scene.sky.position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(sky_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Sky]);
  glDrawArrays(GL_TRIANGLES, first, scene.sky.vertices.count);
  first += scene.sky.vertices.count;

  // Crossties

  glm::mat4 crossties_model_view =
      glm::translate(model_view, scene.crossties.position);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major,
                     glm::value_ptr(crossties_model_view));
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, glm::value_ptr(projection));

  glBindTexture(GL_TEXTURE_2D, textures[kTexture_Crossties]);
  for (uint offset = 0; offset < scene.crossties.vertices.count; offset += 36) {
    glDrawArrays(GL_TRIANGLES, first + offset, 36);
  }

  glBindVertexArray(0);

  glutSwapBuffers();
}

static void Init(SceneConfig *cfg) {
  assert(cfg);

  cfg->aabb_side_len = 256;

  cfg->ground_position = {0, -cfg->aabb_side_len * (1.0f / 4), 0};
  cfg->ground_tex_repeat_count = 36;

  cfg->sky_position = {};
  cfg->sky_tex_repeat_count = 1;

  cfg->rails_position = {};
  cfg->rails_color = {0.5, 0.5, 0.5, 1};
  cfg->rails_head_w = 0.2;
  cfg->rails_head_h = 0.1;
  cfg->rails_web_w = 0.1;
  cfg->rails_web_h = 0.1;
  cfg->rails_gauge = 2;
  cfg->rails_pos_offset_in_campath_norm_dir = -2;

  cfg->crossties_position = {};
  cfg->crossties_separation_dist = 1;
  cfg->crossties_pos_offset_in_campath_norm_dir = -2;
}

void ConfigureGlut(int argc, char **argv, uint window_w, uint window_h,
                   uint window_x, uint window_y, const char *window_title) {
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
  glutInitWindowPosition(window_x, window_y);
  glutCreateWindow(window_title);

  glutDisplayFunc(Display);
  glutIdleFunc(Idle);
  glutMotionFunc(OnMouseDrag);
  glutPassiveMotionFunc(OnPassiveMouseMotion);
  glutMouseFunc(OnMousePressOrRelease);
  glutReshapeFunc(OnWindowReshape);
  glutKeyboardFunc(OnKeyPress);
}

void DefaultInit(Config *c) {
  assert(c);
  c->is_verbose = 0;
}

static Status ParseConfig(uint argc, char *argv[], Config *cfg) {
  assert(argv);
  assert(cfg);

  cli::Opt opts[] = {
      {"verbose", cli::kOptArgType_Int, &cfg->is_verbose},
  };
  uint size = sizeof(opts) / sizeof(opts[0]);
  uint argi;
  cli::Status st = ParseOpts(argc, argv, opts, size, &argi);
  if (st != cli::kStatus_Ok) {
    std::fprintf(stderr, "Failed to parse options: %s\n",
                 cli::StatusMessage(st));
    return kStatus_UnspecifiedError;
  }

  if (argi >= argc) {
    std::fprintf(stderr, "Missing required track filepath argument.\n");
    std::fprintf(stderr, kUsageMessage, argv[0]);
    return kStatus_UnspecifiedError;
  }

  cfg->track_filepath = argv[argi];

  ++argi;
  if (argi >= argc) {
    std::fprintf(stderr,
                 "Missing required ground texture filepath argument.\n");
    std::fprintf(stderr, kUsageMessage, argv[0]);
    return kStatus_UnspecifiedError;
  }

  cfg->ground_texture_filepath = argv[argi];

  ++argi;
  if (argi >= argc) {
    std::fprintf(stderr, "Missing required sky texture filepath argument.\n");
    std::fprintf(stderr, kUsageMessage, argv[0]);
    return kStatus_UnspecifiedError;
  }

  cfg->sky_texture_filepath = argv[argi];

  ++argi;
  if (argi >= argc) {
    std::fprintf(stderr,
                 "Missing required crossties texture filepath argument.\n");
    std::fprintf(stderr, kUsageMessage, argv[0]);
    return kStatus_UnspecifiedError;
  }

  cfg->crossties_texture_filepath = argv[argi];

  return kStatus_Ok;
}

int main(int argc, char **argv) {
  ConfigureGlut(argc, argv, window_w, window_h, 0, 0, kWindowTitle);

  Config cfg;
  DefaultInit(&cfg);
  Status status = ParseConfig(argc, argv, &cfg);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Failed to parse config.\n");
    return EXIT_FAILURE;
  }

  if (cfg.is_verbose) {
    std::printf("OpenGL Info: \n");
    std::printf("  Version: %s\n", glGetString(GL_VERSION));
    std::printf("  Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("  Shading Language Version: %s\n",
                glGetString(GL_SHADING_LANGUAGE_VERSION));
  }

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
  status = LoadSplines(cfg.track_filepath, &splines);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Could not load splines.\n");
  }
  if (cfg.is_verbose) {
    std::printf("Loaded spline count: %lu\n", splines.size());
    for (uint i = 0; i < splines.size(); ++i) {
      std::printf("Control point count in spline %u: %lu\n", i,
                  splines[i].size());
    }
  }

  // TODO: Support for multiple splines.
  assert(splines.size() == 1);
  EvalCatmullRomSpline(splines[0].data(), splines[0].size(),
                       SPLINE_MAX_LINE_LEN, &camera_path_vertices.positions,
                       &camera_path_vertices.tangents,
                       &camera_path_vertices.count);

  camera_path_vertices.normals = new glm::vec3[camera_path_vertices.count];
  camera_path_vertices.binormals = new glm::vec3[camera_path_vertices.count];
  CameraOrientation(camera_path_vertices.tangents, camera_path_vertices.count,
                    camera_path_vertices.normals,
                    camera_path_vertices.binormals);

  SceneConfig scene_cfg;
  Init(&scene_cfg);
  Make(&scene_cfg, &camera_path_vertices, &scene);

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

    if (cfg.is_verbose) {
      std::printf("Maximum degree of anisotropy: %f\n", max_anisotropy_degree);
    }

    GLfloat anisotropy_degree = max_anisotropy_degree * 0.5f;

    status = InitTexture(cfg.ground_texture_filepath, textures[kTexture_Ground],
                         anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize ground texture.\n");
      return EXIT_FAILURE;
    }

    status = InitTexture(cfg.sky_texture_filepath, textures[kTexture_Sky],
                         anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize sky texture.\n");
      return EXIT_FAILURE;
    }

    status = InitTexture(cfg.crossties_texture_filepath,
                         textures[kTexture_Crossties], anisotropy_degree);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to initialize crosstie texture.\n");
      return EXIT_FAILURE;
    }
  }

  glGenBuffers(kVbo__Count, vbo_names);

  uint textured_vertex_count = scene.ground.vertices.count +
                               scene.sky.vertices.count +
                               scene.crossties.vertices.count;
  uint colored_vertex_count = scene.rails.vertices.count;

  // Buffer textured vertices.
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_TexturedVertices]);

    uint buffer_size =
        textured_vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec2));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = scene.ground.vertices.count * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.ground.vertices.positions);
    offset += size;
    size = scene.sky.vertices.count * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.sky.vertices.positions);
    offset += size;
    size = scene.crossties.vertices.count * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.crossties.vertices.positions);

    offset += size;
    size = scene.ground.vertices.count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.ground.vertices.tex_coords);
    offset += size;
    size = scene.sky.vertices.count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.sky.vertices.tex_coords);
    offset += size;
    size = scene.crossties.vertices.count * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.crossties.vertices.tex_coords);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer colored vertices.
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_ColoredVertices]);

    uint buffer_size =
        colored_vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec4));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = colored_vertex_count * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    scene.rails.vertices.positions);
    offset += size;
    size = colored_vertex_count * sizeof(glm::vec4),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, scene.rails.vertices.colors);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer colored indices.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVbo_RailIndices]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               scene.rails.vertices.indices.size() * sizeof(uint),
               scene.rails.vertices.indices.data(), GL_STATIC_DRAW);
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

  // Setup colored VAO.
  {
    GLuint prog = program_names[kVertexFormat_Colored];
    GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
    GLuint color_loc = glGetAttribLocation(prog, "vert_color");

    glBindVertexArray(vao_names[kVertexFormat_Colored]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVbo_ColoredVertices]);

    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(
        color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4),
        BUFFER_OFFSET(colored_vertex_count * sizeof(glm::vec3)));

    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(color_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVbo_RailIndices]);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  FreeVertices(&scene);

  glutMainLoop();
}
