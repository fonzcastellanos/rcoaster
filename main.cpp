#include "main.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

#include "cli.hpp"
#include "meshes.hpp"
#include "opengl.hpp"
#include "opengl_renderer.hpp"
#include "scene.hpp"
#include "shader.hpp"
#include "status.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include "types.hpp"

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

Status SaveScreenshot(const char *filepath, uint window_w, uint window_h) {
  assert(filepath);

  uchar *buffer = new uchar[window_w * window_h * kRgbChannel__Count];

  glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE, buffer);
  stbi_flip_vertically_on_write(1);
  int rc = stbi_write_jpg(filepath, window_w, window_h, kRgbChannel__Count,
                          buffer, 95);

  delete[] buffer;

  if (rc == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file %s.\n", filepath);
    return kStatus_IoError;
  }

  return kStatus_Ok;
}

static Config config;
static Scene scene;

static uint screenshot_count;
static uint record_video;

static uint window_w = 1280;
static uint window_h = 720;

static uint frame_count;

static MouseState mouse_state;

static WorldStateOp world_state_op = kWorldStateOp_Rotate;

static WorldState world_state = {{}, {}, {1, 1, 1}};

static uint camera_path_index;

static GLuint program_names[kVertexFormat__Count];
static GLuint vao_names[kVao__Count];
static GLuint vbo_names[kVbo__Count];

static OpenGlRenderer renderer;

static glm::mat4 view_mat;
static glm::mat4 projection_mat;

static void OnWindowReshape(int w, int h) {
  window_w = w;
  window_h = h;

  glViewport(0, 0, w, h);
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

static void ExitGlutMainLoop(int status_code) {
#ifdef __APPLE__
  std::exit(status_code);
#elif defined(linux)
  glutLeaveMainLoop();
#else
#error Unsupported platform.
#endif
}

static void OnKeyPress(uchar key, int x, int y) {
  switch (key) {
    case 27: {  // ESC key
      ExitGlutMainLoop(EXIT_SUCCESS);
      break;
    }
    case 'i': {
      char filepath[FILEPATH_BUFFER_SIZE];
      int rc =
          std::snprintf(filepath, FILEPATH_BUFFER_SIZE, "%s/%s_%03u.jpg",
                        config.screenshot_directory_path,
                        config.screenshot_filename_prefix, screenshot_count);

      if (rc < 0 || rc >= FILEPATH_BUFFER_SIZE) {
        std::fprintf(stderr, "Failed to make screenshot filepath.\n");
        ExitGlutMainLoop(EXIT_FAILURE);
      }

      Status status = SaveScreenshot(filepath, window_w, window_h);
      if (status != kStatus_Ok) {
        std::fprintf(stderr, "Failed to save screenshot.\n");
        ExitGlutMainLoop(EXIT_FAILURE);
      }

      if (config.is_verbose) {
        std::printf("Saved screenshot to file %s.\n", filepath);
      }

      ++screenshot_count;

      break;
    }
    case 'v': {
      record_video = !record_video;
      break;
    }
  }
}

static Status UpdateWindowTitle(uint update_period, uint current_time,
                                const char *title_prefix, uint w, uint h,
                                uint *frame_count) {
  static uint previous_fps_display_time;

  assert(title_prefix);
  assert(frame_count);

  uint delta_time = current_time - previous_fps_display_time;

  if (delta_time < update_period) {
    return kStatus_Ok;
  }

  uint fps = *frame_count * (1000.0f / delta_time);

  char window_title_buffer[512];

  int rc =
      std::snprintf(window_title_buffer, 512, "%s: %u fps , %u x %u resolution",
                    title_prefix, fps, w, h);
  if (rc < 0 || rc >= 512) {
    std::fprintf(stderr, "Failed to form window title.\n");
    return kStatus_UnspecifiedError;
  }

  glutSetWindowTitle(window_title_buffer);

  *frame_count = 0;
  previous_fps_display_time = current_time;

  return kStatus_Ok;
}

static void Idle() {
  static uint previous_idle_callback_time;

  int current_time = glutGet(GLUT_ELAPSED_TIME);

  Status status =
      UpdateWindowTitle(WINDOW_TITLE_UPDATE_PERIOD_MSEC, current_time,
                        kWindowTitlePrefix, window_w, window_h, &frame_count);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Failed to update window title.\n");
    ExitGlutMainLoop(EXIT_FAILURE);
  }

  if (camera_path_index < scene.camspl.mesh_1p1t1n1b->vertices.count) {
    float delta_time = (current_time - previous_idle_callback_time) / 1000.0f;
    camera_path_index += config.camera_speed * delta_time;
  }

  view_mat = glm::lookAt(
      scene.camspl.mesh_1p1t1n1b->vertices.positions[camera_path_index],
      scene.camspl.mesh_1p1t1n1b->vertices.positions[camera_path_index] +
          scene.camspl.mesh_1p1t1n1b->vertices.tangents[camera_path_index],
      scene.camspl.mesh_1p1t1n1b->vertices.normals[camera_path_index]);

  assert(window_h > 0);
  float aspect = (float)window_w / window_h;
  projection_mat =
      glm::perspective(glm::radians(config.view_frustum.fov_y), aspect,
                       config.view_frustum.near_z, config.view_frustum.far_z);

  if (record_video) {
    char filepath[FILEPATH_BUFFER_SIZE];
    int rc = std::snprintf(filepath, FILEPATH_BUFFER_SIZE, "%s/%s_%03u.jpg",
                           config.screenshot_directory_path,
                           config.screenshot_filename_prefix, screenshot_count);
    if (rc < 0 || rc >= FILEPATH_BUFFER_SIZE) {
      std::fprintf(stderr, "Failed to make screenshot filepath.\n");
      ExitGlutMainLoop(EXIT_FAILURE);
    }

    Status status = SaveScreenshot(filepath, window_w, window_h);
    if (status != kStatus_Ok) {
      std::fprintf(stderr, "Failed to save screenshot.\n");
      ExitGlutMainLoop(EXIT_FAILURE);
    }

    ++screenshot_count;
  }

  previous_idle_callback_time = current_time;

  glutPostRedisplay();
}

static void Display() {
  ++frame_count;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  static constexpr GLboolean kIsRowMajor = GL_FALSE;

  /*********************
   * Colored models
   *********************/

  GLuint prog = program_names[kVertexFormat_Colored];
  GLint model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  // Rails

  glBindVertexArray(vao_names[kVao_Colored]);

  {
    glm::mat4 model_view = view_mat * scene.left_rail.world_transform;

    glUniformMatrix4fv(model_view_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(model_view));
    glUniformMatrix4fv(proj_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(projection_mat));

    glDrawElementsBaseVertex(GL_TRIANGLES,
                             scene.left_rail.mesh_1p1c->index_count,
                             GL_UNSIGNED_INT, BUFFER_OFFSET(0), 0);
  }
  {
    glm::mat4 model_view = view_mat * scene.right_rail.world_transform;

    glUniformMatrix4fv(model_view_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(model_view));
    glUniformMatrix4fv(proj_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(projection_mat));

    glDrawElementsBaseVertex(
        GL_TRIANGLES, scene.right_rail.mesh_1p1c->index_count, GL_UNSIGNED_INT,
        BUFFER_OFFSET(scene.left_rail.mesh_1p1c->index_count * sizeof(GLuint)),
        scene.left_rail.mesh_1p1c->vertex_count);
  }

  glBindVertexArray(0);

  /**************************
   * Textured models
   **************************/
  prog = program_names[kVertexFormat_Textured];
  model_view_mat_loc = glGetUniformLocation(prog, "model_view");
  proj_mat_loc = glGetUniformLocation(prog, "projection");

  glUseProgram(prog);

  glBindVertexArray(vao_names[kVao_Textured]);

  GLsizeiptr buf_offset = 0;
  GLint base_vertex = 0;

  // Ground
  {
    glm::mat4 model_view = view_mat * scene.ground.world_transform;

    glUniformMatrix4fv(model_view_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(model_view));
    glUniformMatrix4fv(proj_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(projection_mat));

    glBindTexture(GL_TEXTURE_2D, renderer.texture_names[kTexture_Ground]);

    glDrawElementsBaseVertex(GL_TRIANGLES, scene.ground.mesh_1p1uv->index_count,
                             GL_UNSIGNED_INT, BUFFER_OFFSET(buf_offset),
                             base_vertex);

    buf_offset += scene.ground.mesh_1p1uv->index_count * sizeof(GLuint);
    base_vertex += scene.ground.mesh_1p1uv->vertex_count;
  }

  // Sky
  {
    glm::mat4 model_view = view_mat * scene.sky.world_transform;

    glUniformMatrix4fv(model_view_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(model_view));
    glUniformMatrix4fv(proj_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(projection_mat));

    glBindTexture(GL_TEXTURE_2D, renderer.texture_names[kTexture_Sky]);

    glDrawElementsBaseVertex(GL_TRIANGLES, scene.sky.mesh_1p1uv->index_count,
                             GL_UNSIGNED_INT, BUFFER_OFFSET(buf_offset),
                             base_vertex);

    buf_offset += scene.sky.mesh_1p1uv->index_count * sizeof(GLuint);
    base_vertex += scene.sky.mesh_1p1uv->vertex_count;
  }

  // Crossties
  {
    glm::mat4 model_view = view_mat * scene.crossties.world_transform;

    glUniformMatrix4fv(model_view_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(model_view));
    glUniformMatrix4fv(proj_mat_loc, 1, kIsRowMajor,
                       glm::value_ptr(projection_mat));

    glBindTexture(GL_TEXTURE_2D, renderer.texture_names[kTexture_Crossties]);

    for (uint offset = 0; offset < scene.crossties.mesh_1p1uv->index_count;
         offset += 36) {
      glDrawElementsBaseVertex(
          GL_TRIANGLES, 36, GL_UNSIGNED_INT,
          BUFFER_OFFSET(buf_offset + offset * sizeof(GLuint)), base_vertex);
      base_vertex += 24;
    }
  }

  glBindVertexArray(0);

  glutSwapBuffers();
}

static void InitSceneConfig(const Config *cfg, SceneConfig *scene_cfg) {
  assert(cfg);
  assert(scene_cfg);

  scene_cfg->aabb_side_len = 256;

  scene_cfg->ground_position = {0, -scene_cfg->aabb_side_len * (1.0f / 8), 0};
  scene_cfg->ground_tex_repeat_count = 36;

  scene_cfg->sky_position = {};
  scene_cfg->sky_tex_repeat_count = 1;

  scene_cfg->rails_position = {};
  scene_cfg->rails_color = {0.5, 0.5, 0.5, 1};
  scene_cfg->rails_head_w = 0.2;
  scene_cfg->rails_head_h = 0.1;
  scene_cfg->rails_web_w = 0.1;
  scene_cfg->rails_web_h = 0.1;
  scene_cfg->rails_gauge = 2;
  scene_cfg->rails_pos_offset_in_camspl_norm_dir = -2;

  scene_cfg->crossties_position = {};
  scene_cfg->crossties_separation_dist = 1;
  scene_cfg->crossties_pos_offset_in_camspl_norm_dir = -2;

  scene_cfg->track_filepath = cfg->track_filepath;
  scene_cfg->max_spline_segment_len = cfg->max_spline_segment_len;
  scene_cfg->is_verbose = cfg->is_verbose;
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

void DefaultInit(Config *cfg) {
  assert(cfg);

  cfg->view_frustum.fov_y = 60;
  cfg->view_frustum.far_z = 10000;
  cfg->view_frustum.near_z = 0.01;

  cfg->max_spline_segment_len = 0.5;
  cfg->camera_speed = 100;

  int rc = std::snprintf(cfg->screenshot_directory_path,
                         sizeof(cfg->screenshot_directory_path), ".");

  assert(rc >= 0 && rc < (int)sizeof(cfg->screenshot_directory_path));

  rc = std::snprintf(cfg->screenshot_filename_prefix,
                     sizeof(cfg->screenshot_filename_prefix), "screenshot");

  assert(rc >= 0 && rc < (int)sizeof(cfg->screenshot_filename_prefix));

  cfg->is_verbose = 0;
}

static Status ParseConfig(uint argc, char *argv[], Config *cfg) {
  assert(argv);
  assert(cfg);

  cli::Opt opts[] = {
      {"max-spline-segment-len", cli::kOptArgType_Float,
       &cfg->max_spline_segment_len},
      {"camera-speed", cli::kOptArgType_Float, &cfg->camera_speed},
      {"screenshot-filename-prefix", cli::kOptArgType_String,
       &cfg->screenshot_filename_prefix},
      {"screenshot-directory-path", cli::kOptArgType_String,
       &cfg->screenshot_directory_path},
      {"verbose", cli::kOptArgType_Int, &cfg->is_verbose}};

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
                 "Missing required ground texture filepath "
                 "argument.\n");
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
                 "Missing required crossties texture "
                 "filepath argument.\n");
    std::fprintf(stderr, kUsageMessage, argv[0]);
    return kStatus_UnspecifiedError;
  }

  cfg->crossties_texture_filepath = argv[argi];

  return kStatus_Ok;
}

int main(int argc, char **argv) {
  ConfigureGlut(argc, argv, window_w, window_h, 0, 0, kWindowTitlePrefix);

  DefaultInit(&config);
  Status status = ParseConfig(argc, argv, &config);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Failed to parse config.\n");
    return EXIT_FAILURE;
  }

  if (config.is_verbose) {
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

  SceneConfig scene_cfg;
  InitSceneConfig(&config, &scene_cfg);
  status = MakeScene(&scene_cfg, &scene);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Failed to make scene.\n");
    return EXIT_FAILURE;
  }

  /************************************
   * Setup OpenGL state.
   ************************************/

  Setup(&renderer);
  const char *tex_filepaths[3];
  tex_filepaths[kTexture_Ground] = config.ground_texture_filepath;
  tex_filepaths[kTexture_Sky] = config.sky_texture_filepath;
  tex_filepaths[kTexture_Crossties] = config.crossties_texture_filepath;
  status = SetupTextures(&renderer, tex_filepaths, kTexture__Count,
                         renderer.max_anisotropy_degree * 0.5f);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Failed to setup textures.\n");
    return EXIT_FAILURE;
  }

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
                   "Failed to make shader program for vertex "
                   "format \"%s\".\n",
                   String((VertexFormat)i));
      return EXIT_FAILURE;
    }
  }

  glGenBuffers(kVbo__Count, vbo_names);
  glGenVertexArrays(kVao__Count, vao_names);

  GLuint prog = program_names[kVertexFormat_Textured];
  GLuint pos_loc = glGetAttribLocation(prog, "vert_position");
  GLuint tex_coord_loc = glGetAttribLocation(prog, "vert_tex_coord");
  SubmitMeshes(scene.meshes_1p1uv, scene.mesh_1p1uv_count,
               vbo_names[kVbo_TexturedVertices],
               vbo_names[kVbo_TexturedIndices], vao_names[kVao_Textured],
               pos_loc, tex_coord_loc);

  prog = program_names[kVertexFormat_Colored];
  pos_loc = glGetAttribLocation(prog, "vert_position");
  GLuint color_loc = glGetAttribLocation(prog, "vert_color");
  SubmitMeshes(scene.meshes_1p1c, scene.mesh_1p1c_count,
               vbo_names[kVbo_ColoredVertices], vbo_names[kVbo_RailIndices],
               vao_names[kVao_Colored], pos_loc, color_loc);

  glutMainLoop();
}
